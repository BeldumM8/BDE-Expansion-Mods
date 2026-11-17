class BDE_VG_Settings
{
    int Version = 5;
    bool Enabled = true;
    ref array<string> GarageAccessObjects = new array<string>;
    bool MatchInheritedAccessObjectClasses = true;
    float VehicleStoreRangeMeters = 30.0;
    int MaxStoredVehiclesPerOwner = 10;  // Trader garage quota per vehicle owner.
    int MaxStoredVehiclesPerOwnerOutsideTraderZone = 10;  // All base terminals combined, per vehicle owner.
    bool AllowGroupAccess = true;
    bool RequireVehicleEmpty = true;
    bool RequireEngineOff = true;
    float MaximumStoreSpeedKmh = 1.0;
    bool AllowRuinedVehicles = false;
    bool AllowStoringDynamicEventVehicles = false;
    bool AllowStoringClaimedDEVehicles = false;
    // Controls same-map sharing between different terminal scopes ONLY.
    bool AllowCrossWorldRetrieval = false;
    float AlternativeSpawnMaximumRadiusMeters = 40.0;
    float AlternativeSpawnStepMeters = 4.0;
    int AlternativeSpawnDirections = 16;
    float AlternativeSpawnMaximumTerrainDeltaMeters = 1.25;
    bool DebugLogging = false;

    protected static ref BDE_VG_Settings s_Instance;
    protected bool m_Loaded;

    void BDE_VG_Settings()
    {
        if (GarageAccessObjects.Count() == 0)
            GarageAccessObjects.Insert("NP_GarageParkingMeter");
    }

    static BDE_VG_Settings Get()
    {
        if (!s_Instance)
        {
            s_Instance = new BDE_VG_Settings();
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

        EnsureDirectories();

        // Native JsonSerializer deserialization of BDE_VG_Settings crashed
        // DayZServer_x64.exe (BDE_VG_SpawnSettings.LoadOrMigrate, 2026-11-09).
        // Read only a scripted JSON tree, never deserialize this runtime class.
        array<ref BDE_VG_SpawnPosition> migratedPositions = new array<ref BDE_VG_SpawnPosition>;
        if (FileExist(BDE_VG_SETTINGS_FILE))
        {
            ExpansionJSON document = BDE_VG_JSONIO.ReadObject(BDE_VG_SETTINGS_FILE);
            if (!document)
            {
                Enabled = false;
                Print("[BDE Virtual Garage] Invalid Settings.json. Refusing to reset or overwrite it.");
                return;
            }
            ReadSharedSettings(document);
            migratedPositions = BDE_VG_SpawnSettings.ReadEntries(document);
        }

        // Create/read the map-local file BEFORE removing the legacy embedded
        // positions from the shared file. Existing map-local file takes priority.
        if (!BDE_VG_SpawnSettings.LoadOrMigrate(migratedPositions))
        {
            Enabled = false;
            Print("[BDE Virtual Garage] SpawnPositions migration failed. Settings.json is unchanged.");
            return;
        }

        ApplyMigrations();
        Normalize();
        Version = 5;
        if (!WriteSharedSettings())
        {
            Enabled = false;
            Print("[BDE Virtual Garage] Settings rewrite failed. Check the .bak file before restarting.");
        }
    }

    // Every supported shared key is imported explicitly. Unknown or retired
    // fields (including m_Loaded and SpawnPositions) are never written back.
    protected void ReadSharedSettings(ExpansionJSONElementBase root)
    {
        Version = BDE_VG_JSONIO.ReadInt(root, "Version", Version);
        Enabled = BDE_VG_JSONIO.ReadBool(root, "Enabled", Enabled);
        MatchInheritedAccessObjectClasses = BDE_VG_JSONIO.ReadBool(root, "MatchInheritedAccessObjectClasses", MatchInheritedAccessObjectClasses);
        VehicleStoreRangeMeters = BDE_VG_JSONIO.ReadFloat(root, "VehicleStoreRangeMeters", VehicleStoreRangeMeters);
        MaxStoredVehiclesPerOwner = BDE_VG_JSONIO.ReadInt(root, "MaxStoredVehiclesPerOwner", MaxStoredVehiclesPerOwner);
        MaxStoredVehiclesPerOwnerOutsideTraderZone = BDE_VG_JSONIO.ReadInt(root, "MaxStoredVehiclesPerOwnerOutsideTraderZone", MaxStoredVehiclesPerOwnerOutsideTraderZone);
        AllowGroupAccess = BDE_VG_JSONIO.ReadBool(root, "AllowGroupAccess", AllowGroupAccess);
        RequireVehicleEmpty = BDE_VG_JSONIO.ReadBool(root, "RequireVehicleEmpty", RequireVehicleEmpty);
        RequireEngineOff = BDE_VG_JSONIO.ReadBool(root, "RequireEngineOff", RequireEngineOff);
        MaximumStoreSpeedKmh = BDE_VG_JSONIO.ReadFloat(root, "MaximumStoreSpeedKmh", MaximumStoreSpeedKmh);
        AllowRuinedVehicles = BDE_VG_JSONIO.ReadBool(root, "AllowRuinedVehicles", AllowRuinedVehicles);
        AllowStoringDynamicEventVehicles = BDE_VG_JSONIO.ReadBool(root, "AllowStoringDynamicEventVehicles", AllowStoringDynamicEventVehicles);
        AllowStoringClaimedDEVehicles = BDE_VG_JSONIO.ReadBool(root, "AllowStoringClaimedDEVehicles", AllowStoringClaimedDEVehicles);
        AllowCrossWorldRetrieval = BDE_VG_JSONIO.ReadBool(root, "AllowCrossWorldRetrieval", AllowCrossWorldRetrieval);
        AlternativeSpawnMaximumRadiusMeters = BDE_VG_JSONIO.ReadFloat(root, "AlternativeSpawnMaximumRadiusMeters", AlternativeSpawnMaximumRadiusMeters);
        AlternativeSpawnStepMeters = BDE_VG_JSONIO.ReadFloat(root, "AlternativeSpawnStepMeters", AlternativeSpawnStepMeters);
        AlternativeSpawnDirections = BDE_VG_JSONIO.ReadInt(root, "AlternativeSpawnDirections", AlternativeSpawnDirections);
        AlternativeSpawnMaximumTerrainDeltaMeters = BDE_VG_JSONIO.ReadFloat(root, "AlternativeSpawnMaximumTerrainDeltaMeters", AlternativeSpawnMaximumTerrainDeltaMeters);
        DebugLogging = BDE_VG_JSONIO.ReadBool(root, "DebugLogging", DebugLogging);

        ExpansionJSONElementBase accessList = root.Get("GarageAccessObjects");
        if (accessList && accessList.IsArray())
        {
            GarageAccessObjects.Clear();
            for (int i = 0; i < accessList.Count(); i++)
            {
                ExpansionJSONElementBase node = accessList.Get(i);
                if (!node || node.IsNull() || node.IsUndefined())
                    continue;
                string className = node.GetString();
                if (className != string.Empty && GarageAccessObjects.Find(className) == -1)
                    GarageAccessObjects.Insert(className);
            }
        }
    }

    protected bool WriteSharedSettings()
    {
        ExpansionJSONObject root = BDE_VG_JSONIO.MakeObject();
        BDE_VG_JSONIO.PutInt(root, "Version", Version);
        BDE_VG_JSONIO.PutBool(root, "Enabled", Enabled);
        ExpansionJSONArray accessList = new ExpansionJSONArray;
        foreach (string className : GarageAccessObjects)
            accessList.Insert(new ExpansionJSONString(className));
        root.Set("GarageAccessObjects", accessList);
        BDE_VG_JSONIO.PutBool(root, "MatchInheritedAccessObjectClasses", MatchInheritedAccessObjectClasses);
        BDE_VG_JSONIO.PutFloat(root, "VehicleStoreRangeMeters", VehicleStoreRangeMeters);
        BDE_VG_JSONIO.PutInt(root, "MaxStoredVehiclesPerOwner", MaxStoredVehiclesPerOwner);
        BDE_VG_JSONIO.PutInt(root, "MaxStoredVehiclesPerOwnerOutsideTraderZone", MaxStoredVehiclesPerOwnerOutsideTraderZone);
        BDE_VG_JSONIO.PutBool(root, "AllowGroupAccess", AllowGroupAccess);
        BDE_VG_JSONIO.PutBool(root, "RequireVehicleEmpty", RequireVehicleEmpty);
        BDE_VG_JSONIO.PutBool(root, "RequireEngineOff", RequireEngineOff);
        BDE_VG_JSONIO.PutFloat(root, "MaximumStoreSpeedKmh", MaximumStoreSpeedKmh);
        BDE_VG_JSONIO.PutBool(root, "AllowRuinedVehicles", AllowRuinedVehicles);
        BDE_VG_JSONIO.PutBool(root, "AllowStoringDynamicEventVehicles", AllowStoringDynamicEventVehicles);
        BDE_VG_JSONIO.PutBool(root, "AllowStoringClaimedDEVehicles", AllowStoringClaimedDEVehicles);
        BDE_VG_JSONIO.PutBool(root, "AllowCrossWorldRetrieval", AllowCrossWorldRetrieval);
        BDE_VG_JSONIO.PutFloat(root, "AlternativeSpawnMaximumRadiusMeters", AlternativeSpawnMaximumRadiusMeters);
        BDE_VG_JSONIO.PutFloat(root, "AlternativeSpawnStepMeters", AlternativeSpawnStepMeters);
        BDE_VG_JSONIO.PutInt(root, "AlternativeSpawnDirections", AlternativeSpawnDirections);
        BDE_VG_JSONIO.PutFloat(root, "AlternativeSpawnMaximumTerrainDeltaMeters", AlternativeSpawnMaximumTerrainDeltaMeters);
        BDE_VG_JSONIO.PutBool(root, "DebugLogging", DebugLogging);
        return BDE_VG_JSONIO.WriteObject(BDE_VG_SETTINGS_FILE, root);
    }

    protected void ApplyMigrations()
    {
        // Preserve custom access-object lists but replace retired DV_Parking defaults.
        if (!GarageAccessObjects)
            GarageAccessObjects = new array<string>;
        for (int i = 0; i < GarageAccessObjects.Count(); i++)
        {
            if (GarageAccessObjects[i] == "DV_Parking" || (Version < 3 && GarageAccessObjects[i] == "ExpansionATMLocker"))
                GarageAccessObjects[i] = "NP_GarageParkingMeter";
        }
        for (int j = GarageAccessObjects.Count() - 1; j >= 0; j--)
        {
            if (GarageAccessObjects[j] == string.Empty)
                GarageAccessObjects.RemoveOrdered(j);
            else if (GarageAccessObjects.Find(GarageAccessObjects[j]) != j)
                GarageAccessObjects.RemoveOrdered(j);
        }
        if (GarageAccessObjects.Count() == 0)
            GarageAccessObjects.Insert("NP_GarageParkingMeter");
        if (Version < 4)
            MaxStoredVehiclesPerOwnerOutsideTraderZone = MaxStoredVehiclesPerOwner;
    }

    void Normalize()
    {
        if (!GarageAccessObjects)
            GarageAccessObjects = new array<string>;
        if (MaxStoredVehiclesPerOwnerOutsideTraderZone < 0)
            MaxStoredVehiclesPerOwnerOutsideTraderZone = 0;
        if (VehicleStoreRangeMeters < 1.0)
            VehicleStoreRangeMeters = 1.0;
        if (MaxStoredVehiclesPerOwner < 0)
            MaxStoredVehiclesPerOwner = 0;
        if (MaximumStoreSpeedKmh < 0.0)
            MaximumStoreSpeedKmh = 0.0;
        if (AlternativeSpawnMaximumRadiusMeters < 0.0)
            AlternativeSpawnMaximumRadiusMeters = 0.0;
        if (AlternativeSpawnStepMeters < 1.0)
            AlternativeSpawnStepMeters = 1.0;
        if (AlternativeSpawnDirections < 4)
            AlternativeSpawnDirections = 4;
        if (AlternativeSpawnMaximumTerrainDeltaMeters < 0.0)
            AlternativeSpawnMaximumTerrainDeltaMeters = 0.0;
    }

    bool IsGarageAccessObject(string typeName)
    {
        if (!Enabled || typeName == string.Empty || !GarageAccessObjects)
            return false;

        foreach (string configuredType : GarageAccessObjects)
        {
            if (configuredType == string.Empty)
                continue;
            if (typeName == configuredType)
                return true;
            if (MatchInheritedAccessObjectClasses && g_Game.IsKindOf(typeName,configuredType))
                return true;
        }

        return false;
    }

    static void EnsureDirectories()
    {
        if (!FileExist(BDE_VG_PROFILE_ROOT))
            ExpansionStatic.MakeDirectoryRecursive(BDE_VG_PROFILE_ROOT);
        if (!FileExist(BDE_VG_PROFILE_DIRECTORY))
            ExpansionStatic.MakeDirectoryRecursive(BDE_VG_PROFILE_DIRECTORY);
        if (!FileExist(BDE_VG_VEHICLE_DATA_DIRECTORY))
            ExpansionStatic.MakeDirectoryRecursive(BDE_VG_VEHICLE_DATA_DIRECTORY);
        if (!FileExist(BDE_VG_CROSSMAP_DATA_DIRECTORY))
            ExpansionStatic.MakeDirectoryRecursive(BDE_VG_CROSSMAP_DATA_DIRECTORY);
    }
};

