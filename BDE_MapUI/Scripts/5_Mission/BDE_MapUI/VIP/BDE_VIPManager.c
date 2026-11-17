// BDE_VIPManager.c
// Complete BDE VIP manager.
// Features:
// - MapUI-native VIP management secured by ReportAdminSteamIDs
// - passive income through Expansion ATM
// - passive reputation through Expansion Hardline
// - BDE MapUI Points Store VIPSteamIDs JSON sync
//

class BDE_VIPManager
{
    protected static ref BDE_VIPManager s_Instance;

    protected const string VIP_DIR = "$profile:BDE/VIP";
    protected const string VIP_CONFIG_PATH = "$profile:BDE/VIP/VIPPlayers.json";
    protected const string MAPUI_DIR = "$profile:/BDE/MapUI";
    protected const string POINTS_STORE_DIR = "$profile:/BDE/MapUI/PointsStore";
    protected const string POINTS_STORE_CONFIG_PATH = "$profile:/BDE/MapUI/PointsStore/PointsStoreConfig.json";
    protected const string MAPUI_DONATION_PATH = "$profile:/BDE/MapUI/DonationProgress.json";
    protected const string LEGACY_SERVER_PANEL_POINTS_STORE_CONFIG_PATH = "$profile:/BDE/ServerPanel/PointsStore/PointsStoreConfig.json";
    protected const string LEGACY_SERVER_PANEL_DONATION_PATH = "$profile:/BDE/ServerPanel/DonationProgress.json";

    protected ref BDE_VIPConfig m_Config;
    protected bool m_Initialised;

    protected int m_MoneyToAdd = 4000;
    protected int m_MoneyAddTimer = 30;
    protected int m_MoneyInit_Single = 0;

    protected int m_ReputationToAdd = 50;
    protected int m_RepAddTimer = 20;
    protected int m_RepInit_Single = 0;

    static BDE_VIPManager Get()
    {
        if (!s_Instance)
        {
            s_Instance = new BDE_VIPManager();
        }

        return s_Instance;
    }

    void Init()
    {
        if (m_Initialised)
        {
            return;
        }

        m_Initialised = true;

        EnsureDirectories();
        LoadConfig();


#ifdef SERVER
        RegisterServerRPCs();
#endif

        if (m_Config && m_Config.AutoReloadSeconds > 0)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ReloadConfig,m_Config.AutoReloadSeconds * 1000,true);
        }

        Log("VIP manager initialised.");
    }

    protected void EnsureDirectories()
    {
        if (!FileExist("$profile:BDE"))
        {
            MakeDirectory("$profile:BDE");
        }

        if (!FileExist(VIP_DIR))
        {
            MakeDirectory(VIP_DIR);
        }

#ifdef SERVER
        if (!FileExist("$profile:/BDE"))
        {
            MakeDirectory("$profile:/BDE");
        }

        if (!FileExist(MAPUI_DIR))
        {
            MakeDirectory(MAPUI_DIR);
        }

        if (!FileExist(POINTS_STORE_DIR))
        {
            MakeDirectory(POINTS_STORE_DIR);
        }

        MigrateLegacyServerPanelData();
#endif
    }

    void ReloadConfig()
    {
        LoadConfig();
    }

    void LoadConfig()
    {
        if (!FileExist(VIP_CONFIG_PATH))
        {
            CreateDefaultConfig();
            SaveConfig();
            Log("Created default VIP config: " + VIP_CONFIG_PATH);
        }
        else
        {
            JsonFileLoader<BDE_VIPConfig>.JsonLoadFile(VIP_CONFIG_PATH, m_Config);

            if (!m_Config)
            {
                Print("[BDE_VIPSystem] Failed to load VIP config. Creating fallback config.");
                m_Config = new BDE_VIPConfig();
            }

            if (!m_Config.Tiers)
            {
                m_Config.Tiers = new array<ref BDE_VIPTier>;
            }

            if (!m_Config.Players)
            {
                m_Config.Players = new array<ref BDE_VIPPlayerEntry>;
            }

            if (!m_Config.PendingPlayers)
            {
                m_Config.PendingPlayers = new array<ref BDE_VIPPlayerEntry>;
            }
        }

        EnsureTierDefaults();
        SyncRuntimeSettingsFromConfig();

#ifdef SERVER
        SyncPointsStoreFromVIPConfig();
#endif

        Log("Loaded VIP config. Tiers=" + m_Config.Tiers.Count() + " Players=" + m_Config.Players.Count());
    }

    void SaveConfig()
    {
        if (!m_Config)
        {
            return;
        }

        JsonFileLoader<BDE_VIPConfig>.JsonSaveFile(VIP_CONFIG_PATH, m_Config);
    }

    protected void SyncRuntimeSettingsFromConfig()
    {
        // MapUI owns base payouts; VIP tiers only adjust their multipliers.
        BDE_MapUI_Config mapConfig = BDE_MapUI_ConfigManager.Get();
        m_MoneyToAdd = mapConfig.PassiveIncomeAmount;
        m_ReputationToAdd = mapConfig.PassiveRPAmount;
        m_MoneyAddTimer = 30;
        m_RepAddTimer = 20;
    }

    protected void EnsureTierDefaults()
    {
        if (!m_Config || !m_Config.Tiers)
            return;

        float legacyPointMultiplier = 1.5;
#ifdef SERVER
        BDE_VIPPointsStoreConfig legacyPoints = LoadPointsStoreConfig();
        if (legacyPoints && legacyPoints.VIPMultiplier >= 1.0)
            legacyPointMultiplier = legacyPoints.VIPMultiplier;
#endif

        bool changed = false;
        int oldVersion = m_Config.ConfigVersion;

        foreach (BDE_VIPTier tier : m_Config.Tiers)
        {
            if (!tier)
                continue;

            // v1 used one PassiveIncomeMultiplier for both money and RP.
            if (oldVersion < 2)
            {
                if (tier.PassiveIncomeMultiplier < 1.0)
                    tier.PassiveIncomeMultiplier = 1.0;
                tier.PassiveReputationMultiplier = tier.PassiveIncomeMultiplier;
                changed = true;
            }
            // v2 used hard bonus amounts. Convert those to independent multipliers.
            else if (oldVersion < 3)
            {
                if (m_Config.PassiveIncomeBaseAmount > 0)
                    tier.PassiveIncomeMultiplier = 1.0 + (tier.PassiveIncomeBonus / (float)m_Config.PassiveIncomeBaseAmount);
                else
                    tier.PassiveIncomeMultiplier = 1.0;

                if (m_Config.PassiveReputationBaseAmount > 0)
                    tier.PassiveReputationMultiplier = 1.0 + (tier.PassiveReputationBonus / (float)m_Config.PassiveReputationBaseAmount);
                else
                    tier.PassiveReputationMultiplier = 1.0;
                changed = true;
            }

            if (tier.PassiveIncomeMultiplier < 1.0)
            {
                tier.PassiveIncomeMultiplier = 1.0;
                changed = true;
            }
            if (tier.PassiveReputationMultiplier < 1.0)
            {
                tier.PassiveReputationMultiplier = 1.0;
                changed = true;
            }
            if (tier.PointStoreMultiplier < 1.0)
            {
                tier.PointStoreMultiplier = 1.0;
                if (tier.PremiumBattlePass == 1)
                    tier.PointStoreMultiplier = legacyPointMultiplier;
                changed = true;
            }
        }

        if (m_Config.ConfigVersion < 3)
        {
            m_Config.ConfigVersion = 3;
            changed = true;
        }

        if (changed)
            SaveConfig();
    }

    protected void CreateDefaultConfig()
    {
        m_Config = new BDE_VIPConfig();

        BDE_VIPTier incomeOnly = new BDE_VIPTier();
        incomeOnly.TierName = BDE_VIPTiers.Income;
        incomeOnly.PassiveIncomeMultiplier = 1.5;
        incomeOnly.PassiveReputationMultiplier = 1.5;
        incomeOnly.PointStoreMultiplier = 1.0;
        incomeOnly.PremiumBattlePass = 0;
        m_Config.Tiers.Insert(incomeOnly);

        BDE_VIPTier battlePassOnly = new BDE_VIPTier();
        battlePassOnly.TierName = BDE_VIPTiers.BattlePass;
        battlePassOnly.PassiveIncomeMultiplier = 1.0;
        battlePassOnly.PassiveReputationMultiplier = 1.0;
        battlePassOnly.PointStoreMultiplier = 1.5;
        battlePassOnly.PremiumBattlePass = 1;
        m_Config.Tiers.Insert(battlePassOnly);

        BDE_VIPTier fullVip = new BDE_VIPTier();
        fullVip.TierName = BDE_VIPTiers.Full;
        fullVip.PassiveIncomeMultiplier = 1.5;
        fullVip.PassiveReputationMultiplier = 1.5;
        fullVip.PointStoreMultiplier = 1.5;
        fullVip.PremiumBattlePass = 1;
        m_Config.Tiers.Insert(fullVip);

        BDE_VIPPlayerEntry exampleIncome = new BDE_VIPPlayerEntry();
        exampleIncome.UID = "PASTE_DAYZ_ID_HERE";
        exampleIncome.PlainID = "PASTE_STEAM64_HERE";
        exampleIncome.Name = "ExampleIncomeOnly";
        exampleIncome.TierName = BDE_VIPTiers.Income;
        exampleIncome.Enabled = 0;
        exampleIncome.Notes = "Income/reputation multipliers only.";
        m_Config.Players.Insert(exampleIncome);

        BDE_VIPPlayerEntry exampleBattlePass = new BDE_VIPPlayerEntry();
        exampleBattlePass.UID = "PASTE_DAYZ_ID_HERE";
        exampleBattlePass.PlainID = "PASTE_STEAM64_HERE";
        exampleBattlePass.Name = "ExampleBattlePassOnly";
        exampleBattlePass.TierName = BDE_VIPTiers.BattlePass;
        exampleBattlePass.Enabled = 0;
        exampleBattlePass.Notes = "Points Store VIP rewards only.";
        m_Config.Players.Insert(exampleBattlePass);

        BDE_VIPPlayerEntry exampleFull = new BDE_VIPPlayerEntry();
        exampleFull.UID = "PASTE_DAYZ_ID_HERE";
        exampleFull.PlainID = "PASTE_STEAM64_HERE";
        exampleFull.Name = "ExampleFull";
        exampleFull.TierName = BDE_VIPTiers.Full;
        exampleFull.Enabled = 0;
        exampleFull.Notes = "Income/reputation multiplier and Points Store VIP rewards.";
        m_Config.Players.Insert(exampleFull);
    }

