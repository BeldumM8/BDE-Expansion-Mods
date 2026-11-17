// One independently committed metadata file per vehicle. These records live
// INSIDE their payload scope, so only cross-map records are Hive synchronized.
// This is a local crash journal, not a distributed lock service.
class BDE_VG_RecordStore
{
    static bool IsProfile(BDE_VG_VehicleData data)
    {
        return data && (data.m_StorageBackend == "profile" || data.m_StorageBackend == "profile-crossmap");
    }

    static string Directory(bool crossMap)
    {
        if (crossMap)
            return BDE_VG_CROSSMAP_DATA_DIRECTORY + "_Records/";
        return BDE_VG_VEHICLE_DATA_DIRECTORY + "_Records/";
    }

    static string PathFor(BDE_VG_VehicleData data)
    {
        return Directory(data.m_StorageBackend == "profile-crossmap") + data.GetStorageKey() + ".json";
    }

    static bool SaveState(BDE_VG_VehicleData data, string state)
    {
        if (!IsProfile(data) || !data.IsGlobalIDValid() || !BDE_VG_Utility.IsSafeStorageKey(data.GetStorageKey()))
        {
            BDE_VG_Logger.Log("ERROR receipt identity validation failed before metadata write", true);
            return false;
        }
        string directory = Directory(data.m_StorageBackend == "profile-crossmap");
        if (!FileExist(directory) && !MakeDirectory(directory))
        {
            BDE_VG_Logger.Log("ERROR cannot create receipt directory: " + directory, true);
            return false;
        }
        string path = PathFor(data);
        // Metadata is an immutable per-vehicle snapshot. Only the tiny state
        // sidecar changes during publication/retrieval, regardless of cargo size.
        if (!FileExist(path))
        {
            data.m_TransactionState = state;
            string contents;
            string error;
            if (!JsonFileLoader<BDE_VG_VehicleData>.MakeData(data, contents, error, true))
            {
                BDE_VG_Logger.Log("ERROR metadata serialization failed: " + path + " class=" + data.m_ClassName + " error=" + error, true);
                return false;
            }
            if (!BDE_VG_JSONIO.WriteText(path, contents))
            {
                BDE_VG_Logger.Log("ERROR metadata JSON write failed: " + path + " class=" + data.m_ClassName + " chars=" + contents.Length().ToString(), true);
                return false;
            }
        }
        BDE_VG_VehicleData verified = Load(path, data.m_StorageBackend == "profile-crossmap");
        if (!verified || verified.m_OwnerUID != data.m_OwnerUID || verified.m_ClassName != data.m_ClassName || verified.m_StoredByUID != data.m_StoredByUID || verified.m_StoredEpoch != data.m_StoredEpoch || verified.m_CargoItemCount != data.m_CargoItemCount || verified.m_PreviewAttachments.Count() != data.m_PreviewAttachments.Count())
        {
            BDE_VG_Logger.Log("ERROR immutable metadata verification failed: " + path, true);
            return false;
        }
        ExpansionJSONObject stateDocument = BDE_VG_JSONIO.MakeObject();
        BDE_VG_JSONIO.PutString(stateDocument, "StorageKey", data.GetStorageKey());
        BDE_VG_JSONIO.PutString(stateDocument, "State", state);
        if (!BDE_VG_JSONIO.WriteObject(path + ".state", stateDocument))
        {
            BDE_VG_Logger.Log("ERROR state commit failed: " + path + ".state", true);
            return false;
        }
        verified = Load(path, data.m_StorageBackend == "profile-crossmap");
        if (!verified || verified.m_TransactionState != state)
        {
            BDE_VG_Logger.Log("ERROR state read-back failed: " + path + ".state expected=" + state, true);
            return false;
        }
        data.m_TransactionState = state;
        BDE_VG_Logger.Log("receipt key=" + data.GetStorageKey() + " state=" + state + " path=" + path + ".state", true);
        return true;
    }

