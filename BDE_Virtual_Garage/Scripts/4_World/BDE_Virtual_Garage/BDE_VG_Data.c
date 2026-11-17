class BDE_VG_AttachmentPreviewData
{
    string m_ClassName;
    int m_SlotID = -1;
    int m_ParentIndex = -1;
    float m_HealthPercent = 100.0;

    void OnSend(ParamsWriteContext ctx)
    {
        ctx.Write(m_ClassName);
        ctx.Write(m_SlotID);
        ctx.Write(m_ParentIndex);
        ctx.Write(m_HealthPercent);
    }

    bool OnReceive(ParamsReadContext ctx)
    {
        if (!ctx.Read(m_ClassName))
            return false;
        if (!ctx.Read(m_SlotID))
            return false;
        if (!ctx.Read(m_ParentIndex))
            return false;
        if (!ctx.Read(m_HealthPercent))
            return false;
        return true;
    }
};

class BDE_VG_CargoEntry
{
    string m_ClassName;
    string m_DisplayName;
    int m_Count = 1;

    void OnSend(ParamsWriteContext ctx)
    {
        ctx.Write(m_ClassName);
        ctx.Write(m_DisplayName);
        ctx.Write(m_Count);
    }

    bool OnReceive(ParamsReadContext ctx)
    {
        if (!ctx.Read(m_ClassName))
            return false;
        if (!ctx.Read(m_DisplayName))
            return false;
        if (!ctx.Read(m_Count))
            return false;
        return true;
    }
};

class BDE_VG_VehicleData
{
    int m_Version = 4;
    // Empty on pre-receipt data. Pending/claim states require operator review
    // after a crash; never automatically replay an ambiguous transaction.
    string m_TransactionState;
    // A storage transaction UUID, independent of Expansion's server-local ID
    // counter. Persisted with metadata; never assigned to the world vehicle.
    string m_RecordID;
    string m_OwnerUID;
    string m_OwnerName;
    string m_StoredByUID;
    string m_StoredByName;
    // Opt-in trader storage for transfer to another map. Only the depositing UID
    // may view, preview, or retrieve this vehicle, regardless of party or ownership.
    bool m_StoreForDifferentMap = false;
    string m_ClassName;
    string m_DisplayName;
    string m_WorldName;
    // "profile": local per-map storage. "profile-crossmap": Hive-synced storage.
    // Empty on old mission records means legacy Expansion mission entity storage.
    string m_StorageBackend;
    // TRADER|world|zone or TERMINAL|world|persistent-terminal-ID.
    // Empty for old global garages (legacy vehicles are available at trader terminals).
    string m_StorageScopeID;
    string m_SkinName;
    vector m_Position;
    vector m_Orientation;
    autoptr array<int> m_GlobalID;
    int m_StoredEpoch;
    int m_CargoItemCount;
    float m_HealthPercent;
    bool m_WasLocked;
    int m_InventorySnapshotVersion;
    int m_MarketValue;
    int m_RepairCost;
    bool m_RepairAvailable;
    string m_RepairUnavailableReason;
    ref array<ref BDE_VG_AttachmentPreviewData> m_PreviewAttachments;
    ref array<ref BDE_VG_CargoEntry> m_CargoEntries;

    [NonSerialized()];
    bool m_IsStored;
    [NonSerialized()];
    bool m_IsLegacy;
    [NonSerialized()];
    string m_StorageKey;
    [NonSerialized()];
    string m_StatusText;
    [NonSerialized()];
    float m_DistanceMeters;
    [NonSerialized()];
    int m_SkinIndex;
    [NonSerialized()];
    int m_NetworkIDLow;
    [NonSerialized()];
    int m_NetworkIDHigh;
    [NonSerialized()];
    Object m_VehicleObject;

    void BDE_VG_VehicleData()
    {
        m_GlobalID = {0,0,0,0};
        m_PreviewAttachments = new array<ref BDE_VG_AttachmentPreviewData>;
        m_CargoEntries = new array<ref BDE_VG_CargoEntry>;
        m_SkinIndex = -1;
    }