#ifdef SERVER
    protected void RegisterServerRPCs()
    {
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "RequestVIPAdminData", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "SetPlayerVIPTier", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "DisablePlayerVIP", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "AddPendingVIPBySteam64", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "RefreshVIPConfig", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "UpdateDonationProgress", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "AddDonationAmount", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "ResetDonationProgress", this, SingleplayerExecutionType.Server);

        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "RequestMapUIVIPAccess", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "RequestMapUIVIPData", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUISetPlayerVIPTier", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUIDisablePlayerVIP", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUIAddPendingVIP", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUISaveVIPTier", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUIUpdateDonation", this, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(BDE_VIPRPC.Server, "MapUIResetDonation", this, SingleplayerExecutionType.Server);
    }

    void QueuePointsStoreConfigSync(int delayMs = 15000)
    {
        if (delayMs < 1000)
        {
            delayMs = 1000;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SyncPointsStoreFromVIPConfig, delayMs, false);
    }

    void QueueBattlePassPlayerFileSync(int delayMs = 15000)
    {
        QueuePointsStoreConfigSync(delayMs);
    }
#endif

    // ------------------------------------------------------------
    // Player lookup
    // ------------------------------------------------------------

    BDE_VIPPlayerEntry GetPlayerEntry(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
        {
            return null;
        }

        return GetPlayerEntryByIDs(player.GetIdentity().GetId(), player.GetIdentity().GetPlainId());
    }

    BDE_VIPPlayerEntry GetPlayerEntryByUID(string uid)
    {
        return GetPlayerEntryByIDs(uid, "");
    }

    BDE_VIPPlayerEntry GetPlayerEntryByPlainID(string plainId)
    {
        return GetPlayerEntryByIDs("", plainId);
    }

    BDE_VIPPlayerEntry GetPlayerEntryByIDs(string uid, string plainId)
    {
        if (!m_Config || !m_Config.Players)
        {
            return null;
        }

        foreach (BDE_VIPPlayerEntry entry : m_Config.Players)
        {
            if (!entry)
            {
                continue;
            }

            if (entry.Enabled != 1)
            {
                continue;
            }

            if (uid != "" && entry.UID == uid)
            {
                return entry;
            }

            if (plainId != "" && entry.PlainID == plainId)
            {
                return entry;
            }
        }

        return null;
    }

    BDE_VIPPlayerEntry GetAnyPlayerEntryByIDs(string uid, string plainId)
    {
        if (!m_Config || !m_Config.Players)
        {
            return null;
        }

        foreach (BDE_VIPPlayerEntry entry : m_Config.Players)
        {
            if (!entry)
            {
                continue;
            }

            if (uid != "" && entry.UID == uid)
            {
                return entry;
            }

            if (plainId != "" && entry.PlainID == plainId)
            {
                return entry;
            }
        }

        return null;
    }


    BDE_VIPPlayerEntry GetPendingPlayerEntryByPlainID(string plainId)
    {
        if (!m_Config || !m_Config.PendingPlayers || plainId == "")
        {
            return null;
        }

        foreach (BDE_VIPPlayerEntry entry : m_Config.PendingPlayers)
        {
            if (!entry)
            {
                continue;
            }

            if (entry.PlainID == plainId)
            {
                return entry;
            }
        }

        return null;
    }

    BDE_VIPTier GetTierByName(string tierName)
    {
        if (!m_Config || !m_Config.Tiers || tierName == "")
        {
            return null;
        }

        foreach (BDE_VIPTier tier : m_Config.Tiers)
        {
            if (!tier)
            {
                continue;
            }

            if (tier.TierName == tierName)
            {
                return tier;
            }
        }

        return null;
    }

    BDE_VIPTier GetPlayerTier(PlayerBase player)
    {
        BDE_VIPPlayerEntry entry = GetPlayerEntry(player);

        if (!entry)
        {
            return null;
        }

        return GetTierByName(entry.TierName);
    }

    BDE_VIPTier GetPlayerTierByIDs(string uid, string plainId)
    {
        BDE_VIPPlayerEntry entry = GetPlayerEntryByIDs(uid, plainId);

        if (!entry)
        {
            return null;
        }

        return GetTierByName(entry.TierName);
    }

    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------

    bool IsVIP(PlayerBase player)
    {
        return GetPlayerEntry(player) != null;
    }

    string GetTierName(PlayerBase player)
    {
        BDE_VIPPlayerEntry entry = GetPlayerEntry(player);

        if (!entry)
        {
            return "";
        }

        return entry.TierName;
    }

    bool HasPremiumBattlePass(PlayerBase player)
    {
        BDE_VIPTier tier = GetPlayerTier(player);

        if (!tier)
        {
            return false;
        }

        return tier.PremiumBattlePass == 1;
    }

    bool HasPremiumBattlePassByIDs(string uid, string plainId)
    {
        BDE_VIPTier tier = GetPlayerTierByIDs(uid, plainId);

        if (!tier)
        {
            return false;
        }

        return tier.PremiumBattlePass == 1;
    }

    bool HasPointsStoreVIP(PlayerBase player)
    {
        return HasPremiumBattlePass(player);
    }

    bool HasPointsStoreVIPByIDs(string uid, string plainId)
    {
        return HasPremiumBattlePassByIDs(uid, plainId);
    }

    bool HasPassiveBonus(PlayerBase player)
    {
        BDE_VIPTier tier = GetPlayerTier(player);

        if (!tier)
        {
            return false;
        }

        return tier.PassiveIncomeMultiplier > 1.0 || tier.PassiveReputationMultiplier > 1.0;
    }

    int ApplyPassiveIncome(PlayerBase player, int baseAmount)
    {
        BDE_VIPTier tier = GetPlayerTier(player);

        if (!tier)
        {
            return baseAmount;
        }

        return ApplyPassiveMultiplier(baseAmount, tier.PassiveIncomeMultiplier);
    }

    int ApplyPassiveReputation(PlayerBase player, int baseAmount)
    {
        BDE_VIPTier tier = GetPlayerTier(player);

        if (!tier)
        {
            return baseAmount;
        }

        return ApplyPassiveMultiplier(baseAmount, tier.PassiveReputationMultiplier);
    }

    protected int ApplyPassiveMultiplier(int baseAmount, float multiplier)
    {
        if (multiplier < 1.0)
            multiplier = 1.0;

        int finalAmount = Math.Round(baseAmount * multiplier);
        if (finalAmount < 0)
            finalAmount = 0;

        return finalAmount;
    }


