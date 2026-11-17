static const string BDE_MapUI_POINTS_PATH = "$profile:/BDE/MapUI/PointsStore";
static const string BDE_MapUI_POINTS_PLAYERS_PATH = "$profile:/BDE/MapUI/PointsStore/Players";
static const string BDE_MapUI_POINTS_BACKUPS_PATH = "$profile:/BDE/MapUI/_backups/PointsStore";
static const string BDE_MapUI_POINTS_PLAYER_BACKUPS_PATH = "$profile:/BDE/MapUI/_backups/PointsStore/Players";
static const string BDE_MapUI_POINTS_CONFIG_PATH = "$profile:/BDE/MapUI/PointsStore/PointsStoreConfig.json";

static const int BDE_MapUI_POINTS_CYCLE_SEASON = 0;
static const int BDE_MapUI_POINTS_CYCLE_DAILY = 1;
static const int BDE_MapUI_POINTS_CYCLE_WEEKLY = 2;

static const string BDE_MapUI_POINTS_TASK_AI = "AI_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMALS = "ANIMAL_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMAL_WOLF = "ANIMAL_WOLF_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMAL_BEAR = "ANIMAL_BEAR_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMAL_DEER = "ANIMAL_DEER_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMAL_PIG = "ANIMAL_PIG_KILLS";
static const string BDE_MapUI_POINTS_TASK_ANIMAL_FOX = "ANIMAL_FOX_KILLS";
static const string BDE_MapUI_POINTS_TASK_INFECTED = "INFECTED_KILLED";
static const string BDE_MapUI_POINTS_TASK_PLAYERS = "PLAYER_KILLS";
static const string BDE_MapUI_POINTS_TASK_DISTANCE = "DISTANCE_TRAVELLED";
static const string BDE_MapUI_POINTS_TASK_SURVIVE = "TIME_SURVIVED";
static const string BDE_MapUI_POINTS_TASK_ONLINE = "ONLINE_TIME";
static const string BDE_MapUI_POINTS_TASK_CUSTOM = "CUSTOM";

class BDE_MapUI_PointStoreItem
{
	string ItemID;
	string Name;
	string Description;
	string ClassName;
	string Category;
	int Price;
	int Quantity;

	void BDE_MapUI_PointStoreItem()
	{
		ItemID = "";
		Name = "Store Item";
		Description = "";
		ClassName = "";
		Category = "General";
		Price = 100;
		Quantity = 1;
	}

	void EnsureDefaults()
	{
		if (ItemID == "")
			ItemID = Category + "_" + ClassName + "_" + Name;
		if (Name == "")
			Name = "Store Item";
		if (Category == "")
			Category = "General";
		if (Price < 0)
			Price = 0;
		if (Quantity < 1)
			Quantity = 1;
	}
};

class BDE_MapUI_PointTask
{
	string TaskID;
	string Name;
	string Description;
	string TaskType;
	int Target;
	int Points;
	int Cycle;

	void BDE_MapUI_PointTask()
	{
		TaskID = "";
		Name = "Task";
		Description = "";
		TaskType = BDE_MapUI_POINTS_TASK_CUSTOM;
		Target = 1;
		Points = 10;
		Cycle = BDE_MapUI_POINTS_CYCLE_DAILY;
	}

	void EnsureDefaults()
	{
		if (TaskID == "")
			TaskID = TaskType + "_" + Name;
		if (Name == "")
			Name = "Task";
		if (TaskType == "")
			TaskType = BDE_MapUI_POINTS_TASK_CUSTOM;
		if (Target < 1)
			Target = 1;
		if (Points < 0)
			Points = 0;
		if (Cycle < BDE_MapUI_POINTS_CYCLE_SEASON || Cycle > BDE_MapUI_POINTS_CYCLE_WEEKLY)
			Cycle = BDE_MapUI_POINTS_CYCLE_DAILY;
	}

