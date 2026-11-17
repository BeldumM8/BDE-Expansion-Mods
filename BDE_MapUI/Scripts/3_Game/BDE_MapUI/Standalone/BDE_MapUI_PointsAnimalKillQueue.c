class BDE_MapUI_PointsAnimalKillEvent
{
	string KillerUID;
	string KillerName;
	string AnimalClassName;
	int EventTime;

	void BDE_MapUI_PointsAnimalKillEvent()
	{
		KillerUID = "";
		KillerName = "";
		AnimalClassName = "";
		EventTime = 0;
	}
};

class BDE_MapUI_PointsAnimalKillQueue
{
	private static ref array<ref BDE_MapUI_PointsAnimalKillEvent> s_Events;

	private static void EnsureEvents()
	{
		if (!s_Events)
			s_Events = new array<ref BDE_MapUI_PointsAnimalKillEvent>();
	}

	static void Push(string killerUID, string killerName, string animalClassName)
	{
		if (killerUID == "")
			return;

		EnsureEvents();

		BDE_MapUI_PointsAnimalKillEvent eventData = new BDE_MapUI_PointsAnimalKillEvent();
		eventData.KillerUID = killerUID;
		eventData.KillerName = killerName;
		eventData.AnimalClassName = animalClassName;
		if (g_Game)
			eventData.EventTime = g_Game.GetTime();

		s_Events.Insert(eventData);
	}

	static ref array<ref BDE_MapUI_PointsAnimalKillEvent> Consume()
	{
		EnsureEvents();

		ref array<ref BDE_MapUI_PointsAnimalKillEvent> events = s_Events;
		s_Events = new array<ref BDE_MapUI_PointsAnimalKillEvent>();
		return events;
	}
};
