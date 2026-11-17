class BDE_VL_RaidSettings
{
    int Version = 1;
    bool Enabled = true;
    float MinimumSeconds = 30;
    float MaximumSeconds = 300;
    float MinimumSuccessPercent = 10;
    float MaximumSuccessPercent = 100;
    float HealthCeiling = 10000;
    float StorageCeiling = 1000;
    float StorageWeight = 0.25;
    float GlobalDifficultyMultiplier = 1;
    float LockpickDamagePercent = 25;
    bool HealthModifierEnabled = true;
    float FullHealthEaseMultiplier = 0;
    float LowHealthEaseMultiplier = 2;
    float LowHealthThresholdPercent = 25;
}

class BDE_VL_LockDifficulty
{
    int Version = 1;
    string BaseClass;
    bool Enabled = true;
    float BaseHitpoints;
    int BaseCargoSlots;
    // -1 calculates from health/storage. Set 0..1 to override this family's difficulty.
    float DifficultyOverride = -1;
}

class BDE_VL_RaidConfig
{
    protected static ref BDE_VL_RaidSettings s_Settings;
    protected static ref map<string, ref BDE_VL_LockDifficulty> s_Families = new map<string, ref BDE_VL_LockDifficulty>;
    protected static ref map<string, string> s_BaseClasses = new map<string, string>;

    static BDE_VL_RaidSettings Get()
    {
        if (s_Settings)
        {
            return s_Settings;
        }
        s_Settings = new BDE_VL_RaidSettings();
        if (!GetGame().IsServer())
        {
            s_Settings.Enabled = false;
            return s_Settings;
        }
        MakeDirectory(BDE_VL_PROFILE_ROOT);
        MakeDirectory(BDE_VL_PROFILE_DIRECTORY);
        MakeDirectory(BDE_VL_PROFILE_DIRECTORY + "/LockDifficulty");
        string path = BDE_VL_PROFILE_DIRECTORY + "/RaidSettings.json";
        string error;
        if (FileExist(path) && !JsonFileLoader<BDE_VL_RaidSettings>.LoadFile(path, s_Settings, error))
        {
            Print("[BDE Vehicle Locks] Invalid raid settings; raiding disabled. " + error);
            s_Settings = new BDE_VL_RaidSettings();
            s_Settings.Enabled = false;
            return s_Settings;
        }
        s_Settings.FullHealthEaseMultiplier = Math.Max(0, s_Settings.FullHealthEaseMultiplier);
        s_Settings.LowHealthEaseMultiplier = Math.Max(0, s_Settings.LowHealthEaseMultiplier);
        s_Settings.LowHealthThresholdPercent = Math.Clamp(s_Settings.LowHealthThresholdPercent, 0, 99.9);
        s_Settings.MinimumSeconds = Math.Max(1, s_Settings.MinimumSeconds);
        s_Settings.MaximumSeconds = Math.Max(s_Settings.MinimumSeconds, s_Settings.MaximumSeconds);
        s_Settings.MinimumSuccessPercent = Math.Clamp(s_Settings.MinimumSuccessPercent, 0, 100);
        s_Settings.MaximumSuccessPercent = Math.Clamp(s_Settings.MaximumSuccessPercent, s_Settings.MinimumSuccessPercent, 100);
        s_Settings.HealthCeiling = Math.Max(1, s_Settings.HealthCeiling);
        s_Settings.StorageCeiling = Math.Max(1, s_Settings.StorageCeiling);
        s_Settings.StorageWeight = Math.Clamp(s_Settings.StorageWeight, 0, 1);
        s_Settings.GlobalDifficultyMultiplier = Math.Max(0, s_Settings.GlobalDifficultyMultiplier);
        s_Settings.LockpickDamagePercent = Math.Clamp(s_Settings.LockpickDamagePercent, 0, 100);
        if (!JsonFileLoader<BDE_VL_RaidSettings>.SaveFile(path, s_Settings, error))
        {
            Print("[BDE Vehicle Locks] Cannot save raid settings: " + error);
        }
        return s_Settings;
    }

