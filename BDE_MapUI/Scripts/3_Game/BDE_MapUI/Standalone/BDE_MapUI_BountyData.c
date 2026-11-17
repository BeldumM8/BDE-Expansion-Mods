class BDE_MapUI_PresencePlayer
{
    string UID;
    string Name;
};

class BDE_MapUI_ServerPresence
{
    string ServerID;
    string MapName;
    int Epoch;
    ref array<ref BDE_MapUI_PresencePlayer> Players;
    void BDE_MapUI_ServerPresence()
    {
        Players = new array<ref BDE_MapUI_PresencePlayer>;
    }
};

class BDE_MapUI_OnlinePlayerPresence
{
    string UID;
    string Name;
    string ServerID;
    string MapName;
    int Epoch;
};

class BDE_MapUI_PresenceText
{
    static string Status(bool isOnline, bool isRemote, string mapName)
    {
        if (!isOnline)
            return "Offline";
        if (isRemote && mapName != "")
            return "Online - " + mapName;
        return "Online";
    }
};

static const string BDE_MapUI_PLAYER_DB_DIR_NAME = "PlayerDatabase";
static const string BDE_MapUI_PLAYER_DB_PATH = "$profile:/BDE/MapUI/PlayerDatabase";

static const int BDE_MapUI_BOUNTY_FILTER_ALL = 0;
static const int BDE_MapUI_BOUNTY_FILTER_ONLINE = 1;
static const int BDE_MapUI_BOUNTY_FILTER_ACTIVE = 2;

static const int BDE_MapUI_BOUNTY_DEFAULT_AMOUNT = 50000;
static const int BDE_MapUI_BOUNTY_MIN_AMOUNT = 50000;
static const int BDE_MapUI_BOUNTY_REFUND_PERCENT = 50;

class BDE_MapUI_BountyContributor
{
	string UID;
	string Name;
	int Amount;

	void BDE_MapUI_BountyContributor()
	{
		UID = "";
		Name = "";
		Amount = 0;
	}
};

// Immutable, uniquely identified cross-server bounty state transition.
class BDE_MapUI_HiveBountyEvent
{
    string ID;
    string Kind;
    string TargetUID;
    string TargetName;
    string ActorUID;
    string ActorName;
    string MapName;
    int Amount;
    int TotalAmount;
    int Revision;
    int Epoch;
    ref array<ref BDE_MapUI_BountyContributor> Contributors;

    void BDE_MapUI_HiveBountyEvent()
    {
        Contributors = new array<ref BDE_MapUI_BountyContributor>;
    }
};

class BDE_MapUI_GearItem
{
	string ClassName;
	string DisplayName;
	string SlotName;
	float Health;
	float Quantity;
	bool InHands;
	int GearID;
	int ParentGearID;
	bool IsAttachment;

	void BDE_MapUI_GearItem()
	{
		ClassName = "";
		DisplayName = "";
		SlotName = "";
		Health = 0.0;
		Quantity = 0.0;
		InHands = false;
		GearID = -1;
		ParentGearID = -1;
		IsAttachment = false;
	}
};

class BDE_MapUI_PlayerRecord
{
	string UID;
	string Name;
	bool IsOnline;
    bool IsRemoteServer;
    string CurrentServerID;
    string CurrentMapName;
	bool HasActiveBounty;
	int BountyAmount;
    int BountyRevision;
    int BountyRevisionEpoch;
    string BountyRevisionID;
	vector LastKnownPosition;
	string LastKnownLocation;
	string CharacterType;
	int LastSeenGameTime;
	int LastUpdatedGameTime;
	int BankMoney;
	int PlayerKills;
	int AIKills;
	int InfectedKilled;
	float DistanceTravelled;
	float TimeSurvived;
	float LongestSurvivorHit;
	int LastStatsUpdatedGameTime;
	ref array<ref BDE_MapUI_BountyContributor> BountyContributors;
	ref array<ref BDE_MapUI_GearItem> Gear;

	void BDE_MapUI_PlayerRecord()
	{
		UID = "";
		Name = "";
		IsOnline = false;
		HasActiveBounty = false;
		BountyAmount = 0;
		LastKnownPosition = "0 0 0";
		LastKnownLocation = "Unknown";
		CharacterType = "";
		LastSeenGameTime = 0;
		LastUpdatedGameTime = 0;
		BankMoney = 0;
		PlayerKills = 0;
		AIKills = 0;
		InfectedKilled = 0;
		DistanceTravelled = 0.0;
		TimeSurvived = 0.0;
		LongestSurvivorHit = 0.0;
		LastStatsUpdatedGameTime = 0;
		BountyContributors = new array<ref BDE_MapUI_BountyContributor>();
		Gear = new array<ref BDE_MapUI_GearItem>();
	}

	void EnsureArrays()
	{
		if (!BountyContributors)
			BountyContributors = new array<ref BDE_MapUI_BountyContributor>();
		if (!Gear)
			Gear = new array<ref BDE_MapUI_GearItem>();
	}

	void RecalculateBounty()
	{
		EnsureArrays();
		BountyAmount = 0;
		for (int i = 0; i < BountyContributors.Count(); i++)
		{
			BDE_MapUI_BountyContributor contributor = BountyContributors.Get(i);
			if (!contributor)
				continue;
			if (contributor.Amount > 0)
				BountyAmount += contributor.Amount;
		}
		HasActiveBounty = BountyAmount > 0;
	}

	string GetContributorSummary()
	{
		EnsureArrays();
		string text = "";
		for (int i = 0; i < BountyContributors.Count(); i++)
		{
			BDE_MapUI_BountyContributor contributor = BountyContributors.Get(i);
			if (!contributor || contributor.Amount <= 0)
				continue;

			if (text != "")
				text += ", ";
			text += contributor.Name + " ($" + contributor.Amount.ToString() + ")";
		}
		if (text == "")
			text = "None";
		return text;
	}

	int GetContributionByUID(string uid)
	{
		EnsureArrays();
		for (int i = 0; i < BountyContributors.Count(); i++)
		{
			BDE_MapUI_BountyContributor contributor = BountyContributors.Get(i);
			if (contributor && contributor.UID == uid)
				return contributor.Amount;
		}
		return 0;
	}
};

class BDE_MapUI_BountyListEntry
{
	string UID;
	string Name;
	bool IsOnline;
    bool IsRemoteServer;
    string CurrentServerID;
    string CurrentMapName;
	bool HasActiveBounty;
	int BountyAmount;
	string LastKnownLocation;
	vector LastKnownPosition;
	string BountySetBy;

	void BDE_MapUI_BountyListEntry()
	{
		UID = "";
		Name = "";
		IsOnline = false;
		HasActiveBounty = false;
		BountyAmount = 0;
		LastKnownLocation = "Unknown";
		LastKnownPosition = "0 0 0";
		BountySetBy = "None";
	}
};