	string GetCycleName()
	{
		if (Cycle == BDE_MapUI_POINTS_CYCLE_DAILY)
			return "Daily";
		if (Cycle == BDE_MapUI_POINTS_CYCLE_WEEKLY)
			return "Weekly";
		return "Season";
	}
};

class BDE_MapUI_PointTaskState
{
	string TaskID;
	int Progress;
	bool Completed;
	bool Paid;

	void BDE_MapUI_PointTaskState()
	{
		TaskID = "";
		Progress = 0;
		Completed = false;
		Paid = false;
	}
};

class BDE_MapUI_PointPlayerData
{
	string UID;
	string Name;
	int Points;
	int LastDailyKey;
	int LastWeeklyKey;
	bool HasStatsBaseline;
	int TrackedInfectedKilled;
	float TrackedDistanceTravelled;
	float TrackedTimeSurvived;
	ref array<ref BDE_MapUI_PointTaskState> TaskStates;

	void BDE_MapUI_PointPlayerData()
	{
		UID = "";
		Name = "";
		Points = 0;
		LastDailyKey = 0;
		LastWeeklyKey = 0;
		HasStatsBaseline = false;
		TrackedInfectedKilled = 0;
		TrackedDistanceTravelled = 0.0;
		TrackedTimeSurvived = 0.0;
		TaskStates = new array<ref BDE_MapUI_PointTaskState>();
	}

	void EnsureArrays()
	{
		if (!TaskStates)
			TaskStates = new array<ref BDE_MapUI_PointTaskState>();
		if (Points < 0)
			Points = 0;
	}

	BDE_MapUI_PointTaskState GetOrCreateTaskState(string taskID)
	{
		EnsureArrays();
		for (int i = 0; i < TaskStates.Count(); i++)
		{
			BDE_MapUI_PointTaskState state = TaskStates.Get(i);
			if (state && state.TaskID == taskID)
				return state;
		}

		BDE_MapUI_PointTaskState newState = new BDE_MapUI_PointTaskState();
		newState.TaskID = taskID;
		TaskStates.Insert(newState);
		return newState;
	}
};

class BDE_MapUI_PointsStoreConfig
{
	string StoreName;
	float VIPMultiplier;
	ref array<string> VIPSteamIDs;
	ref array<ref BDE_MapUI_PointTask> Tasks;
	ref array<ref BDE_MapUI_PointStoreItem> StoreItems;

	void BDE_MapUI_PointsStoreConfig()
	{
		StoreName = "Task Points Store";
		VIPMultiplier = 1.5;
		VIPSteamIDs = new array<string>();
		Tasks = new array<ref BDE_MapUI_PointTask>();
		StoreItems = new array<ref BDE_MapUI_PointStoreItem>();
	}

	void EnsureDefaults()
	{
		if (StoreName == "")
			StoreName = "Task Points Store";
		if (VIPMultiplier <= 0.0)
			VIPMultiplier = 1.5;
		if (!VIPSteamIDs)
			VIPSteamIDs = new array<string>();
		if (!Tasks)
			Tasks = new array<ref BDE_MapUI_PointTask>();
		if (!StoreItems)
			StoreItems = new array<ref BDE_MapUI_PointStoreItem>();

		for (int i = 0; i < Tasks.Count(); i++)
		{
			BDE_MapUI_PointTask task = Tasks.Get(i);
			if (task)
				task.EnsureDefaults();
		}

		for (int j = 0; j < StoreItems.Count(); j++)
		{
			BDE_MapUI_PointStoreItem item = StoreItems.Get(j);
			if (item)
				item.EnsureDefaults();
		}
	}
};

class BDE_MapUI_PointsStoreSyncData
{
	ref BDE_MapUI_PointsStoreConfig Config;
	ref BDE_MapUI_PointPlayerData PlayerData;
	bool IsVIP;
	float VIPMultiplier;

	void BDE_MapUI_PointsStoreSyncData()
	{
		Config = null;
		PlayerData = null;
		IsVIP = false;
		VIPMultiplier = 1.0;
	}
};