#ifdef SERVER
    void ProcessPendingVIPForIdentity(PlayerIdentity identity)
    {
        if (!identity || !m_Config || !m_Config.PendingPlayers)
        {
            return;
        }

        string plainId = identity.GetPlainId();

        if (plainId == "")
        {
            return;
        }

        bool changed = false;

        for (int i = m_Config.PendingPlayers.Count() - 1; i >= 0; i--)
        {
            BDE_VIPPlayerEntry pending = m_Config.PendingPlayers[i];

            if (!pending || pending.PlainID != plainId)
            {
                continue;
            }

            UpsertVIPPlayer(identity.GetId(), identity.GetPlainId(), identity.GetName(), pending.TierName, pending.Enabled);
            m_Config.PendingPlayers.Remove(i);
            changed = true;
        }

        if (changed)
        {
            SaveConfig();
            SyncPointsStoreFromVIPConfig();
            QueuePointsStoreConfigSync(15000);
            Log("Promoted pending VIP entry for " + identity.GetName() + " / " + plainId);
        }
    }
#endif

    // ------------------------------------------------------------
    // Passive money / reputation timers
    // ------------------------------------------------------------

#ifdef SERVER
    void StartPassiveIncomeTimerOnce()
    {
        SyncRuntimeSettingsFromConfig();
        if (!m_Config || m_MoneyToAdd <= 0)
        {
            return;
        }

        if (m_MoneyInit_Single == 1)
        {
            return;
        }

        m_MoneyInit_Single = 1;

        int intervalMs = m_MoneyAddTimer * 60 * 1000;

        if (intervalMs <= 0)
        {
            intervalMs = 30 * 60 * 1000;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AddMoneyToActivePlayers, intervalMs, true);
        Log("Passive income timer started. Amount=" + m_MoneyToAdd + " TimerMinutes=" + m_MoneyAddTimer);
    }

    void StartPassiveRepTimerOnce()
    {
        SyncRuntimeSettingsFromConfig();
        if (!m_Config || m_ReputationToAdd <= 0)
        {
            return;
        }

        if (m_RepInit_Single == 1)
        {
            return;
        }

        m_RepInit_Single = 1;

        int intervalMs = m_RepAddTimer * 60 * 1000;

        if (intervalMs <= 0)
        {
            intervalMs = 20 * 60 * 1000;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AddRepToActivePlayers, intervalMs, true);
        Log("Passive reputation timer started. Amount=" + m_ReputationToAdd + " TimerMinutes=" + m_RepAddTimer);
    }

    void AddMoneyToActivePlayers()
    {
        SyncRuntimeSettingsFromConfig();
        if (!m_Config || m_MoneyToAdd <= 0)
        {
            return;
        }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        ExpansionMarketModule marketModule = ExpansionMarketModule.Cast(CF_ModuleCoreManager.Get(ExpansionMarketModule));

        if (!marketModule)
        {
            Print("[BDE_VIPSystem] ExpansionMarketModule not found. Passive income skipped.");
            return;
        }

        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);

            if (!player || !player.GetIdentity())
            {
                continue;
            }

            int amountToAdd = ApplyPassiveIncome(player, m_MoneyToAdd);

            if (amountToAdd <= 0)
            {
                continue;
            }

            ExpansionMarketATM_Data playerATMData = marketModule.GetPlayerATMData(player.GetIdentity().GetId());

            if (!playerATMData)
            {
                continue;
            }

            int currentATMMoney = playerATMData.GetMoney();
            int maxDepositMoney = GetExpansionSettings().GetMarket().MaxDepositMoney;
            int possibleDepositAmount = maxDepositMoney - currentATMMoney;

            if (possibleDepositAmount <= 0)
            {
                ExpansionNotification(new StringLocaliser("Payment"), new StringLocaliser("Your Bank account is full!"), ExpansionIcons.GetPath("Exclamationmark"), COLOR_EXPANSION_NOTIFICATION_SUCCSESS, 7, ExpansionNotificationType.TOAST).Create(player.GetIdentity());
                continue;
            }

            int finalDeposit = amountToAdd;

            if (finalDeposit > possibleDepositAmount)
            {
                finalDeposit = possibleDepositAmount;
            }

            playerATMData.AddMoney(finalDeposit);

            if (finalDeposit < amountToAdd)
            {
                ExpansionNotification(new StringLocaliser("Payment"), new StringLocaliser("You have received a lower pay cheque of %1 to your Bank account. Your Bank account is full!", finalDeposit.ToString()), ExpansionIcons.GetPath("Exclamationmark"), COLOR_EXPANSION_NOTIFICATION_SUCCSESS, 7, ExpansionNotificationType.TOAST).Create(player.GetIdentity());
            }
            else
            {
                ExpansionNotification(new StringLocaliser("Payment"), new StringLocaliser("You have received a pay cheque of %1 to your Bank account.", finalDeposit.ToString()), ExpansionIcons.GetPath("Exclamationmark"), COLOR_EXPANSION_NOTIFICATION_SUCCSESS, 7, ExpansionNotificationType.TOAST).Create(player.GetIdentity());
            }
        }
    }

    void AddRepToActivePlayers()
    {
        SyncRuntimeSettingsFromConfig();
        if (!m_Config || m_ReputationToAdd <= 0)
        {
            return;
        }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        auto hardlineSettings = GetExpansionSettings().GetHardline();

        bool useRep = true;
        int maxRep = 0;

        if (hardlineSettings)
        {
            useRep = hardlineSettings.UseReputation;
            maxRep = hardlineSettings.MaxReputation;
        }

        if (!useRep)
        {
            return;
        }

        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);

            if (!player || !player.GetIdentity())
            {
                continue;
            }

            int amountToAdd = ApplyPassiveReputation(player, m_ReputationToAdd);

            if (amountToAdd <= 0)
            {
                continue;
            }

            if (maxRep > 0)
            {
                int currentRep = player.Expansion_GetReputation();

                if (currentRep >= maxRep)
                {
                    ExpansionNotification(new StringLocaliser("Reputation"), new StringLocaliser("Your reputation is already at the maximum."), ExpansionIcons.GetPath("Exclamationmark"), COLOR_EXPANSION_NOTIFICATION_SUCCSESS, 7, ExpansionNotificationType.TOAST).Create(player.GetIdentity());
                    continue;
                }

                int possibleRep = maxRep - currentRep;

                if (amountToAdd > possibleRep)
                {
                    amountToAdd = possibleRep;
                }
            }

            if (amountToAdd <= 0)
            {
                continue;
            }

            player.Expansion_AddReputation(amountToAdd);

            ExpansionNotification(new StringLocaliser("Reputation"), new StringLocaliser("You have gained %1 reputation.", amountToAdd.ToString()), ExpansionIcons.GetPath("Exclamationmark"), COLOR_EXPANSION_NOTIFICATION_SUCCSESS, 7, ExpansionNotificationType.TOAST).Create(player.GetIdentity());
        }
    }
