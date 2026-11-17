#ifdef SERVER
#ifdef EXPANSIONMODGROUPS

modded class ExpansionPartyModule
{
    protected string m_BDE_HivePendingGroups;
    protected bool m_BDE_HiveGroupsStopped;

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);
        if (IsMissionHost())
        {
            m_BDE_HiveGroupsStopped = false;
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_HiveReloadGroups, 30000, true);
        }
    }

    override void OnMissionFinish(Class sender, CF_EventArgs args)
    {
        m_BDE_HiveGroupsStopped = true;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(BDE_HiveReloadGroups);
        super.OnMissionFinish(sender, args);
    }

    protected void BDE_HiveReloadGroups()
    {
        if (!IsMissionHost() || m_BDE_HiveGroupsStopped || !FileExist(EXPANSION_GROUPS_FOLDER))
            return;
        array<string> files = ExpansionStatic.FindFilesInLocation(EXPANSION_GROUPS_FOLDER, ".bin");
        files.Sort();
        map<int, ref ExpansionPartyData> incomingGroups = new map<int, ref ExpansionPartyData>;
        map<string, int> memberGroups = new map<string, int>;
        array<string> signatures = new array<string>;
        foreach (string fileName : files)
        {
            FileSerializer reader = new FileSerializer;
            ExpansionPartyData incoming = new ExpansionPartyData;
            if (!reader.Open(EXPANSION_GROUPS_FOLDER + fileName, FileMode.READ))
            {
                m_BDE_HivePendingGroups = "";
                return;
            }
            int version;
            bool loaded = reader.Read(version);
            if (loaded && version > 0 && version <= EXPANSION_VERSION_MAP_MARKER_SAVE)
                loaded = incoming.OnStoreLoad(reader, version);
            else
                loaded = false;
            reader.Close();
            if (!loaded || incoming.GetPartyID() < 0 || fileName != incoming.GetPartyID().ToString() + ".bin")
            {
                m_BDE_HivePendingGroups = "";
                return;
            }
            incoming.InitMaps();
            string signature = incoming.BDE_HiveGroupSignature();
            ExpansionPartyData current = GetPartyByID(incoming.GetPartyID());
            if (signature == "" || incomingGroups.Contains(incoming.GetPartyID()) || (current && !current.BDE_HiveSameIdentity(incoming)))
            {
                // Independent servers can allocate the same numeric party ID.
                // Never silently replace a different group in that situation.
                m_BDE_HivePendingGroups = "";
                return;
            }
            if (!incoming.IsMember(incoming.GetOwnerUID()))
            {
                m_BDE_HivePendingGroups = "";
                return;
            }
            foreach (ExpansionPartyPlayerData membership : incoming.GetPlayers())
            {
                if (membership.UID == "" || memberGroups.Contains(membership.UID))
                {
                    m_BDE_HivePendingGroups = "";
                    return;
                }
                memberGroups.Insert(membership.UID, incoming.GetPartyID());
            }
            incomingGroups.Insert(incoming.GetPartyID(), incoming);
            signatures.Insert(signature);
        }
        JsonSerializer serializer = new JsonSerializer;
        string snapshot;
        if (!serializer.WriteToString(signatures, false, snapshot))
            return;
        if (m_BDE_HivePendingGroups != snapshot)
        {
            m_BDE_HivePendingGroups = snapshot;
            return;
        }
        bool changed = incomingGroups.Count() != m_Parties.Count();
        foreach (int incomingID, ExpansionPartyData compared : incomingGroups)
        {
            ExpansionPartyData existing = GetPartyByID(incomingID);
            if (!existing || existing.BDE_HiveGroupSignature() != compared.BDE_HiveGroupSignature())
                changed = true;
        }
        if (!changed)
            return;

        array<Man> activePlayers = new array<Man>;
        g_Game.GetPlayers(activePlayers);
        // Detach first, before removing any records, then rebind from the
        // fully validated snapshot. No Save/Delete calls in this refresh.
        foreach (Man activeMan : activePlayers)
        {
            PlayerBase activePlayer = PlayerBase.Cast(activeMan);
            if (activePlayer && activePlayer.Expansion_GetPartyPlayerData())
                activePlayer.Expansion_GetPartyPlayerData().OnLeave();
        }
        array<int> removedIDs = new array<int>;
        foreach (int presentID, ExpansionPartyData present : m_Parties)
        {
            if (!incomingGroups.Contains(presentID))
                removedIDs.Insert(presentID);
        }
        foreach (int removedID : removedIDs)
            RemoveParty(removedID);
        foreach (int appliedID, ExpansionPartyData applied : incomingGroups)
        {
            ExpansionPartyData destination = GetPartyByID(appliedID);
            if (destination)
            {
                if (destination.BDE_HiveGroupSignature() != applied.BDE_HiveGroupSignature())
                    destination.BDE_HiveApply(applied);
            }
            else
                AddParty(appliedID, applied);
            if (m_NextPartyID <= appliedID)
                m_NextPartyID = appliedID + 1;
        }
        m_CurrentPartyTick = 0;
        m_CurrentPlayerTick = 0;
        foreach (Man reboundMan : activePlayers)
        {
            PlayerBase reboundPlayer = PlayerBase.Cast(reboundMan);
            if (!reboundPlayer || !reboundPlayer.GetIdentity())
                continue;
            ExpansionPartyPlayerData reboundMember = GetPartyPlayerData(reboundPlayer.GetIdentity().GetId(), true);
            ExpansionPartyData reboundParty;
            if (reboundMember)
            {
                reboundMember.OnJoin(reboundPlayer);
                reboundParty = reboundMember.GetParty();
                reboundParty.OnJoin(reboundMember);
            }
            UpdatePlayerServer(reboundParty, reboundPlayer.GetIdentity());
            SyncPlayerInvitesServer(reboundPlayer.GetIdentity());
        }
        Print("[BDE_MapUI groups] Applied stable Hive group snapshot; groups=" + m_Parties.Count());
    }
};
#endif
#endif
