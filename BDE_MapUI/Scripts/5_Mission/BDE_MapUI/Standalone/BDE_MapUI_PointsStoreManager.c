class BDE_MapUI_PointsStoreManager
{
    private static ref BDE_MapUI_PointsStoreManager s_Instance;

    private ref BDE_MapUI_PointsStoreConfig m_Config;
    private ref map<string, ref BDE_MapUI_PointPlayerData> m_PlayerData;
    private ref map<string, float> m_OnlineTimers;
    private ref map<string, bool> m_BDE_FailedPlayerLoads = new map<string, bool>();
    private ref map<string, vector> m_TravelPositions = new map<string, vector>();
    private ref map<string, float> m_TravelRemainders = new map<string, float>();
    private ref map<string, PlayerBase> m_TravelPlayers = new map<string, PlayerBase>();
    private ref map<string, bool> m_EmptyAccountsRestarted = new map<string, bool>();
    private ref map<string, bool> m_DirtyPlayers = new map<string, bool>();
    private float m_ConfigReloadTimer;
    private float m_OnlineProgressUpdateAccumulator;
    private const float CONFIG_RELOAD_INTERVAL_SECONDS = 30.0;
    private const float ONLINE_PROGRESS_UPDATE_INTERVAL_SECONDS = 1.0;

    static BDE_MapUI_PointsStoreManager Get()
    {
        if (!s_Instance)
            s_Instance = new BDE_MapUI_PointsStoreManager();
        return s_Instance;
    }

    void BDE_MapUI_PointsStoreManager()
    {
        m_PlayerData = new map<string, ref BDE_MapUI_PointPlayerData>();
        m_OnlineTimers = new map<string, float>();
        m_ConfigReloadTimer = 0.0;
        m_OnlineProgressUpdateAccumulator = 0.0;
        EnsureDirectories();
        LoadConfig();
        // Load each account on demand; offline cache must not hide Hive updates.

    }

    void OnServerUpdate(float timeslice)
    {
        ProcessAnimalKillQueue();

        // Online-task progress is measured in minutes, so enumerating every connected
        // player every server frame is unnecessary. Accumulate the real elapsed time
        // and process it once per second instead.
        m_OnlineProgressUpdateAccumulator += timeslice;
        if (m_OnlineProgressUpdateAccumulator >= ONLINE_PROGRESS_UPDATE_INTERVAL_SECONDS)
        {
            UpdateOnlineTaskProgress(m_OnlineProgressUpdateAccumulator);
            m_OnlineProgressUpdateAccumulator = 0.0;
        }

        m_ConfigReloadTimer += timeslice;
        if (m_ConfigReloadTimer >= CONFIG_RELOAD_INTERVAL_SECONDS)
        {
            m_ConfigReloadTimer = 0.0;
            LoadConfig();
        }


    }

    void OnPlayerConnected(PlayerBase player, PlayerIdentity identity)
    {
        if (!identity)
            return;

        ResetTravelSample(identity.GetId());
        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(identity.GetId(), identity.GetName());
        ApplyCycleResets(data);
        MarkPlayerDirty(data);
        Print("[BDE_MapUI points] Connected " + data.UID + " points=" + data.Points.ToString() + " tasks=" + data.TaskStates.Count().ToString());
        if (m_EmptyAccountsRestarted.Contains(data.UID))
        {
            NotifyPlayer(data.UID, "Points save repaired", "Your old save was empty. New task progress can now be saved; contact an administrator about lost points.");
            m_EmptyAccountsRestarted.Remove(data.UID);
        }
        if (m_BDE_FailedPlayerLoads.Contains(data.UID))
        {
            NotifyPlayer(data.UID, "Points Store unavailable", "Your points save could not be read. Contact an administrator; the file has been preserved.");
        }
    }

    void OnPlayerDisconnected(PlayerBase player, PlayerIdentity identity)
    {
        if (!identity)
            return;

        string uid = identity.GetId();
        ResetTravelSample(uid);
        if (m_OnlineTimers && m_OnlineTimers.Contains(uid))
            m_OnlineTimers.Remove(uid);

        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(uid, identity.GetName());
        ApplyCycleResets(data);
        MarkPlayerDirty(data);
        if (FlushPlayer(data))
        {
            // Reconnect must read the latest synced file, not this session's cache.
            m_PlayerData.Remove(uid);
            m_BDE_FailedPlayerLoads.Remove(uid);
        }
    }

    BDE_MapUI_PointsStoreSyncData BuildSyncData(PlayerIdentity identity)
    {
        BDE_MapUI_PointsStoreSyncData sync = new BDE_MapUI_PointsStoreSyncData();
        sync.Config = m_Config;
        sync.PlayerData = new BDE_MapUI_PointPlayerData();
        sync.IsVIP = false;
        sync.VIPMultiplier = 1.0;

        if (!identity)
            return sync;

        BDE_MapUI_PointPlayerData source = GetOrCreatePlayer(identity.GetId(), identity.GetName());
        ApplyCycleResets(source);

        BDE_MapUI_PointPlayerData copy = new BDE_MapUI_PointPlayerData();
        copy.UID = source.UID;
        copy.Name = source.Name;
        copy.Points = source.Points;
        copy.LastDailyKey = source.LastDailyKey;
        copy.LastWeeklyKey = source.LastWeeklyKey;
        copy.HasStatsBaseline = source.HasStatsBaseline;
        copy.TrackedInfectedKilled = source.TrackedInfectedKilled;
        copy.TrackedDistanceTravelled = source.TrackedDistanceTravelled;
        copy.TrackedTimeSurvived = source.TrackedTimeSurvived;

        for (int i = 0; i < source.TaskStates.Count(); i++)
        {
            BDE_MapUI_PointTaskState sourceState = source.TaskStates.Get(i);
            if (!sourceState)
                continue;

            BDE_MapUI_PointTaskState state = new BDE_MapUI_PointTaskState();
            state.TaskID = sourceState.TaskID;
            state.Progress = sourceState.Progress;
            state.Completed = sourceState.Completed;
            state.Paid = sourceState.Paid;
            copy.TaskStates.Insert(state);
        }

        sync.PlayerData = copy;
        sync.IsVIP = IsVIP(identity.GetId());
        sync.VIPMultiplier = GetVIPMultiplier(identity.GetId());
        return sync;
    }

    bool PurchaseItem(PlayerIdentity identity, PlayerBase player, string itemID, out string message)
    {
        message = "";
        if (!identity)
        {
            message = "No player identity.";
            return false;
        }
        if (!player)
        {
            message = "Player not found.";
            return false;
        }

        BDE_MapUI_PointStoreItem item = FindStoreItem(itemID);
        if (!item)
        {
            message = "Store item not found.";
            return false;
        }

        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(identity.GetId(), identity.GetName());
        ApplyCycleResets(data);
        if (m_BDE_FailedPlayerLoads.Contains(data.UID))
        {
            message = "Your saved points could not be loaded. Contact an administrator; your file has been preserved.";
            return false;
        }
        if (data.Points < item.Price)
        {
            message = "Not enough points.";
            return false;
        }

        if (!SpawnStoreItem(player, item))
        {
            message = "Could not spawn item.";
            return false;
        }

        data.Points -= item.Price;
        MarkPlayerDirty(data);
        if (!FlushPlayer(data))
            Print("[BDE_MapUI points] Purchase save failed; current balance retained in memory for retry: " + data.UID);
        message = "Purchased " + item.Name + " for " + item.Price.ToString() + " points.";
        return true;
    }

    void AddManualPoints(PlayerBase player, int amount)
    {
        if (!player || !player.GetIdentity() || amount <= 0)
            return;

        AwardPoints(player.GetIdentity().GetId(), player.GetIdentity().GetName(), amount, "Admin/manual points");
    }

    void AddTaskProgress(PlayerBase player, string taskType, int amount)
    {
        if (!player || !player.GetIdentity())
            return;

        AddTaskProgressByUID(player.GetIdentity().GetId(), player.GetIdentity().GetName(), taskType, amount);
    }

    void AddTaskProgressByUID(string uid, string name, string taskType, int amount)
    {
        if (uid == "" || taskType == "" || amount <= 0)
            return;

        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(uid, name);
        if (m_BDE_FailedPlayerLoads.Contains(uid))
        {
            return;
        }
        ApplyCycleResets(data);
        bool changed = false;
        bool completedTask = false;

        for (int i = 0; i < m_Config.Tasks.Count(); i++)
        {
            BDE_MapUI_PointTask task = m_Config.Tasks.Get(i);
            if (!task || task.TaskType != taskType)
                continue;

            task.EnsureDefaults();
            BDE_MapUI_PointTaskState state = data.GetOrCreateTaskState(task.TaskID);
            if (state.Completed || state.Paid)
                continue;

            state.Progress += amount;
            if (state.Progress >= task.Target)
            {
                state.Progress = task.Target;
                state.Completed = true;
                completedTask = true;
                state.Paid = true;
                int awarded = CalculateAward(uid, task.Points);
                data.Points += awarded;
                NotifyPlayer(uid, "Points Store Task Complete", "You completed " + task.Name + " and earned " + awarded.ToString() + " points for the store.");
            }
            changed = true;
        }

        if (changed)
        {
            MarkPlayerDirty(data);
            // One commit includes every task completed by this progress event.
            if (completedTask && !FlushPlayer(data))
                Print("[BDE_MapUI points] Task completion save failed; rewards retained in memory for retry: " + data.UID);
        }
    }

    void SyncStatsFromPlayerDatabase(string uid, string name, int infectedKilled, float distanceTravelled, float timeSurvived)
    {
        if (uid == "")
            return;

        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(uid, name);
        ApplyCycleResets(data);

        if (!data.HasStatsBaseline)
        {
            data.TrackedInfectedKilled = infectedKilled;
            data.TrackedDistanceTravelled = distanceTravelled;
            data.TrackedTimeSurvived = timeSurvived;
            data.HasStatsBaseline = true;
            MarkPlayerDirty(data);
            return;
        }

        int infectedDelta = infectedKilled - data.TrackedInfectedKilled;
        int survivedDelta = Math.Floor(timeSurvived - data.TrackedTimeSurvived);

        if (infectedDelta > 0)
            AddTaskProgressByUID(uid, name, BDE_MapUI_POINTS_TASK_INFECTED, infectedDelta);
        // Traveller credit comes only from filtered server position samples.
        if (survivedDelta > 0)
            AddTaskProgressByUID(uid, name, BDE_MapUI_POINTS_TASK_SURVIVE, survivedDelta);

        data.TrackedInfectedKilled = infectedKilled;
        data.TrackedDistanceTravelled = distanceTravelled;
        data.TrackedTimeSurvived = timeSurvived;
        MarkPlayerDirty(data);
    }

    void OnPlayerKill(string killerUID, string killerName)
    {
        AddTaskProgressByUID(killerUID, killerName, BDE_MapUI_POINTS_TASK_PLAYERS, 1);
    }

    void OnAIKill(string killerUID, string killerName)
    {
        AddTaskProgressByUID(killerUID, killerName, BDE_MapUI_POINTS_TASK_AI, 1);
    }

    void OnAnimalKill(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
            return;

        OnAnimalKillByUID(player.GetIdentity().GetId(), player.GetIdentity().GetName(), "");
    }

    void OnAnimalKillByUID(string killerUID, string killerName, string animalClassName)
    {
        if (killerUID == "")
            return;

        AddTaskProgressByUID(killerUID, killerName, BDE_MapUI_POINTS_TASK_ANIMALS, 1);

        string specificTaskType = GetAnimalSpecificTaskType(animalClassName);
        if (specificTaskType != "")
            AddTaskProgressByUID(killerUID, killerName, specificTaskType, 1);
    }

    private void ProcessAnimalKillQueue()
    {
        ref array<ref BDE_MapUI_PointsAnimalKillEvent> events = BDE_MapUI_PointsAnimalKillQueue.Consume();
        if (!events)
            return;

        for (int i = 0; i < events.Count(); i++)
        {
            BDE_MapUI_PointsAnimalKillEvent eventData = events.Get(i);
            if (!eventData)
                continue;

            OnAnimalKillByUID(eventData.KillerUID, eventData.KillerName, eventData.AnimalClassName);
        }
    }

    private string GetAnimalSpecificTaskType(string animalClassName)
    {
        if (animalClassName == "")
            return "";

        if (IsWolfClass(animalClassName))
            return BDE_MapUI_POINTS_TASK_ANIMAL_WOLF;
        if (IsBearClass(animalClassName))
            return BDE_MapUI_POINTS_TASK_ANIMAL_BEAR;
        if (IsDeerClass(animalClassName))
            return BDE_MapUI_POINTS_TASK_ANIMAL_DEER;
        if (IsPigClass(animalClassName))
            return BDE_MapUI_POINTS_TASK_ANIMAL_PIG;
        if (IsFoxClass(animalClassName))
            return BDE_MapUI_POINTS_TASK_ANIMAL_FOX;

        return "";
    }

    private bool IsWolfClass(string animalClassName)
    {
        return animalClassName == "Animal_CanisLupus_Grey" || animalClassName == "Animal_CanisLupus_White" || animalClassName == "Zombie_Wolf1_Beld" || animalClassName == "Zombie_Wolf2_Beld" || animalClassName == "Zombie_Wolf3_Beld" || animalClassName == "Zombie_Wolf4_Beld" || animalClassName == "Zombie_Wolf5_Beld";
    }

    private bool IsBearClass(string animalClassName)
    {
        return animalClassName == "Animal_UrsusArctos" || animalClassName == "Animal_UrsusMaritimus";
    }

    private bool IsDeerClass(string animalClassName)
    {
        return animalClassName == "Animal_CervusElaphus" || animalClassName == "Animal_CervusElaphusF" || animalClassName == "Animal_CapreolusCapreolusF" || animalClassName == "Animal_CapreolusCapreolus";
    }

    private bool IsPigClass(string animalClassName)
    {
        return animalClassName == "Animal_SusScrofa" || animalClassName == "Animal_SusDomesticus";
    }

    private bool IsFoxClass(string animalClassName)
    {
        return animalClassName == "Animal_VulpesVulpes";
    }

    private void UpdateOnlineTaskProgress(float timeslice)
    {
        array<Man> players = new array<Man>();
        g_Game.GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase player = PlayerBase.Cast(players.Get(i));
            if (!player || !player.GetIdentity())
                continue;

            string uid = player.GetIdentity().GetId();
            UpdateTravelProgress(player, uid);
            float elapsed = 0.0;
            if (m_OnlineTimers.Contains(uid))
                elapsed = m_OnlineTimers.Get(uid);

            elapsed += timeslice;
            if (elapsed >= 60.0)
            {
                int minutes = Math.Floor(elapsed / 60.0);
                elapsed = elapsed - (minutes * 60.0);
                AddTaskProgressByUID(uid, player.GetIdentity().GetName(), BDE_MapUI_POINTS_TASK_ONLINE, minutes);
            }

            m_OnlineTimers.Set(uid, elapsed);
        }
    }

    private void ResetTravelSample(string uid)
    {
        m_TravelPositions.Remove(uid);
        m_TravelPlayers.Remove(uid);
        m_TravelRemainders.Remove(uid);
    }

    private void UpdateTravelProgress(PlayerBase player, string uid)
    {
        ExpansionVehicle vehicle;
        bool inHelicopter = ExpansionVehicle.Get(vehicle, player) && vehicle.IsHelicopter();
        if (!player.IsAlive() || inHelicopter)
        {
            ResetTravelSample(uid);
            return;
        }

        vector currentPosition = player.GetPosition();
        vector previousPosition;
        PlayerBase previousPlayer;
        bool sampled = m_TravelPositions.Find(uid, previousPosition);
        m_TravelPlayers.Find(uid, previousPlayer);
        m_TravelPositions.Set(uid, currentPosition);
        m_TravelPlayers.Set(uid, player);
        if (!sampled || previousPlayer != player)
        {
            m_TravelRemainders.Set(uid, 0.0);
            return;
        }

        float distance = vector.Distance(previousPosition, currentPosition);
        if (distance > 50.0)
        {
            // Advance the baseline even when rejecting a teleport.
            return;
        }

        float remainder;
        m_TravelRemainders.Find(uid, remainder);
        distance += remainder;
        int metres = Math.Floor(distance);
        m_TravelRemainders.Set(uid, distance - metres);
        if (metres > 0)
        {
            AddTaskProgressByUID(uid, player.GetIdentity().GetName(), BDE_MapUI_POINTS_TASK_DISTANCE, metres);
        }
    }

    private int CalculateAward(string uid, int basePoints)
    {
        if (basePoints <= 0)
            return 0;

        float multiplier = GetVIPMultiplier(uid);
        if (multiplier <= 1.0)
            return basePoints;

        int award = Math.Floor(basePoints * multiplier);
        if (award < basePoints)
            award = basePoints;
        return award;
    }

    // Integration hook. BDE_VIP_SYSTEM overrides this in a modded class so each
    // VIP tier can have its own Point Store multiplier. Standalone MapUI keeps
    // the legacy VIPSteamIDs + global VIPMultiplier behaviour.
    protected float GetVIPMultiplier(string uid)
    {
        if (!IsVIP(uid) || !m_Config)
            return 1.0;

        if (m_Config.VIPMultiplier < 1.0)
            return 1.0;
        return m_Config.VIPMultiplier;
    }

    private void AwardPoints(string uid, string name, int amount, string reason)
    {
        if (uid == "" || amount <= 0)
            return;

        BDE_MapUI_PointPlayerData data = GetOrCreatePlayer(uid, name);
        ApplyCycleResets(data);
        if (m_BDE_FailedPlayerLoads.Contains(uid))
        {
            return;
        }
        data.Points += amount;
        MarkPlayerDirty(data);
        NotifyPlayer(uid, "Points Added", "+" + amount.ToString() + " points. " + reason);
    }

    protected bool IsVIP(string uid)
    {
        if (uid == "" || !m_Config || !m_Config.VIPSteamIDs)
            return false;

        if (IsVIPValue(uid))
            return true;

        PlayerIdentity identity = GetOnlineIdentityByUID(uid);
        if (identity && IsVIPValue(identity.GetPlainId()))
            return true;

        return false;
    }

    private bool IsVIPValue(string id)
    {
        if (id == "" || !m_Config || !m_Config.VIPSteamIDs)
            return false;

        for (int i = 0; i < m_Config.VIPSteamIDs.Count(); i++)
        {
            if (m_Config.VIPSteamIDs.Get(i) == id)
                return true;
        }
        return false;
    }

    private void ApplyCycleResets(BDE_MapUI_PointPlayerData data)
    {
        if (!data)
            return;

        data.EnsureArrays();
        int dailyKey = GetDailyKey();
        int weeklyKey = GetWeeklyKey();

        if (data.LastDailyKey != dailyKey)
        {
            Print("[BDE_MapUI points] Daily rollover " + data.UID + " " + data.LastDailyKey.ToString() + " -> " + dailyKey.ToString());
            ResetTasksForCycle(data, BDE_MapUI_POINTS_CYCLE_DAILY);
            data.LastDailyKey = dailyKey;
        }

        if (data.LastWeeklyKey != weeklyKey)
        {
            Print("[BDE_MapUI points] Weekly rollover " + data.UID + " " + data.LastWeeklyKey.ToString() + " -> " + weeklyKey.ToString());
            ResetTasksForCycle(data, BDE_MapUI_POINTS_CYCLE_WEEKLY);
            data.LastWeeklyKey = weeklyKey;
        }
    }

    private void ResetTasksForCycle(BDE_MapUI_PointPlayerData data, int cycle)
    {
        for (int i = data.TaskStates.Count() - 1; i >= 0; i--)
        {
            BDE_MapUI_PointTaskState state = data.TaskStates.Get(i);
            if (!state)
            {
                data.TaskStates.Remove(i);
                continue;
            }
            BDE_MapUI_PointTask task = FindTask(state.TaskID);
            if (task && task.Cycle == cycle)
                data.TaskStates.Remove(i);
        }
    }

    private int GetDailyKey()
    {
        int y;
        int m;
        int d;
        GetYearMonthDayUTC(y, m, d);
        return (y * 10000) + (m * 100) + d;
    }

    private int GetWeeklyKey()
    {
        int y;
        int m;
        int d;
        GetYearMonthDayUTC(y, m, d);
        int dayOfYear = GetDayOfYear(y, m, d);
        int week = ((dayOfYear - 1) / 7) + 1;
        return (y * 100) + week;
    }

    private int GetDayOfYear(int year, int month, int day)
    {
        int total = day;
        for (int i = 1; i < month; i++)
            total += GetDaysInMonth(year, i);
        return total;
    }

    private int GetDaysInMonth(int year, int month)
    {
        if (month == 2)
        {
            if (IsLeapYear(year))
                return 29;
            return 28;
        }
        if (month == 4 || month == 6 || month == 9 || month == 11)
            return 30;
        return 31;
    }

    private bool IsLeapYear(int year)
    {
        if ((year % 400) == 0)
            return true;
        if ((year % 100) == 0)
            return false;
        return (year % 4) == 0;
    }

    private BDE_MapUI_PointTask FindTask(string taskID)
    {
        if (!m_Config || !m_Config.Tasks)
            return null;

        for (int i = 0; i < m_Config.Tasks.Count(); i++)
        {
            BDE_MapUI_PointTask task = m_Config.Tasks.Get(i);
            if (task && task.TaskID == taskID)
                return task;
        }
        return null;
    }

    private BDE_MapUI_PointStoreItem FindStoreItem(string itemID)
    {
        if (!m_Config || !m_Config.StoreItems)
            return null;

        for (int i = 0; i < m_Config.StoreItems.Count(); i++)
        {
            BDE_MapUI_PointStoreItem item = m_Config.StoreItems.Get(i);
            if (item && item.ItemID == itemID)
                return item;
        }
        return null;
    }

    private bool SpawnStoreItem(PlayerBase player, BDE_MapUI_PointStoreItem item)
    {
        if (!player || !item || item.ClassName == "")
            return false;

        EntityAI entity = null;
        for (int i = 0; i < item.Quantity; i++)
        {
            vector pos = GetSpawnPosNearPlayer(player);
            entity = EntityAI.Cast(g_Game.CreateObjectEx(item.ClassName, pos, ECE_SETUP | ECE_CREATEPHYSICS | ECE_UPDATEPATHGRAPH));
            if (!entity)
                return false;

            if (Transport.Cast(entity))
            {
                dBodyApplyImpulse(entity, vector.Up);
                entity.OnDebugSpawn();
            }
        }

        return true;
    }

    private vector GetSpawnPosNearPlayer(PlayerBase player)
    {
        return player.GetPosition();
    }

    private BDE_MapUI_PointPlayerData GetOrCreatePlayer(string uid, string name)
    {
        BDE_MapUI_PointPlayerData data;
        if (m_PlayerData.Find(uid, data))
        {
            data.EnsureArrays();
            if (name != "")
                data.Name = name;
            return data;
        }

        // Do not rely solely on startup directory enumeration for persisted balances.
        string existingPath = GetPlayerFilePath(uid);
        if (FileExist(existingPath) || FileExist(existingPath + ".pending") || FileExist(existingPath + ".bak") || FileExist(GetPlayerBackupPath(existingPath)))
        {
            LoadPlayerFile(existingPath);
            if (m_PlayerData.Find(uid, data))
            {
                if (name != "")
                {
                    data.Name = name;
                }
                return data;
            }
            if (PreserveEmptySave(existingPath))
            {
                m_EmptyAccountsRestarted.Set(uid, true);
                Print("[BDE_MapUI points] Empty account archived; starting new progress without guessing lost points: " + existingPath);
            }
            else
            {
                m_BDE_FailedPlayerLoads.Set(uid, true);
                Print("[BDE_MapUI points] Account disabled; existing file could not be loaded: " + existingPath);
            }
        }

        data = new BDE_MapUI_PointPlayerData();
        data.UID = uid;
        data.Name = name;
        data.LastDailyKey = GetDailyKey();
        data.LastWeeklyKey = GetWeeklyKey();
        data.EnsureArrays();
        m_PlayerData.Insert(uid, data);
        return data;
    }

    private void EnsureDirectories()
    {
        if (!FileExist("$profile:/BDE"))
            MakeDirectory("$profile:/BDE");
        if (!FileExist("$profile:/BDE/MapUI"))
            MakeDirectory("$profile:/BDE/MapUI");
        if (!FileExist(BDE_MapUI_POINTS_PATH))
            MakeDirectory(BDE_MapUI_POINTS_PATH);
        if (!FileExist(BDE_MapUI_POINTS_PLAYERS_PATH))
            MakeDirectory(BDE_MapUI_POINTS_PLAYERS_PATH);
        MakeDirectory("$profile:/BDE/MapUI/_backups");
        MakeDirectory(BDE_MapUI_POINTS_BACKUPS_PATH);
        MakeDirectory(BDE_MapUI_POINTS_PLAYER_BACKUPS_PATH);
    }

    private void LoadConfig()
    {
        BDE_MapUI_PointsStoreConfig previousConfig = m_Config;
        m_Config = new BDE_MapUI_PointsStoreConfig();
        bool existing = FileExist(BDE_MapUI_POINTS_CONFIG_PATH);
        string error;
        if (existing && !JsonFileLoader<BDE_MapUI_PointsStoreConfig>.LoadFile(BDE_MapUI_POINTS_CONFIG_PATH, m_Config, error))
        {
            Print("[BDE_MapUI points] Config reload failed; file unchanged: " + error);
            if (previousConfig)
                m_Config = previousConfig;
            else
            {
                m_Config = new BDE_MapUI_PointsStoreConfig();
                EnsureDefaultCatalog();
            }
            return;
        }
        string beforeDefaults;
        if (existing && !JsonFileLoader<BDE_MapUI_PointsStoreConfig>.MakeData(m_Config, beforeDefaults, error, true))
            return;
        if (!existing)
            WriteDefaultConfig();
        m_Config.EnsureDefaults();
        EnsureDefaultCatalog();
        string afterDefaults;
        if (!JsonFileLoader<BDE_MapUI_PointsStoreConfig>.MakeData(m_Config, afterDefaults, error, true))
            return;
        // Reloading an unchanged config must not trigger Hive every 30 seconds.
        if (!existing || !SameConfigText(beforeDefaults, afterDefaults))
        {
            if (!JsonFileLoader<BDE_MapUI_PointsStoreConfig>.SaveFile(BDE_MapUI_POINTS_CONFIG_PATH, m_Config, error))
                Print("[BDE_MapUI points] Config save failed: " + error);
        }
    }

    private bool SameConfigText(string firstText, string secondText)
    {
        if (firstText.Length() != secondText.Length())
            return false;
        for (int offset = 0; offset < firstText.Length(); offset += 256)
        {
            int length = Math.Min(256, firstText.Length() - offset);
            if (firstText.Substring(offset, length) != secondText.Substring(offset, length))
                return false;
        }
        return true;
    }

    private void EnsureDefaultCatalog()
    {
        if (!m_Config)
            return;

        m_Config.EnsureDefaults();
        RemoveSeasonTasks();
        EnsureDefaultTasks();

        if (m_Config.StoreItems.Count() == 0)
            AddDefaultItems();
    }

    private void WriteDefaultConfig()
    {
        m_Config = new BDE_MapUI_PointsStoreConfig();
        m_Config.StoreName = "Task Points Store";
        m_Config.VIPMultiplier = 1.5;

        AddDefaultTasks();
        AddDefaultItems();
    }

    private void AddDefaultTasks()
    {
        EnsureDefaultTasks();
    }

    private void EnsureDefaultTasks()
    {
        EnsureDefaultTask("DAILY_AI_5", "Daily AI Hunter", "Kill 5 AI enemies.", BDE_MapUI_POINTS_TASK_AI, 5, 60, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_INFECTED_25", "Daily Cleanup", "Kill 25 infected.", BDE_MapUI_POINTS_TASK_INFECTED, 25, 40, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_TRAVEL_2000", "Daily Traveller", "Travel 2km.", BDE_MapUI_POINTS_TASK_DISTANCE, 2000, 35, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_ONLINE_30", "Stay Online", "Stay online for 30 minutes.", BDE_MapUI_POINTS_TASK_ONLINE, 30, 30, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_HUNT_ANIMALS_5", "Daily Hunter", "Kill 5 animals.", BDE_MapUI_POINTS_TASK_ANIMALS, 5, 65, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_HUNT_WOLVES_2", "Daily Wolf Cull", "Kill 2 wolves.", BDE_MapUI_POINTS_TASK_ANIMAL_WOLF, 2, 90, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_HUNT_BEAR_1", "Daily Big Game", "Kill 1 bear.", BDE_MapUI_POINTS_TASK_ANIMAL_BEAR, 1, 140, BDE_MapUI_POINTS_CYCLE_DAILY);
        EnsureDefaultTask("DAILY_HUNT_DEER_3", "Daily Deer Stalker", "Kill 3 deer.", BDE_MapUI_POINTS_TASK_ANIMAL_DEER, 3, 80, BDE_MapUI_POINTS_CYCLE_DAILY);

        EnsureDefaultTask("WEEKLY_AI_30", "Weekly AI Sweep", "Kill 30 AI enemies.", BDE_MapUI_POINTS_TASK_AI, 30, 250, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_PLAYERS_5", "Weekly Rivalry", "Kill 5 players.", BDE_MapUI_POINTS_TASK_PLAYERS, 5, 350, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_SURVIVE_18000", "Weekly Survivor", "Survive 5 hours.", BDE_MapUI_POINTS_TASK_SURVIVE, 18000, 200, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_DISTANCE_20000", "Weekly Nomad", "Travel 20km.", BDE_MapUI_POINTS_TASK_DISTANCE, 20000, 180, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_ANIMALS_25", "Weekly Hunter", "Kill 25 animals.", BDE_MapUI_POINTS_TASK_ANIMALS, 25, 320, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_WOLVES_10", "Weekly Wolf Hunter", "Kill 10 wolves.", BDE_MapUI_POINTS_TASK_ANIMAL_WOLF, 10, 380, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_BEARS_3", "Weekly Bear Hunter", "Kill 3 bears.", BDE_MapUI_POINTS_TASK_ANIMAL_BEAR, 3, 500, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_DEER_15", "Weekly Deer Season", "Kill 15 deer.", BDE_MapUI_POINTS_TASK_ANIMAL_DEER, 15, 330, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_PIGS_8", "Weekly Pig Hunt", "Kill 8 pigs.", BDE_MapUI_POINTS_TASK_ANIMAL_PIG, 8, 240, BDE_MapUI_POINTS_CYCLE_WEEKLY);
        EnsureDefaultTask("WEEKLY_HUNT_FOXES_5", "Weekly Fox Hunt", "Kill 5 foxes.", BDE_MapUI_POINTS_TASK_ANIMAL_FOX, 5, 220, BDE_MapUI_POINTS_CYCLE_WEEKLY);
    }

    private void EnsureDefaultTask(string id, string name, string description, string taskType, int target, int points, int cycle)
    {
        if (FindTask(id))
            return;

        AddDefaultTask(id, name, description, taskType, target, points, cycle);
    }

    private void RemoveSeasonTasks()
    {
        if (!m_Config || !m_Config.Tasks)
            return;

        for (int i = m_Config.Tasks.Count() - 1; i >= 0; i--)
        {
            BDE_MapUI_PointTask task = m_Config.Tasks.Get(i);
            if (!task || task.Cycle == BDE_MapUI_POINTS_CYCLE_SEASON)
                m_Config.Tasks.Remove(i);
        }
    }

    private void AddDefaultItems()
    {
        AddDefaultItem("FOOD_TACBACON", "Tactical Bacon", "TacticalBaconCan", "Food", 25, 1, "A quick meal.");
        AddDefaultItem("MED_BANDAGE", "Bandage", "BandageDressing", "Medical", 20, 1, "Basic medical supplies.");
        AddDefaultItem("TOOL_NAILBOX", "Nail Box", "NailBox", "Basebuilding", 120, 1, "Basebuilding supplies.");
        AddDefaultItem("AMMO_556", "5.56 Ammo", "Ammo_556x45", "Ammo", 90, 1, "A box/stack of 5.56 ammo.");
        AddDefaultItem("WEAPON_AKS74U", "AKS-74U", "AKS74U", "Weapons", 500, 1, "Compact rifle reward.");
    }

    private void AddDefaultTask(string id, string name, string description, string taskType, int target, int points, int cycle)
    {
        BDE_MapUI_PointTask task = new BDE_MapUI_PointTask();
        task.TaskID = id;
        task.Name = name;
        task.Description = description;
        task.TaskType = taskType;
        task.Target = target;
        task.Points = points;
        task.Cycle = cycle;
        task.EnsureDefaults();
        m_Config.Tasks.Insert(task);
    }

    private void AddDefaultItem(string id, string name, string className, string category, int price, int quantity, string description)
    {
        BDE_MapUI_PointStoreItem item = new BDE_MapUI_PointStoreItem();
        item.ItemID = id;
        item.Name = name;
        item.ClassName = className;
        item.Category = category;
        item.Price = price;
        item.Quantity = quantity;
        item.Description = description;
        item.EnsureDefaults();
        m_Config.StoreItems.Insert(item);
    }

    private bool PreserveEmptySave(string path)
    {
        FileHandle handle = OpenFile(path, FileMode.READ);
        if (!handle)
        {
            return false;
        }
        string contents;
        ReadFile(handle, contents, 10000000);
        CloseFile(handle);
        // Non-empty damaged files may still contain recoverable balances.
        if (contents != "")
        {
            return false;
        }
        return CopyFile(path, GetPlayerBackupPath(path) + ".empty");
    }

    private bool ReadPlayerSave(string path, string canonicalPath, out BDE_MapUI_PointPlayerData data)
    {
        data = null;
        if (!FileExist(path))
        {
            return false;
        }
        FileHandle handle = OpenFile(path, FileMode.READ);
        if (!handle)
        {
            return false;
        }
        string contents;
        ReadFile(handle, contents, 10000000);
        CloseFile(handle);
        if (contents.IndexOf("\"Points\"") < 0 || contents.IndexOf("\"TaskStates\"") < 0)
        {
            return false;
        }
        string error;
        if (!JsonFileLoader<BDE_MapUI_PointPlayerData>.LoadData(contents, data, error) || !data || data.UID == "")
        {
            return false;
        }
        return GetPlayerFilePath(data.UID) == canonicalPath && data.Points >= 0 && data.TaskStates != null;
    }

    private void LoadPlayerFile(string path)
    {
        BDE_MapUI_PointPlayerData data;
        string recoveredFrom;
        // A validated pending file is newer than the main file after an interrupted commit.
        if (ReadPlayerSave(path + ".pending", path, data))
        {
            recoveredFrom = path + ".pending";
        }
        else if (!ReadPlayerSave(path, path, data))
        {
            if (ReadPlayerSave(GetPlayerBackupPath(path), path, data))
                recoveredFrom = GetPlayerBackupPath(path);
            else if (!ReadPlayerSave(path + ".bak", path, data))
            {
                Print("[BDE_MapUI points] No valid save/recovery file: " + path);
                return;
            }
            if (recoveredFrom == "")
                recoveredFrom = path + ".bak";
        }
        if (recoveredFrom != "")
        {
            Print("[BDE_MapUI points] Recovered " + data.UID + " from " + recoveredFrom);
        }
        data.EnsureArrays();
        ApplyCycleResets(data);
        m_PlayerData.Set(data.UID, data);
        Print("[BDE_MapUI points] Loaded " + data.UID + " points=" + data.Points.ToString() + " tasks=" + data.TaskStates.Count().ToString());
    }

    private string GetPlayerFilePath(string uid)
    {
        string cleanUID = uid;
        cleanUID.Replace("\\", "_");
        cleanUID.Replace("/", "_");
        cleanUID.Replace(":", "_");
        cleanUID.Replace(" ", "_");
        return BDE_MapUI_POINTS_PLAYERS_PATH + "/" + cleanUID + ".json";
    }

    private string GetPlayerBackupPath(string playerPath)
    {
        string fileName = playerPath.Substring(BDE_MapUI_POINTS_PLAYERS_PATH.Length() + 1, playerPath.Length() - BDE_MapUI_POINTS_PLAYERS_PATH.Length() - 1);
        return BDE_MapUI_POINTS_PLAYER_BACKUPS_PATH + "/" + fileName + ".bak";
    }

    private void MarkPlayerDirty(BDE_MapUI_PointPlayerData data)
    {
        if (data && data.UID != "")
            m_DirtyPlayers.Set(data.UID, true);
    }

    private bool FlushPlayer(BDE_MapUI_PointPlayerData data)
    {
        if (!data)
            return false;
        if (!m_DirtyPlayers.Contains(data.UID))
            return true;
        if (!PersistPlayer(data))
            return false;
        m_DirtyPlayers.Remove(data.UID);
        return true;
    }

    private bool PersistPlayer(BDE_MapUI_PointPlayerData data)
    {
        if (!data || data.UID == "")
            return false;

        if (m_BDE_FailedPlayerLoads.Contains(data.UID))
        {
            return false;
        }
        data.EnsureArrays();
        string path = GetPlayerFilePath(data.UID);
        string pending = path + ".pending";
        string error;
        BDE_MapUI_PointPlayerData verified;
        if (!JsonFileLoader<BDE_MapUI_PointPlayerData>.SaveFile(pending, data, error) || !ReadPlayerSave(pending, path, verified))
        {
            Print("[BDE_MapUI points] Staging save failed; main save preserved: " + path + " " + error);
            return false;
        }
        // Never replace a good recovery copy with an empty or unreadable main file.
        BDE_MapUI_PointPlayerData previous;
        if (ReadPlayerSave(path, path, previous) && !CopyFile(path, GetPlayerBackupPath(path)))
        {
            Print("[BDE_MapUI points] Backup failed; validated pending save retained: " + path);
            return false;
        }
        if (!CopyFile(pending, path) || !ReadPlayerSave(path, path, verified))
        {
            Print("[BDE_MapUI points] Commit failed; validated pending save retained: " + path);
            return false;
        }
        DeleteFile(pending);
        return true;
    }

    // Only called by MissionServer.OnMissionFinish for an orderly shutdown.
    // Other flushes are restricted to disconnect, task completion and purchases.
    void SaveAllPlayers()
    {
        for (int i = 0; i < m_PlayerData.Count(); i++)
        {
            BDE_MapUI_PointPlayerData data = m_PlayerData.GetElement(i);
            if (data && m_DirtyPlayers.Contains(data.UID))
                FlushPlayer(data);
        }
    }

    private void NotifyPlayer(string uid, string title, string message)
    {
        PlayerIdentity identity = GetOnlineIdentityByUID(uid);
        if (identity)
            NotificationSystem.SendNotificationToPlayerIdentityExtended(identity, 8.0, title, message, "set:dayz_gui image:iconTrophy");
    }

    private PlayerIdentity GetOnlineIdentityByUID(string uid)
    {
        array<Man> players = new array<Man>();
        g_Game.GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase player = PlayerBase.Cast(players.Get(i));
            if (!player)
                continue;

            PlayerIdentity identity = player.GetIdentity();
            if (identity && identity.GetId() == uid)
                return identity;
        }

        return null;
    }
};

class BDE_MapUI_PointsStoreAPI
{
    static void AddPoints(PlayerBase player, int amount)
    {
        BDE_MapUI_PointsStoreManager.Get().AddManualPoints(player, amount);
    }

    static void AddProgress(PlayerBase player, string taskType, int amount)
    {
        BDE_MapUI_PointsStoreManager.Get().AddTaskProgress(player, taskType, amount);
    }

    static void AddAnimalKill(PlayerBase player)
    {
        BDE_MapUI_PointsStoreManager.Get().OnAnimalKill(player);
    }

    static void AddAnimalKillByClass(PlayerBase player, string animalClassName)
    {
        if (!player || !player.GetIdentity())
            return;

        BDE_MapUI_PointsStoreManager.Get().OnAnimalKillByUID(player.GetIdentity().GetId(), player.GetIdentity().GetName(), animalClassName);
    }
};