    void EnsureCollections()
    {
        if (!m_GlobalID)
            m_GlobalID = {0,0,0,0};
        if (!m_PreviewAttachments)
            m_PreviewAttachments = new array<ref BDE_VG_AttachmentPreviewData>;
        if (!m_CargoEntries)
            m_CargoEntries = new array<ref BDE_VG_CargoEntry>;
    }

    void MigrateStoredData()
    {
        EnsureCollections();
        if (m_StoredByUID == string.Empty && m_StorageBackend != "profile-crossmap")
            m_StoredByUID = m_OwnerUID;
        if (m_StoredByName == string.Empty)
            m_StoredByName = m_OwnerName;
        m_WorldName.ToLower();
        // Do not upgrade the per-vehicle version on read: version 3 profile
        // records retain their pre-v2.5 cross-map retrieval compatibility.
        if (m_Version <= 0)
            m_Version = 3;
    }

    bool IsGlobalIDValid()
    {
        if (!m_GlobalID || m_GlobalID.Count() != 4)
            return false;
        return m_GlobalID[0] != 0 || m_GlobalID[1] != 0 || m_GlobalID[2] != 0 || m_GlobalID[3] != 0;
    }

    bool IsGlobalIDEqual(TIntArray globalID)
    {
        if (!IsGlobalIDValid() || !globalID || globalID.Count() != 4)
            return false;
        for (int i = 0; i < 4; i++)
        {
            if (m_GlobalID[i] != globalID[i])
                return false;
        }
        return true;
    }

    string GetStorageKey()
    {
        if (m_StorageKey != string.Empty)
            return m_StorageKey;
        if (m_RecordID != string.Empty)
            return m_RecordID;
        if (!IsGlobalIDValid())
            return string.Empty;
        return ExpansionStatic.IntToHex(m_GlobalID);
    }

    string GetEntityStorageFileName()
    {
        if (m_IsLegacy)
            return BDE_VG_LEGACY_VEHICLE_DIRECTORY + m_StorageKey + ExpansionEntityStorageModule.EXT;
        if (m_StorageBackend == "profile-crossmap")
            return BDE_VG_CROSSMAP_DATA_DIRECTORY + GetStorageKey() + ExpansionEntityStorageModule.EXT;
        if (m_StorageBackend == "profile")
            return BDE_VG_VEHICLE_DATA_DIRECTORY + GetStorageKey() + ExpansionEntityStorageModule.EXT;
        return ExpansionEntityStorageModule.GetFileName(ExpansionStatic.IntToHex(m_GlobalID));
    }

    string GetEntityStorageDirectory()
    {
        if (m_StorageBackend == "profile-crossmap")
            return BDE_VG_CROSSMAP_DATA_DIRECTORY;
        if (m_StorageBackend == "profile")
            return BDE_VG_VEHICLE_DATA_DIRECTORY;
        return ExpansionEntityStorageModule.GetStorageDirectory();
    }

    // Commit-time cleanup for Expansion root and nested cargo payloads.
    // For legacy metadata the storage key is the old StorageID, not a global ID.
    void DeleteStorageFiles()
    {
        if (m_IsLegacy)
        {
            if (m_StorageKey != string.Empty)
                ExpansionEntityStorageModule.DeleteFiles(m_StorageKey,BDE_VG_LEGACY_VEHICLE_DIRECTORY);
            return;
        }
        if (IsGlobalIDValid())
            ExpansionEntityStorageModule.DeleteFiles(GetStorageKey(),GetEntityStorageDirectory());
    }

    string GetStoredByName()
    {
        if (m_StoredByName != string.Empty)
            return m_StoredByName;
        if (m_OwnerName != string.Empty)
            return m_OwnerName;
        return "Unknown";
    }

