class BDE_MapUI_PlayerDatabaseManager

{

    private static ref BDE_MapUI_PlayerDatabaseManager s_Instance;



    private ref map<string, ref BDE_MapUI_PlayerRecord> m_Records;

    private ref BDE_MapUI_BountyAreaLocator m_AreaLocator;

    private ref array<string> m_UnreadableRecordPaths = new array<string>();
    private float m_SaveTimer;
    private float m_HiveReloadTimer;
    private const float HIVE_RELOAD_INTERVAL_SECONDS = 30.0;
    private ref BDE_MapUI_HiveBountyEvents m_HiveBountyEvents;
    private ref BDE_MapUI_HivePresence m_HivePresence;

    private float m_AreaTimer;



    private const float SAVE_INTERVAL_SECONDS = 300.0;

    private const float AREA_CHECK_INTERVAL_SECONDS = 5.0;



    static BDE_MapUI_PlayerDatabaseManager Get()

    {

        if (!s_Instance)

            s_Instance = new BDE_MapUI_PlayerDatabaseManager();

        return s_Instance;

    }



    void BDE_MapUI_PlayerDatabaseManager()

    {

        m_Records = new map<string, ref BDE_MapUI_PlayerRecord>();

        m_AreaLocator = new BDE_MapUI_BountyAreaLocator();

        m_SaveTimer = 0.0;

        m_AreaTimer = 0.0;



        EnsureDirectories();

        m_HiveBountyEvents = new BDE_MapUI_HiveBountyEvents;
        m_HivePresence = new BDE_MapUI_HivePresence;
        ReloadSharedDatabase();



        // Do not rewrite remote online status or cached shared records at startup.

    }



    void OnServerUpdate(float timeslice)

    {

        m_HivePresence.Update(timeslice);
        m_HiveReloadTimer += timeslice;
        if (m_HiveReloadTimer >= HIVE_RELOAD_INTERVAL_SECONDS)
        {
            m_HiveReloadTimer = 0.0;
            ReloadSharedDatabase();
        }

        ProcessQueuedKillEvents();

        ProcessQueuedStatKillEvents();



        m_SaveTimer += timeslice;

        m_AreaTimer += timeslice;



        if (m_AreaTimer >= AREA_CHECK_INTERVAL_SECONDS)

        {

            m_AreaTimer = 0.0;

            UpdateOnlinePlayers(false);

        }



        if (m_SaveTimer >= SAVE_INTERVAL_SECONDS)

        {

            m_SaveTimer = 0.0;

            UpdateOnlinePlayers(true);

            // Active local players were saved above; never bulk-write remote caches.

        }

    }



    void OnPlayerConnected(PlayerBase player, PlayerIdentity identity)

    {

        if (!identity)

            return;



        m_HiveBountyEvents.Reload();
        string sharedPath = GetRecordFilePath(identity.GetId());
        if (FileExist(sharedPath))
            LoadRecordFile(sharedPath, false);

        BDE_MapUI_PlayerRecord record = GetOrCreateRecord(identity.GetId(), identity.GetName());

        record.IsOnline = true;

        UpdateRecordFromPlayer(record, player, true);

        SaveRecord(record);
        m_HivePresence.Publish();

    }



    void OnPlayerDisconnected(PlayerBase player, PlayerIdentity identity)

    {

        if (!identity)

            return;



        BDE_MapUI_PlayerRecord record = GetOrCreateRecord(identity.GetId(), identity.GetName());

        UpdateRecordFromPlayer(record, player, true);

        record.RecalculateBounty();



        bool hadActiveBounty = record.HasActiveBounty && record.BountyAmount > 0;

        int bountyAmount = record.BountyAmount;

        string playerName = record.Name;

        if (playerName == "")

            playerName = identity.GetName();

        if (playerName == "")

            playerName = "Unknown Survivor";



        record.IsOnline = false;

        record.LastSeenGameTime = g_Game.GetTime();

        m_HivePresence.Publish(identity.GetId());
        SaveRecord(record);



        if (hadActiveBounty)

            SendBountyLogoutNotificationToAll(playerName, bountyAmount, identity.GetId());

    }



    BDE_MapUI_PlayerRecord GetRecord(string uid)

    {

        BDE_MapUI_PlayerRecord record;

        if (m_Records.Find(uid, record))

            return record;

        return null;

    }



    BDE_MapUI_PlayerRecord GetOrCreateRecord(string uid, string name)

    {

        BDE_MapUI_PlayerRecord record;

        if (m_Records.Find(uid, record))

        {

            record.EnsureArrays();

            if (name != "")

                record.Name = name;

            return record;

        }



        record = new BDE_MapUI_PlayerRecord();

        record.UID = uid;

        record.Name = name;

        record.LastKnownLocation = "Unknown";

        m_Records.Insert(uid, record);

        return record;

    }





    int GetPlayerBankMoney(PlayerIdentity identity)

    {

#ifdef EXPANSIONMODMARKET

        if (!identity)

            return 0;



        ExpansionMarketATM_Data atmData = GetATMData(identity.GetId());

        if (!atmData)

            return 0;



        return atmData.GetMoney();

#else

        return 0;

#endif

    }



    int GetPlayerInventoryMoney(PlayerBase player)

    {

#ifdef EXPANSIONMODMARKET

        if (!player)

            return 0;



        ExpansionMarketModule marketModule = GetMarketModule();

        if (!marketModule)

            return 0;



        array<int> monies = new array<int>;

        return marketModule.GetPlayerWorth(player, monies);

#else

        return 0;

#endif

    }



    ref array<ref BDE_MapUI_BountyListEntry> BuildList(int filter)

