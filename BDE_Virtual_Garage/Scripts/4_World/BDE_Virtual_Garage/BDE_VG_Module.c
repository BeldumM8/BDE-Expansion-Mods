[CF_RegisterModule(BDE_VG_Module)]
class BDE_VG_Module : CF_ModuleWorld
{
    static BDE_VG_Module s_Instance;
    static string s_GarageFolderPath;
    static string s_LegacyGarageFolderPath;

    protected int m_ClientAccessNetworkIDLow;
    protected int m_ClientAccessNetworkIDHigh;
    protected bool m_AccessSettingsReceived;
    protected bool m_AccessSettingsEnabled;
    protected bool m_AccessSettingsMatchInherited;
    protected ref array<string> m_AccessSettingsClasses;
    protected int m_NextAccessSettingsRequest;


    protected ref array<ref BDE_VG_GarageData> m_Garages;
    protected ref array<string> m_BusyKeys;
    protected ref ScriptInvoker m_MenuInvoker;
    protected ref ScriptInvoker m_CallbackInvoker;
    protected ref ScriptInvoker m_AlternatePromptInvoker;
    protected ref ScriptInvoker m_ExclusivePromptInvoker;
    protected ref map<string, ref BDE_VG_ExclusiveConsent> m_ExclusivePending;

    void BDE_VG_Module()
    {
        s_Instance = this;
    }

    override void OnInit()
    {
        super.OnInit();
        EnableMissionStart();
        Expansion_EnableRPCManager();
        Expansion_RegisterServerRPC("RPC_RequestAccessSettings");
        Expansion_RegisterClientRPC("RPC_SendAccessSettings");
        Expansion_RegisterServerRPC("RPC_RequestVehicles");
        Expansion_RegisterClientRPC("RPC_SendVehicles");
        Expansion_RegisterServerRPC("RPC_StoreVehicle");
        Expansion_RegisterServerRPC("RPC_RetrieveVehicle");
        Expansion_RegisterClientRPC("RPC_Callback");
        Expansion_RegisterClientRPC("RPC_AlternatePrompt");
        Expansion_RegisterClientRPC("RPC_ExclusivePrompt");
        Expansion_RegisterServerRPC("RPC_CancelExclusive");
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        if (g_Game.IsServer() && g_Game.IsMultiplayer())
            ServerInit();
        if (g_Game.IsClient())
            ClientInit();
    }

    protected void ClientInit()
    {
        if (!m_MenuInvoker)
            m_MenuInvoker = new ScriptInvoker;
        if (!m_CallbackInvoker)
            m_CallbackInvoker = new ScriptInvoker;
        if (!m_AlternatePromptInvoker)
            m_AlternatePromptInvoker = new ScriptInvoker;
    }

    protected void ServerInit()
    {
        BDE_VG_Settings.Get();
        BDE_VG_Settings.EnsureDirectories();
        CreateDirectoryStructure();
        BDE_VG_ExclusivePolicy.Reload();
        m_ExclusivePending = new map<string, ref BDE_VG_ExclusiveConsent>;
        BDE_VG_TraderZones.Reload();
        m_Garages = new array<ref BDE_VG_GarageData>;
        m_BusyKeys = new array<string>;
        LoadGarages();
        BDE_VG_Logger.Log("Loaded BDE Virtual Garage v2.10.0 with " + m_Garages.Count().ToString() + " owner garage files.",true);
    }

    protected void CreateDirectoryStructure()
    {
        int instanceID = g_Game.ServerConfigGetInt("instanceId");
        s_LegacyGarageFolderPath = "$mission:storage_" + instanceID.ToString() + "\\bde\\virtualgarage\\";
        s_GarageFolderPath = BDE_VG_VEHICLE_DATA_DIRECTORY;
        BDE_VG_Settings.EnsureDirectories();
    }

    protected BDE_VG_GarageData LoadGarageWithCache(string filePath, bool missionIndex, bool crossMapIndex, array<ref BDE_VG_GarageData> previous)
    {
        BDE_VG_GarageData loaded = BDE_VG_GarageData.Load(filePath, missionIndex, crossMapIndex);
        if (loaded)
            return loaded;

        // Sync software may replace a JSON before completing the transfer.
        // If an existing index is temporarily unreadable, keep its previously
        // validated in-memory version. Deleted indexes remain deleted.
        if (FileExist(filePath) && previous)
        {
            foreach (BDE_VG_GarageData cached : previous)
            {
                if (cached && cached.GetFileName() == filePath)
                {
                    BDE_VG_Logger.Log("WARNING: Index unreadable during refresh; retaining last valid in-memory copy: " + filePath,true);
                    return cached;
                }
            }
        }
        return null;
    }

    protected void LoadGarages()
    {
        ref array<ref BDE_VG_GarageData> previous = m_Garages;
        ref array<ref BDE_VG_GarageData> loaded = new array<ref BDE_VG_GarageData>;

        TStringArray crossMapFiles = ExpansionStatic.FindFilesInLocation(BDE_VG_CROSSMAP_DATA_DIRECTORY,".json");
        foreach (string crossMapFileName : crossMapFiles)
        {
            if (!ExpansionString.EndsWithIgnoreCase(crossMapFileName, ".json"))
                continue;
            string path = BDE_VG_CROSSMAP_DATA_DIRECTORY + crossMapFileName;
            BDE_VG_GarageData crossMapGarage = LoadGarageWithCache(path,false,true,previous);
            if (crossMapGarage)
                loaded.Insert(crossMapGarage);
        }
        TStringArray files = ExpansionStatic.FindFilesInLocation(s_GarageFolderPath,".json");
        foreach (string fileName : files)
        {
            if (!ExpansionString.EndsWithIgnoreCase(fileName, ".json"))
                continue;
            string localPath = s_GarageFolderPath + fileName;
            BDE_VG_GarageData garageData = LoadGarageWithCache(localPath,false,false,previous);
            if (garageData)
                loaded.Insert(garageData);
        }
        if (FileExist(s_LegacyGarageFolderPath))
        {
            TStringArray legacyFiles = ExpansionStatic.FindFilesInLocation(s_LegacyGarageFolderPath,".json");
            foreach (string legacyFileName : legacyFiles)
            {
                if (!ExpansionString.EndsWithIgnoreCase(legacyFileName, ".json"))
                    continue;
                string legacyPath = s_LegacyGarageFolderPath + legacyFileName;
                BDE_VG_GarageData legacyGarage = LoadGarageWithCache(legacyPath,true,false,previous);
                if (legacyGarage)
                    loaded.Insert(legacyGarage);
            }
        }
        BDE_VG_RecordStore.Merge(loaded, false);
        BDE_VG_RecordStore.Merge(loaded, true);
        m_Garages = loaded;
    }

    // Hive synchronizers may update profile files while the server is running.
    // Refresh only between operations, never invalidating a live restore context.
    protected void RefreshGaragesFromDisk()
    {
        if (m_BusyKeys && m_BusyKeys.Count() > 0)
            return;
        LoadGarages();
    }

    bool IsGarageActionTarget(Object accessObject)
    {
        if (!accessObject || !EntityAI.Cast(accessObject))
        {
            return false;
        }

        if (g_Game.IsServer())
        {
            return BDE_VG_Settings.Get().IsGarageAccessObject(accessObject.GetType());
        }

        if (!m_AccessSettingsReceived)
        {
            // Request lazily after the player can interact; retry if the first RPC was lost.
            int now = g_Game.GetTime();
            if (now >= m_NextAccessSettingsRequest)
            {
                m_NextAccessSettingsRequest = now + 5000;
                auto request = Expansion_CreateRPC("RPC_RequestAccessSettings");
                request.Expansion_Send(true);
            }
            return false;
        }

        if (!m_AccessSettingsEnabled || !m_AccessSettingsClasses)
        {
            return false;
        }

        string accessType = accessObject.GetType();
        foreach (string configuredType : m_AccessSettingsClasses)
        {
            if (configuredType == string.Empty)
            {
                continue;
            }
            if (accessType == configuredType)
            {
                return true;
            }
            if (m_AccessSettingsMatchInherited && g_Game.IsKindOf(accessType, configuredType))
            {
                return true;
            }
        }
        return false;
    }

    protected void RPC_RequestAccessSettings(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        if (!identity)
        {
            return;
        }

        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        auto rpc = Expansion_CreateRPC("RPC_SendAccessSettings");
        rpc.Write(settings.Enabled);
        rpc.Write(settings.MatchInheritedAccessObjectClasses);
        rpc.Write(settings.GarageAccessObjects);
        rpc.Expansion_Send(true, identity);
    }

    protected void RPC_SendAccessSettings(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        bool enabled;
        bool matchInherited;
        array<string> classes;
        if (!ctx.Read(enabled))
            return;
        if (!ctx.Read(matchInherited))
            return;
        if (!ctx.Read(classes))
            return;
        if (!classes)
        {
            return;
        }

        m_AccessSettingsEnabled = enabled;
        m_AccessSettingsMatchInherited = matchInherited;
        m_AccessSettingsClasses = classes;
        m_AccessSettingsReceived = true;
        Print("[BDE Virtual Garage] Access settings received: enabled=" + enabled.ToString() + " classes=" + classes.Count().ToString());
    }

