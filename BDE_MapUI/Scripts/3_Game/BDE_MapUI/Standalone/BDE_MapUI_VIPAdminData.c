static const string BDE_MAPUI_VIP_RPC_SERVER = "RPC_BDE_VIPAdmin";
static const string BDE_MAPUI_VIP_RPC_CLIENT = "RPC_BDE_VIPAdminClient";

static const int BDE_MAPUI_VIP_FILTER_ALL = 0;
static const int BDE_MAPUI_VIP_FILTER_VIP = 1;
static const int BDE_MAPUI_VIP_FILTER_NONVIP = 2;
static const int BDE_MAPUI_VIP_FILTER_PENDING = 3;

class BDE_MapUI_VIPTierData
{
    string TierName;
    float PassiveIncomeMultiplier;
    float PassiveReputationMultiplier;
    float PointStoreMultiplier;
    int PremiumBattlePass;

    void BDE_MapUI_VIPTierData()
    {
        TierName = "";
        PassiveIncomeMultiplier = 1.0;
        PassiveReputationMultiplier = 1.0;
        PointStoreMultiplier = 1.0;
        PremiumBattlePass = 0;
    }
}

class BDE_MapUI_VIPPlayerAdminEntry
{
    string UID;
    string PlainID;
    string Name;
    string TierName;
    string Notes;
    bool IsOnline;
    bool HasVIP;
    bool Enabled;
    bool Pending;

    void BDE_MapUI_VIPPlayerAdminEntry()
    {
        UID = "";
        PlainID = "";
        Name = "Unknown";
        TierName = "";
        Notes = "";
        IsOnline = false;
        HasVIP = false;
        Enabled = false;
        Pending = false;
    }
}

class BDE_MapUI_VIPAdminSnapshot
{
    bool CanView;
    bool CanManage;
    int PassiveIncomeBaseAmount;
    int PassiveIncomeTimerMinutes;
    int PassiveReputationBaseAmount;
    int PassiveReputationTimerMinutes;
    int DonationCurrentAmount;
    int DonationMonthlyGoal;
    string DonationCurrencySymbol;
    string DonationMessage;
    ref array<ref BDE_MapUI_VIPPlayerAdminEntry> Players;
    ref array<ref BDE_MapUI_VIPTierData> Tiers;

    void BDE_MapUI_VIPAdminSnapshot()
    {
        CanView = false;
        CanManage = false;
        PassiveIncomeBaseAmount = 0;
        PassiveIncomeTimerMinutes = 0;
        PassiveReputationBaseAmount = 0;
        PassiveReputationTimerMinutes = 0;
        DonationCurrentAmount = 0;
        DonationMonthlyGoal = 0;
        DonationCurrencySymbol = "$";
        DonationMessage = "";
        Players = new array<ref BDE_MapUI_VIPPlayerAdminEntry>();
        Tiers = new array<ref BDE_MapUI_VIPTierData>();
    }
}