    {

        ref array<ref BDE_MapUI_BountyListEntry> result = new array<ref BDE_MapUI_BountyListEntry>();



        for (int i = 0; i < m_Records.Count(); i++)

        {

            string uid = m_Records.GetKey(i);

            BDE_MapUI_PlayerRecord record = m_Records.Get(uid);

            if (!record)

                continue;



            ApplyPresence(record);
            record.EnsureArrays();

            record.RecalculateBounty();



            if (filter == BDE_MapUI_BOUNTY_FILTER_ONLINE && !record.IsOnline)

                continue;

            if (filter == BDE_MapUI_BOUNTY_FILTER_ACTIVE && !record.HasActiveBounty)

                continue;



            BDE_MapUI_BountyListEntry entry = new BDE_MapUI_BountyListEntry();

            entry.UID = record.UID;

            entry.Name = record.Name;

            entry.IsOnline = record.IsOnline;
            entry.IsRemoteServer = record.IsRemoteServer;
            entry.CurrentMapName = record.CurrentMapName;

            entry.HasActiveBounty = record.HasActiveBounty;

            entry.BountyAmount = record.BountyAmount;

            if (record.HasActiveBounty)

            {

                entry.LastKnownLocation = LocationForViewer(record);

                entry.LastKnownPosition = vector.Zero;
                if (GetOnlinePlayerByUID(record.UID))
                    entry.LastKnownPosition = record.LastKnownPosition;

            }

            else

            {

                entry.LastKnownLocation = "Unknown";
                if (record.IsRemoteServer && record.CurrentMapName != "")
                    entry.LastKnownLocation = record.CurrentMapName;

                entry.LastKnownPosition = "0 0 0";

            }

            entry.BountySetBy = record.GetContributorSummary();

            result.Insert(entry);

        }



        SortList(result);

        return result;

    }



    BDE_MapUI_PlayerRecord BuildPublicRecord(string uid)

    {

        BDE_MapUI_PlayerRecord source = GetRecord(uid);

        if (!source)

            return null;



        ApplyPresence(source);
        source.EnsureArrays();

        source.RecalculateBounty();



        BDE_MapUI_PlayerRecord record = new BDE_MapUI_PlayerRecord();

        record.UID = source.UID;

        record.Name = source.Name;

        record.IsOnline = source.IsOnline;
        record.IsRemoteServer = source.IsRemoteServer;
        record.CurrentServerID = source.CurrentServerID;
        record.CurrentMapName = source.CurrentMapName;

        record.HasActiveBounty = source.HasActiveBounty;

        record.BountyAmount = source.BountyAmount;
        record.BountyRevision = source.BountyRevision;
        record.BountyRevisionEpoch = source.BountyRevisionEpoch;
        record.BountyRevisionID = source.BountyRevisionID;

        record.CharacterType = source.CharacterType;

        record.LastSeenGameTime = source.LastSeenGameTime;

        record.LastUpdatedGameTime = source.LastUpdatedGameTime;

        record.BankMoney = source.BankMoney;

        record.PlayerKills = source.PlayerKills;

        record.AIKills = source.AIKills;

        record.InfectedKilled = source.InfectedKilled;

        record.DistanceTravelled = source.DistanceTravelled;

        record.TimeSurvived = source.TimeSurvived;

        record.LongestSurvivorHit = source.LongestSurvivorHit;

        record.LastStatsUpdatedGameTime = source.LastStatsUpdatedGameTime;



        if (source.HasActiveBounty)

        {

            record.LastKnownLocation = LocationForViewer(source);

            record.LastKnownPosition = vector.Zero;
            if (GetOnlinePlayerByUID(source.UID))
                record.LastKnownPosition = source.LastKnownPosition;

        }

        else

        {

            record.LastKnownLocation = "Unknown";
            if (source.IsRemoteServer && source.CurrentMapName != "")
                record.LastKnownLocation = source.CurrentMapName;

            record.LastKnownPosition = "0 0 0";

        }



        for (int i = 0; i < source.BountyContributors.Count(); i++)

        {

            BDE_MapUI_BountyContributor sourceContributor = source.BountyContributors.Get(i);

            if (!sourceContributor || sourceContributor.Amount <= 0)

                continue;



            BDE_MapUI_BountyContributor contributor = new BDE_MapUI_BountyContributor();

            contributor.UID = sourceContributor.UID;

            contributor.Name = sourceContributor.Name;

            contributor.Amount = sourceContributor.Amount;

            record.BountyContributors.Insert(contributor);

        }



        for (int g = 0; g < source.Gear.Count(); g++)

        {

            BDE_MapUI_GearItem sourceGear = source.Gear.Get(g);

            if (!sourceGear)

                continue;



            BDE_MapUI_GearItem gearItem = new BDE_MapUI_GearItem();

            gearItem.ClassName = sourceGear.ClassName;

            gearItem.DisplayName = sourceGear.DisplayName;

            gearItem.SlotName = sourceGear.SlotName;

            gearItem.Health = sourceGear.Health;

            gearItem.Quantity = sourceGear.Quantity;

            gearItem.InHands = sourceGear.InHands;

            gearItem.GearID = sourceGear.GearID;

            gearItem.ParentGearID = sourceGear.ParentGearID;

            gearItem.IsAttachment = sourceGear.IsAttachment;

            record.Gear.Insert(gearItem);

        }



        record.RecalculateBounty();

        return record;

    }



    ref array<ref BDE_MapUI_LeaderboardListEntry> BuildLeaderboardList(int filter)