    bool SetClientAccessObject(Object accessObject)
    {
        if (!accessObject)
            return false;

        accessObject.GetNetworkID(m_ClientAccessNetworkIDLow,m_ClientAccessNetworkIDHigh);
        return m_ClientAccessNetworkIDLow != 0 || m_ClientAccessNetworkIDHigh != 0;
    }

    bool GetClientAccessNetworkIDs(out int accessNetworkIDLow, out int accessNetworkIDHigh)
    {
        accessNetworkIDLow = m_ClientAccessNetworkIDLow;
        accessNetworkIDHigh = m_ClientAccessNetworkIDHigh;
        return accessNetworkIDLow != 0 || accessNetworkIDHigh != 0;
    }

    void RequestRefresh(int accessNetworkIDLow, int accessNetworkIDHigh)
    {
        auto rpc = Expansion_CreateRPC("RPC_RequestVehicles");
        rpc.Write(accessNetworkIDLow);
        rpc.Write(accessNetworkIDHigh);
        rpc.Expansion_Send(true);
    }

    protected void RPC_RequestVehicles(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        if (!identity || !ctx)
            return;
        int accessNetworkIDLow;
        int accessNetworkIDHigh;
        if (!ctx.Read(accessNetworkIDLow))
            return;
        if (!ctx.Read(accessNetworkIDHigh))
            return;
        PlayerBase player = PlayerBase.GetPlayerByUID(identity.GetId());
        EntityAI accessObject = ResolveAccessObject(accessNetworkIDLow,accessNetworkIDHigh);
        if (!ValidateAccessObject(player,accessObject,true))
            return;
        RefreshGaragesFromDisk();
        SendVehicles(player,accessObject);
    }

    protected void SendVehicles(PlayerBase player, EntityAI accessObject)
    {
        array<ref BDE_VG_VehicleData> worldVehicles = GetWorldVehicles(player,accessObject);
        array<ref BDE_VG_VehicleData> storedVehicles = GetStoredVehicles(player,accessObject);
        int accessNetworkIDLow;
        int accessNetworkIDHigh;
        accessObject.GetNetworkID(accessNetworkIDLow,accessNetworkIDHigh);
        string accessName = accessObject.GetDisplayName();
        if (accessName == string.Empty)
            accessName = accessObject.GetType();

        auto rpc = Expansion_CreateRPC("RPC_SendVehicles");
        rpc.Write(accessNetworkIDLow);
        rpc.Write(accessNetworkIDHigh);
        rpc.Write(accessName);
        rpc.Write(IsTraderAccess(accessObject));
        rpc.Write(BDE_VG_Settings.Get().VehicleStoreRangeMeters);
        rpc.Write(GetPlayerATMBalance(player));
        rpc.Write(worldVehicles.Count());
        foreach (BDE_VG_VehicleData worldVehicle : worldVehicles)
            worldVehicle.OnSend(rpc);
        rpc.Write(storedVehicles.Count());
        foreach (BDE_VG_VehicleData storedVehicle : storedVehicles)
            storedVehicle.OnSend(rpc);
        rpc.Expansion_Send(true,player.GetIdentity());
    }

    protected void RPC_SendVehicles(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        int accessNetworkIDLow;
        int accessNetworkIDHigh;
        string accessName;
        bool isTraderAccess;
        float storeRangeMeters;
        int atmBalance;
        int worldCount;
        int storedCount;
        if (!ctx.Read(accessNetworkIDLow))
            return;
        if (!ctx.Read(accessNetworkIDHigh))
            return;
        if (!ctx.Read(accessName))
            return;
        if (!ctx.Read(isTraderAccess))
            return;
        if (!ctx.Read(storeRangeMeters))
            return;
        if (!ctx.Read(atmBalance))
            return;
        if (!ctx.Read(worldCount))
            return;

        array<ref BDE_VG_VehicleData> worldVehicles = new array<ref BDE_VG_VehicleData>;
        for (int i = 0; i < worldCount; i++)
        {
            BDE_VG_VehicleData worldVehicle = new BDE_VG_VehicleData;
            if (!worldVehicle.OnReceive(ctx))
                return;
            worldVehicles.Insert(worldVehicle);
        }

        if (!ctx.Read(storedCount))
            return;
        array<ref BDE_VG_VehicleData> storedVehicles = new array<ref BDE_VG_VehicleData>;
        for (int j = 0; j < storedCount; j++)
        {
            BDE_VG_VehicleData storedVehicle = new BDE_VG_VehicleData;
            if (!storedVehicle.OnReceive(ctx))
                return;
            storedVehicles.Insert(storedVehicle);
        }

        if (m_MenuInvoker)
            m_MenuInvoker.Invoke(accessNetworkIDLow,accessNetworkIDHigh,accessName,isTraderAccess,storeRangeMeters,atmBalance,worldVehicles,storedVehicles);
    }

    void StoreVehicleRequest(int accessNetworkIDLow, int accessNetworkIDHigh, int vehicleNetworkIDLow, int vehicleNetworkIDHigh, bool repairVehicle, bool storeForDifferentMap, string consentToken = "")
    {
        // A streamed-out client object must not silently swallow this request.
        // Resolve the authoritative network object on the server instead.
        auto rpc = Expansion_CreateRPC("RPC_StoreVehicle");
        rpc.Write(accessNetworkIDLow);
        rpc.Write(accessNetworkIDHigh);
        rpc.Write(vehicleNetworkIDLow);
        rpc.Write(vehicleNetworkIDHigh);
        rpc.Write(repairVehicle);
        rpc.Write(storeForDifferentMap);
        rpc.Write(consentToken);
        rpc.Expansion_Send(true);
    }

    protected void RPC_StoreVehicle(PlayerIdentity identity, Object vehicleObject, ParamsReadContext ctx)
    {
        if (!identity || !ctx)
            return;
        int accessNetworkIDLow;
        int accessNetworkIDHigh;
        if (!ctx.Read(accessNetworkIDLow))
            return;
        if (!ctx.Read(accessNetworkIDHigh))
            return;
        int vehicleNetworkIDLow;
        int vehicleNetworkIDHigh;
        if (!ctx.Read(vehicleNetworkIDLow) || !ctx.Read(vehicleNetworkIDHigh))
            return;
        bool repairVehicle;
        if (!ctx.Read(repairVehicle))
            return;
        bool storeForDifferentMap;
        if (!ctx.Read(storeForDifferentMap))
            return;
        string consentToken;
        ctx.Read(consentToken); // Missing tokens never authorize item removal.
        PlayerBase player = PlayerBase.GetPlayerByUID(identity.GetId());
        EntityAI accessObject = ResolveAccessObject(accessNetworkIDLow,accessNetworkIDHigh);
        EntityAI vehicleEntity = EntityAI.Cast(g_Game.GetObjectByNetworkId(vehicleNetworkIDLow,vehicleNetworkIDHigh));
        if (!vehicleEntity)
        {
            SendCallback(identity,BDE_VG_CALLBACK_ERROR,false,"The vehicle is no longer available. Refresh the nearby vehicle list and try again.");
            return;
        }
        StoreVehicle(player,accessObject,vehicleEntity,repairVehicle,storeForDifferentMap,consentToken);
    }

    protected void StoreVehicle(PlayerBase player, EntityAI accessObject, EntityAI vehicleEntity, bool repairRequested, bool storeForDifferentMap, string consentToken)
    {
        if (!ValidateAccessObject(player,accessObject,true) || !vehicleEntity)
            return;
        if (m_BusyKeys && m_BusyKeys.Count() > 0)
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Another garage transaction is finishing. Please retry shortly.");
            return;
        }
        RefreshGaragesFromDisk();

