class BDE_VG_GarageData
{
    static const int VERSION = 4;

    int m_Version = VERSION;
    [NonSerialized()]
    bool m_IsMissionIndex;  // READ legacy mission indexes, never add new vehicles to them.
    [NonSerialized()]
    bool m_IsCrossMapIndex; // Dedicated Hive-synced index, separate from local owners.
    string m_OwnerUID;
    string m_OwnerName;
    ref array<ref BDE_VG_VehicleData> m_Vehicles;

    void BDE_VG_GarageData()
    {
        m_Vehicles = new array<ref BDE_VG_VehicleData>;
    }

    string GetFileName()
    {
        if (m_IsMissionIndex)
            return BDE_VG_Module.s_LegacyGarageFolderPath + m_OwnerUID + ".json";
        if (m_IsCrossMapIndex)
            return BDE_VG_CROSSMAP_DATA_DIRECTORY + m_OwnerUID + ".json";
        return BDE_VG_Module.s_GarageFolderPath + m_OwnerUID + ".json";
    }

    bool Save()
    {
        m_Version = VERSION;
        string saveError;
        string dataJSON;
        if (!JsonFileLoader<BDE_VG_GarageData>.MakeData(this, dataJSON, saveError, true))
        {
            BDE_VG_Logger.Log("ERROR serializing garage index " + GetFileName() + ": " + saveError,true);
            return false;
        }
        if (!BDE_VG_JSONIO.WriteText(GetFileName(), dataJSON))
        {
            BDE_VG_Logger.Log("ERROR saving verified garage index " + GetFileName(),true);
            return false;
        }
        return true;
    }

    void AddVehicle(BDE_VG_VehicleData vehicleData)
    {
        if (vehicleData)
            m_Vehicles.Insert(vehicleData);
    }

    void RemoveVehicle(BDE_VG_VehicleData vehicleData)
    {
        if (!vehicleData)
            return;

        for (int i = m_Vehicles.Count() - 1; i >= 0; i--)
        {
            if (m_Vehicles[i] == vehicleData)
            {
                m_Vehicles.RemoveOrdered(i);
                return;
            }
        }
    }

    static BDE_VG_GarageData Load(string fileName, bool isMissionIndex = false, bool isCrossMapIndex = false)
    {
        // A Hive sync can expose a partially written file. Do not pass malformed
        // JSON into the native deserializer or silently erase our in-memory cache.
        string contents;
        ExpansionJSON indexDocument = BDE_VG_JSONIO.ReadObjectWithText(fileName, contents);
        if (!indexDocument || !indexDocument.Get("m_Vehicles").IsArray())
            return null;
        BDE_VG_GarageData data;
        string readError;
        // Deserialize the same validated snapshot; do NOT reopen a Hive-managed
        // file, which might be replaced between our first and second read.
        if (!ExpansionJsonFileParser<BDE_VG_GarageData>.ReadFromString(contents, data, readError) || !data || data.m_OwnerUID == string.Empty)
        {
            Print("[BDE Virtual Garage] Invalid vehicle owner index (not modified): " + fileName + " " + readError);
            return null;
        }
        string expectedPath = BDE_VG_Module.s_GarageFolderPath + data.m_OwnerUID + ".json";
        if (isCrossMapIndex)
            expectedPath = BDE_VG_CROSSMAP_DATA_DIRECTORY + data.m_OwnerUID + ".json";
        else if (isMissionIndex)
            expectedPath = BDE_VG_Module.s_LegacyGarageFolderPath + data.m_OwnerUID + ".json";
        if (fileName != expectedPath)
            return null; // Never load backups/conflict copies as another live owner index.
        data.m_IsMissionIndex = isMissionIndex;
        data.m_IsCrossMapIndex = isCrossMapIndex;
        if (!data.m_Vehicles)
            data.m_Vehicles = new array<ref BDE_VG_VehicleData>;

        foreach (BDE_VG_VehicleData vehicleData : data.m_Vehicles)
        {
            if (!vehicleData)
                continue;

            vehicleData.MigrateStoredData();
            if (!BDE_VG_Utility.IsSafeStorageKey(vehicleData.GetStorageKey()))
                return null;
            if (!vehicleData.IsGlobalIDValid())
                return null;
            if (vehicleData.m_OwnerUID != data.m_OwnerUID)
                return null;
            if (isCrossMapIndex && (vehicleData.m_StorageBackend != "profile-crossmap" || !vehicleData.m_StoreForDifferentMap || vehicleData.m_StoredByUID == string.Empty))
                return null;
            if (!isCrossMapIndex && vehicleData.m_StorageBackend == "profile-crossmap")
                return null;

            // Persisted global IDs are already in m_GlobalID; Set() is only legal
            // before mission load and must never run in a Hive index refresh.
            if (vehicleData.m_SkinName != string.Empty && ExpansionSkinModule.s_Instance)
                vehicleData.m_SkinIndex = ExpansionSkinModule.s_Instance.GetSkinIndex(vehicleData.m_ClassName,vehicleData.m_SkinName);
        }

        return data;
    }
};

