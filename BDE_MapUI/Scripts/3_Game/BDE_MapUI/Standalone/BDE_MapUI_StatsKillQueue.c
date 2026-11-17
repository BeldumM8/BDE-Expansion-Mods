class BDE_MapUI_StatsKillEvent
{
	string KillerUID;
	string KillerName;
	bool IsAIKill;
	int EventTime;

	void BDE_MapUI_StatsKillEvent()
	{
		KillerUID = "";
		KillerName = "";
		IsAIKill = false;
		EventTime = 0;
	}
};

class BDE_MapUI_StatsKillQueue
{
	private static ref array<ref BDE_MapUI_StatsKillEvent> s_Events;

	private static void EnsureEvents()
	{
		if (!s_Events)
			s_Events = new array<ref BDE_MapUI_StatsKillEvent>();
	}

	static void PushAIKill(string killerUID, string killerName)
	{
		if (killerUID == "")
			return;

		EnsureEvents();

		BDE_MapUI_StatsKillEvent eventData = new BDE_MapUI_StatsKillEvent();
		eventData.KillerUID = killerUID;
		eventData.KillerName = killerName;
		eventData.IsAIKill = true;
		if (g_Game)
			eventData.EventTime = g_Game.GetTime();

		s_Events.Insert(eventData);
	}

	static ref array<ref BDE_MapUI_StatsKillEvent> Consume()
	{
		EnsureEvents();

		ref array<ref BDE_MapUI_StatsKillEvent> events = s_Events;
		s_Events = new array<ref BDE_MapUI_StatsKillEvent>();
		return events;
	}
};
