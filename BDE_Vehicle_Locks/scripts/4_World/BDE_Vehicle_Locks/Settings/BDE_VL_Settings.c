class BDE_VL_Settings
{
    int Version = 1;

    bool EnableVehicleLocks = true;
    bool LockMarketVehiclesOnPurchase = true;
    bool LockClaimedVehicles = true;
    bool LockMigratedVehicles = true;

    bool AllowFreeClaiming = true;
    string ClaimItemClassName = "";
    bool DeleteClaimItemOnSuccess = true;

    bool AutoMigrateExpansionOwners = true;
    bool DeleteLoadedExpansionKeysOnAutoMigration = true;
    bool AllowExpansionKeyClaim = true;
    bool DeleteExpansionKeyOnClaim = true;
    bool DeleteMarketExpansionKey = true;

    bool EnableVehicleMarkers = true;
    bool MarkerDriverOnly = true;
    bool MarkerRequiresVehicleAccess = true;
    string MarkerName = "My Vehicle";
    string MarkerIcon = "Car";
    int MarkerColor = -16711936;

    bool EnableAdminActions = true;
    bool DebugLogging = false;

    protected static ref BDE_VL_Settings s_Instance;
    protected bool m_Loaded;

    static BDE_VL_Settings Get()
    {
        if (!s_Instance)
        {
            s_Instance = new BDE_VL_Settings();
            s_Instance.Load();
        }

        return s_Instance;
    }

    void Load()
    {
        if (m_Loaded)
            return;

        m_Loaded = true;

        if (!g_Game || !g_Game.IsServer())
            return;

        MakeDirectory(BDE_VL_PROFILE_ROOT);
        MakeDirectory(BDE_VL_PROFILE_DIRECTORY);

        if (FileExist(BDE_VL_SETTINGS_FILE))
        {
            JsonFileLoader<BDE_VL_Settings>.JsonLoadFile(BDE_VL_SETTINGS_FILE, this);
        }
        else
        {
            JsonFileLoader<BDE_VL_Settings>.JsonSaveFile(BDE_VL_SETTINGS_FILE, this);
        }
    }
};
