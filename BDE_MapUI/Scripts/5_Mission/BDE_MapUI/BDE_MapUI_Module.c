class BDE_MapUI_Module
{
    protected static ref BDE_MapUI_Module s_Instance;
    protected float m_ServerUptime;

    protected ref BDE_MapUI_PlayerDatabaseManager m_PlayerDatabase;
    protected ref BDE_MapUI_PointsStoreManager m_PointsStore;

    static BDE_MapUI_Module Get()
    {
        if (!s_Instance)
            s_Instance = new BDE_MapUI_Module();
        return s_Instance;
    }

    void BDE_MapUI_Module()
    {
        if (!g_Game)
            return;

        if (g_Game.IsServer())
        {
            BDE_MapUI_RestartBackups.Run();
            BDE_MapUI_ConfigManager.LoadServer();
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();
            m_PointsStore = BDE_MapUI_PointsStoreManager.Get();

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestPlayerStats", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestConfig", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestSidebar", this, SingeplayerExecutionType.Server);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestBountyList", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestBountyDetails", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_AddBounty", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RemoveOwnBounty", this, SingeplayerExecutionType.Server);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestLeaderboardList", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestLeaderboardDetails", this, SingeplayerExecutionType.Server);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_RequestPointsStoreData", this, SingeplayerExecutionType.Server);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_PurchasePointsStoreItem", this, SingeplayerExecutionType.Server);
        }

        if (g_Game.IsClient())
        {
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceivePlayerStats", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveConfig", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveMoney", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveDonation", this, SingeplayerExecutionType.Client);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyList", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyDetails", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyActionResult", this, SingeplayerExecutionType.Client);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveLeaderboardList", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceiveLeaderboardDetails", this, SingeplayerExecutionType.Client);

            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceivePointsStoreData", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_RPC, "RPC_ReceivePointsStoreAction", this, SingeplayerExecutionType.Client);

            // Optional VIP System integration. These handlers are safe even when the
            // VIP addon is not loaded; no response simply means the admin tab stays hidden.
            GetRPCManager().AddRPC(BDE_MAPUI_VIP_RPC_CLIENT, "ReceiveMapUIVIPAccess", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_VIP_RPC_CLIENT, "ReceiveMapUIVIPData", this, SingeplayerExecutionType.Client);
            GetRPCManager().AddRPC(BDE_MAPUI_VIP_RPC_CLIENT, "ReceiveMapUIVIPMessage", this, SingeplayerExecutionType.Client);

            g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(RequestConfig, 750, false);
        }
        BDE_MapUI_PortalService.Get();
    }

    void RequestConfig()
    {
        if (!g_Game || !g_Game.IsClient())
            return;

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestConfig", NULL, true, NULL);
    }

    void RequestSidebarData()
    {
        if (!g_Game || !g_Game.IsClient())
            return;

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestSidebar", NULL, true, NULL);
    }

    void RequestPlayerStats()
    {
        if (g_Game && g_Game.IsClient())
            GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestPlayerStats", NULL, true, NULL);
    }

    void RPC_RequestPlayerStats(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender) return;
        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (!player) return;
        TIntArray ints = new TIntArray();
        TFloatArray floats = new TFloatArray();
        ints.Insert(player.GetHealth());
        ints.Insert(player.GetHealth("", "Blood"));
        floats.Insert(player.GetHealth("", "Shock"));
        floats.Insert(player.GetStatStamina().Get());
        floats.Insert(player.StatGet(AnalyticsManagerServer.STAT_DISTANCE));
        floats.Insert(player.StatGet(AnalyticsManagerServer.STAT_PLAYTIME));
        floats.Insert(player.StatGet(AnalyticsManagerServer.STAT_PLAYERS_KILLED));
        floats.Insert(player.StatGet(AnalyticsManagerServer.STAT_INFECTED_KILLED));
        floats.Insert(player.StatGet(AnalyticsManagerServer.STAT_LONGEST_SURVIVOR_HIT));
        floats.Insert(m_ServerUptime);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceivePlayerStats", new Param3<ref TIntArray, ref TFloatArray, bool>(ints, floats, player.HasDisease()), true, sender);
    }

    void RPC_ReceivePlayerStats(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient()) return;
        Param3<ref TIntArray, ref TFloatArray, bool> data;
        if (!ctx.Read(data)) return;
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu) menu.BDE_MapUI_ReceivePlayerStats(data.param1, data.param2, data.param3);
    }

    void OnServerShutdown()
    {
        if (m_PlayerDatabase)
            m_PlayerDatabase.OnServerShutdown();
    }

    void OnServerUpdate(float timeslice)
    {
        m_ServerUptime += timeslice;
        if (m_PlayerDatabase) m_PlayerDatabase.OnServerUpdate(timeslice);
        if (m_PointsStore) m_PointsStore.OnServerUpdate(timeslice);
    }

    void OnPlayerConnected(PlayerBase player, PlayerIdentity identity)
    {
        if (m_PlayerDatabase) m_PlayerDatabase.OnPlayerConnected(player, identity);
        if (m_PointsStore) m_PointsStore.OnPlayerConnected(player, identity);
    }

    void OnPlayerDisconnected(PlayerBase player, PlayerIdentity identity)
    {
        if (m_PlayerDatabase) m_PlayerDatabase.OnPlayerDisconnected(player, identity);
        if (m_PointsStore) m_PointsStore.OnPlayerDisconnected(player, identity);
    }

    protected ExpansionMapMenu GetOpenMapMenu()
    {
        if (!g_Game || !g_Game.GetUIManager())
            return null;
        return ExpansionMapMenu.Cast(g_Game.GetUIManager().GetMenu());
    }

    // ---------------------------------------------------------------------
    // Server-authoritative requests backed by MapUI-owned managers.
    void RPC_RequestConfig(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!cfg)
            return;

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveConfig", new Param1<ref BDE_MapUI_Config>(cfg), true, sender);
    }

    void RPC_RequestSidebar(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        SendMoney(sender);
        SendDonation(sender);
    }

    protected void SendMoney(PlayerIdentity sender)
    {
        if (!sender)
            return;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        int bankMoney = 0;
        int inventoryMoney = 0;
        if (m_PlayerDatabase)
        {
            bankMoney = m_PlayerDatabase.GetPlayerBankMoney(sender);
            PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
            inventoryMoney = m_PlayerDatabase.GetPlayerInventoryMoney(player);
        }

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveMoney", new Param2<int, int>(bankMoney, inventoryMoney), true, sender);
    }

    protected void SendDonation(PlayerIdentity sender)
    {
        if (!sender)
            return;

        BDE_MapUI_DonationProgressData data = BDE_MapUI_DonationProgressManager.GetData();
        if (!data)
            data = new BDE_MapUI_DonationProgressData();

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveDonation", new Param6<int, int, string, int, int, string>(data.ConfigVersion, data.Enabled, data.Message, data.CurrentAmount, data.MonthlyGoal, data.CurrencySymbol), true, sender);
    }

    void RPC_RequestBountyList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<int> data;
        int filter = BDE_MapUI_BOUNTY_FILTER_ALL;
        if (ctx.Read(data))
            filter = data.param1;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        ref array<ref BDE_MapUI_BountyListEntry> list = new array<ref BDE_MapUI_BountyListEntry>();
        if (m_PlayerDatabase)
            list = m_PlayerDatabase.BuildList(filter);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyList", new Param1<ref array<ref BDE_MapUI_BountyListEntry>>(list), true, sender);
    }

    void RPC_RequestBountyDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        BDE_MapUI_PlayerRecord record = null;
        if (m_PlayerDatabase)
            record = m_PlayerDatabase.BuildPublicRecord(data.param1);
        if (!record)
            record = new BDE_MapUI_PlayerRecord();

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyDetails", new Param1<ref BDE_MapUI_PlayerRecord>(record), true, sender);
    }

    void RPC_AddBounty(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param2<string, int> data;
        if (!ctx.Read(data))
            return;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        string message = "Bounty database is unavailable.";
        bool success = false;
        if (m_PlayerDatabase)
            success = m_PlayerDatabase.AddBounty(data.param1, sender, player, data.param2, message);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyActionResult", new Param3<bool, string, string>(success, message, data.param1), true, sender);
        SendMoney(sender);
    }

    void RPC_RemoveOwnBounty(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        string message = "Bounty database is unavailable.";
        bool success = false;
        if (m_PlayerDatabase)
            success = m_PlayerDatabase.RemoveOwnBounty(data.param1, sender, player, message);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveBountyActionResult", new Param3<bool, string, string>(success, message, data.param1), true, sender);
        SendMoney(sender);
    }

    void RPC_RequestLeaderboardList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<int> data;
        int filter = BDE_MapUI_LEADERBOARD_FILTER_ALL;
        if (ctx.Read(data))
            filter = data.param1;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        ref array<ref BDE_MapUI_LeaderboardListEntry> list = new array<ref BDE_MapUI_LeaderboardListEntry>();
        if (m_PlayerDatabase)
            list = m_PlayerDatabase.BuildLeaderboardList(filter);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveLeaderboardList", new Param1<ref array<ref BDE_MapUI_LeaderboardListEntry>>(list), true, sender);
    }

    void RPC_RequestLeaderboardDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        if (!m_PlayerDatabase)
            m_PlayerDatabase = BDE_MapUI_PlayerDatabaseManager.Get();

        BDE_MapUI_PlayerRecord record = null;
        if (m_PlayerDatabase)
            record = m_PlayerDatabase.BuildPublicRecord(data.param1);
        if (!record)
            record = new BDE_MapUI_PlayerRecord();

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveLeaderboardDetails", new Param1<ref BDE_MapUI_PlayerRecord>(record), true, sender);
    }

    void RPC_RequestPointsStoreData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        if (!m_PointsStore)
            m_PointsStore = BDE_MapUI_PointsStoreManager.Get();

        BDE_MapUI_PointsStoreSyncData sync = new BDE_MapUI_PointsStoreSyncData();
        if (m_PointsStore)
            sync = m_PointsStore.BuildSyncData(sender);
        if (!sync)
            sync = new BDE_MapUI_PointsStoreSyncData();
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceivePointsStoreData", new Param1<ref BDE_MapUI_PointsStoreSyncData>(sync), true, sender);
    }

    void RPC_PurchasePointsStoreItem(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        if (!m_PointsStore)
            m_PointsStore = BDE_MapUI_PointsStoreManager.Get();

        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        string message = "Point store is unavailable.";
        bool success = false;
        if (m_PointsStore)
            success = m_PointsStore.PurchaseItem(sender, player, data.param1, message);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceivePointsStoreAction", new Param2<bool, string>(success, message), true, sender);

        BDE_MapUI_PointsStoreSyncData sync = new BDE_MapUI_PointsStoreSyncData();
        if (m_PointsStore)
            sync = m_PointsStore.BuildSyncData(sender);
        if (!sync)
            sync = new BDE_MapUI_PointsStoreSyncData();
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceivePointsStoreData", new Param1<ref BDE_MapUI_PointsStoreSyncData>(sync), true, sender);
    }

    // ---------------------------------------------------------------------
    // Client responses.
    // ---------------------------------------------------------------------
    void RPC_ReceiveConfig(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref BDE_MapUI_Config> data;
        if (!ctx.Read(data) || !data || !data.param1)
            return;

        BDE_MapUI_ConfigManager.Set(data.param1);
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
        {
            menu.BDE_MapUI_RefreshConfig();
            menu.BDE_MapUI_OnConfigReady();
        }
    }

    void RPC_ReceiveMoney(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param2<int, int> data;
        if (!ctx.Read(data))
            return;

        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveMoney(data.param1, data.param2);
    }

    void RPC_ReceiveDonation(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param6<int, int, string, int, int, string> data;
        if (!ctx.Read(data))
            return;

        BDE_MapUI_DonationProgressData donation = new BDE_MapUI_DonationProgressData();
        donation.ConfigVersion = data.param1;
        donation.Enabled = data.param2;
        donation.Message = data.param3;
        donation.CurrentAmount = data.param4;
        donation.MonthlyGoal = data.param5;
        donation.CurrencySymbol = data.param6;

        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveDonation(donation);
    }

    void RPC_ReceiveBountyList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveBountyList(type, ctx, sender, target);
    }

    void RPC_ReceiveBountyDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveBountyDetails(type, ctx, sender, target);
    }

    void RPC_ReceiveBountyActionResult(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveBountyActionResult(type, ctx, sender, target);
    }

    void RPC_ReceiveLeaderboardList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveLeaderboardList(type, ctx, sender, target);
    }

    void RPC_ReceiveLeaderboardDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveLeaderboardDetails(type, ctx, sender, target);
    }

    void RPC_ReceivePointsStoreData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceivePointsStoreData(type, ctx, sender, target);
    }

    void RPC_ReceivePointsStoreAction(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceivePointsStoreAction(type, ctx, sender, target);
    }

    void ReceiveMapUIVIPAccess(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param2<bool, bool> access;
        if (!ctx.Read(access))
            return;

        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveVIPAccess(access.param1, access.param2);
    }

    void ReceiveMapUIVIPData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveVIPData(type, ctx, sender, target);
    }

    void ReceiveMapUIVIPMessage(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        ExpansionMapMenu menu = GetOpenMapMenu();
        if (menu)
            menu.BDE_MapUI_ReceiveVIPMessage(type, ctx, sender, target);
    }
};

modded class MissionGameplay
{
    void MissionGameplay()
    {
        BDE_MapUI_Module.Get();
    }
};

modded class MissionServer
{
    void MissionServer()
    {
        BDE_MapUI_Module.Get();
    }

    override void OnMissionFinish()
    {
        BDE_MapUI_Module.Get().OnServerShutdown();
        BDE_MapUI_PointsStoreManager.Get().SaveAllPlayers();
        super.OnMissionFinish();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        BDE_MapUI_Module.Get().OnServerUpdate(timeslice);
    }
    override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
    {
        super.InvokeOnConnect(player, identity);
        BDE_MapUI_Module.Get().OnPlayerConnected(player, identity);
    }
    override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
    {
        BDE_MapUI_Module.Get().OnPlayerDisconnected(player, identity);
        super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
    }
};