#endif

    // ------------------------------------------------------------
    // BDE MapUI Points Store VIP sync
    // ------------------------------------------------------------

#ifdef SERVER
    protected void MigrateLegacyServerPanelData()
    {
        if (!FileExist(MAPUI_DIR))
        {
            MakeDirectory(MAPUI_DIR);
        }

        if (!FileExist(POINTS_STORE_DIR))
        {
            MakeDirectory(POINTS_STORE_DIR);
        }

        // Preserve the old Server Panel Points Store configuration on first MapUI run.
        if (!FileExist(POINTS_STORE_CONFIG_PATH) && FileExist(LEGACY_SERVER_PANEL_POINTS_STORE_CONFIG_PATH))
        {
            ref BDE_VIPPointsStoreConfig legacyPointsConfig = new BDE_VIPPointsStoreConfig();
            JsonFileLoader<BDE_VIPPointsStoreConfig>.JsonLoadFile(LEGACY_SERVER_PANEL_POINTS_STORE_CONFIG_PATH, legacyPointsConfig);

            if (legacyPointsConfig)
            {
                JsonFileLoader<BDE_VIPPointsStoreConfig>.JsonSaveFile(POINTS_STORE_CONFIG_PATH, legacyPointsConfig);
                Log("Migrated legacy Server Panel Points Store config to MapUI.");
            }
        }

        // Preserve donation progress before MapUI creates a fresh default file.
        if (!FileExist(MAPUI_DONATION_PATH) && FileExist(LEGACY_SERVER_PANEL_DONATION_PATH))
        {
            ref BDE_MapUI_DonationProgressData legacyDonation = new BDE_MapUI_DonationProgressData();
            JsonFileLoader<BDE_MapUI_DonationProgressData>.JsonLoadFile(LEGACY_SERVER_PANEL_DONATION_PATH, legacyDonation);

            if (legacyDonation)
            {
                JsonFileLoader<BDE_MapUI_DonationProgressData>.JsonSaveFile(MAPUI_DONATION_PATH, legacyDonation);
                Log("Migrated legacy Server Panel donation progress to MapUI.");
            }
        }
    }

    protected ref BDE_VIPPointsStoreConfig LoadPointsStoreConfig()
    {
        ref BDE_VIPPointsStoreConfig pointsConfig = new BDE_VIPPointsStoreConfig();

        if (!FileExist("$profile:/BDE"))
        {
            MakeDirectory("$profile:/BDE");
        }

        if (!FileExist(MAPUI_DIR))
        {
            MakeDirectory(MAPUI_DIR);
        }

        if (!FileExist(POINTS_STORE_DIR))
        {
            MakeDirectory(POINTS_STORE_DIR);
        }

        if (FileExist(POINTS_STORE_CONFIG_PATH))
        {
            JsonFileLoader<BDE_VIPPointsStoreConfig>.JsonLoadFile(POINTS_STORE_CONFIG_PATH, pointsConfig);
        }

        if (!pointsConfig)
        {
            pointsConfig = new BDE_VIPPointsStoreConfig();
        }

        if (pointsConfig.StoreName == "")
        {
            pointsConfig.StoreName = "Task Points Store";
        }

        if (pointsConfig.VIPMultiplier <= 0.0)
        {
            pointsConfig.VIPMultiplier = 1.5;
        }

        if (!pointsConfig.VIPSteamIDs)
        {
            pointsConfig.VIPSteamIDs = new array<string>;
        }

        if (!pointsConfig.Tasks)
        {
            pointsConfig.Tasks = new array<ref BDE_VIPPointTask>;
        }

        if (!pointsConfig.StoreItems)
        {
            pointsConfig.StoreItems = new array<ref BDE_VIPPointStoreItem>;
        }

        return pointsConfig;
    }

    protected void SavePointsStoreConfig(BDE_VIPPointsStoreConfig pointsConfig)
    {
        if (!pointsConfig)
        {
            return;
        }

        JsonFileLoader<BDE_VIPPointsStoreConfig>.JsonSaveFile(POINTS_STORE_CONFIG_PATH, pointsConfig);
    }

    protected void AddStringUnique(array<string> arr, string value)
    {
        if (!arr || value == "")
        {
            return;
        }

        if (arr.Find(value) == -1)
        {
            arr.Insert(value);
        }
    }

    protected void RemoveStringValue(array<string> arr, string value)
    {
        if (!arr || value == "")
        {
            return;
        }

        int index = arr.Find(value);

        while (index > -1)
        {
            arr.Remove(index);
            index = arr.Find(value);
        }
    }


    protected void SyncPointsStoreEntry(BDE_VIPPointsStoreConfig pointsConfig, BDE_VIPPlayerEntry entry)
    {
        if (!pointsConfig || !entry)
        {
            return;
        }

        if (!pointsConfig.VIPSteamIDs)
        {
            pointsConfig.VIPSteamIDs = new array<string>;
        }

        bool shouldHaveVIP = false;

        if (entry.Enabled == 1)
        {
            BDE_VIPTier tier = GetTierByName(entry.TierName);

            if (tier && tier.PremiumBattlePass == 1)
            {
                shouldHaveVIP = true;
            }
        }

        if (shouldHaveVIP)
        {
            AddStringUnique(pointsConfig.VIPSteamIDs, entry.PlainID);
            AddStringUnique(pointsConfig.VIPSteamIDs, entry.UID);
        }
        else
        {
            RemoveStringValue(pointsConfig.VIPSteamIDs, entry.PlainID);
            RemoveStringValue(pointsConfig.VIPSteamIDs, entry.UID);
        }
    }

    protected void SyncPointsStoreFromVIPConfig()
    {
        if (!m_Config)
        {
            return;
        }

        if (!m_Config.Players)
        {
            m_Config.Players = new array<ref BDE_VIPPlayerEntry>;
        }

        if (!m_Config.PendingPlayers)
        {
            m_Config.PendingPlayers = new array<ref BDE_VIPPlayerEntry>;
        }

        ref BDE_VIPPointsStoreConfig pointsConfig = LoadPointsStoreConfig();

        if (!pointsConfig)
        {
            return;
        }

        foreach (BDE_VIPPlayerEntry entry : m_Config.Players)
        {
            SyncPointsStoreEntry(pointsConfig, entry);
        }

        foreach (BDE_VIPPlayerEntry pendingEntry : m_Config.PendingPlayers)
        {
            SyncPointsStoreEntry(pointsConfig, pendingEntry);
        }

        SavePointsStoreConfig(pointsConfig);
    }