    {

        ref array<ref BDE_MapUI_LeaderboardListEntry> result = new array<ref BDE_MapUI_LeaderboardListEntry>();



        for (int i = 0; i < m_Records.Count(); i++)

        {

            string uid = m_Records.GetKey(i);

            BDE_MapUI_PlayerRecord record = m_Records.Get(uid);

            if (!record)

                continue;



            ApplyPresence(record);
            record.EnsureArrays();

            record.RecalculateBounty();

            RefreshRecordBankMoney(record);



            BDE_MapUI_LeaderboardListEntry entry = new BDE_MapUI_LeaderboardListEntry();

            entry.UID = record.UID;

            entry.Name = record.Name;

            entry.IsOnline = record.IsOnline;
            entry.IsRemoteServer = record.IsRemoteServer;
            entry.CurrentMapName = record.CurrentMapName;

            entry.BankMoney = record.BankMoney;

            entry.PlayerKills = record.PlayerKills;

            entry.AIKills = record.AIKills;

            entry.InfectedKilled = record.InfectedKilled;

            entry.DistanceTravelled = record.DistanceTravelled;

            entry.TimeSurvived = record.TimeSurvived;

            entry.LongestSurvivorHit = record.LongestSurvivorHit;

            entry.CharacterType = record.CharacterType;

            result.Insert(entry);

        }



        SortLeaderboardList(result, filter);

        return result;

    }



    private void RefreshRecordBankMoney(BDE_MapUI_PlayerRecord record)

    {

        if (!record || record.UID == "")

            return;



        if (!GetOnlinePlayerByUID(record.UID))
            return; // Remote bank/rank data comes from its shared player JSON.

#ifdef EXPANSIONMODMARKET

        ExpansionMarketATM_Data atmData = GetATMData(record.UID);

        if (atmData)

        {

            int money = atmData.GetMoney();

            if (money < 0)

                money = 0;

            record.BankMoney = money;

        }

#endif

    }





    bool AddBounty(string targetUID, PlayerIdentity setterIdentity, PlayerBase setterPlayer, int amount, out string message)

    {

        message = "";



        if (!setterIdentity)

        {

            message = "No player identity.";

            return false;

        }



        if (targetUID == "")

        {

            message = "No bounty target selected.";

            return false;

        }



        if (amount < BDE_MapUI_ConfigManager.Get().MinBountyAmount)

        {

            message = "Minimum bounty is $" + BDE_MapUI_ConfigManager.Get().MinBountyAmount.ToString() + ".";

            return false;

        }



        if (targetUID == setterIdentity.GetId())

        {

            message = "You cannot set a bounty on yourself.";

            return false;

        }



        BDE_MapUI_PlayerRecord target = GetRecord(targetUID);

        if (!target)

        {

            message = "Target player is not in the database yet.";

            return false;

        }



        if (!RefreshSharedBounty(target, true))
        {
            message = "Bounty data is temporarily unreadable during synchronization. Please retry.";
            return false;
        }

        if (!TryTakeBountyMoney(setterPlayer, setterIdentity, amount, message))

            return false;



        AddOrIncreaseContribution(target, setterIdentity.GetId(), setterIdentity.GetName(), amount);

        target.RecalculateBounty();

        target.LastUpdatedGameTime = g_Game.GetTime();

        m_HiveBountyEvents.Queue(target, "Placed", setterIdentity.GetId(), setterIdentity.GetName(), amount);
        SaveRecord(target, true);



        AnnounceHiveBountyEvents();



        message = "Bounty updated. Total bounty: $" + target.BountyAmount.ToString();

        return true;

    }



    bool RemoveOwnBounty(string targetUID, PlayerIdentity setterIdentity, PlayerBase setterPlayer, out string message)

    {

        message = "";



        if (!setterIdentity)

        {

            message = "No player identity.";

            return false;

        }



        BDE_MapUI_PlayerRecord target = GetRecord(targetUID);

        if (!target)

        {

            message = "Target player is not in the database yet.";

            return false;

        }



        target.EnsureArrays();



        if (!RefreshSharedBounty(target, true))
        {
            message = "Bounty data is temporarily unreadable during synchronization. Please retry.";
            return false;
        }

        int ownContribution = 0;

        for (int i = target.BountyContributors.Count() - 1; i >= 0; i--)

        {

            BDE_MapUI_BountyContributor contributor = target.BountyContributors.Get(i);

            if (!contributor)

                continue;



            if (contributor.UID == setterIdentity.GetId())

            {

                ownContribution += contributor.Amount;

                target.BountyContributors.Remove(i);

            }

        }



        if (ownContribution <= 0)

        {

            message = "You have not placed a bounty on this player.";

            return false;

        }



        int refund = (ownContribution * BDE_MapUI_BOUNTY_REFUND_PERCENT) / 100;

        RefundBountyMoney(setterPlayer, setterIdentity, refund);



        target.RecalculateBounty();

        target.LastUpdatedGameTime = g_Game.GetTime();

        m_HiveBountyEvents.Queue(target, "Removed", setterIdentity.GetId(), setterIdentity.GetName(), ownContribution);
        SaveRecord(target, true);



        SendBountyRemovedNotificationToAll(target, setterIdentity.GetName(), ownContribution);



        message = "Bounty removed. Refund: $" + refund.ToString();

        return true;

    }





    void OnPlayerKilled(PlayerBase victim, PlayerBase killer)

    {

        if (!victim || !killer)

            return;



        PlayerIdentity victimIdentity = victim.GetIdentity();

        PlayerIdentity killerIdentity = killer.GetIdentity();

        if (!victimIdentity || !killerIdentity)

            return;



        OnPlayerKilledByUID(victimIdentity.GetId(), victimIdentity.GetName(), killerIdentity.GetId(), killerIdentity.GetName());

    }



    void ProcessQueuedKillEvents()

    {

        ref array<ref BDE_MapUI_BountyKillEvent> killEvents = BDE_MapUI_BountyKillQueue.Consume();

        if (!killEvents)

            return;



        for (int i = 0; i < killEvents.Count(); i++)

        {

            BDE_MapUI_BountyKillEvent eventData = killEvents.Get(i);

            if (!eventData)

                continue;



            OnPlayerKilledByUID(eventData.VictimUID, eventData.VictimName, eventData.KillerUID, eventData.KillerName);

        }

    }



    void ProcessQueuedStatKillEvents()