    void SetDataFromVehicle(ExpansionVehicle vehicle, PlayerBase storedBy)
    {
        EnsureCollections();
        EntityAI entity = vehicle.GetEntity();
        m_OwnerUID = vehicle.GetOwnerUID();
        m_OwnerName = vehicle.GetOwnerName();
        if (storedBy && storedBy.GetIdentity())
        {
            m_StoredByUID = storedBy.GetIdentity().GetId();
            m_StoredByName = storedBy.GetIdentity().GetName();
        }
        m_ClassName = vehicle.GetType();
        m_DisplayName = entity.GetDisplayName();
        if (m_DisplayName == string.Empty)
            m_DisplayName = ExpansionStatic.GetItemDisplayNameWithType(m_ClassName);
        m_WorldName = g_Game.GetWorldName();
        m_WorldName.ToLower();
        m_StorageBackend = "profile";
        m_Position = vehicle.GetPosition();
        m_Position = Vector(m_Position[0],m_Position[1] - vehicle.GetModelZeroPointDistanceFromGround(),m_Position[2]);
        m_Orientation = vehicle.GetOrientation();
        m_SkinName = vehicle.GetCurrentSkinName();
        m_SkinIndex = vehicle.GetCurrentSkinIndex();
        entity.GetNetworkID(m_NetworkIDLow,m_NetworkIDHigh);
        m_VehicleObject = entity;
        m_Version = 4;
        m_StoredEpoch = CF_Date.Now(true).DateToEpoch();
        BDE_VG_Utility.CaptureVehicleInventory(entity,m_PreviewAttachments,m_CargoEntries);
        m_CargoItemCount = BDE_VG_Utility.CountCargoEntries(m_CargoEntries);
        m_InventorySnapshotVersion = 1;
        m_HealthPercent = entity.GetHealth01("","Health") * 100.0;
        m_WasLocked = vehicle.IsLocked();

        if (!vehicle.GetGlobalID().m_IsSet)
            vehicle.GetGlobalID().Acquire();
        for (int i = 0; i < 4; i++)
            m_GlobalID[i] = vehicle.GetGlobalID().m_ID[i];
    }

    void PrepareForClient(bool isStored, string storageKey, string statusText, float distanceMeters)
    {
        m_IsStored = isStored;
        m_StorageKey = storageKey;
        m_StatusText = statusText;
        m_DistanceMeters = distanceMeters;
    }

    void OnSend(ParamsWriteContext ctx)
    {
        EnsureCollections();
        ctx.Write(m_IsStored);
        ctx.Write(m_IsLegacy);
        ctx.Write(GetStorageKey());
        ctx.Write(m_ClassName);
        ctx.Write(m_DisplayName);
        ctx.Write(m_OwnerName);
        ctx.Write(m_StoredByName);
        // Write takes an untyped value. Resolve the compound expression into
        // a concrete bool before passing it through the native serializer.
        bool retrieveOnAllServers = m_StoreForDifferentMap && m_StorageBackend == "profile-crossmap";
        ctx.Write(retrieveOnAllServers);
        ctx.Write(m_Position);
        ctx.Write(m_SkinIndex);
        ctx.Write(m_NetworkIDLow);
        ctx.Write(m_NetworkIDHigh);
        ctx.Write(m_DistanceMeters);
        ctx.Write(m_HealthPercent);
        ctx.Write(m_CargoItemCount);
        ctx.Write(m_InventorySnapshotVersion);
        ctx.Write(m_MarketValue);
        ctx.Write(m_RepairCost);
        ctx.Write(m_RepairAvailable);
        ctx.Write(m_RepairUnavailableReason);

        ctx.Write(m_PreviewAttachments.Count());
        foreach (BDE_VG_AttachmentPreviewData attachmentData : m_PreviewAttachments)
            attachmentData.OnSend(ctx);

        ctx.Write(m_CargoEntries.Count());
        foreach (BDE_VG_CargoEntry cargoEntry : m_CargoEntries)
            cargoEntry.OnSend(ctx);

        ctx.Write(m_StatusText);
    }

