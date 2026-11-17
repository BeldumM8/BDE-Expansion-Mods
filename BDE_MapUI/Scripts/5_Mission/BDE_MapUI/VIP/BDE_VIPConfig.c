// BDE_VIPConfig.c
// JSON config classes for BDE VIP System.

class BDE_VIPTier
{
    string TierName;

    // Passive payout multipliers. 1.0 = normal configured base payout.
    float PassiveIncomeMultiplier;
    float PassiveReputationMultiplier;

    // v2 legacy fields retained only so existing bonus-based configs can migrate to v3.
    int PassiveIncomeBonus;
    int PassiveReputationBonus;

    // Per-tier MapUI Point Store reward multiplier. 1.0 = standard rewards.
    float PointStoreMultiplier;

    // Legacy/feature flag retained for premium Battle Pass style access.
    int PremiumBattlePass;

    void BDE_VIPTier()
    {
        TierName = BDE_VIPTiers.Income;
        PassiveIncomeMultiplier = 1.0;
        PassiveReputationMultiplier = 1.0;
        PassiveIncomeBonus = 0;
        PassiveReputationBonus = 0;
        PointStoreMultiplier = 1.0;
        PremiumBattlePass = 0;
    }
}

class BDE_VIPPlayerEntry
{
    // DayZ/Bohemia identity ID. This is the long ID used by whitelist.txt.
    string UID;

    // Steam64 / plain ID. Points Store VIPSteamIDs uses this ID when available.
    string PlainID;

    // Optional display/reference name.
    string Name;

    // Must match one of the configured TierName values.
    string TierName;

    // 1 = active, 0 = disabled
    int Enabled;

    // Optional admin note.
    string Notes;

    void BDE_VIPPlayerEntry()
    {
        UID = "";
        PlainID = "";
        Name = "";
        TierName = BDE_VIPTiers.Income;
        Enabled = 1;
        Notes = "";
    }
}

class BDE_VIPConfig
{
    int ConfigVersion;

    // Reloads VIPPlayers.json while the server is running.
    // Set to 0 to disable automatic reload.
    int AutoReloadSeconds;

    // 1 = print server-side logs, 0 = silent
    int DebugLogging;

    // Passive money settings.
    int EnablePassiveIncome;
    int PassiveIncomeBaseAmount;
    int PassiveIncomeTimerMinutes;

    // Passive reputation settings.
    int EnablePassiveReputation;
    int PassiveReputationBaseAmount;
    int PassiveReputationTimerMinutes;

    ref array<ref BDE_VIPTier> Tiers;
    ref array<ref BDE_VIPPlayerEntry> Players;

    // Offline Steam64 entries added through the MapUI VIP Manager.
    // When one of these players logs in, the system fills UID/Name and moves them into Players.
    ref array<ref BDE_VIPPlayerEntry> PendingPlayers;

    void BDE_VIPConfig()
    {
        ConfigVersion = 3;

        AutoReloadSeconds = 60;
        DebugLogging = 0;

        EnablePassiveIncome = 1;
        PassiveIncomeBaseAmount = 4000;
        PassiveIncomeTimerMinutes = 30;

        EnablePassiveReputation = 1;
        PassiveReputationBaseAmount = 50;
        PassiveReputationTimerMinutes = 20;

        Tiers = new array<ref BDE_VIPTier>;
        Players = new array<ref BDE_VIPPlayerEntry>;
        PendingPlayers = new array<ref BDE_VIPPlayerEntry>;
    }
}

class BDE_VIPPointTask
{
    string TaskID;
    string Name;
    string Description;
    string TaskType;
    int Target;
    int Points;
    int Cycle;

    void BDE_VIPPointTask()
    {
        TaskID = "";
        Name = "";
        Description = "";
        TaskType = "";
        Target = 0;
        Points = 0;
        Cycle = 0;
    }
}

class BDE_VIPPointStoreItem
{
    string ItemID;
    string Name;
    string Description;
    string ClassName;
    string Category;
    int Price;
    int Quantity;

    void BDE_VIPPointStoreItem()
    {
        ItemID = "";
        Name = "";
        Description = "";
        ClassName = "";
        Category = "";
        Price = 0;
        Quantity = 0;
    }
}

class BDE_VIPPointsStoreConfig
{
    string StoreName;
    float VIPMultiplier;
    ref array<string> VIPSteamIDs;
    ref array<ref BDE_VIPPointTask> Tasks;
    ref array<ref BDE_VIPPointStoreItem> StoreItems;

    void BDE_VIPPointsStoreConfig()
    {
        StoreName = "Task Points Store";
        VIPMultiplier = 1.5;
        VIPSteamIDs = new array<string>;
        Tasks = new array<ref BDE_VIPPointTask>;
        StoreItems = new array<ref BDE_VIPPointStoreItem>;
    }
}
