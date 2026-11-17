
static const int BDE_MapUI_LEADERBOARD_FILTER_ALL = 0;
static const int BDE_MapUI_LEADERBOARD_FILTER_KILLS = 1;
static const int BDE_MapUI_LEADERBOARD_FILTER_RICHEST = 2;

class BDE_MapUI_LeaderboardListEntry
{
	string UID;
	string Name;
	bool IsOnline;
    bool IsRemoteServer;
    string CurrentMapName;
	int BankMoney;
	int PlayerKills;
	int AIKills;
	int InfectedKilled;
	float DistanceTravelled;
	float TimeSurvived;
	float LongestSurvivorHit;
	string CharacterType;

	void BDE_MapUI_LeaderboardListEntry()
	{
		UID = "";
		Name = "";
		IsOnline = false;
		BankMoney = 0;
		PlayerKills = 0;
		AIKills = 0;
		InfectedKilled = 0;
		DistanceTravelled = 0.0;
		TimeSurvived = 0.0;
		LongestSurvivorHit = 0.0;
		CharacterType = "";
	}

	int GetTotalKills()
	{
		return PlayerKills + AIKills + InfectedKilled;
	}

	int GetCombatKills()
	{
		return PlayerKills + AIKills;
	}
};