    static string ResolveBase(string className)
    {
        string cached;
        if (s_BaseClasses.Find(className, cached))
        {
            return cached;
        }
        string family = className;
        string current = className;
        string parent;
        // Climb concrete vehicle families, stopping before generic engine/mod roots.
        for (int depth = 0; depth < 64; depth++)
        {
            if (!GetGame().ConfigGetBaseName("CfgVehicles " + current, parent) || parent == "" || parent == current)
            {
                break;
            }
            if (parent == "CarScript" || parent == "Car" || parent == "BoatScript" || parent == "Boat" || parent == "MotorbikeScript" || parent == "Motorbike" || parent == "ExpansionVehicleBase" || parent == "ExpansionHelicopterScript" || parent == "ExpansionBoatScript" || parent == "Veh_WeaponBase" || parent == "Transport")
            {
                break;
            }
            string model;
            GetGame().ConfigGetText("CfgVehicles " + parent + " model", model);
            if (model == "")
            {
                break;
            }
            family = parent;
            current = parent;
        }
        s_BaseClasses.Insert(className, family);
        return family;
    }

    static BDE_VL_LockDifficulty GetFamily(string className)
    {
        BDE_VL_RaidSettings settings = Get();
        string family = ResolveBase(className);
        BDE_VL_LockDifficulty data;
        if (s_Families.Find(family, data))
        {
            return data;
        }
        data = new BDE_VL_LockDifficulty();
        data.BaseClass = family;
        data.BaseHitpoints = GetGame().ConfigGetFloat("CfgVehicles " + family + " DamageSystem GlobalHealth Health hitpoints");
        TIntArray size = new TIntArray();
        GetGame().ConfigGetIntArray("CfgVehicles " + family + " Cargo itemsCargoSize", size);
        if (size.Count() >= 2)
        {
            data.BaseCargoSlots = size[0] * size[1];
        }
        string path = BDE_VL_PROFILE_DIRECTORY + "/LockDifficulty/" + family + ".json";
        string error;
        if (FileExist(path) && !JsonFileLoader<BDE_VL_LockDifficulty>.LoadFile(path, data, error))
        {
            Print("[BDE Vehicle Locks] Invalid difficulty file " + path + "; family disabled. " + error);
            data = new BDE_VL_LockDifficulty();
            data.BaseClass = family;
            data.Enabled = false;
            s_Families.Insert(family, data);
            return data;
        }
        data.BaseClass = family;
        if (!JsonFileLoader<BDE_VL_LockDifficulty>.SaveFile(path, data, error))
        {
            Print("[BDE Vehicle Locks] Cannot save " + path + ": " + error);
        }
        s_Families.Insert(family, data);
        return data;
    }

    static float GetDifficulty(BDE_VL_LockDifficulty data)
    {
        BDE_VL_RaidSettings settings = Get();
        float health = Math.Clamp(data.BaseHitpoints / settings.HealthCeiling, 0, 1);
        float storage = Math.Clamp(data.BaseCargoSlots / settings.StorageCeiling, 0, 1);
        float difficulty = health + (1 - health) * storage * settings.StorageWeight;
        if (data.DifficultyOverride >= 0)
        {
            difficulty = data.DifficultyOverride;
        }
        difficulty = Math.Clamp(difficulty * settings.GlobalDifficultyMultiplier, 0, 1);
        if (data.BaseHitpoints >= settings.HealthCeiling)
        {
            difficulty = 1;
        }
        return difficulty;
    }

    static void GenerateFiles()
    {
        Get();
        int count = GetGame().ConfigGetChildrenCount("CfgVehicles");
        for (int i = 0; i < count; i++)
        {
            string className;
            GetGame().ConfigGetChildName("CfgVehicles", i, className);
            if (GetGame().ConfigGetInt("CfgVehicles " + className + " scope") != 2)
            {
                continue;
            }
            if (GetGame().IsKindOf(className, "CarScript") || GetGame().IsKindOf(className, "BoatScript") || GetGame().IsKindOf(className, "MotorbikeScript") || GetGame().IsKindOf(className, "ExpansionVehicleBase"))
            {
                GetFamily(className);
            }
        }
    }
}