    {

        ref array<ref BDE_MapUI_StatsKillEvent> statEvents = BDE_MapUI_StatsKillQueue.Consume();

        if (!statEvents)

            return;



        for (int i = 0; i < statEvents.Count(); i++)

        {

            BDE_MapUI_StatsKillEvent eventData = statEvents.Get(i);

            if (!eventData)

                continue;



            if (eventData.IsAIKill)

                AddAIKillToRecord(eventData.KillerUID, eventData.KillerName);

        }

    }



    private void AddAIKillToRecord(string killerUID, string killerName)

    {

        if (killerUID == "")

            return;



        BDE_MapUI_PlayerRecord record = GetOrCreateRecord(killerUID, killerName);

        record.EnsureArrays();

        record.AIKills++;

        record.LastStatsUpdatedGameTime = g_Game.GetTime();

        record.LastUpdatedGameTime = g_Game.GetTime();



        PlayerBase player = GetOnlinePlayerByUID(killerUID);

        if (player)

            UpdateStatsSnapshot(record, player);



        SaveRecord(record);



        BDE_MapUI_PointsStoreManager.Get().OnAIKill(killerUID, killerName);

    }





    void OnPlayerKilledByUID(string victimUID, string victimName, string killerUID, string killerName)

    {

        if (victimUID == "" || killerUID == "")

            return;



        if (victimUID == killerUID)

            return;



        BDE_MapUI_PointsStoreManager.Get().OnPlayerKill(killerUID, killerName);



        BDE_MapUI_PlayerRecord victimRecord = GetRecord(victimUID);

        if (!victimRecord)

            return;



        if (!RefreshSharedBounty(victimRecord, true))
            return;
        victimRecord.EnsureArrays();

        victimRecord.RecalculateBounty();

        if (!victimRecord.HasActiveBounty || victimRecord.BountyAmount <= 0)

            return;



        int reward = victimRecord.BountyAmount;



        if (victimName == "")

            victimName = victimRecord.Name;

        if (victimName == "")

            victimName = "Unknown Survivor";



        if (killerName == "")

            killerName = "Unknown Survivor";



        bool paid = AddMoneyToATMByUID(killerUID, reward);

        PlayerIdentity killerIdentity = GetOnlineIdentityByUID(killerUID);



        if (paid)

        {

            victimRecord.BountyContributors.Clear();

            victimRecord.RecalculateBounty();

            victimRecord.LastUpdatedGameTime = g_Game.GetTime();

            m_HiveBountyEvents.Queue(victimRecord, "Claimed", killerUID, killerName, reward);
            SaveRecord(victimRecord, true);



            // The Hive event sends one common claim announcement, including to the killer.



            AnnounceHiveBountyEvents();

        }

        else

        {

            if (killerIdentity)

                NotificationSystem.SendNotificationToPlayerIdentityExtended(killerIdentity, 10.0, "Bounty Claim Failed", "You killed " + victimName + ", but the bounty could not be paid to your ATM account.", "set:dayz_gui image:iconSkull");



            BDE_MapUI_Log.Log("[BDE_MapUI Bounties] Failed to pay bounty reward $" + reward.ToString() + " to " + killerName + " for killing " + victimName,"BDE_MapUI/Bounties/BDE_MapUI_PlayerDatabaseManager");

        }

    }



    private void AddOrIncreaseContribution(BDE_MapUI_PlayerRecord target, string setterUID, string setterName, int amount)

    {

        target.EnsureArrays();



        for (int i = 0; i < target.BountyContributors.Count(); i++)

        {

            BDE_MapUI_BountyContributor contributor = target.BountyContributors.Get(i);

            if (contributor && contributor.UID == setterUID)

            {

                contributor.Amount += amount;

                contributor.Name = setterName;

                return;

            }

        }



        BDE_MapUI_BountyContributor newContributor = new BDE_MapUI_BountyContributor();

        newContributor.UID = setterUID;

        newContributor.Name = setterName;

        newContributor.Amount = amount;

        target.BountyContributors.Insert(newContributor);

    }



    private bool TryTakeBountyMoney(PlayerBase player, PlayerIdentity identity, int amount, out string message)

    {

        message = "";



        if (!identity)

        {

            message = "No player identity.";

            return false;

        }



        if (amount < BDE_MapUI_ConfigManager.Get().MinBountyAmount)

        {

            message = "Minimum bounty is $" + BDE_MapUI_ConfigManager.Get().MinBountyAmount.ToString() + ".";

            return false;

        }



#ifdef EXPANSIONMODMARKET

        ExpansionMarketATM_Data atmData = GetATMData(identity.GetId());

        if (!atmData)

        {

            message = "ATM account not found.";

            return false;

        }



        int currentATMMoney = atmData.GetMoney();

        if (currentATMMoney < amount)

        {

            message = "Not enough money in bank.";

            return false;

        }



        int expectedATMMoney = currentATMMoney - amount;



        // Expansion ATM exposes AddMoney. Passing a negative value deducts the bounty immediately.

        atmData.AddMoney(-amount);

        if (atmData.GetMoney() != expectedATMMoney)

        {

            message = "ATM deduction failed.";

            return false;

        }



        return true;

#else

        message = "Expansion Market ATM is not loaded.";

        return false;

#endif

    }



    private void RefundBountyMoney(PlayerBase player, PlayerIdentity identity, int amount)

    {

        if (amount <= 0 || !identity)

            return;



        AddMoneyToATM(identity, amount);

    }



    private bool AddMoneyToATM(PlayerIdentity identity, int amount)

    {

        if (!identity || amount <= 0)

            return false;



        return AddMoneyToATMByUID(identity.GetId(), amount);

    }



    private bool AddMoneyToATMByUID(string uid, int amount)