    bool OnReceive(ParamsReadContext ctx)
    {
        EnsureCollections();
        if (!ctx.Read(m_IsStored))
            return false;
        if (!ctx.Read(m_IsLegacy))
            return false;
        if (!ctx.Read(m_StorageKey))
            return false;
        if (!ctx.Read(m_ClassName))
            return false;
        if (!ctx.Read(m_DisplayName))
            return false;
        if (!ctx.Read(m_OwnerName))
            return false;
        if (!ctx.Read(m_StoredByName))
            return false;
        if (!ctx.Read(m_StoreForDifferentMap))
            return false;
        if (!ctx.Read(m_Position))
            return false;
        if (!ctx.Read(m_SkinIndex))
            return false;
        if (!ctx.Read(m_NetworkIDLow))
            return false;
        if (!ctx.Read(m_NetworkIDHigh))
            return false;
        if (!ctx.Read(m_DistanceMeters))
            return false;
        if (!ctx.Read(m_HealthPercent))
            return false;
        if (!ctx.Read(m_CargoItemCount))
            return false;
        if (!ctx.Read(m_InventorySnapshotVersion))
            return false;
        if (!ctx.Read(m_MarketValue))
            return false;
        if (!ctx.Read(m_RepairCost))
            return false;
        if (!ctx.Read(m_RepairAvailable))
            return false;
        if (!ctx.Read(m_RepairUnavailableReason))
            return false;

        int attachmentCount;
        if (!ctx.Read(attachmentCount))
            return false;
        m_PreviewAttachments.Clear();
        for (int i = 0; i < attachmentCount; i++)
        {
            BDE_VG_AttachmentPreviewData attachmentData = new BDE_VG_AttachmentPreviewData;
            if (!attachmentData.OnReceive(ctx))
                return false;
            m_PreviewAttachments.Insert(attachmentData);
        }

        int cargoEntryCount;
        if (!ctx.Read(cargoEntryCount))
            return false;
        m_CargoEntries.Clear();
        for (int j = 0; j < cargoEntryCount; j++)
        {
            BDE_VG_CargoEntry cargoEntry = new BDE_VG_CargoEntry;
            if (!cargoEntry.OnReceive(ctx))
                return false;
            m_CargoEntries.Insert(cargoEntry);
        }

        if (!ctx.Read(m_StatusText))
            return false;
        if (!g_Game.IsDedicatedServer() && !m_IsStored)
            m_VehicleObject = g_Game.GetObjectByNetworkId(m_NetworkIDLow,m_NetworkIDHigh);
        return true;
    }
};


class BDE_VG_HealthSnapshot
{
    float m_GlobalHealth;
    ref array<string> m_Zones;
    ref array<float> m_ZoneHealth;

    void BDE_VG_HealthSnapshot()
    {
        m_Zones = new array<string>;
        m_ZoneHealth = new array<float>;
    }

    void Capture(EntityAI entity)
    {
        if (!entity)
            return;
        m_GlobalHealth = entity.GetHealth("","Health");
        TStringArray zones = new TStringArray;
        entity.GetDamageZones(zones);
        foreach (string zoneName : zones)
        {
            m_Zones.Insert(zoneName);
            m_ZoneHealth.Insert(entity.GetHealth(zoneName,"Health"));
        }
    }

    void Restore(EntityAI entity)
    {
        if (!entity)
            return;
        entity.SetHealth("","Health",m_GlobalHealth);
        for (int i = 0; i < m_Zones.Count(); i++)
            entity.SetHealth(m_Zones[i],"Health",m_ZoneHealth[i]);
        entity.SetSynchDirty();
    }
};

class BDE_VG_AlternatePromptData
{
    string m_StorageKey;
    string m_BlockingObjectName;
    vector m_OriginalPosition;
    vector m_AlternativePosition;
    float m_AlternativeDistanceMeters;
};

class BDE_VG_RestoreContext
{
    PlayerBase m_Player;
    EntityAI m_AccessObject;
    ref BDE_VG_VehicleData m_VehicleData;
    ref BDE_VG_GarageData m_GarageData;
    ref BDE_VG_LegacyMetadata m_LegacyMetadata;
    EntityAI m_LoadedEntity;
    string m_BusyKey;
    vector m_AlternativeBasePosition;
    bool m_UseAlternative;
    bool m_CreateVehicleMarker;
    vector m_TargetOrientation;
    string m_LocationText;
};

class BDE_VG_Utility
{
    static bool IsSafeStorageKey(string key)
    {
        return key != string.Empty && key.Length() <= 128 && key.IndexOf("/") < 0 && key.IndexOf("\\") < 0 && key.IndexOf(":") < 0 && key.IndexOf("..") < 0;
    }

