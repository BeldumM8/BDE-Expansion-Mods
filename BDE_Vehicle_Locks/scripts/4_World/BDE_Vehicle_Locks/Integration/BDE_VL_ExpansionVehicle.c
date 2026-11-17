modded class ExpansionVehicle
{
    protected bool m_BDE_VL_HasOwner;
    protected bool m_BDE_VL_RaidEnabled;
    protected float m_BDE_VL_RaidSeconds;

    float BDE_VL_GetRaidHealthEase()
    {
        BDE_VL_RaidSettings settings = BDE_VL_RaidConfig.Get();
        EntityAI entity = GetEntity();
        if (!settings.HealthModifierEnabled || !entity)
        {
            return 1;
        }
        // Zero means no bonus, not zero success chance. One is also neutral.
        float fullEase = Math.Max(1, settings.FullHealthEaseMultiplier);
        float lowEase = Math.Max(1, settings.LowHealthEaseMultiplier);
        float health = Math.Clamp(entity.GetHealth01("", "Health"), 0, 1);
        float threshold = settings.LowHealthThresholdPercent / 100;
        float damageFraction = Math.Clamp((1 - health) / (1 - threshold), 0, 1);
        return fullEase + (lowEase - fullEase) * damageFraction;
    }

    float BDE_VL_GetRaidChance()
    {
        BDE_VL_RaidSettings settings = BDE_VL_RaidConfig.Get();
        BDE_VL_LockDifficulty family = BDE_VL_RaidConfig.GetFamily(GetType());
        float difficulty = BDE_VL_RaidConfig.GetDifficulty(family);
        float chance = settings.MaximumSuccessPercent - (settings.MaximumSuccessPercent - settings.MinimumSuccessPercent) * difficulty;
        return Math.Clamp(chance * BDE_VL_GetRaidHealthEase(), settings.MinimumSuccessPercent, settings.MaximumSuccessPercent);
    }

    bool BDE_VL_CanRaid()
    {
        return m_BDE_VL_RaidEnabled;
    }

    float BDE_VL_GetRaidSeconds()
    {
        return m_BDE_VL_RaidSeconds;
    }

    void BDE_VL_InitRaid()
    {
        BDE_VL_RaidSettings settings = BDE_VL_RaidConfig.Get();
        BDE_VL_LockDifficulty family = BDE_VL_RaidConfig.GetFamily(GetType());
        m_BDE_VL_RaidEnabled = BDE_VL_Settings.Get().EnableVehicleLocks && settings.Enabled && family.Enabled;
        float difficulty = BDE_VL_RaidConfig.GetDifficulty(family);
        m_BDE_VL_RaidSeconds = settings.MinimumSeconds + (settings.MaximumSeconds - settings.MinimumSeconds) * difficulty;
        GetEntity().SetSynchDirty();
    }

    override void Init()
    {
        super.Init();

        EntityAI entity = GetEntity();
        if (entity)
        {
            entity.RegisterNetSyncVariableBool("m_ExpansionVehicle.m_BDE_VL_HasOwner");
            entity.RegisterNetSyncVariableBool("m_ExpansionVehicle.m_BDE_VL_RaidEnabled");
            entity.RegisterNetSyncVariableFloat("m_ExpansionVehicle.m_BDE_VL_RaidSeconds");
        }
    }

    override void DeferredInit()
    {
        super.DeferredInit();

        if (g_Game.IsServer())
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_VL_DeferredOwnershipInit, 250, false);
    }

    void BDE_VL_DeferredOwnershipInit()
    {
        BDE_VL_InitRaid();
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleLocks)
            return;

        BDE_VL_SyncMarketOwner(true);
        string ownerUID = GetOwnerUID();
        string ownerName = GetOwnerName();

        if (ownerUID != string.Empty)
        {
            m_BDE_VL_HasOwner = true;

            if (settings.AutoMigrateExpansionOwners && HasKey())
            {
                if (settings.DeleteLoadedExpansionKeysOnAutoMigration)
                    BDE_VL_DeleteLoadedPairedKeys();

                ResetKeyPairing();
                BDE_VL_SetOwnerByUID(ownerUID, ownerName, true);

                if (settings.LockMigratedVehicles)
                    BDE_VL_SetLocked(true);

                BDE_VL_Logger.Log("Automatically migrated Expansion ownership for " + GetType() + " to " + ownerName + " (" + ownerUID + ")");
            }
            else
            {
                EntityAI entity = GetEntity();
                if (entity)
                    entity.SetSynchDirty();
            }
        }
    }

    bool BDE_VL_HasOwner()
    {
        if (g_Game.IsServer())
            return GetOwnerUID() != string.Empty;

        return m_BDE_VL_HasOwner;
    }

    void BDE_VL_SetOwnerFromPlayer(notnull PlayerBase player, bool send = true)
    {
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return;

        BDE_VL_SetOwnerByUID(identity.GetId(), identity.GetName(), send);
    }

    void BDE_VL_SetOwnerByUID(string ownerUID, string ownerName, bool send = true)
    {
        if (ownerUID == string.Empty)
            return;

        if (m_KeyChain)
            m_KeyChain.Expansion_AssignOwner(ownerUID, ownerName, send);
        else
            AssignOwner(ownerUID, ownerName, send);

        m_BDE_VL_HasOwner = true;
        BDE_VL_SyncMarketOwner(send);

        EntityAI entity = GetEntity();
        if (entity)
            entity.SetSynchDirty();
    }

    void BDE_VL_ResetOwner()
    {
        if (m_KeyChain)
            m_KeyChain.Expansion_ResetOwner();
        else
            ResetOwner();

        m_BDE_VL_HasOwner = false;
        BDE_VL_SyncMarketOwner(true);
        BDE_VL_SetLocked(false);

        EntityAI entity = GetEntity();
        if (entity)
            entity.SetSynchDirty();
    }

    // Expansion synchronizes the owner's name at index 0, but not their UID.
    // Keep the UID at index 1 for client market filtering; the server still
    // resolves the authoritative owner when it validates the sale.
    void BDE_VL_SyncMarketOwner(bool send)
    {
        if (!g_Game.IsServer() || !m_NetsyncData)
        {
            return;
        }
        m_NetsyncData.Set(0, GetOwnerName());
        m_NetsyncData.Set(1, GetOwnerUID());
        if (send)
        {
            m_NetsyncData.Send(null);
        }
    }

    string BDE_VL_GetMarketOwnerUID()
    {
        if (g_Game.IsServer())
        {
            return GetOwnerUID();
        }
        string ownerUID;
        if (m_NetsyncData)
        {
            m_NetsyncData.Get(1, ownerUID);
        }
        return ownerUID;
    }

    bool BDE_VL_IsGroupMember(notnull PlayerBase player)
    {
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return false;

        string ownerUID = GetOwnerUID();
        string playerUID = identity.GetId();
        if (ownerUID == string.Empty || playerUID == string.Empty || ownerUID == playerUID)
            return false;

        ExpansionPartyModule partyModule;
        if (!CF_Modules<ExpansionPartyModule>.Get(partyModule) || !partyModule)
            return false;

        // Force the module to search its persisted party array. The default lookup can
        // return NULL early for an online player while PlayerBase party data is still
        // being initialized. This also works when the vehicle owner is offline.
        ExpansionPartyPlayerData ownerData = partyModule.GetPartyPlayerData(ownerUID, true);
        ExpansionPartyPlayerData playerData = partyModule.GetPartyPlayerData(playerUID, true);
        if (!ownerData || !playerData)
            return false;

        ExpansionPartyData ownerParty = ownerData.GetParty();
        ExpansionPartyData playerParty = playerData.GetParty();
        if (!ownerParty || !playerParty)
            return false;

        int ownerPartyID = ownerParty.GetPartyID();
        int playerPartyID = playerParty.GetPartyID();
        return ownerPartyID >= 0 && ownerPartyID == playerPartyID;
    }

    bool BDE_VL_CanAccess(notnull PlayerBase player)
    {
        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return false;

        #ifdef JM_COT
        if (GetCommunityOnlineToolsBase().IsActive(player))
            return true;
        #endif

        if (identity.GetId() == GetOwnerUID())
            return true;

        return BDE_VL_IsGroupMember(player);
    }

    bool BDE_VL_HasCrew()
    {
        for (int i = 0; i < CrewSize(); i++)
        {
            if (CrewMember(i))
                return true;
        }

        return false;
    }

    void BDE_VL_SetLocked(bool locked)
    {
        ExpansionVehicleLockState previousLockState = GetLockState();

        if (locked)
        {
            SetLockState(ExpansionVehicleLockState.FORCEDLOCKED);
            OnLocked(previousLockState);
        }
        else
        {
            ForceUnlock(ExpansionVehicleLockState.UNLOCKED);
        }
    }

    bool BDE_VL_ClaimUnowned(notnull PlayerBase player)
    {
        if (BDE_VL_HasOwner() || HasKey())
            return false;

        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleLocks)
            return false;

        EntityAI claimItem;
        if (!settings.AllowFreeClaiming)
        {
            claimItem = player.GetItemInHands();
            if (!claimItem || settings.ClaimItemClassName == string.Empty || claimItem.GetType() != settings.ClaimItemClassName)
            {
                ExpansionNotification("Vehicle Locks", "You need " + settings.ClaimItemClassName + " in your hands to claim this vehicle.").Error(player.GetIdentity());
                return false;
            }
        }

        BDE_VL_SetOwnerFromPlayer(player, true);

        if (settings.LockClaimedVehicles)
            BDE_VL_SetLocked(true);

        if (claimItem && settings.DeleteClaimItemOnSuccess)
            g_Game.ObjectDelete(claimItem);

        ExpansionNotification("Vehicle Claimed", "You are now the owner of this vehicle.").Success(player.GetIdentity());
        BDE_VL_Logger.Log(player.GetIdentity().GetName() + " (" + player.GetIdentity().GetId() + ") claimed " + GetType() + " (" + GetPersistentIDString() + ")");
        return true;
    }

    bool BDE_VL_ClaimWithExpansionKey(notnull PlayerBase player)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleLocks || !settings.AllowExpansionKeyClaim || BDE_VL_HasOwner() || !HasKey())
            return false;

        ExpansionCarKey key = ExpansionCarKey.Cast(player.GetItemInHands());
        if (!key || key.IsInherited(ExpansionCarAdminKey) || !IsPairedTo(key))
        {
            ExpansionNotification("Vehicle Locks", "The Expansion key in your hands is not paired to this vehicle.").Error(player.GetIdentity());
            return false;
        }

        if (settings.DeleteExpansionKeyOnClaim)
            BDE_VL_DeleteLoadedPairedKeys();

        ResetKeyPairing();
        BDE_VL_SetOwnerFromPlayer(player, true);

        if (settings.LockClaimedVehicles)
            BDE_VL_SetLocked(true);

        ExpansionNotification("Vehicle Migrated", "The Expansion key was converted into vehicle ownership.").Success(player.GetIdentity());
        BDE_VL_Logger.Log(player.GetIdentity().GetName() + " (" + player.GetIdentity().GetId() + ") migrated keyed vehicle " + GetType() + " (" + GetPersistentIDString() + ")");
        return true;
    }

    void BDE_VL_SetMarketOwner(notnull PlayerBase player)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleLocks)
            return;

        if (settings.DeleteMarketExpansionKey)
            BDE_VL_DeleteLoadedPairedKeys();

        if (HasKey())
            ResetKeyPairing();

        BDE_VL_SetOwnerFromPlayer(player, true);

        if (settings.LockMarketVehiclesOnPurchase)
            BDE_VL_SetLocked(true);

        BDE_VL_Logger.Log("Expansion Market assigned " + GetType() + " (" + GetPersistentIDString() + ") to " + player.GetIdentity().GetName() + " (" + player.GetIdentity().GetId() + ")");
    }

    void BDE_VL_DeleteLoadedPairedKeys()
    {
        array<ExpansionCarKey> keys = new array<ExpansionCarKey>();
        ExpansionCarKey.GetKeysForVehicle(this, keys);

        foreach (ExpansionCarKey key : keys)
        {
            if (key && !key.IsInherited(ExpansionCarAdminKey))
                g_Game.ObjectDelete(key);
        }
    }

    static ExpansionVehicle BDE_VL_FindByPersistentID(string persistentID)
    {
        if (persistentID == string.Empty)
            return null;

        auto node = s_All.m_Head;
        while (node)
        {
            ExpansionVehicle vehicle = node.m_Value;
            if (vehicle && vehicle.GetPersistentIDString() == persistentID)
                return vehicle;

            node = node.m_Next;
        }

        return null;
    }
};