    {

        if (uid == "" || amount <= 0)

            return false;



#ifdef EXPANSIONMODMARKET

        ExpansionMarketATM_Data atmData = GetATMData(uid);

        if (!atmData)

            return false;



        atmData.AddMoney(amount);

        return true;

#else

        return false;

#endif

    }



    private PlayerBase GetOnlinePlayerByUID(string uid)

    {

        if (uid == "")

            return null;



        array<Man> players = new array<Man>();

        g_Game.GetPlayers(players);



        for (int i = 0; i < players.Count(); i++)

        {

            PlayerBase player = PlayerBase.Cast(players.Get(i));

            if (!player)

                continue;



            PlayerIdentity identity = player.GetIdentity();

            if (identity && identity.GetId() == uid)

                return player;

        }



        return null;

    }





    private PlayerIdentity GetOnlineIdentityByUID(string uid)

    {

        if (uid == "")

            return null;



        array<Man> players = new array<Man>();

        g_Game.GetPlayers(players);



        for (int i = 0; i < players.Count(); i++)

        {

            PlayerBase player = PlayerBase.Cast(players.Get(i));

            if (!player)

                continue;



            PlayerIdentity identity = player.GetIdentity();

            if (identity && identity.GetId() == uid)

                return identity;

        }



        return null;

    }



#ifdef EXPANSIONMODMARKET

    private ExpansionMarketModule GetMarketModule()

    {

        return ExpansionMarketModule.Cast(CF_ModuleCoreManager.Get(ExpansionMarketModule));

    }



    private ExpansionMarketATM_Data GetATMData(string uid)

    {

        if (uid == "")

            return null;



        ExpansionMarketModule marketModule = GetMarketModule();

        if (!marketModule)

            return null;



        return marketModule.GetPlayerATMData(uid);

    }

#endif



    private void UpdateOnlinePlayers(bool forceSave)

    {

        array<Man> players = new array<Man>();

        g_Game.GetPlayers(players);



        for (int i = 0; i < players.Count(); i++)

        {

            PlayerBase player = PlayerBase.Cast(players.Get(i));

            if (!player)

                continue;



            PlayerIdentity identity = player.GetIdentity();

            if (!identity)

                continue;



            BDE_MapUI_PlayerRecord record = GetOrCreateRecord(identity.GetId(), identity.GetName());

            record.IsOnline = true;



            bool changed = UpdateRecordFromPlayer(record, player, forceSave);

            if (changed || forceSave)

                SaveRecord(record);

        }

    }



    private bool UpdateRecordFromPlayer(BDE_MapUI_PlayerRecord record, PlayerBase player, bool forceGearSnapshot)

    {

        if (!record)

            return false;



        record.EnsureArrays();

        record.RecalculateBounty();

        bool changed = false;



        if (player)

        {

            record.CurrentServerID = m_HivePresence.ServerID();
            record.CurrentMapName = m_HivePresence.MapName();
            record.IsRemoteServer = false;
            vector pos = player.GetPosition();

            string areaName = m_AreaLocator.GetLocationNameForPosition(pos);



            if (record.LastKnownLocation != areaName)

            {

                record.LastKnownLocation = areaName;

                record.LastKnownPosition = pos;

                changed = true;



                MaybeSendBountySightingNotification(record, areaName);

            }



            if (forceGearSnapshot && record.LastKnownPosition != pos)

            {

                record.LastKnownPosition = pos;

                changed = true;

            }



            UpdateStatsSnapshot(record, player);



            string characterType = player.GetType();

            if (record.CharacterType != characterType)

            {

                record.CharacterType = characterType;

                changed = true;

            }



            if (forceGearSnapshot)

            {

                SaveGearSnapshot(record, player);

                changed = true;

            }

        }



        record.LastUpdatedGameTime = g_Game.GetTime();

        record.RecalculateBounty();

        return changed;

    }



    private void UpdateStatsSnapshot(BDE_MapUI_PlayerRecord record, PlayerBase player)

    {

        if (!record || !player)

            return;



        PlayerIdentity identity = player.GetIdentity();

        if (identity)

        {

            int bankMoney = GetPlayerBankMoney(identity);

            if (bankMoney < 0)

                bankMoney = 0;

            record.BankMoney = bankMoney;

        }



        player.StatUpdateByPosition(AnalyticsManagerServer.STAT_DISTANCE);

        player.StatUpdateByTime(AnalyticsManagerServer.STAT_PLAYTIME);



        float distanceTravelled = player.StatGet(AnalyticsManagerServer.STAT_DISTANCE);

        float timeSurvived = player.StatGet(AnalyticsManagerServer.STAT_PLAYTIME);

        float rawPlayerKills = player.StatGet(AnalyticsManagerServer.STAT_PLAYERS_KILLED);

        float infectedKilled = player.StatGet(AnalyticsManagerServer.STAT_INFECTED_KILLED);

        float longestHit = player.StatGet(AnalyticsManagerServer.STAT_LONGEST_SURVIVOR_HIT);



        if (distanceTravelled < 0)

            distanceTravelled = 0;

        if (timeSurvived < 0)

            timeSurvived = 0;

        if (rawPlayerKills < 0)

            rawPlayerKills = 0;

        if (infectedKilled < 0)

            infectedKilled = 0;

        if (longestHit < 0)

            longestHit = 0;



        int rawPlayerKillCount = Math.Floor(rawPlayerKills + 0.5);

        int adjustedPlayerKills = rawPlayerKillCount;

        if (record.AIKills > 0 && adjustedPlayerKills >= record.AIKills)

            adjustedPlayerKills = adjustedPlayerKills - record.AIKills;

        if (adjustedPlayerKills < 0)

            adjustedPlayerKills = 0;



        record.PlayerKills = adjustedPlayerKills;

        record.InfectedKilled = Math.Floor(infectedKilled + 0.5);

        record.DistanceTravelled = distanceTravelled;

        record.TimeSurvived = timeSurvived;

        record.LongestSurvivorHit = longestHit;

        record.LastStatsUpdatedGameTime = g_Game.GetTime();



        BDE_MapUI_PointsStoreManager.Get().SyncStatsFromPlayerDatabase(record.UID, record.Name, record.InfectedKilled, record.DistanceTravelled, record.TimeSurvived);

    }