    static BDE_VG_VehicleData Load(string path, bool crossMap)
    {
        string contents;
        if (!BDE_VG_JSONIO.ReadObjectWithText(path, contents))
            return null;
        BDE_VG_VehicleData data;
        string error;
        if (!ExpansionJsonFileParser<BDE_VG_VehicleData>.ReadFromString(contents, data, error) || !data || !data.IsGlobalIDValid())
            return null;
        if (!BDE_VG_Utility.IsSafeStorageKey(data.GetStorageKey()))
            return null;
        string backend = "profile";
        if (crossMap)
            backend = "profile-crossmap";
        if (data.m_StorageBackend != backend || data.m_OwnerUID == string.Empty || data.m_ClassName == string.Empty || PathFor(data) != path)
            return null;
        // Never infer the depositor of a portable receipt from vehicle ownership.
        if (crossMap && (!data.m_StoreForDifferentMap || data.m_StoredByUID == string.Empty))
            return null;
        if (FileExist(path + ".state"))
        {
            ExpansionJSON stateDocument = BDE_VG_JSONIO.ReadObject(path + ".state");
            if (!stateDocument || BDE_VG_JSONIO.ReadString(stateDocument, "StorageKey", string.Empty) != data.GetStorageKey())
                return null;
            data.m_TransactionState = BDE_VG_JSONIO.ReadString(stateDocument, "State", string.Empty);
            if (data.m_TransactionState != "store-pending" && data.m_TransactionState != "stored" && data.m_TransactionState != "retrieving" && data.m_TransactionState != "retrieved")
                return null;
        }
        data.MigrateStoredData();
        return data;
    }

    static bool IsAvailable(BDE_VG_VehicleData data)
    {
        if (!IsProfile(data))
            return true;
        string path = PathFor(data);
        // Old indexes remain usable until their first transaction is journaled.
        if (!FileExist(path))
            return data.m_TransactionState == string.Empty;
        BDE_VG_VehicleData receipt = Load(path, data.m_StorageBackend == "profile-crossmap");
        return receipt && receipt.m_TransactionState == "stored" && receipt.m_OwnerUID == data.m_OwnerUID && receipt.m_ClassName == data.m_ClassName && receipt.m_StoredEpoch == data.m_StoredEpoch && receipt.m_StoredByUID == data.m_StoredByUID;
    }

    static void Merge(array<ref BDE_VG_GarageData> garages, bool crossMap)
    {
        string directory = Directory(crossMap);
        TStringArray files = ExpansionStatic.FindFilesInLocation(directory, ".json");
        foreach (string name : files)
        {
            if (!ExpansionString.EndsWithIgnoreCase(name, ".json"))
                continue;
            BDE_VG_VehicleData data = Load(directory + name, crossMap);
            if (!data || data.m_TransactionState != "stored" || !FileExist(data.GetEntityStorageFileName()))
                continue;
            BDE_VG_GarageData target;
            foreach (BDE_VG_GarageData garage : garages)
            {
                if (garage && !garage.m_IsMissionIndex && garage.m_IsCrossMapIndex == crossMap && garage.m_OwnerUID == data.m_OwnerUID)
                {
                    target = garage;
                    break;
                }
            }
            if (!target)
            {
                target = new BDE_VG_GarageData;
                target.m_OwnerUID = data.m_OwnerUID;
                target.m_OwnerName = data.m_OwnerName;
                target.m_IsCrossMapIndex = crossMap;
                garages.Insert(target);
            }
            for (int i = target.m_Vehicles.Count() - 1; i >= 0; i--)
            {
                if (target.m_Vehicles[i] && target.m_Vehicles[i].GetStorageKey() == data.GetStorageKey())
                    target.m_Vehicles.RemoveOrdered(i);
            }
            if (data.m_SkinName != string.Empty && ExpansionSkinModule.s_Instance)
                data.m_SkinIndex = ExpansionSkinModule.s_Instance.GetSkinIndex(data.m_ClassName, data.m_SkinName);
            target.AddVehicle(data);
        }
        // A claim, tombstone or unreadable receipt masks any stale index copy.
        foreach (BDE_VG_GarageData current : garages)
        {
            if (!current || current.m_IsMissionIndex || current.m_IsCrossMapIndex != crossMap)
                continue;
            for (int j = current.m_Vehicles.Count() - 1; j >= 0; j--)
            {
                if (!current.m_Vehicles[j] || !IsAvailable(current.m_Vehicles[j]))
                    current.m_Vehicles.RemoveOrdered(j);
            }
        }
    }
};