class BDE_VG_LegacyMetadata
{
    int Version;
    string StorageID;
    string VehicleType;
    string DisplayName;
    string OwnerUID;
    string OwnerName;
    string StoredByUID;
    string StoredByName;
    string WorldName;
    string PersistentID;
    int VehicleKind;
    vector OriginalPosition;
    vector OriginalOrientation;
    vector CollisionMinimum;
    vector CollisionMaximum;
    int StoredEpoch;
    int CargoItemCount;
    float HealthPercent;

    string GetMetadataPath()
    {
        return BDE_VG_LEGACY_METADATA_DIRECTORY + StorageID + ".json";
    }

    string GetVehiclePath()
    {
        return BDE_VG_LEGACY_VEHICLE_DIRECTORY + StorageID + ExpansionEntityStorageModule.EXT;
    }

    void DeleteMetadata()
    {
        if (FileExist(GetMetadataPath()))
            DeleteFile(GetMetadataPath());
    }

    BDE_VG_VehicleData ToVehicleData()
    {
        BDE_VG_VehicleData data = new BDE_VG_VehicleData;
        data.m_OwnerUID = OwnerUID;
        data.m_OwnerName = OwnerName;
        data.m_StoredByUID = StoredByUID;
        data.m_StoredByName = StoredByName;
        if (data.m_StoredByUID == string.Empty)
            data.m_StoredByUID = OwnerUID;
        if (data.m_StoredByName == string.Empty)
            data.m_StoredByName = OwnerName;
        data.m_ClassName = VehicleType;
        data.m_DisplayName = DisplayName;
        data.m_WorldName = WorldName;
        data.m_Position = OriginalPosition;
        data.m_Orientation = OriginalOrientation;
        data.m_StoredEpoch = StoredEpoch;
        data.m_CargoItemCount = CargoItemCount;
        data.m_InventorySnapshotVersion = 0;
        data.m_HealthPercent = HealthPercent;
        data.m_IsLegacy = true;
        data.m_StorageKey = StorageID;
        data.m_SkinIndex = -1;
        return data;
    }

    static BDE_VG_LegacyMetadata Load(string storageID)
    {
        if (!BDE_VG_Utility.IsSafeStorageKey(storageID))
            return null;
        string path = BDE_VG_LEGACY_METADATA_DIRECTORY + storageID + ".json";
        if (!FileExist(path))
            return null;

        BDE_VG_LegacyMetadata metadata = new BDE_VG_LegacyMetadata;
        string error;
        if (!JsonFileLoader<BDE_VG_LegacyMetadata>.LoadFile(path,metadata,error))
            return null;
        return metadata;
    }

    static array<ref BDE_VG_LegacyMetadata> LoadAll()
    {
        array<ref BDE_VG_LegacyMetadata> entries = new array<ref BDE_VG_LegacyMetadata>;
        if (!FileExist(BDE_VG_LEGACY_METADATA_DIRECTORY))
            return entries;

        TStringArray files = ExpansionStatic.FindFilesInLocation(BDE_VG_LEGACY_METADATA_DIRECTORY,".json");
        foreach (string fileName : files)
        {
            string path = BDE_VG_LEGACY_METADATA_DIRECTORY + fileName;
            BDE_VG_LegacyMetadata metadata = new BDE_VG_LegacyMetadata;
            string error;
            if (!JsonFileLoader<BDE_VG_LegacyMetadata>.LoadFile(path,metadata,error))
                continue;
            if (metadata && metadata.StorageID != string.Empty && FileExist(metadata.GetVehiclePath()))
                entries.Insert(metadata);
        }

        return entries;
    }
};