    private void MaybeSendBountySightingNotification(BDE_MapUI_PlayerRecord record, string newLocation)

    {

        if (!record)

            return;



        record.RecalculateBounty();

        if (!record.HasActiveBounty || record.BountyAmount <= 0)

            return;



        if (IsWildernessLocation(newLocation))

            return;



        string playerName = record.Name;

        if (playerName == "")

            playerName = "Unknown Survivor";



        string message = playerName + " with a $" + record.BountyAmount.ToString() + " bounty was seen at " + newLocation + ".";

        SendBountySightingNotificationToAll(message);

    }





    private void SendBountyPlacedNotificationToAll(BDE_MapUI_PlayerRecord target, string setterName, int placedAmount)

    {

        if (!target)

            return;



        target.RecalculateBounty();



        string targetName = target.Name;

        if (targetName == "")

            targetName = "Unknown Survivor";



        if (setterName == "")

            setterName = "Unknown Survivor";



        string message = "A $" + placedAmount.ToString() + " bounty has been placed on " + targetName + " by " + setterName + ", total bounty $" + target.BountyAmount.ToString() + ".";

        SendBountyNotificationToAll("Bounty Placed", message);

    }



    private void SendBountyRemovedNotificationToAll(BDE_MapUI_PlayerRecord target, string setterName, int removedAmount)

    {

        if (!target)

            return;



        target.RecalculateBounty();



        string targetName = target.Name;

        if (targetName == "")

            targetName = "Unknown Survivor";



        if (setterName == "")

            setterName = "Unknown Survivor";



        string message = "A $" + removedAmount.ToString() + " bounty has been removed from " + targetName + " by " + setterName + ", total bounty $" + target.BountyAmount.ToString() + ".";

        SendBountyNotificationToAll("Bounty Removed", message);

    }



    private void SendBountyNotificationToAll(string title, string message)

    {

        array<Man> players = new array<Man>();

        g_Game.GetPlayers(players);



        for (int i = 0; i < players.Count(); i++)

        {

            PlayerBase receiver = PlayerBase.Cast(players.Get(i));

            if (!receiver)

                continue;



            PlayerIdentity identity = receiver.GetIdentity();

            if (!identity)

                continue;



            NotificationSystem.SendNotificationToPlayerIdentityExtended(identity, 10.0, title, message, "set:dayz_gui image:iconSkull");

        }



        BDE_MapUI_Log.Log("[BDE_MapUI Bounties] " + message,"BDE_MapUI/Bounties/BDE_MapUI_PlayerDatabaseManager");

    }



    private bool IsWildernessLocation(string locationName)

    {

        if (locationName == "")

            return true;



        string normalizedLocation = locationName;

        normalizedLocation.ToLower();

        return normalizedLocation.Contains("wilderness");

    }



    private void SendBountySightingNotificationToAll(string message)

    {

        SendBountyNotificationToAll("Bounty Sighting", message);

    }



    private void SendBountyLogoutNotificationToAll(string playerName, int bountyAmount, string excludedUID)

    {

        if (playerName == "")

            playerName = "Unknown Survivor";



        string message = playerName + " with a $" + bountyAmount.ToString() + " bounty has disconnected.";



        array<Man> players = new array<Man>();

        g_Game.GetPlayers(players);



        for (int i = 0; i < players.Count(); i++)

        {

            PlayerBase receiver = PlayerBase.Cast(players.Get(i));

            if (!receiver)

                continue;



            PlayerIdentity identity = receiver.GetIdentity();

            if (!identity)

                continue;



            if (excludedUID != "" && identity.GetId() == excludedUID)

                continue;



            NotificationSystem.SendNotificationToPlayerIdentityExtended(identity, 10.0, "Bounty Target Disconnected", message, "set:dayz_gui image:iconSkull");

        }



        BDE_MapUI_Log.Log("[BDE_MapUI Bounties] " + message,"BDE_MapUI/Bounties/BDE_MapUI_PlayerDatabaseManager");

    }



    private void SaveGearSnapshot(BDE_MapUI_PlayerRecord record, PlayerBase player)

    {

        if (!record || !player)

            return;



        record.EnsureArrays();

        record.Gear.Clear();



        EntityAI itemInHands = player.GetHumanInventory().GetEntityInHands();



        array<EntityAI> items = new array<EntityAI>();

        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);



        map<EntityAI, int> gearIds = new map<EntityAI, int>();

        for (int idIndex = 0; idIndex < items.Count(); idIndex++)

        {

            EntityAI idItem = items.Get(idIndex);

            if (idItem && idItem != player)

                gearIds.Insert(idItem, idIndex);

        }



        for (int i = 0; i < items.Count(); i++)

