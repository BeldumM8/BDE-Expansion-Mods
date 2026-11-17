class BDE_MapUI_BountyKillEvent
{
	string VictimUID;
	string VictimName;
	string KillerUID;
	string KillerName;
	int EventTime;

	void BDE_MapUI_BountyKillEvent()
	{
		VictimUID = "";
		VictimName = "";
		KillerUID = "";
		KillerName = "";
		EventTime = 0;
	}
};

class BDE_MapUI_BountyKillQueue
{
	private static ref array<ref BDE_MapUI_BountyKillEvent> s_Events;

	private static void EnsureEvents()
	{
		if (!s_Events)
			s_Events = new array<ref BDE_MapUI_BountyKillEvent>();
	}

	static void Push(string victimUID, string victimName, string killerUID, string killerName)
	{
		if (victimUID == "" || killerUID == "")
			return;

		if (victimUID == killerUID)
			return;

		EnsureEvents();

		BDE_MapUI_BountyKillEvent eventData = new BDE_MapUI_BountyKillEvent();
		eventData.VictimUID = victimUID;
		eventData.VictimName = victimName;
		eventData.KillerUID = killerUID;
		eventData.KillerName = killerName;
		if (g_Game)
			eventData.EventTime = g_Game.GetTime();

		s_Events.Insert(eventData);
	}

	static ref array<ref BDE_MapUI_BountyKillEvent> Consume()
	{
		EnsureEvents();

		ref array<ref BDE_MapUI_BountyKillEvent> events = s_Events;
		s_Events = new array<ref BDE_MapUI_BountyKillEvent>();
		return events;
	}
};