    static void CaptureVehicleInventory(EntityAI vehicleEntity, array<ref BDE_VG_AttachmentPreviewData> attachments, array<ref BDE_VG_CargoEntry> cargoEntries)
    {
        if (!attachments || !cargoEntries)
            return;

        attachments.Clear();
        cargoEntries.Clear();
        if (!vehicleEntity || !vehicleEntity.GetInventory())
            return;

        CaptureAttachmentsRecursive(vehicleEntity,-1,attachments);

        array<EntityAI> inventoryEntities = new array<EntityAI>;
        vehicleEntity.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER,inventoryEntities);
        foreach (EntityAI inventoryEntity : inventoryEntities)
        {
            if (!inventoryEntity || inventoryEntity == vehicleEntity)
                continue;
            if (!IsEntityInsideVehicleCargo(inventoryEntity,vehicleEntity))
                continue;
            AddCargoEntry(cargoEntries,inventoryEntity);
        }
    }

    protected static void CaptureAttachmentsRecursive(EntityAI parentEntity, int parentIndex, array<ref BDE_VG_AttachmentPreviewData> attachments)
    {
        if (!parentEntity || !parentEntity.GetInventory())
            return;

        int attachmentCount = parentEntity.GetInventory().AttachmentCount();
        for (int i = 0; i < attachmentCount; i++)
        {
            EntityAI attachment = parentEntity.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment)
                continue;

            InventoryLocation inventoryLocation = new InventoryLocation;
            int slotID = -1;
            if (attachment.GetInventory() && attachment.GetInventory().GetCurrentInventoryLocation(inventoryLocation))
                slotID = inventoryLocation.GetSlot();

            BDE_VG_AttachmentPreviewData attachmentData = new BDE_VG_AttachmentPreviewData;
            attachmentData.m_ClassName = attachment.GetType();
            attachmentData.m_SlotID = slotID;
            attachmentData.m_ParentIndex = parentIndex;
            attachmentData.m_HealthPercent = attachment.GetHealth01("","Health") * 100.0;
            int attachmentIndex = attachments.Count();
            attachments.Insert(attachmentData);
            CaptureAttachmentsRecursive(attachment,attachmentIndex,attachments);
        }
    }

    protected static bool IsEntityInsideVehicleCargo(EntityAI inventoryEntity, EntityAI vehicleEntity)
    {
        EntityAI currentEntity = inventoryEntity;
        while (currentEntity && currentEntity != vehicleEntity)
        {
            InventoryLocation inventoryLocation = new InventoryLocation;
            if (currentEntity.GetInventory() && currentEntity.GetInventory().GetCurrentInventoryLocation(inventoryLocation))
            {
                if (inventoryLocation.GetType() == InventoryLocationType.CARGO || inventoryLocation.GetType() == InventoryLocationType.PROXYCARGO)
                    return true;
            }

            currentEntity = currentEntity.GetHierarchyParent();
        }

        return false;
    }

    protected static void AddCargoEntry(array<ref BDE_VG_CargoEntry> cargoEntries, EntityAI inventoryEntity)
    {
        string className = inventoryEntity.GetType();
        foreach (BDE_VG_CargoEntry existingEntry : cargoEntries)
        {
            if (existingEntry && existingEntry.m_ClassName == className)
            {
                existingEntry.m_Count++;
                return;
            }
        }

        BDE_VG_CargoEntry cargoEntry = new BDE_VG_CargoEntry;
        cargoEntry.m_ClassName = className;
        cargoEntry.m_DisplayName = inventoryEntity.GetDisplayName();
        if (cargoEntry.m_DisplayName == string.Empty)
            cargoEntry.m_DisplayName = ExpansionStatic.GetItemDisplayNameWithType(className);
        cargoEntry.m_Count = 1;
        cargoEntries.Insert(cargoEntry);
    }

    static int CountCargoEntries(array<ref BDE_VG_CargoEntry> cargoEntries)
    {
        int count;
        if (!cargoEntries)
            return count;
        foreach (BDE_VG_CargoEntry cargoEntry : cargoEntries)
        {
            if (cargoEntry)
                count += cargoEntry.m_Count;
        }
        return count;
    }

    static string GetWorldNameLower()
    {
        string worldName = g_Game.GetWorldName();
        worldName.ToLower();
        return worldName;
    }
};