        {

            EntityAI item = items.Get(i);

            if (!item)

                continue;

            if (item == player)

                continue;



            BDE_MapUI_GearItem gearItem = new BDE_MapUI_GearItem();

            gearItem.ClassName = item.GetType();

            gearItem.DisplayName = item.GetDisplayName();

            gearItem.SlotName = GetSlotNameForItem(item);

            gearItem.Health = item.GetHealth("", "");

            gearItem.InHands = item == itemInHands;

            gearItem.GearID = i;

            gearItem.ParentGearID = -1;

            gearItem.IsAttachment = gearItem.SlotName != "";



            EntityAI parent = EntityAI.Cast(item.GetHierarchyParent());

            if (parent && parent != player)

            {

                int parentId;

                if (gearIds.Find(parent, parentId))

                    gearItem.ParentGearID = parentId;

            }



            ItemBase itemBase = ItemBase.Cast(item);

            if (itemBase && itemBase.HasQuantity())

                gearItem.Quantity = itemBase.GetQuantity();



            record.Gear.Insert(gearItem);

        }

    }



    private string GetSlotNameForItem(EntityAI item)

    {

        if (!item)

            return "";



        InventoryLocation location = new InventoryLocation();

        if (item.GetInventory() && item.GetInventory().GetCurrentInventoryLocation(location))

        {

            int slotId = location.GetSlot();

            if (slotId >= 0)

                return InventorySlots.GetSlotName(slotId);

        }



        return "";

    }



    private void EnsureDirectories()

    {

        if (!FileExist("$profile:/BDE"))

            MakeDirectory("$profile:/BDE");

        if (!FileExist("$profile:/BDE/MapUI"))

            MakeDirectory("$profile:/BDE/MapUI");

        if (!FileExist(BDE_MapUI_PLAYER_DB_PATH))

            MakeDirectory(BDE_MapUI_PLAYER_DB_PATH);

    }



    private string GetRecordFilePath(string uid)

    {

        string cleanUID = uid;

        cleanUID.Replace("\\", "_");

        cleanUID.Replace("/", "_");

        cleanUID.Replace(":", "_");

        cleanUID.Replace(" ", "_");

        return BDE_MapUI_PLAYER_DB_PATH + "/" + cleanUID + ".json";

    }



    private void SaveRecord(BDE_MapUI_PlayerRecord record, bool bountyChanged = false)

    {

        if (!record || record.UID == "")

            return;



        if (!bountyChanged && !RefreshSharedBounty(record))
            return;

        if (bountyChanged && !GetOnlinePlayerByUID(record.UID))
        {
            string sharedPath = GetRecordFilePath(record.UID);
            if (FileExist(sharedPath))
            {
                BDE_MapUI_PlayerRecord latest;
                if (!TryReadSharedRecord(sharedPath, latest))
                    return;
                CopySharedBounty(record, latest);
                record = latest;
                m_Records.Set(record.UID, record);
            }
        }
        record.IsRemoteServer = false; // This field is computed for each viewer.

        record.EnsureArrays();

        record.RecalculateBounty();

        string path = GetRecordFilePath(record.UID);
        if (m_UnreadableRecordPaths.Find(path) >= 0)
        {
            Print("[BDE_MapUI database] Save blocked to preserve unreadable record: " + path);
            return;
        }
        EnsureDirectories();
        string error;
        if (!JsonFileLoader<BDE_MapUI_PlayerRecord>.SaveFile(path, record, error))
        {
            Print("[BDE_MapUI database] Save failed: " + path + ": " + error);
        }

    }



    private bool TryReadSharedRecord(string path, out BDE_MapUI_PlayerRecord record)
    {
        record = null;
        string error;
        if (!JsonFileLoader<BDE_MapUI_PlayerRecord>.LoadFile(path, record, error) || !record || record.UID == "" || GetRecordFilePath(record.UID) != path)
        {
            if (m_UnreadableRecordPaths.Find(path) < 0)
            {
                m_UnreadableRecordPaths.Insert(path);
                Print("[BDE_MapUI database] Hive record incomplete/unreadable; retaining cached data: " + path);
            }
            return false;
        }
        record.EnsureArrays();
        record.RecalculateBounty();
        m_HiveBountyEvents.Apply(record);
        m_UnreadableRecordPaths.RemoveItem(path);
        return true;
    }

    private void CopySharedBounty(BDE_MapUI_PlayerRecord incoming, BDE_MapUI_PlayerRecord record)
    {
        record.BountyContributors = incoming.BountyContributors;
        record.BountyRevision = incoming.BountyRevision;
        record.BountyRevisionEpoch = incoming.BountyRevisionEpoch;
        record.BountyRevisionID = incoming.BountyRevisionID;
        record.RecalculateBounty();
    }

    private bool RefreshSharedBounty(BDE_MapUI_PlayerRecord record, bool refreshEvents = false)
    {
        if (refreshEvents)
            m_HiveBountyEvents.Reload();
        string path = GetRecordFilePath(record.UID);
        if (FileExist(path))
        {
            BDE_MapUI_PlayerRecord incoming;
            if (!TryReadSharedRecord(path, incoming))
                return false;
            CopySharedBounty(incoming, record);
        }
        else
            m_HiveBountyEvents.Apply(record);
        return true;
    }

    private void ApplyPresence(BDE_MapUI_PlayerRecord record)
    {
        record.IsRemoteServer = false;
        if (GetOnlinePlayerByUID(record.UID))
        {
            record.IsOnline = true;
            record.CurrentServerID = m_HivePresence.ServerID();
            record.CurrentMapName = m_HivePresence.MapName();
            return;
        }
        BDE_MapUI_OnlinePlayerPresence presence;
        record.IsOnline = m_HivePresence.Find(record.UID, presence);
        if (record.IsOnline)
        {
            record.CurrentServerID = presence.ServerID;
            record.CurrentMapName = presence.MapName;
            record.IsRemoteServer = presence.ServerID != m_HivePresence.ServerID();
        }
    }

    private void EnsureOnlinePresenceRecords()
    {
        array<ref BDE_MapUI_OnlinePlayerPresence> online = m_HivePresence.OnlinePlayers();
        foreach (BDE_MapUI_OnlinePlayerPresence presence : online)
        {
            BDE_MapUI_PlayerRecord record;
            if (!m_Records.Find(presence.UID, record))
            {
                record = new BDE_MapUI_PlayerRecord;
                record.UID = presence.UID;
                record.Name = presence.Name;
                m_Records.Set(record.UID, record);
            }
            ApplyPresence(record);
        }
    }

    private string LocationForViewer(BDE_MapUI_PlayerRecord record)
    {
        if (GetOnlinePlayerByUID(record.UID))
            return record.LastKnownLocation;
        if (record.CurrentMapName != "")
            return record.CurrentMapName;
        return "Unknown";
    }

    void OnServerShutdown()
    {
        array<Man> players = new array<Man>;
        g_Game.GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity())
                continue;
            BDE_MapUI_PlayerRecord record = GetOrCreateRecord(player.GetIdentity().GetId(), player.GetIdentity().GetName());
            UpdateRecordFromPlayer(record, player, true);
            record.IsOnline = false;
            SaveRecord(record);
        }
        m_HiveBountyEvents.Reload();
        m_HivePresence.Publish("", true);
    }


    private void ReloadSharedDatabase()
    {
        m_HiveBountyEvents.Reload();
        m_HivePresence.Reload();
        EnsureOnlinePresenceRecords();
        LoadAllRecords(BDE_MapUI_PLAYER_DB_PATH);
        AnnounceHiveBountyEvents();
    }

    private void AnnounceHiveBountyEvents()
    {
        array<ref BDE_MapUI_HiveBountyEvent> events = m_HiveBountyEvents.Unseen();
        foreach (BDE_MapUI_HiveBountyEvent eventData : events)
        {
            if (!m_HiveBountyEvents.MarkAnnounced(eventData))
                continue;
            string targetName = eventData.TargetName;
            string actorName = eventData.ActorName;
            if (targetName == "")
                targetName = "Unknown Survivor";
            if (actorName == "")
                actorName = "Unknown Survivor";
            if (eventData.Kind == "Placed")
            {
                string placedMessage = "A $" + eventData.Amount.ToString() + " bounty has been placed on " + targetName + " by " + actorName + " from " + eventData.MapName + ", total bounty $" + eventData.TotalAmount.ToString() + ".";
                SendBountyNotificationToAll("Bounty Placed", placedMessage);
            }
            else if (eventData.Kind == "Claimed")
            {
                string claimedMessage = actorName + " claimed the $" + eventData.Amount.ToString() + " bounty on " + targetName + " on " + eventData.MapName + ".";
                SendBountyNotificationToAll("Bounty Claimed", claimedMessage);
            }
        }
    }


    private void LoadAllRecords(string directory)

    {

        EnsureDirectories();



        string fileName;

        int fileAttr;

        FindFileHandle handle = CF.FindFileEx(directory + "/*.json", fileName, fileAttr, FindFileFlags.ALL);

        if (!handle)
        {
            Print("[BDE_MapUI database] No files found in " + directory);
            return;
        }



        bool found = true;

        while (found)

        {

            if (!(fileAttr & FileAttr.DIRECTORY) && fileName.Length() >= 5 && fileName.Substring(fileName.Length() - 5, 5) == ".json")

                LoadRecordFile(directory + "/" + fileName);

            found = FindNextFile(handle, fileName, fileAttr);

        }



        CloseFindFile(handle);
        // Reloading valid shared records does not write them back to disk.

    }



    private void LoadRecordFile(string path, bool preserveLiveStats = true)
    {
        BDE_MapUI_PlayerRecord incoming;
        if (!TryReadSharedRecord(path, incoming))
            return;
        BDE_MapUI_PlayerRecord current;
        if (preserveLiveStats && GetOnlinePlayerByUID(incoming.UID) && m_Records.Find(incoming.UID, current))
        {
            CopySharedBounty(incoming, current);
            current.IsOnline = true;
        }
        else
            m_Records.Set(incoming.UID, incoming);
    }





    private void SortList(array<ref BDE_MapUI_BountyListEntry> list)

    {

        if (!list)

            return;



        for (int i = 0; i < list.Count() - 1; i++)

        {

            for (int j = i + 1; j < list.Count(); j++)

            {

                BDE_MapUI_BountyListEntry a = list.Get(i);

                BDE_MapUI_BountyListEntry b = list.Get(j);

                if (!a || !b)

                    continue;



                bool swap = false;

                if (b.HasActiveBounty && !a.HasActiveBounty)

                    swap = true;

                else if (b.HasActiveBounty == a.HasActiveBounty && b.BountyAmount > a.BountyAmount)

                    swap = true;



                if (swap)

                {

                    list.Set(i, b);

                    list.Set(j, a);

                }

            }

        }

    }





    private void SortLeaderboardList(array<ref BDE_MapUI_LeaderboardListEntry> list, int filter)

    {

        if (!list)

            return;



        for (int i = 0; i < list.Count() - 1; i++)

        {

            for (int j = i + 1; j < list.Count(); j++)

            {

                BDE_MapUI_LeaderboardListEntry a = list.Get(i);

                BDE_MapUI_LeaderboardListEntry b = list.Get(j);

                if (!a || !b)

                    continue;



                bool swap = false;



                if (filter == BDE_MapUI_LEADERBOARD_FILTER_RICHEST)

                {

                    if (b.BankMoney > a.BankMoney)

                        swap = true;

                    else if (b.BankMoney == a.BankMoney && b.GetCombatKills() > a.GetCombatKills())

                        swap = true;

                }

                else if (filter == BDE_MapUI_LEADERBOARD_FILTER_KILLS)

                {

                    if (b.GetCombatKills() > a.GetCombatKills())

                        swap = true;

                    else if (b.GetCombatKills() == a.GetCombatKills() && b.PlayerKills > a.PlayerKills)

                        swap = true;

                    else if (b.GetCombatKills() == a.GetCombatKills() && b.PlayerKills == a.PlayerKills && b.AIKills > a.AIKills)

                        swap = true;

                }

                else

                {

                    if (b.IsOnline && !a.IsOnline)

                        swap = true;

                    else if (b.IsOnline == a.IsOnline && b.GetCombatKills() > a.GetCombatKills())

                        swap = true;

                }



                if (swap)

                {

                    list.Set(i, b);

                    list.Set(j, a);

                }

            }

        }

    }



};