#endif

    // ------------------------------------------------------------
    // MapUI admin management
    // Uses the exact same Steam64 whitelist as the Reports admin view.
    // ------------------------------------------------------------

#ifdef SERVER
    protected bool IsMapUIWhitelistedAdmin(PlayerIdentity sender)
    {
        if (!sender)
        {
            return false;
        }

        string steamId = sender.GetPlainId();
        if (steamId == "")
        {
            return false;
        }

        BDE_MapUI_Config mapConfig = BDE_MapUI_ConfigManager.Get();
        if (!mapConfig || !mapConfig.ReportAdminSteamIDs)
        {
            return false;
        }

        return mapConfig.ReportAdminSteamIDs.Find(steamId) >= 0;
    }

    protected bool HasVIPAdminViewPermission(PlayerIdentity sender)
    {
        return IsMapUIWhitelistedAdmin(sender);
    }

    protected bool HasVIPAdminManagePermission(PlayerIdentity sender)
    {
        return IsMapUIWhitelistedAdmin(sender);
    }

    protected void SendVIPAdminMessage(PlayerIdentity receiver, string message, int color = 0)
    {
        if (!receiver)
        {
            return;
        }

        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveVIPAdminMessage", new Param2<string, int>(message, color), true, receiver);
    }

    protected void SendVIPAdminRows(PlayerIdentity receiver)
    {
        if (!receiver)
        {
            return;
        }

        ref array<string> rows = BuildVIPAdminRows();
        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveVIPAdminData", new Param1<ref array<string>>(rows), true, receiver);
    }

    protected void SendDonationAdminData(PlayerIdentity targetIdentity)
    {
        if (!targetIdentity)
        {
            return;
        }

        ref BDE_MapUI_DonationProgressData donationData = BDE_MapUI_DonationProgressManager.GetData();

        if (!donationData)
        {
            donationData = new BDE_MapUI_DonationProgressData();
        }

        // Primitive-only payload exactly matches BDEVIPForm.ReceiveDonationAdminData.
        Param4<int, int, string, int> rpcData = new Param4<int, int, string, int>(donationData.CurrentAmount, donationData.MonthlyGoal, donationData.CurrencySymbol, donationData.Enabled);
        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveDonationAdminData", rpcData, true, targetIdentity);
    }

    protected void SendMapUIVIPMessage(PlayerIdentity receiver, string message, int color = 0)
    {
        if (!receiver)
            return;
        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveMapUIVIPMessage", new Param2<string, int>(message, color), true, receiver);
    }

    protected PlayerBase FindOnlinePlayerByUID(string uid)
    {
        if (uid == "")
            return null;

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (player && player.GetIdentity() && player.GetIdentity().GetId() == uid)
                return player;
        }
        return null;
    }

    protected int GetVIPAdminSortRank(BDE_MapUI_VIPPlayerAdminEntry entry)
    {
        if (!entry)
            return 0;
        if (entry.HasVIP && !entry.Pending)
            return 4;
        if (entry.Pending)
            return 3;
        if (entry.IsOnline)
            return 2;
        return 1;
    }

    protected void SortMapUIVIPPlayers(array<ref BDE_MapUI_VIPPlayerAdminEntry> list)
    {
        if (!list)
            return;

        for (int i = 0; i < list.Count() - 1; i++)
        {
            for (int j = i + 1; j < list.Count(); j++)
            {
                BDE_MapUI_VIPPlayerAdminEntry a = list.Get(i);
                BDE_MapUI_VIPPlayerAdminEntry b = list.Get(j);
                if (!a || !b)
                    continue;

                int rankA = GetVIPAdminSortRank(a);
                int rankB = GetVIPAdminSortRank(b);
                bool swap = false;
                if (rankB > rankA)
                    swap = true;
                else if (rankA == rankB && b.IsOnline && !a.IsOnline)
                    swap = true;

                if (swap)
                {
                    list.Set(i, b);
                    list.Set(j, a);
                }
            }
        }
    }

    protected bool MapUIVIPSnapshotContains(array<ref BDE_MapUI_VIPPlayerAdminEntry> list, string uid, string plainId)
    {
        if (!list)
            return false;
        foreach (BDE_MapUI_VIPPlayerAdminEntry row : list)
        {
            if (!row)
                continue;
            if (uid != "" && row.UID == uid)
                return true;
            if (plainId != "" && row.PlainID == plainId)
                return true;
        }
        return false;
    }

    protected BDE_MapUI_VIPAdminSnapshot BuildMapUIVIPSnapshot(PlayerIdentity receiver)
    {
        BDE_MapUI_VIPAdminSnapshot snapshot = new BDE_MapUI_VIPAdminSnapshot();
        snapshot.CanView = HasVIPAdminViewPermission(receiver);
        snapshot.CanManage = HasVIPAdminManagePermission(receiver);
        if (!snapshot.CanView || !m_Config)
            return snapshot;

        SyncRuntimeSettingsFromConfig();
        snapshot.PassiveIncomeBaseAmount = m_MoneyToAdd;
        snapshot.PassiveIncomeTimerMinutes = m_MoneyAddTimer;
        snapshot.PassiveReputationBaseAmount = m_ReputationToAdd;
        snapshot.PassiveReputationTimerMinutes = m_RepAddTimer;

        if (m_Config.Tiers)
        {
            foreach (BDE_VIPTier tier : m_Config.Tiers)
            {
                if (!tier)
                    continue;
                BDE_MapUI_VIPTierData tierData = new BDE_MapUI_VIPTierData();
                tierData.TierName = tier.TierName;
                tierData.PassiveIncomeMultiplier = tier.PassiveIncomeMultiplier;
                tierData.PassiveReputationMultiplier = tier.PassiveReputationMultiplier;
                tierData.PointStoreMultiplier = tier.PointStoreMultiplier;
                tierData.PremiumBattlePass = tier.PremiumBattlePass;
                snapshot.Tiers.Insert(tierData);
            }
        }

        BDE_MapUI_DonationProgressData donation = BDE_MapUI_DonationProgressManager.GetData();
        if (donation)
        {
            snapshot.DonationCurrentAmount = donation.CurrentAmount;
            snapshot.DonationMonthlyGoal = donation.MonthlyGoal;
            snapshot.DonationCurrencySymbol = donation.CurrencySymbol;
            snapshot.DonationMessage = donation.Message;
        }

        ref array<ref BDE_MapUI_LeaderboardListEntry> allPlayers = BDE_MapUI_PlayerDatabaseManager.Get().BuildLeaderboardList(BDE_MapUI_LEADERBOARD_FILTER_ALL);
        if (allPlayers)
        {
            foreach (BDE_MapUI_LeaderboardListEntry playerRow : allPlayers)
            {
                if (!playerRow)
                    continue;

                BDE_MapUI_VIPPlayerAdminEntry row = new BDE_MapUI_VIPPlayerAdminEntry();
                row.UID = playerRow.UID;
                row.Name = playerRow.Name;
                row.IsOnline = playerRow.IsOnline;

                PlayerBase onlinePlayer = FindOnlinePlayerByUID(row.UID);
                if (onlinePlayer && onlinePlayer.GetIdentity())
                    row.PlainID = onlinePlayer.GetIdentity().GetPlainId();

                BDE_VIPPlayerEntry vipEntry = GetAnyPlayerEntryByIDs(row.UID, row.PlainID);
                if (vipEntry)
                {
                    if (row.PlainID == "") row.PlainID = vipEntry.PlainID;
                    if (row.Name == "" || row.Name == "Unknown") row.Name = vipEntry.Name;
                    row.TierName = vipEntry.TierName;
                    row.Enabled = vipEntry.Enabled == 1;
                    row.HasVIP = row.Enabled;
                    row.Notes = vipEntry.Notes;
                }

                snapshot.Players.Insert(row);
            }
        }

        // Also include configured VIP entries that predate the MapUI player database.
        if (m_Config.Players)
        {
            foreach (BDE_VIPPlayerEntry configured : m_Config.Players)
            {
                if (!configured || MapUIVIPSnapshotContains(snapshot.Players, configured.UID, configured.PlainID))
                    continue;
                BDE_MapUI_VIPPlayerAdminEntry configuredRow = new BDE_MapUI_VIPPlayerAdminEntry();
                configuredRow.UID = configured.UID;
                configuredRow.PlainID = configured.PlainID;
                configuredRow.Name = configured.Name;
                configuredRow.TierName = configured.TierName;
                configuredRow.Enabled = configured.Enabled == 1;
                configuredRow.HasVIP = configuredRow.Enabled;
                configuredRow.Notes = configured.Notes;
                PlayerBase configuredOnline = FindOnlinePlayerByUID(configured.UID);
                configuredRow.IsOnline = configuredOnline != null;
                snapshot.Players.Insert(configuredRow);
            }
        }

        // Keep manual Steam64 entries visible until their first login promotes them.
        if (m_Config.PendingPlayers)
        {
            foreach (BDE_VIPPlayerEntry pending : m_Config.PendingPlayers)
            {
                if (!pending)
                    continue;
                BDE_MapUI_VIPPlayerAdminEntry pendingRow = new BDE_MapUI_VIPPlayerAdminEntry();
                pendingRow.UID = pending.UID;
                pendingRow.PlainID = pending.PlainID;
                pendingRow.Name = pending.Name;
                pendingRow.TierName = pending.TierName;
                pendingRow.Enabled = pending.Enabled == 1;
                pendingRow.HasVIP = pendingRow.Enabled;
                pendingRow.Pending = true;
                pendingRow.Notes = pending.Notes;
                snapshot.Players.Insert(pendingRow);
            }
        }

        SortMapUIVIPPlayers(snapshot.Players);
        return snapshot;
    }

    protected void SendMapUIVIPData(PlayerIdentity receiver)
    {
        if (!receiver || !HasVIPAdminViewPermission(receiver))
            return;
        BDE_MapUI_VIPAdminSnapshot snapshot = BuildMapUIVIPSnapshot(receiver);
        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveMapUIVIPData", new Param1<ref BDE_MapUI_VIPAdminSnapshot>(snapshot), true, receiver);
    }

    void RequestMapUIVIPAccess(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender)
            return;
        bool canView = HasVIPAdminViewPermission(sender);
        bool canManage = HasVIPAdminManagePermission(sender);
        GetRPCManager().SendRPC(BDE_VIPRPC.Client, "ReceiveMapUIVIPAccess", new Param2<bool, bool>(canView, canManage), true, sender);
    }

    void RequestMapUIVIPData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminViewPermission(sender))
            return;
        SendMapUIVIPData(sender);
    }

    void MapUISetPlayerVIPTier(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }

        Param4<string, string, string, string> data;
        if (!ctx.Read(data))
            return;
        if (data.param4 == "" || !GetTierByName(data.param4))
        {
            SendMapUIVIPMessage(sender, "Invalid VIP tier: " + data.param4);
            return;
        }

        UpsertVIPPlayer(data.param1, data.param2, data.param3, data.param4, 1);
        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendMapUIVIPMessage(sender, "VIP updated: " + data.param3 + " -> " + data.param4);
        SendMapUIVIPData(sender);
    }

    void MapUIDisablePlayerVIP(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }

        Param2<string, string> data;
        if (!ctx.Read(data))
            return;
        DisableVIPPlayer(data.param1, data.param2);
        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendMapUIVIPMessage(sender, "VIP removed/disabled.");
        SendMapUIVIPData(sender);
    }

    void MapUIAddPendingVIP(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }

        Param2<string, string> data;
        if (!ctx.Read(data))
            return;
        string plainId = data.param1;
        string tierName = data.param2;
        if (plainId == "" || plainId.Length() < 16)
        {
            SendMapUIVIPMessage(sender, "Invalid Steam64 ID.");
            return;
        }
        if (tierName == "" || !GetTierByName(tierName))
        {
            SendMapUIVIPMessage(sender, "Invalid VIP tier: " + tierName);
            return;
        }

        PlayerBase onlinePlayer = FindOnlinePlayerByPlainID(plainId);
        if (onlinePlayer && onlinePlayer.GetIdentity())
            UpsertVIPPlayer(onlinePlayer.GetIdentity().GetId(), plainId, onlinePlayer.GetIdentity().GetName(), tierName, 1);
        else
            UpsertPendingVIPPlayer(plainId, tierName, 1);

        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendMapUIVIPMessage(sender, "Pending VIP saved: " + plainId + " -> " + tierName);
        SendMapUIVIPData(sender);
    }

    void MapUISaveVIPTier(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }

        Param1<ref BDE_MapUI_VIPTierData> data;
        if (!ctx.Read(data) || !data.param1)
            return;
        BDE_MapUI_VIPTierData incoming = data.param1;
        if (incoming.TierName == "")
        {
            SendMapUIVIPMessage(sender, "Tier name cannot be empty.");
            return;
        }
        if (incoming.PassiveIncomeMultiplier < 1.0 || incoming.PassiveReputationMultiplier < 1.0 || incoming.PointStoreMultiplier < 1.0)
        {
            SendMapUIVIPMessage(sender, "VIP multipliers must be 1.0 or greater.");
            return;
        }

        BDE_VIPTier tier = GetTierByName(incoming.TierName);
        if (!tier)
        {
            tier = new BDE_VIPTier();
            tier.TierName = incoming.TierName;
            m_Config.Tiers.Insert(tier);
        }
        tier.PassiveIncomeMultiplier = incoming.PassiveIncomeMultiplier;
        tier.PassiveReputationMultiplier = incoming.PassiveReputationMultiplier;
        tier.PointStoreMultiplier = incoming.PointStoreMultiplier;
        tier.PremiumBattlePass = incoming.PremiumBattlePass;
        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendMapUIVIPMessage(sender, "VIP tier saved: " + tier.TierName);
        SendMapUIVIPData(sender);
    }

    void MapUIUpdateDonation(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }

        Param2<int, int> data;
        if (!ctx.Read(data))
            return;
        if (data.param1 < 0 || data.param2 < 0)
        {
            SendMapUIVIPMessage(sender, "Donation values cannot be negative.");
            return;
        }
        if (!BDE_MapUI_DonationProgressManager.SetAmounts(data.param1, data.param2))
        {
            SendMapUIVIPMessage(sender, "Failed to save donation progress.");
            return;
        }
        SendMapUIVIPMessage(sender, "Donation goal saved.");
        SendMapUIVIPData(sender);
    }

    void MapUIResetDonation(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender || !HasVIPAdminManagePermission(sender))
        {
            SendMapUIVIPMessage(sender, "Missing VIP manage permission.");
            return;
        }
        if (!BDE_MapUI_DonationProgressManager.ResetCurrentAmount())
        {
            SendMapUIVIPMessage(sender, "Failed to reset donation progress.");
            return;
        }
        SendMapUIVIPMessage(sender, "Donation current amount reset.");
        SendMapUIVIPData(sender);
    }

    void RequestVIPAdminData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminViewPermission(sender))
        {
            return;
        }

        SendVIPAdminRows(sender);
        SendDonationAdminData(sender);
    }

    void RefreshVIPConfig(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminViewPermission(sender))
        {
            return;
        }

        LoadConfig();
        BDE_MapUI_DonationProgressManager.GetData(true);
        BDE_MapUI_DonationProgressManager.Broadcast();
        SendVIPAdminRows(sender);
        SendDonationAdminData(sender);
        SendVIPAdminMessage(sender, "VIP and donation progress refreshed.");
    }

    void UpdateDonationProgress(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        Param2<int, int> data;

        if (!ctx.Read(data))
        {
            return;
        }

        int currentAmount = data.param1;
        int monthlyGoal = data.param2;

        if (currentAmount < 0 || monthlyGoal < 0)
        {
            SendVIPAdminMessage(sender, "Donation values cannot be negative.");
            return;
        }

        if (!BDE_MapUI_DonationProgressManager.SetAmounts(currentAmount, monthlyGoal))
        {
            SendVIPAdminMessage(sender, "Failed to save donation progress.");
            return;
        }

        SendDonationAdminData(sender);
        SendVIPAdminMessage(sender, "Monthly donation progress saved.");
    }

    void AddDonationAmount(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        Param1<int> data;

        if (!ctx.Read(data))
        {
            return;
        }

        int amount = data.param1;

        if (amount <= 0)
        {
            SendVIPAdminMessage(sender, "Donation amount must be greater than zero.");
            return;
        }

        if (!BDE_MapUI_DonationProgressManager.AddDonation(amount))
        {
            SendVIPAdminMessage(sender, "Failed to add donation amount.");
            return;
        }

        SendDonationAdminData(sender);
        SendVIPAdminMessage(sender, "Added " + amount.ToString() + " to the monthly donation total.");
    }

    void ResetDonationProgress(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        if (!BDE_MapUI_DonationProgressManager.ResetCurrentAmount())
        {
            SendVIPAdminMessage(sender, "Failed to reset donation progress.");
            return;
        }

        SendDonationAdminData(sender);
        SendVIPAdminMessage(sender, "Monthly donation total reset to zero.");
    }

    void SetPlayerVIPTier(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        Param4<string, string, string, string> data;

        if (!ctx.Read(data))
        {
            return;
        }

        string uid = data.param1;
        string plainId = data.param2;
        string name = data.param3;
        string tierName = data.param4;

        if (tierName == "" || !GetTierByName(tierName))
        {
            SendVIPAdminMessage(sender, "Invalid VIP tier: " + tierName);
            return;
        }

        UpsertVIPPlayer(uid, plainId, name, tierName, 1);
        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendVIPAdminMessage(sender, "VIP updated: " + name + " -> " + tierName);
        SendVIPAdminRows(sender);
    }

    void AddPendingVIPBySteam64(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        Param2<string, string> data;

        if (!ctx.Read(data))
        {
            return;
        }

        string plainId = data.param1;
        string tierName = data.param2;

        if (plainId == "" || plainId.Length() < 16)
        {
            SendVIPAdminMessage(sender, "Invalid Steam64 ID.");
            return;
        }

        if (tierName == "" || !GetTierByName(tierName))
        {
            SendVIPAdminMessage(sender, "Invalid VIP tier: " + tierName);
            return;
        }

        PlayerBase onlinePlayer = FindOnlinePlayerByPlainID(plainId);

        if (onlinePlayer && onlinePlayer.GetIdentity())
        {
            UpsertVIPPlayer(onlinePlayer.GetIdentity().GetId(), onlinePlayer.GetIdentity().GetPlainId(), onlinePlayer.GetIdentity().GetName(), tierName, 1);
            SendVIPAdminMessage(sender, "VIP updated for online player: " + onlinePlayer.GetIdentity().GetName() + " -> " + tierName);
        }
        else
        {
            UpsertPendingVIPPlayer(plainId, tierName, 1);
            SendVIPAdminMessage(sender, "Pending VIP added for Steam64: " + plainId + " -> " + tierName);
        }

        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendVIPAdminRows(sender);
    }

    void DisablePlayerVIP(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
        {
            return;
        }

        if (!sender || !HasVIPAdminManagePermission(sender))
        {
            SendVIPAdminMessage(sender, "VIP Manager access requires your Steam64 ID in ReportAdminSteamIDs.");
            return;
        }

        Param2<string, string> data;

        if (!ctx.Read(data))
        {
            return;
        }

        string uid = data.param1;
        string plainId = data.param2;

        DisableVIPPlayer(uid, plainId);
        SaveConfig();
        SyncPointsStoreFromVIPConfig();
        SendVIPAdminMessage(sender, "VIP disabled.");
        SendVIPAdminRows(sender);
    }

    protected void UpsertVIPPlayer(string uid, string plainId, string name, string tierName, int enabled)
    {
        if (!m_Config)
        {
            m_Config = new BDE_VIPConfig();
        }

        if (!m_Config.Players)
        {
            m_Config.Players = new array<ref BDE_VIPPlayerEntry>;
        }

        BDE_VIPPlayerEntry entry = GetAnyPlayerEntryByIDs(uid, plainId);

        if (!entry)
        {
            entry = new BDE_VIPPlayerEntry();
            m_Config.Players.Insert(entry);
        }

        if (uid != "")
        {
            entry.UID = uid;
        }

        if (plainId != "")
        {
            entry.PlainID = plainId;
        }

        if (name != "")
        {
            entry.Name = name;
        }

        entry.TierName = tierName;
        entry.Enabled = enabled;
        entry.Notes = "Managed through MapUI VIP Manager";
    }


    protected PlayerBase FindOnlinePlayerByPlainID(string plainId)
    {
        if (plainId == "")
        {
            return null;
        }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);

            if (!player || !player.GetIdentity())
            {
                continue;
            }

            if (player.GetIdentity().GetPlainId() == plainId)
            {
                return player;
            }
        }

        return null;
    }

    protected void UpsertPendingVIPPlayer(string plainId, string tierName, int enabled)
    {
        if (!m_Config)
        {
            m_Config = new BDE_VIPConfig();
        }

        if (!m_Config.PendingPlayers)
        {
            m_Config.PendingPlayers = new array<ref BDE_VIPPlayerEntry>;
        }

        // If the player already exists in the main VIP list, update that instead.
        BDE_VIPPlayerEntry existing = GetAnyPlayerEntryByIDs("", plainId);

        if (existing)
        {
            existing.TierName = tierName;
            existing.Enabled = enabled;
            existing.Notes = "Updated by manual Steam64 entry through MapUI VIP Manager";
            return;
        }

        BDE_VIPPlayerEntry entry = GetPendingPlayerEntryByPlainID(plainId);

        if (!entry)
        {
            entry = new BDE_VIPPlayerEntry();
            m_Config.PendingPlayers.Insert(entry);
        }

        entry.UID = "";
        entry.PlainID = plainId;
        entry.Name = "Pending: " + plainId;
        entry.TierName = tierName;
        entry.Enabled = enabled;
        entry.Notes = "Pending manual Steam64 entry. Will be promoted when the player logs in.";
    }

    protected void DisableVIPPlayer(string uid, string plainId)
    {
        BDE_VIPPlayerEntry entry = GetAnyPlayerEntryByIDs(uid, plainId);

        if (entry)
        {
            entry.Enabled = 0;
        }

        if (plainId != "" && m_Config && m_Config.PendingPlayers)
        {
            for (int i = m_Config.PendingPlayers.Count() - 1; i >= 0; i--)
            {
                BDE_VIPPlayerEntry pending = m_Config.PendingPlayers[i];

                if (pending && pending.PlainID == plainId)
                {
                    m_Config.PendingPlayers.Remove(i);
                }
            }
        }
    }

    protected string GetEntryTierName(BDE_VIPPlayerEntry entry)
    {
        if (!entry || entry.Enabled != 1)
        {
            return "";
        }

        return entry.TierName;
    }

    protected int EntryHasPassiveBonus(BDE_VIPPlayerEntry entry)
    {
        if (!entry || entry.Enabled != 1)
        {
            return 0;
        }

        BDE_VIPTier tier = GetTierByName(entry.TierName);

        if (!tier)
        {
            return 0;
        }

        if (tier.PassiveIncomeMultiplier > 1.0 || tier.PassiveReputationMultiplier > 1.0)
        {
            return 1;
        }

        return 0;
    }

    protected int EntryHasBattlePass(BDE_VIPPlayerEntry entry)
    {
        if (!entry || entry.Enabled != 1)
        {
            return 0;
        }

        BDE_VIPTier tier = GetTierByName(entry.TierName);

        if (!tier)
        {
            return 0;
        }

        return tier.PremiumBattlePass;
    }

    protected string MakeVIPAdminRow(string uid, string plainId, string name, string tierName, int enabled, int hasPassiveBonus, int hasBattlePass, int online)
    {
        if (uid == "")
        {
            uid = " ";
        }

        if (plainId == "")
        {
            plainId = " ";
        }

        if (name == "")
        {
            name = "Unknown";
        }

        if (tierName == "")
        {
            tierName = "None";
        }

        return uid + "|" + plainId + "|" + name + "|" + tierName + "|" + enabled.ToString() + "|" + hasPassiveBonus.ToString() + "|" + hasBattlePass.ToString() + "|" + online.ToString();
    }

    protected ref array<string> BuildVIPAdminRows()
    {
        ref array<string> rows = new array<string>;
        ref array<string> usedKeys = new array<string>;

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);

            if (!player || !player.GetIdentity())
            {
                continue;
            }

            string uid = player.GetIdentity().GetId();
            string plainId = player.GetIdentity().GetPlainId();
            string name = player.GetIdentity().GetName();

            BDE_VIPPlayerEntry entry = GetAnyPlayerEntryByIDs(uid, plainId);

            string tierName = GetEntryTierName(entry);
            int enabled = 0;
            int hasPassiveBonus = 0;
            int hasBattlePass = 0;

            if (entry)
            {
                enabled = entry.Enabled;
                hasPassiveBonus = EntryHasPassiveBonus(entry);
                hasBattlePass = EntryHasBattlePass(entry);
            }

            rows.Insert(MakeVIPAdminRow(uid, plainId, name, tierName, enabled, hasPassiveBonus, hasBattlePass, 1));

            if (plainId != "")
            {
                usedKeys.Insert(plainId);
            }
            else
            {
                usedKeys.Insert(uid);
            }
        }

        if (m_Config && m_Config.Players)
        {
            foreach (BDE_VIPPlayerEntry vipEntry : m_Config.Players)
            {
                if (!vipEntry)
                {
                    continue;
                }

                string key = vipEntry.PlainID;

                if (key == "")
                {
                    key = vipEntry.UID;
                }

                if (key != "" && usedKeys.Find(key) > -1)
                {
                    continue;
                }

                rows.Insert(MakeVIPAdminRow(vipEntry.UID, vipEntry.PlainID, vipEntry.Name, GetEntryTierName(vipEntry), vipEntry.Enabled, EntryHasPassiveBonus(vipEntry), EntryHasBattlePass(vipEntry), 0));

                if (key != "")
                {
                    usedKeys.Insert(key);
                }
            }
        }

        if (m_Config && m_Config.PendingPlayers)
        {
            foreach (BDE_VIPPlayerEntry pendingEntry : m_Config.PendingPlayers)
            {
                if (!pendingEntry || pendingEntry.PlainID == "")
                {
                    continue;
                }

                if (usedKeys.Find(pendingEntry.PlainID) > -1)
                {
                    continue;
                }

                rows.Insert(MakeVIPAdminRow(pendingEntry.UID, pendingEntry.PlainID, pendingEntry.Name, GetEntryTierName(pendingEntry), pendingEntry.Enabled, EntryHasPassiveBonus(pendingEntry), EntryHasBattlePass(pendingEntry), 2));
                usedKeys.Insert(pendingEntry.PlainID);
            }
        }

        return rows;
    }
#endif

    protected void Log(string message)
    {
        if (m_Config && m_Config.DebugLogging != 1)
        {
            return;
        }
        Print("[BDE_VIPSystem] " + message);
    }
}