        ExpansionVehicle vehicle;
        if (!ExpansionVehicle.Get(vehicle,vehicleEntity))
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The selected object is not a supported Expansion vehicle.");
            return;
        }

        string busyKey = "WORLD_" + vehicle.GetPersistentIDString();
        if (busyKey == "WORLD_")
            busyKey = "WORLD_" + vehicleEntity.GetType() + "_" + vehicleEntity.GetPosition().ToString();
        if (IsBusy(busyKey))
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This vehicle is already being processed.");
            return;
        }

        string validationError = GetStoreValidationError(player,accessObject,vehicle);
        if (validationError != string.Empty)
        {
            Print("[BDE_VG] Store rejected for " + vehicleEntity.GetType() + " vehicle=" + vehicleEntity.GetPosition().ToString() + " terminal=" + accessObject.GetPosition().ToString() + " reason=" + validationError);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,validationError);
            return;
        }

        string ownerUID = vehicle.GetOwnerUID();
        // Choose the index AND payload directory based on the trader opt-in.
        // A normal store must never write anything into the Hive-synced folder.
        // Base-terminal attempts to opt in are rejected below.
        BDE_VG_GarageData garageData;

        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        bool isTraderGarage = IsTraderAccess(accessObject);
        // This is a trader-only opt-in. Reject forged RPCs at base terminals.
        if (storeForDifferentMap && !isTraderGarage)
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Different-map storage can only be selected at a trader garage.");
            return;
        }
        // An existing but unreadable index must NEVER be treated as an empty
        // new garage. This is especially important during Hive synchronization.
        string ownerIndexPath = s_GarageFolderPath + ownerUID + ".json";
        if (storeForDifferentMap)
            ownerIndexPath = BDE_VG_CROSSMAP_DATA_DIRECTORY + ownerUID + ".json";
        if (FileExist(ownerIndexPath) && !BDE_VG_GarageData.Load(ownerIndexPath,false,storeForDifferentMap))
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Garage index is temporarily unreadable; storage has been blocked to protect existing vehicles. Check Hive synchronization.");
            return;
        }
        garageData = GetOrCreateGarage(ownerUID,vehicle.GetOwnerName(),storeForDifferentMap);
        if (!garageData)
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Garage owner index could not be safely loaded.");
            return;
        }
        int maxStored = settings.MaxStoredVehiclesPerOwner;
        if (!isTraderGarage)
            maxStored = settings.MaxStoredVehiclesPerOwnerOutsideTraderZone;
        if (maxStored > 0 && CountOwnerStoredVehicles(ownerUID,isTraderGarage) >= maxStored)
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The vehicle owner has reached the configured storage limit for this type of garage.");
            return;
        }

        bool extractedExclusiveItems = false;
        ref array<EntityAI> exclusiveItems;
        ref array<string> exclusiveRows;
        string exclusiveSignature;
        if (storeForDifferentMap)
        {
            if (!BDE_VG_ExclusivePolicy.Reload())
            {
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Server-exclusive configuration is unreadable. Cross-server storage is blocked; nothing was moved.");
                return;
            }
            if (BDE_VG_ExclusivePolicy.VehicleBlocked(vehicleEntity))
            {
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This vehicle is exclusive to this server. Disable retrieval on a different server to store it locally.");
                return;
            }
            BDE_VG_ExclusivePolicy.Scan(vehicleEntity,exclusiveItems,exclusiveRows,exclusiveSignature);
            if (exclusiveItems.Count() > 0)
            {
                BDE_VG_ExclusiveConsent consent = m_ExclusivePending.Get(player.GetIdentity().GetId());
                bool approved = consent && consentToken != string.Empty && consent.Token == consentToken && consent.Vehicle == vehicleEntity && consent.Access == accessObject && consent.Repair == repairRequested && consent.Signature == exclusiveSignature && consent.Expires >= g_Game.GetTime();
                if (!approved)
                {
                    SendExclusivePrompt(player,accessObject,vehicleEntity,repairRequested,exclusiveSignature,exclusiveRows);
                    return;
                }
                m_ExclusivePending.Remove(player.GetIdentity().GetId());
            }
        }

        int repairCost;
        int marketValue;
        ExpansionMarketATM_Data atmData;
        BDE_VG_HealthSnapshot healthSnapshot;
        if (repairRequested)
        {
            string repairError = GetRepairValidationError(player,accessObject,vehicleEntity,repairCost,marketValue,atmData);
            if (repairError != string.Empty)
            {
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,repairError);
                return;
            }
            healthSnapshot = new BDE_VG_HealthSnapshot;
            healthSnapshot.Capture(vehicleEntity);
            RepairVehicleHealth(vehicleEntity);
        }

        SetBusy(busyKey,true);
        BDE_VG_VehicleData vehicleData = new BDE_VG_VehicleData;
        vehicleData.SetDataFromVehicle(vehicle,player);
        int storageUUID[4];
        UUIDApi.Generate(storageUUID);
        vehicleData.m_RecordID = UUIDApi.FormatString(storageUUID);
        vehicleData.m_StorageScopeID = GetStorageScopeID(accessObject);
        vehicleData.m_StoreForDifferentMap = storeForDifferentMap;
        if (storeForDifferentMap)
            vehicleData.m_StorageBackend = "profile-crossmap";
        if (!vehicleData.IsGlobalIDValid() || vehicleData.m_RecordID.Length() != 36)
        {
            if (healthSnapshot)
                healthSnapshot.Restore(vehicleEntity);
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The vehicle could not acquire a valid Expansion global ID.");
            return;
        }

        string storageFileName = vehicleData.GetEntityStorageFileName();
        if (FileExist(storageFileName))
        {
            if (healthSnapshot)
                healthSnapshot.Restore(vehicleEntity);
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"An entity-storage file already exists for this vehicle global ID.");
            return;
        }

        string receiptPath = BDE_VG_RecordStore.PathFor(vehicleData);
        if (FileExist(receiptPath))
        {
            BDE_VG_VehicleData previousReceipt = BDE_VG_RecordStore.Load(receiptPath,storeForDifferentMap);
            if (!previousReceipt || previousReceipt.m_TransactionState != "retrieved")
            {
                if (healthSnapshot)
                    healthSnapshot.Restore(vehicleEntity);
                SetBusy(busyKey,false);
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This vehicle has an unresolved storage transaction. Administrator review is required.");
                return;
            }
        }

        if (storeForDifferentMap && exclusiveItems && exclusiveItems.Count() > 0)
        {
            if (!BDE_VG_ExclusivePolicy.MoveRestricted(player,vehicleEntity,exclusiveItems))
            {
                if (healthSnapshot)
                    healthSnapshot.Restore(vehicleEntity);
                SetBusy(busyKey,false);
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"A restricted item could not be moved. The vehicle was kept. Any items already moved remain in your temporary storage container(s).");
                return;
            }
        }
        if (storeForDifferentMap && exclusiveItems && exclusiveItems.Count() > 0)
            extractedExclusiveItems = true;
        // Full-tree verification immediately before payload serialization.
        if (storeForDifferentMap)
        {
            BDE_VG_ExclusivePolicy.Scan(vehicleEntity,exclusiveItems,exclusiveRows,exclusiveSignature);
            if (exclusiveItems.Count() > 0)
            {
                if (healthSnapshot)
                    healthSnapshot.Restore(vehicleEntity);
                SetBusy(busyKey,false);
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Restricted items remain. Nothing was stored; check your temporary storage container(s).");
                return;
            }
        }

        if (storeForDifferentMap)
        {
            // Initial metadata was captured before extraction. Rebuild it
            // from the cleaned vehicle so previews/cargo indexes cannot retain
            // server-only class names after the binary payload is made portable.
            BDE_VG_Utility.CaptureVehicleInventory(vehicleEntity,vehicleData.m_PreviewAttachments,vehicleData.m_CargoEntries);
            vehicleData.m_CargoItemCount = BDE_VG_Utility.CountCargoEntries(vehicleData.m_CargoEntries);
            vehicleData.m_WasLocked = vehicle.IsLocked();
        }

        if (!ExpansionEntityStorageModule.SaveToFile(vehicleEntity,storageFileName))
        {
            // Incomplete binary/child files must never block subsequent attempts.
            vehicleData.DeleteStorageFiles();
            if (healthSnapshot)
                healthSnapshot.Restore(vehicleEntity);
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Expansion failed to serialize the vehicle and its cargo.");
            return;
        }

        // A pending receipt is deliberately NOT retrievable. A crash between
        // world deletion and publication must quarantine, not duplicate, a vehicle.
        if (!BDE_VG_RecordStore.SaveState(vehicleData, "store-pending"))
        {
            vehicleData.DeleteStorageFiles();
            if (healthSnapshot)
                healthSnapshot.Restore(vehicleEntity);
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Vehicle metadata could not be committed. The original vehicle was kept; inspect the failed receipt before retrying.");
            return;
        }
        ExpansionEntityStorageModule.DeleteSavedEntity(vehicleEntity);
        if (!BDE_VG_RecordStore.SaveState(vehicleData, "stored"))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Vehicle payload is preserved, but publication failed. Administrator recovery is required; do not delete its files.");
            return;
        }
        garageData.AddVehicle(vehicleData);
        // Profile listings are built from individual records. Never rewrite
        // a growing owner JSON after each store.

        if (repairRequested && atmData && repairCost > 0)
        {
            atmData.RemoveMoney(repairCost);
            atmData.Save();
        }

        SetBusy(busyKey,false);
        string repairLog = string.Empty;
        string successMessage = vehicleData.m_DisplayName + " was stored with its complete inventory.";
        if (repairRequested)
        {
            repairLog = " repairCost=" + repairCost.ToString() + " marketValue=" + marketValue.ToString();
            successMessage = vehicleData.m_DisplayName + " was repaired for " + ExpansionStatic.FormatInt(repairCost) + " from the ATM account and stored with its complete inventory.";
        }
        if (extractedExclusiveItems)
        {
            successMessage.Replace("complete inventory", "compatible inventory");
            successMessage += " Restricted items are in your temporary storage container(s).";
        }
        if (storeForDifferentMap)
            successMessage = successMessage + " Different-map retrieval: storing player only.";
        BDE_VG_Logger.Log(player.GetIdentity().GetName() + " stored " + vehicleData.m_ClassName + " globalID=" + ExpansionStatic.IntToHex(vehicleData.m_GlobalID) + " key=" + vehicleData.GetStorageKey() + " owner=" + vehicleData.m_OwnerUID + " differentMap=" + storeForDifferentMap.ToString() + repairLog,true);
        SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_STORED,true,successMessage);
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SendVehiclesAfterOperation,200,false,player,accessObject);
    }

    protected string GetStoreValidationError(PlayerBase player, EntityAI accessObject, ExpansionVehicle vehicle)
    {
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        EntityAI entity = vehicle.GetEntity();
        if (!vehicle.BDE_VL_HasOwner())
            return "Unclaimed vehicles cannot be stored.";
        // Base terminals intentionally support shared vehicle storage, regardless of owner/party.
        if (IsTraderAccess(accessObject) && !CanAccessLiveVehicle(player,vehicle))
            return "You are not the owner or a member of the owner's current Expansion group.";
        if (vector.Distance(accessObject.GetPosition(),entity.GetPosition()) > settings.VehicleStoreRangeMeters)
            return "The vehicle is outside the configured garage storage range.";
        if (!vehicle.HasLifetime() && !settings.AllowStoringDynamicEventVehicles)
        {
            if (!settings.AllowStoringClaimedDEVehicles || !vehicle.BDE_VL_HasOwner())
                return "Dynamic-event vehicles cannot be stored.";
        }
        if (vehicle.IsTowing())
            return "A vehicle cannot be stored while it is towing another object.";
        if (settings.RequireVehicleEmpty && vehicle.GetCrew().Count() > 0)
            return "Everyone must exit the vehicle before it can be stored.";
        if (settings.RequireEngineOff && vehicle.EngineIsOn())
            return "Turn the vehicle engine off before storing it.";
        if (vehicle.GetSpeedometerAbsolute() > settings.MaximumStoreSpeedKmh)
            return "The vehicle must be stopped before it can be stored.";
        if (!settings.AllowRuinedVehicles && entity.IsRuined())
            return "Ruined vehicles cannot be stored.";
        return string.Empty;
    }

    void RetrieveVehicleRequest(int accessNetworkIDLow, int accessNetworkIDHigh, string storageKey, bool allowAlternative)
    {
        auto rpc = Expansion_CreateRPC("RPC_RetrieveVehicle");
        rpc.Write(accessNetworkIDLow);
        rpc.Write(accessNetworkIDHigh);
        rpc.Write(storageKey);
        rpc.Write(allowAlternative);
        rpc.Expansion_Send(true);
    }

    protected void RPC_RetrieveVehicle(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        if (!identity || !ctx)
            return;
        int accessNetworkIDLow;
        int accessNetworkIDHigh;
        string storageKey;
        bool allowAlternative;
        if (!ctx.Read(accessNetworkIDLow))
            return;
        if (!ctx.Read(accessNetworkIDHigh))
            return;
        if (!ctx.Read(storageKey))
            return;
        if (!ctx.Read(allowAlternative))
            return;
        PlayerBase player = PlayerBase.GetPlayerByUID(identity.GetId());
        EntityAI accessObject = ResolveAccessObject(accessNetworkIDLow,accessNetworkIDHigh);
        RetrieveVehicle(player,accessObject,storageKey,allowAlternative);
    }

    protected void RetrieveVehicle(PlayerBase player, EntityAI accessObject, string storageKey, bool allowAlternative)
    {
        if (!ValidateAccessObject(player,accessObject,true) || !BDE_VG_Utility.IsSafeStorageKey(storageKey))
            return;
        if (m_BusyKeys && m_BusyKeys.Count() > 0)
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Another garage transaction is finishing. Please retry shortly.");
            return;
        }
        RefreshGaragesFromDisk();

        string busyKey = "STORED_" + storageKey;
        if (IsBusy(busyKey))
        {
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This stored vehicle is already being processed.");
            return;
        }
        SetBusy(busyKey,true);

        BDE_VG_GarageData garageData;
        BDE_VG_VehicleData vehicleData = FindStoredVehicle(storageKey,garageData);
        BDE_VG_LegacyMetadata legacyMetadata;
        if (!vehicleData)
        {
            legacyMetadata = BDE_VG_LegacyMetadata.Load(storageKey);
            if (legacyMetadata)
                vehicleData = legacyMetadata.ToVehicleData();
        }

        if (!vehicleData || !FileExist(vehicleData.GetEntityStorageFileName()) || !BDE_VG_RecordStore.IsAvailable(vehicleData))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The stored vehicle data or entity-storage file is missing.");
            return;
        }
        if (!CanViewStoredVehicle(player,accessObject,vehicleData))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This vehicle is not accessible from this garage terminal.");
            return;
        }
        // Cached indexes are display-only if their on-disk copy became
        // unreadable. Never retrieve and delete a payload against a bad index.
        if (garageData && !BDE_VG_RecordStore.IsProfile(vehicleData) && !BDE_VG_GarageData.Load(garageData.GetFileName(),garageData.m_IsMissionIndex,garageData.m_IsCrossMapIndex))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The garage index is unavailable during Hive sync. Vehicle retrieval was cancelled safely.");
            return;
        }

        vector spawnPosition;
        vector spawnOrientation;
        bool hasConfiguredPoint;
        if (BDE_VG_SpawnFinder.FindConfigured(accessObject,vehicleData,spawnPosition,spawnOrientation,hasConfiguredPoint))
        {
            RestoreVehicle(player,accessObject,vehicleData,garageData,legacyMetadata,spawnPosition,spawnOrientation,true,false,"at the configured garage spawn",busyKey);
            return;
        }

        if (hasConfiguredPoint)
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The defined garage spawns are blocked and could not be cleared safely. Nothing was retrieved.");
            return;
        }

        bool sameWorld = vehicleData.m_WorldName == BDE_VG_Utility.GetWorldNameLower();
        Object blockingObject;
        if (sameWorld && BDE_VG_SpawnFinder.IsOriginalPositionFree(vehicleData,blockingObject))
        {
            RestoreVehicle(player,accessObject,vehicleData,garageData,legacyMetadata,vector.Zero,vehicleData.m_Orientation,false,false,"at its original stored position",busyKey);
            return;
        }

        // A foreign world position may be meaningless on this map; search near
        // the current access terminal instead of spawning blindly at old coordinates.
        vector origin = vehicleData.m_Position;
        if (!sameWorld)
            origin = accessObject.GetPosition();
        vector alternativePosition;
        Object alternativeBlocker;
        if (!BDE_VG_SpawnFinder.FindAlternative(vehicleData,origin,vehicleData.m_Orientation,alternativePosition,alternativeBlocker))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"No safe vehicle spawn could be found. Nothing was retrieved.");
            return;
        }

        // Automatic safe fallback: no old manual-confirmation prompt needed.
        RestoreVehicle(player,accessObject,vehicleData,garageData,legacyMetadata,alternativePosition,vehicleData.m_Orientation,true,true,"at a safe nearby position",busyKey);
    }

    protected void RestoreVehicle(PlayerBase player, EntityAI accessObject, BDE_VG_VehicleData vehicleData, BDE_VG_GarageData garageData, BDE_VG_LegacyMetadata legacyMetadata, vector alternativePosition, vector targetOrientation, bool useAlternative, bool createMarker, string locationText, string busyKey)
    {
        if (BDE_VG_RecordStore.IsProfile(vehicleData) && (!BDE_VG_RecordStore.IsAvailable(vehicleData) || !BDE_VG_RecordStore.SaveState(vehicleData, "retrieving")))
        {
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The vehicle could not be claimed safely. Nothing was retrieved.");
            return;
        }
        EntityAI loadedEntity;
        // Keep the payload until index removal is committed successfully.
        // Expansion's default RestoreFromFile() deletes root/nested .bin files
        // immediately, before our asynchronous finalization can succeed.
        bool restored;
        if (useAlternative)
            restored = BDE_VG_RestorePlacement.RestoreAtFile(vehicleData,alternativePosition,targetOrientation,loadedEntity);
        else
            restored = ExpansionEntityStorageModule.RestoreFromFile(vehicleData.GetEntityStorageFileName(),loadedEntity,null,null,false);
        if (!restored || !loadedEntity)
        {
            if (loadedEntity)
                g_Game.ObjectDelete(loadedEntity);
            if (BDE_VG_RecordStore.IsProfile(vehicleData))
                BDE_VG_RecordStore.SaveState(vehicleData, "stored");
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Expansion failed to restore the vehicle from entity storage.");
            return;
        }

        ExpansionVehicle restoredVehicle;
        if (!ExpansionVehicle.Get(restoredVehicle,loadedEntity))
        {
            loadedEntity.Delete();
            if (BDE_VG_RecordStore.IsProfile(vehicleData))
                BDE_VG_RecordStore.SaveState(vehicleData, "stored");
            SetBusy(busyKey,false);
            SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"The restored entity is not a supported Expansion vehicle.");
            return;
        }

        BDE_VG_RestoreContext context = new BDE_VG_RestoreContext;
        context.m_Player = player;
        context.m_AccessObject = accessObject;
        context.m_VehicleData = vehicleData;
        context.m_GarageData = garageData;
        context.m_LegacyMetadata = legacyMetadata;
        context.m_LoadedEntity = loadedEntity;
        context.m_BusyKey = busyKey;
        context.m_AlternativeBasePosition = alternativePosition;
        context.m_UseAlternative = useAlternative;
        context.m_TargetOrientation = targetOrientation;
        context.m_CreateVehicleMarker = createMarker;
        context.m_LocationText = locationText;

        // Keep cargo inaccessible while the asynchronous restore is provisional.
        if (loadedEntity.GetInventory())
            loadedEntity.GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
        FreezeRestoredVehicle(loadedEntity,true);
        // Placement is now chosen in the root creation phase, before the
        // network object and vehicle physics initialize at a previous terminal.
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinalizeRestoredVehicle,350,false,context);
    }

    protected void FreezeRestoredVehicle(EntityAI entity, bool freeze)
    {
        if (!entity)
            return;

        Transport transport;
        Class.CastTo(transport,entity);
        if (freeze)
        {
            if (transport)
                transport.DisableSimulation(true);
            dBodyActive(entity,ActiveState.INACTIVE);
            SetVelocity(entity,vector.Zero);
            dBodySetAngularVelocity(entity,vector.Zero);
        }
        else
        {
            SetVelocity(entity,vector.Zero);
            dBodySetAngularVelocity(entity,vector.Zero);
            if (transport)
                transport.DisableSimulation(false);
            dBodyActive(entity,ActiveState.ACTIVE);
            entity.SetSynchDirty();
        }
    }

    protected void FinalizeRestoredVehicle(BDE_VG_RestoreContext context)
    {
        if (!context || !context.m_LoadedEntity || !context.m_Player || !context.m_Player.GetIdentity())
        {
            FailRestoreContext(context,"The restored vehicle could not be finalized.");
            return;
        }

        ExpansionVehicle restoredVehicle;
        if (!ExpansionVehicle.Get(restoredVehicle,context.m_LoadedEntity))
        {
            FailRestoreContext(context,"The restored entity is not a supported vehicle.");
            return;
        }

        if (context.m_UseAlternative)
        {
            vector actualPosition = context.m_LoadedEntity.GetPosition();
            vector intendedPosition = context.m_AlternativeBasePosition;
            if (Math.AbsFloat(actualPosition[0] - intendedPosition[0]) > 0.5 || Math.AbsFloat(actualPosition[2] - intendedPosition[2]) > 0.5)
            {
                Print("[BDE_VG] Placement mismatch before commit: intended=" + intendedPosition.ToString() + " actual=" + actualPosition.ToString());
                FailRestoreContext(context,"The restored vehicle did not remain at the selected spawn. Nothing was retrieved.");
                return;
            }
        }
        vector bounds[2];
        if (context.m_LoadedEntity.GetCollisionBox(bounds))
        {
            vector size = bounds[1] - bounds[0];
            vector checkPosition = context.m_LoadedEntity.ModelToWorld(Vector(0, bounds[0][1] + size[1] * 0.05, 0));
            ref array<Object> excluded = new array<Object>;
            excluded.Insert(context.m_LoadedEntity);
            ref array<EntityAI> hierarchy = new array<EntityAI>;
            context.m_LoadedEntity.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, hierarchy);
            foreach (EntityAI child : hierarchy)
                excluded.Insert(child);
            if (ExpansionItemSpawnHelper.GetObjectBlockingPosition(checkPosition, context.m_LoadedEntity.GetOrientation(), size * 0.95, excluded))
            {
                FailRestoreContext(context,"The spawn became occupied during retrieval. The vehicle remains in storage.");
                return;
            }
        }

        // Motorbikes use their native kickstand to remain parked after retrieval.
        MotorbikeScript restoredMotorbike = MotorbikeScript.Cast(context.m_LoadedEntity);
        if (restoredMotorbike)
        {
            restoredMotorbike.SetKickstand(1.0);
        }

        if (restoredVehicle.GetOwnerUID() != context.m_VehicleData.m_OwnerUID && context.m_VehicleData.m_OwnerUID != string.Empty)
            restoredVehicle.BDE_VL_SetOwnerByUID(context.m_VehicleData.m_OwnerUID,context.m_VehicleData.m_OwnerName,true);
        if (!context.m_VehicleData.m_IsLegacy && context.m_VehicleData.m_WasLocked != restoredVehicle.IsLocked())
            restoredVehicle.BDE_VL_SetLocked(context.m_VehicleData.m_WasLocked);

        // Persist a tombstone BEFORE releasing the restored world entity.
        // If the server crashes from here onward, never replay the stored copy.
        if (BDE_VG_RecordStore.IsProfile(context.m_VehicleData))
        {
            if (!BDE_VG_RecordStore.SaveState(context.m_VehicleData, "retrieved"))
            {
                // The write may have partially published: leave the claim for
                // operator review instead of automatically making it available.
                if (context.m_LoadedEntity)
                    g_Game.ObjectDelete(context.m_LoadedEntity);
                SetBusy(context.m_BusyKey,false);
                SendCallback(context.m_Player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"Retrieval commit failed. Payload retained for administrator recovery.");
                return;
            }
            if (context.m_GarageData)
            {
                context.m_GarageData.RemoveVehicle(context.m_VehicleData);
                // Keep old owner indexes read-only; the per-vehicle tombstone
                // masks any legacy entry without another aggregate write.
            }
        }
        else if (context.m_GarageData)
        {
            if (!BDE_VG_GarageData.Load(context.m_GarageData.GetFileName(),context.m_GarageData.m_IsMissionIndex,context.m_GarageData.m_IsCrossMapIndex))
            {
                FailRestoreContext(context,"Garage index became unreadable during retrieval; the vehicle remains in storage.");
                return;
            }
            context.m_GarageData.RemoveVehicle(context.m_VehicleData);
            if (!context.m_GarageData.Save())
            {
                context.m_GarageData.AddVehicle(context.m_VehicleData);
                FailRestoreContext(context,"Garage index could not be saved; the vehicle remains safely in storage.");
                return;
            }
        }

        if (context.m_LegacyMetadata)
            context.m_LegacyMetadata.DeleteMetadata();

        context.m_VehicleData.DeleteStorageFiles();
        if (FileExist(context.m_VehicleData.GetEntityStorageFileName()))
            BDE_VG_Logger.Log("WARNING: Retrieved vehicle but its entity-storage payload still exists: " + context.m_VehicleData.GetEntityStorageFileName(),true);

        if (context.m_LoadedEntity.GetInventory())
            context.m_LoadedEntity.GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);
        FreezeRestoredVehicle(context.m_LoadedEntity,false);

        if (context.m_CreateVehicleMarker)
            BDE_VL_MarkerManager.SendMarker(context.m_Player.GetIdentity(),context.m_LoadedEntity.GetPosition());
        BDE_VG_Logger.Log(context.m_Player.GetIdentity().GetName() + " retrieved " + context.m_VehicleData.m_ClassName + " key=" + context.m_VehicleData.GetStorageKey() + " " + context.m_LocationText,true);
        SendCallback(context.m_Player.GetIdentity(),BDE_VG_CALLBACK_RETRIEVED,true,context.m_VehicleData.m_DisplayName + " was retrieved " + context.m_LocationText + ".");
        SetBusy(context.m_BusyKey,false);
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SendVehiclesAfterOperation,250,false,context.m_Player,context.m_AccessObject);
    }

    protected void FailRestoreContext(BDE_VG_RestoreContext context, string message)
    {
        if (!context)
            return;
        // RestoreFromFile(deleteRestored=false) is deliberately non-destructive.
        // Roll back the temporary world entity if any finalization step fails.
        if (context.m_LoadedEntity)
            g_Game.ObjectDelete(context.m_LoadedEntity);
        if (BDE_VG_RecordStore.IsProfile(context.m_VehicleData))
            BDE_VG_RecordStore.SaveState(context.m_VehicleData, "stored");
        SetBusy(context.m_BusyKey,false);
        if (context.m_Player && context.m_Player.GetIdentity())
            SendCallback(context.m_Player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,message);
    }

    void CancelExclusiveRequest(string token)
    {
        auto rpc = Expansion_CreateRPC("RPC_CancelExclusive");
        rpc.Write(token);
        rpc.Expansion_Send(true);
    }

    protected void RPC_CancelExclusive(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        string token;
        if (!identity || !ctx.Read(token) || !m_ExclusivePending)
            return;
        BDE_VG_ExclusiveConsent pending = m_ExclusivePending.Get(identity.GetId());
        if (pending && pending.Token == token)
            m_ExclusivePending.Remove(identity.GetId());
    }

    protected void SendExclusivePrompt(PlayerBase player, EntityAI access, EntityAI vehicle, bool repair, string signature, array<string> rows)
    {
        array<string> pendingOwners = m_ExclusivePending.GetKeyArray();
        foreach (string pendingOwner : pendingOwners)
        {
            BDE_VG_ExclusiveConsent expired = m_ExclusivePending.Get(pendingOwner);
            if (!expired || expired.Expires < g_Game.GetTime())
                m_ExclusivePending.Remove(pendingOwner);
        }
        ref BDE_VG_ExclusiveConsent pending = new BDE_VG_ExclusiveConsent;
        int uuid[4];
        UUIDApi.Generate(uuid);
        pending.Token = UUIDApi.FormatString(uuid);
        pending.Vehicle = vehicle;
        pending.Access = access;
        pending.Repair = repair;
        pending.Signature = signature;
        pending.Expires = g_Game.GetTime() + 120000;
        m_ExclusivePending.Set(player.GetIdentity().GetId(),pending);
        int accessLow, accessHigh, vehicleLow, vehicleHigh;
        access.GetNetworkID(accessLow,accessHigh);
        vehicle.GetNetworkID(vehicleLow,vehicleHigh);
        auto rpc = Expansion_CreateRPC("RPC_ExclusivePrompt");
        rpc.Write(pending.Token);
        rpc.Write(accessLow);
        rpc.Write(accessHigh);
        rpc.Write(vehicleLow);
        rpc.Write(vehicleHigh);
        rpc.Write(repair);
        rpc.Write(rows);
        rpc.Expansion_Send(true,player.GetIdentity());
    }

    protected void RPC_ExclusivePrompt(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        ref BDE_VG_ExclusivePromptData prompt = new BDE_VG_ExclusivePromptData;
        if (!ctx.Read(prompt.Token) || !ctx.Read(prompt.AccessLow) || !ctx.Read(prompt.AccessHigh) || !ctx.Read(prompt.VehicleLow) || !ctx.Read(prompt.VehicleHigh) || !ctx.Read(prompt.Repair) || !ctx.Read(prompt.Rows))
            return;
        GetExclusivePromptInvoker().Invoke(prompt);
    }

    ScriptInvoker GetExclusivePromptInvoker()
    {
        if (!m_ExclusivePromptInvoker)
            m_ExclusivePromptInvoker = new ScriptInvoker;
        return m_ExclusivePromptInvoker;
    }

    protected void SendAlternatePrompt(PlayerIdentity identity, BDE_VG_VehicleData vehicleData, vector alternativePosition, Object blockingObject)
    {
        string blockingObjectName = "another object";
        if (blockingObject)
        {
            blockingObjectName = blockingObject.GetDisplayName();
            if (blockingObjectName == string.Empty)
                blockingObjectName = blockingObject.GetType();
        }
        auto rpc = Expansion_CreateRPC("RPC_AlternatePrompt");
        rpc.Write(vehicleData.GetStorageKey());
        rpc.Write(blockingObjectName);
        rpc.Write(vehicleData.m_Position);
        rpc.Write(alternativePosition);
        rpc.Write(vector.Distance(vehicleData.m_Position,alternativePosition));
        rpc.Expansion_Send(true,identity);
    }

    protected void RPC_AlternatePrompt(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        BDE_VG_AlternatePromptData promptData = new BDE_VG_AlternatePromptData;
        if (!ctx.Read(promptData.m_StorageKey))
            return;
        if (!ctx.Read(promptData.m_BlockingObjectName))
            return;
        if (!ctx.Read(promptData.m_OriginalPosition))
            return;
        if (!ctx.Read(promptData.m_AlternativePosition))
            return;
        if (!ctx.Read(promptData.m_AlternativeDistanceMeters))
            return;
        if (m_AlternatePromptInvoker)
            m_AlternatePromptInvoker.Invoke(promptData);
    }

    protected void SendCallback(PlayerIdentity identity, int callback, bool success, string message)
    {
        if (!identity)
            return;
        auto rpc = Expansion_CreateRPC("RPC_Callback");
        rpc.Write(callback);
        rpc.Write(success);
        rpc.Write(message);
        rpc.Expansion_Send(true,identity);
    }

    protected void RPC_Callback(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        int callback;
        bool success;
        string message;
        if (!ctx.Read(callback))
            return;
        if (!ctx.Read(success))
            return;
        if (!ctx.Read(message))
            return;
        if (m_CallbackInvoker)
            m_CallbackInvoker.Invoke(callback,success,message);
    }



    protected int GetPlayerATMBalance(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
            return 0;
        ExpansionMarketModule marketModule;
        if (!CF_Modules<ExpansionMarketModule>.Get(marketModule))
            return 0;
        ExpansionMarketATM_Data atmData = marketModule.GetPlayerATMData(player.GetIdentity().GetId());
        if (!atmData)
            return 0;
        return atmData.GetMoney();
    }

    protected ExpansionMarketItem ResolveVehicleMarketItem(string className, out string resolvedClassName)
    {
        resolvedClassName = className;
        resolvedClassName.ToLower();

        //! ExpansionMarketItem::SanityCheckAndRepair lowercases every configured
        //! classname before inserting it into ExpansionMarketCategory's global map.
        //! Vehicle GetType() preserves config casing, so the lookup must be normalized.
        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(resolvedClassName);
        if (marketItem)
            return marketItem;

        //! Match Expansion Market's own skinned-item fallback. Some purchasable vehicle
        //! skins expose a skinBase rather than having a separate explicit item entry.
        string skinBase;
        string skinBasePath = "CfgVehicles " + className + " skinBase";
        if (g_Game.ConfigIsExisting(skinBasePath))
        {
            g_Game.ConfigGetText(skinBasePath,skinBase);
            skinBase.ToLower();
            marketItem = ExpansionMarketCategory.GetGlobalItem(skinBase);
            if (marketItem)
            {
                resolvedClassName = skinBase;
                return marketItem;
            }
        }

        BDE_VG_Logger.Log("No Expansion Market item for vehicle class=" + className + " normalized=" + resolvedClassName,false);
        return null;
    }

    protected bool GetVehicleMarketValue(string className, vector position, out int marketValue)
    {
        marketValue = 0;
        string resolvedClassName;
        ExpansionMarketItem marketItem = ResolveVehicleMarketItem(className,resolvedClassName);
        if (!marketItem)
            return false;

        ExpansionMarketTraderZone zone = GetExpansionSettings().GetMarket().GetTraderZoneByPosition(position);
        if (zone && zone.ItemExists(marketItem.ClassName))
        {
            int stock = marketItem.MaxStockThreshold;
            if (!marketItem.IsStaticStock())
                stock = zone.GetStock(marketItem.ClassName);
            marketValue = marketItem.CalculatePrice(stock,1.0,true);
        }
        else
        {
            //! Repair can be requested away from a trader. In that case there is no
            //! meaningful live zone stock to quote, so use the configured maximum
            //! purchase threshold from the resolved Expansion Market item.
            marketValue = marketItem.MaxPriceThreshold;
            if (marketValue <= 0)
                marketValue = marketItem.MinPriceThreshold;
        }

        BDE_VG_Logger.Log("Expansion Market value " + className + " -> " + resolvedClassName + " = " + marketValue.ToString(),false);
        return marketValue > 0;
    }

    protected string GetRepairValidationError(PlayerBase player, EntityAI accessObject, EntityAI vehicleEntity, out int repairCost, out int marketValue, out ExpansionMarketATM_Data atmData)
    {
        repairCost = 0;
        marketValue = 0;
        atmData = null;
        if (!player || !player.GetIdentity() || !vehicleEntity || !accessObject)
            return "The repair request is missing required vehicle or player data.";

        float healthPercent = vehicleEntity.GetHealth01("","Health") * 100.0;
        if (healthPercent < 10.0)
            return "Health is below the 10% repair limit.";
        if (healthPercent >= 99.5)
            return "Vehicle health is already 100%.";
        if (!GetVehicleMarketValue(vehicleEntity.GetType(),accessObject.GetPosition(),marketValue))
            return "No Expansion Market value exists for this vehicle.";
        if (!GetExpansionSettings().GetMarket().ATMSystemEnabled)
            return "Vehicle repair requires the Expansion ATM system to be enabled.";

        float damageFraction = 1.0 - vehicleEntity.GetHealth01("","Health");
        repairCost = Math.Round(marketValue * 0.50 * damageFraction);
        if (repairCost < 1)
            repairCost = 1;

        ExpansionMarketModule marketModule;
        if (!CF_Modules<ExpansionMarketModule>.Get(marketModule))
            return "The Expansion Market module is unavailable.";
        atmData = marketModule.GetPlayerATMData(player.GetIdentity().GetId());
        if (!atmData)
            return "Expansion ATM account unavailable.";
        if (atmData.GetMoney() < repairCost)
            return "Insufficient ATM funds.";
        return string.Empty;
    }

    protected void PopulateRepairData(PlayerBase player, EntityAI accessObject, EntityAI vehicleEntity, BDE_VG_VehicleData data)
    {
        if (!data)
            return;
        int repairCost;
        int marketValue;
        ExpansionMarketATM_Data atmData;
        string error = GetRepairValidationError(player,accessObject,vehicleEntity,repairCost,marketValue,atmData);
        data.m_RepairCost = repairCost;
        data.m_MarketValue = marketValue;
        data.m_RepairAvailable = error == string.Empty;
        data.m_RepairUnavailableReason = error;
    }

    protected void RepairVehicleHealth(EntityAI vehicleEntity)
    {
        if (!vehicleEntity)
            return;
        TStringArray damageZones = new TStringArray;
        vehicleEntity.GetDamageZones(damageZones);
        foreach (string damageZone : damageZones)
            vehicleEntity.SetHealthMax(damageZone,"Health");
        vehicleEntity.SetHealthMax("","Health");
        vehicleEntity.SetSynchDirty();
    }

    protected array<ref BDE_VG_VehicleData> GetWorldVehicles(PlayerBase player, EntityAI accessObject)
    {
        array<ref BDE_VG_VehicleData> vehicles = new array<ref BDE_VG_VehicleData>;
        if (!player || !accessObject)
            return vehicles;

        float range = BDE_VG_Settings.Get().VehicleStoreRangeMeters;
        float rangeSq = range * range;
        vector accessPosition = accessObject.GetPosition();
        array<Object> nearbyObjects = new array<Object>;
        array<CargoBase> proxyCargos = new array<CargoBase>;
        array<EntityAI> processedVehicles = new array<EntityAI>;
        g_Game.GetObjectsAtPosition3D(accessPosition,range,nearbyObjects,proxyCargos);

        foreach (Object nearbyObject : nearbyObjects)
        {
            EntityAI candidateEntity = EntityAI.Cast(nearbyObject);
            if (!candidateEntity)
                continue;

            EntityAI rootEntity = candidateEntity.GetHierarchyRoot();
            if (!rootEntity)
                rootEntity = candidateEntity;

            ExpansionVehicle vehicle;
            if (!ExpansionVehicle.Get(vehicle,rootEntity))
                continue;

            EntityAI vehicleEntity = vehicle.GetEntity();
            if (!vehicleEntity || processedVehicles.Find(vehicleEntity) > -1)
                continue;
            processedVehicles.Insert(vehicleEntity);

            vector vehiclePosition = vehicleEntity.GetPosition();
            float distanceSq = vector.DistanceSq(accessPosition,vehiclePosition);
            if (distanceSq > rangeSq)
                continue;
            if (!vehicle.BDE_VL_HasOwner() || (IsTraderAccess(accessObject) && !CanAccessLiveVehicle(player,vehicle)))
                continue;

            BDE_VG_VehicleData data = new BDE_VG_VehicleData;
            data.m_OwnerUID = vehicle.GetOwnerUID();
            data.m_OwnerName = vehicle.GetOwnerName();
            data.m_ClassName = vehicle.GetType();
            data.m_DisplayName = vehicleEntity.GetDisplayName();
            if (data.m_DisplayName == string.Empty)
                data.m_DisplayName = ExpansionStatic.GetItemDisplayNameWithType(data.m_ClassName);
            data.m_Position = vehiclePosition;
            data.m_Orientation = vehicleEntity.GetOrientation();
            data.m_SkinIndex = vehicle.GetCurrentSkinIndex();
            vehicleEntity.GetNetworkID(data.m_NetworkIDLow,data.m_NetworkIDHigh);
            data.m_VehicleObject = vehicleEntity;
            data.m_HealthPercent = vehicleEntity.GetHealth01("","Health") * 100.0;
            BDE_VG_Utility.CaptureVehicleInventory(vehicleEntity,data.m_PreviewAttachments,data.m_CargoEntries);
            data.m_CargoItemCount = BDE_VG_Utility.CountCargoEntries(data.m_CargoEntries);
            data.m_InventorySnapshotVersion = 1;
            PopulateRepairData(player,accessObject,vehicleEntity,data);
            string statusText = GetStoreValidationError(player,accessObject,vehicle);
            if (statusText == string.Empty)
                statusText = "Ready to store";
            data.PrepareForClient(false,string.Empty,statusText,vector.Distance(accessPosition,vehiclePosition));
            vehicles.Insert(data);
        }

        return vehicles;
    }

    protected array<ref BDE_VG_VehicleData> GetStoredVehicles(PlayerBase player, EntityAI accessObject)
    {
        array<ref BDE_VG_VehicleData> vehicles = new array<ref BDE_VG_VehicleData>;
        ref array<string> visitedKeys = new array<string>;
        foreach (BDE_VG_GarageData garageData : m_Garages)
        {
            if (!garageData)
                continue;
            foreach (BDE_VG_VehicleData vehicleData : garageData.m_Vehicles)
            {
                if (!vehicleData || !FileExist(vehicleData.GetEntityStorageFileName()) || !BDE_VG_RecordStore.IsAvailable(vehicleData))
                    continue;
                string key = vehicleData.GetStorageKey();
                if (visitedKeys.Find(key) > -1 || !CanViewStoredVehicle(player,accessObject,vehicleData))
                    continue;
                visitedKeys.Insert(key);
                string storedStatus = "Stored by " + vehicleData.GetStoredByName();
                vehicleData.PrepareForClient(true,key,storedStatus,0.0);
                vehicles.Insert(vehicleData);
            }
        }
        array<ref BDE_VG_LegacyMetadata> legacyEntries = BDE_VG_LegacyMetadata.LoadAll();
        foreach (BDE_VG_LegacyMetadata legacyMetadata : legacyEntries)
        {
            if (!legacyMetadata || visitedKeys.Find(legacyMetadata.StorageID) > -1)
                continue;
            BDE_VG_VehicleData legacyData = legacyMetadata.ToVehicleData();
            legacyData.m_WorldName.ToLower();
            if (!CanViewStoredVehicle(player,accessObject,legacyData))
                continue;
            legacyData.PrepareForClient(true,legacyMetadata.StorageID,"Stored by " + legacyData.GetStoredByName(),0.0);
            vehicles.Insert(legacyData);
        }
        return vehicles;
    }

    // Trader zones share storage by stable zone filename, base terminals by persistent ID.
    // The scope always embeds world name to avoid same-ID collisions on other maps.
    protected bool IsTraderAccess(EntityAI accessObject)
    {
        return accessObject && BDE_VG_TraderZones.GetLocalTraderZoneByPosition(accessObject.GetPosition()) != null;
    }

    protected string GetStorageScopeID(EntityAI accessObject)
    {
        if (!accessObject)
            return string.Empty;
        string worldName = BDE_VG_Utility.GetWorldNameLower();
        ExpansionMarketTraderZone zone = BDE_VG_TraderZones.GetLocalTraderZoneByPosition(accessObject.GetPosition());
        if (zone)
            return "TRADER|" + worldName + "|" + zone.m_FileName;

        int b1, b2, b3, b4;
        accessObject.GetPersistentID(b1,b2,b3,b4);
        string terminalID;
        if (b1 || b2 || b3 || b4)
            terminalID = ExpansionStatic.GetPersistentIDString(b1,b2,b3,b4);
        else
            terminalID = accessObject.GetType() + "_" + accessObject.GetPosition().ToString();
        return "TERMINAL|" + worldName + "|" + terminalID;
    }

    protected bool CanViewStoredVehicle(PlayerBase player, EntityAI accessObject, BDE_VG_VehicleData vehicleData)
    {
        if (!player || !player.GetIdentity() || !accessObject || !vehicleData)
            return false;

        if (!BDE_VG_RecordStore.IsAvailable(vehicleData))
            return false;
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        string playerUID = player.GetIdentity().GetId();
        string currentWorld = BDE_VG_Utility.GetWorldNameLower();
        bool sameWorld = vehicleData.m_WorldName == currentWorld;
        bool isTraderTerminal = IsTraderAccess(accessObject);

        // A portable entry lives in the Hive directory and is visible ONLY to
        // its original depositor, never to team mates or the claimed owner.
        // On a DIFFERENT map, the player can use a trader terminal regardless
        // of AllowCrossWorldRetrieval. On the SAME map, other-terminal sharing
        // still respects that setting (the originating trader zone always works).
        bool portableEntry = vehicleData.m_StorageBackend == "profile-crossmap";
        if (portableEntry && !vehicleData.m_StoreForDifferentMap)
            return false;
        if (portableEntry)
        {
            if (vehicleData.m_StoredByUID == string.Empty || vehicleData.m_StoredByUID != playerUID)
                return false;
            if (!sameWorld)
                return isTraderTerminal;
            if (isTraderTerminal && vehicleData.m_StorageScopeID == GetStorageScopeID(accessObject))
                return true;
            return settings.AllowCrossWorldRetrieval;
        }

        // Compatibility with any v2.5 records flagged for transfer but still
        // physically stored locally: keep storer-only on the source map, but
        // DO NOT misrepresent them as portable until they are re-stored into
        // _CrossMapVehicleData. All other records are local to their world.
        if (!sameWorld)
            return false;
        bool storerOnlyLocal = vehicleData.m_StoreForDifferentMap;
        if (storerOnlyLocal && (vehicleData.m_StoredByUID == string.Empty || vehicleData.m_StoredByUID != playerUID))
            return false;

        string scopeID = GetStorageScopeID(accessObject);
        bool sameTerminal = !isTraderTerminal && vehicleData.m_StorageScopeID != string.Empty && vehicleData.m_StorageScopeID == scopeID;
        if (sameTerminal)
            return true; // Locally deposited base vehicles can be retrieved by anyone.

        bool sameTrader = isTraderTerminal && (vehicleData.m_StorageScopeID == scopeID || vehicleData.m_StorageScopeID == string.Empty);
        bool ownerOrGroup = CanAccessOwnerUID(player,vehicleData.m_OwnerUID);
        if (storerOnlyLocal)
            ownerOrGroup = true;
        if (sameTrader && ownerOrGroup)
            return true;

        // Legacy name retained for settings compatibility: this switch ONLY
        // enables cross-terminal / trader-to-base sharing on the SAME map.
        // It never grants cross-map access to a local vehicle.
        if (settings.AllowCrossWorldRetrieval && ownerOrGroup)
            return true;
        return false;
    }

    protected int CountOwnerStoredVehicles(string ownerUID, bool traderGarage)
    {
        int count;
        ref array<string> countedKeys = new array<string>;
        foreach (BDE_VG_GarageData garage : m_Garages)
        {
            if (!garage || garage.m_OwnerUID != ownerUID)
                continue;
            foreach (BDE_VG_VehicleData data : garage.m_Vehicles)
            {
                if (!data || !FileExist(data.GetEntityStorageFileName()) || !BDE_VG_RecordStore.IsAvailable(data))
                    continue;
                bool storedAtBase = data.m_StorageScopeID.IndexOf("TERMINAL|") == 0;
                if (traderGarage == storedAtBase)
                    continue;
                string key = data.GetStorageKey();
                if (countedKeys.Find(key) > -1)
                    continue;
                countedKeys.Insert(key);
                count++;
            }
        }
        if (traderGarage)
        {
            array<ref BDE_VG_LegacyMetadata> legacyEntries = BDE_VG_LegacyMetadata.LoadAll();
            foreach (BDE_VG_LegacyMetadata meta : legacyEntries)
            {
                if (meta && meta.OwnerUID == ownerUID && countedKeys.Find(meta.StorageID) == -1)
                {
                    countedKeys.Insert(meta.StorageID);
                    count++;
                }
            }
        }
        return count;
    }

    protected BDE_VG_GarageData GetOrCreateGarage(string ownerUID, string ownerName, bool crossMap)
    {
        BDE_VG_GarageData garageData = GetGarage(ownerUID,crossMap);
        if (garageData)
        {
            garageData.m_OwnerName = ownerName;
            return garageData;
        }
        // Do not create a blank index over an existing Hive file.
        string existingIndex = s_GarageFolderPath + ownerUID + ".json";
        if (crossMap)
            existingIndex = BDE_VG_CROSSMAP_DATA_DIRECTORY + ownerUID + ".json";
        if (FileExist(existingIndex))
        {
            garageData = BDE_VG_GarageData.Load(existingIndex,false,crossMap);
            if (garageData)
                m_Garages.Insert(garageData);
            return garageData;
        }
        garageData = new BDE_VG_GarageData;
        garageData.m_OwnerUID = ownerUID;
        garageData.m_OwnerName = ownerName;
        garageData.m_IsCrossMapIndex = crossMap;
        m_Garages.Insert(garageData);
        return garageData;
    }

    protected BDE_VG_GarageData GetGarage(string ownerUID, bool crossMap)
    {
        foreach (BDE_VG_GarageData garageData : m_Garages)
        {
            if (garageData && !garageData.m_IsMissionIndex && garageData.m_IsCrossMapIndex == crossMap && garageData.m_OwnerUID == ownerUID)
                return garageData;
        }
        return null;
    }

    protected BDE_VG_VehicleData FindStoredVehicle(string storageKey, out BDE_VG_GarageData garageData)
    {
        garageData = null;
        foreach (BDE_VG_GarageData currentGarage : m_Garages)
        {
            if (!currentGarage)
                continue;
            foreach (BDE_VG_VehicleData vehicleData : currentGarage.m_Vehicles)
            {
                if (vehicleData && vehicleData.GetStorageKey() == storageKey)
                {
                    garageData = currentGarage;
                    return vehicleData;
                }
            }
        }
        return null;
    }

    protected bool CanAccessLiveVehicle(PlayerBase player, ExpansionVehicle vehicle)
    {
        if (!player || !player.GetIdentity() || !vehicle || !vehicle.BDE_VL_HasOwner())
            return false;
#ifdef JM_COT
        if (GetCommunityOnlineToolsBase().IsActive(player))
            return true;
#endif
        if (player.GetIdentity().GetId() == vehicle.GetOwnerUID())
            return true;
        if (!BDE_VG_Settings.Get().AllowGroupAccess)
            return false;
        return vehicle.BDE_VL_CanAccess(player);
    }

    protected bool CanAccessOwnerUID(PlayerBase player, string ownerUID)
    {
        if (!player || !player.GetIdentity() || ownerUID == string.Empty)
            return false;
#ifdef JM_COT
        if (GetCommunityOnlineToolsBase().IsActive(player))
            return true;
#endif
        string playerUID = player.GetIdentity().GetId();
        if (playerUID == ownerUID)
            return true;
        if (!BDE_VG_Settings.Get().AllowGroupAccess || playerUID == string.Empty)
            return false;

        ExpansionPartyModule partyModule;
        if (!CF_Modules<ExpansionPartyModule>.Get(partyModule) || !partyModule)
            return false;

        // Force lookup through Expansion's persisted party array so this works for
        // offline owners and during PlayerBase/party initialization races. Comparing
        // party IDs avoids dereferencing PartyData::GetPlayers(), which can be NULL
        // during lifecycle transitions and was the source of the reported VM exception.
        ExpansionPartyPlayerData ownerData = partyModule.GetPartyPlayerData(ownerUID, true);
        ExpansionPartyPlayerData playerData = partyModule.GetPartyPlayerData(playerUID, true);
        if (!ownerData || !playerData)
            return false;

        ExpansionPartyData ownerParty = ownerData.GetParty();
        ExpansionPartyData playerParty = playerData.GetParty();
        if (!ownerParty || !playerParty)
            return false;

        int ownerPartyID = ownerParty.GetPartyID();
        int playerPartyID = playerParty.GetPartyID();
        return ownerPartyID >= 0 && ownerPartyID == playerPartyID;
    }

    protected bool ValidateAccessObject(PlayerBase player, EntityAI accessObject, bool sendError)
    {
        if (!player || !player.GetIdentity() || !accessObject)
            return false;
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        if (!settings || !settings.Enabled || !settings.IsGarageAccessObject(accessObject.GetType()))
        {
            if (sendError)
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"This object is not configured as a BDE garage access object.");
            return false;
        }
        if (vector.Distance(player.GetPosition(),accessObject.GetPosition()) > BDE_VG_ACCESS_ACTION_DISTANCE + 1.0)
        {
            if (sendError)
                SendCallback(player.GetIdentity(),BDE_VG_CALLBACK_ERROR,false,"You are too far away from the garage access object.");
            return false;
        }
        return true;
    }

    protected EntityAI ResolveAccessObject(int networkIDLow, int networkIDHigh)
    {
        return EntityAI.Cast(g_Game.GetObjectByNetworkId(networkIDLow,networkIDHigh));
    }

    protected void SendVehiclesAfterOperation(PlayerBase player, EntityAI accessObject)
    {
        if (ValidateAccessObject(player,accessObject,false))
            SendVehicles(player,accessObject);
    }

    ScriptInvoker GetMenuInvoker()
    {
        if (!m_MenuInvoker)
            m_MenuInvoker = new ScriptInvoker;
        return m_MenuInvoker;
    }

    ScriptInvoker GetCallbackInvoker()
    {
        if (!m_CallbackInvoker)
            m_CallbackInvoker = new ScriptInvoker;
        return m_CallbackInvoker;
    }

    ScriptInvoker GetAlternatePromptInvoker()
    {
        if (!m_AlternatePromptInvoker)
            m_AlternatePromptInvoker = new ScriptInvoker;
        return m_AlternatePromptInvoker;
    }


    protected bool IsBusy(string key)
    {
        if (!m_BusyKeys)
            m_BusyKeys = new array<string>;
        return m_BusyKeys.Find(key) > -1;
    }

    protected void SetBusy(string key, bool state)
    {
        if (!m_BusyKeys)
            m_BusyKeys = new array<string>;
        int index = m_BusyKeys.Find(key);
        if (state && index == -1)
            m_BusyKeys.Insert(key);
        if (!state && index > -1)
            m_BusyKeys.Remove(index);
    }
};
