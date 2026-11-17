class BDE_MapUI_BountyAreaZone
{
	string Map;
	string Name;
	string Type;
	string ClassName;
	float PositionX;
	float PositionZ;
	float Radius;

	void BDE_MapUI_BountyAreaZone()
	{
		Map = "";
		Name = "";
		Type = "";
		ClassName = "";
		PositionX = 0.0;
		PositionZ = 0.0;
		Radius = 0.0;
	}

	vector GetPosition2D()
	{
		return Vector(PositionX, 0, PositionZ);
	}
};

class BDE_MapUI_BountyAreaLocator
{
	private static const int BDE_MapUI_LOC_CAMP = 1;
	private static const int BDE_MapUI_LOC_HILL = 2;
	private static const int BDE_MapUI_LOC_LOCAL = 4;
	private static const int BDE_MapUI_LOC_LOCALOFFICE = 8;
	private static const int BDE_MapUI_LOC_MARINE = 16;
	private static const int BDE_MapUI_LOC_RAILROADSTATION = 32;
	private static const int BDE_MapUI_LOC_RUIN = 64;
	private static const int BDE_MapUI_LOC_SETTLEMENT = 128;
	private static const int BDE_MapUI_LOC_VIEWPOINT = 256;

	private ref array<ref BDE_MapUI_BountyAreaZone> m_Zones;
	private bool m_Loaded;
	private string m_ActiveWorldName;
	private int m_IncludeTypes;

	void BDE_MapUI_BountyAreaLocator()
	{
		m_Zones = new array<ref BDE_MapUI_BountyAreaZone>();
		m_Loaded = false;
		m_ActiveWorldName = "";
		m_IncludeTypes = 0;
	}

	void EnsureLoaded()
	{
		if (m_Loaded)
			return;

		m_Loaded = true;
		m_Zones.Clear();

		m_ActiveWorldName = GetGame().GetWorldName();
		m_ActiveWorldName.ToLower();

		// Mirrors DayZ Expansion Navigation's location source:
		// CfgWorlds <worldName> Names.
		// No AreaAnnouncements mod or external JSON is required.
		LoadWorldConfigLocations(GetDefaultIncludedLocationTypes());

		BDE_MapUI_Log.Log("[BDE_MapUI Bounties] CfgWorlds area locator loaded zones: " + m_Zones.Count().ToString() + " for world " + m_ActiveWorldName,"BDE_MapUI/Bounties/BDE_MapUI_BountyAreaLocator");
	}

	string GetLocationNameForPosition(vector position)
	{
		EnsureLoaded();

		BDE_MapUI_BountyAreaZone zone = FindCurrentZone(position);
		if (zone && zone.Name != "")
			return zone.Name;

		return "Wilderness";
	}

	private int GetDefaultIncludedLocationTypes()
	{
		// Same default group used by ExpansionLocation.GetWorldLocations():
		// Camp, Hill, Local, Marine, Ruin, and Settlements.
		int include = 0;
		include |= BDE_MapUI_LOC_CAMP;
		include |= BDE_MapUI_LOC_HILL;
		include |= BDE_MapUI_LOC_LOCAL;
		include |= BDE_MapUI_LOC_MARINE;
		include |= BDE_MapUI_LOC_RUIN;
		include |= BDE_MapUI_LOC_SETTLEMENT;
		return include;
	}

	private void LoadWorldConfigLocations(int include)
	{
		m_IncludeTypes = include;

		string worldName = GetGame().GetWorldName();
		if (worldName == "")
			return;

		string locationConfigPath = "CfgWorlds " + worldName + " Names";
		int classNamesCount = GetGame().ConfigGetChildrenCount(locationConfigPath);
		if (classNamesCount <= 0)
		{
			BDE_MapUI_Log.Log("[BDE_MapUI Bounties] No CfgWorlds Names entries found at: " + locationConfigPath,"BDE_MapUI/Bounties/BDE_MapUI_BountyAreaLocator");
			return;
		}

		string worldNameLower = worldName;
		worldNameLower.ToLower();

		int loadedIndex = 0;
		for (int i = 0; i < classNamesCount; i++)
		{
			string locationClassName;
			GetGame().ConfigGetChildName(locationConfigPath, i, locationClassName);
			if (locationClassName == "")
				continue;

			string locationPath = locationConfigPath + " " + locationClassName;
			string locationType;
			string locationName;
			TFloatArray locationPosition = {};

			GetGame().ConfigGetText(locationPath + " type", locationType);
			GetGame().ConfigGetText(locationPath + " name", locationName);
			GetGame().ConfigGetFloatArray(locationPath + " position", locationPosition);

			if (locationName == "")
				continue;

			if (locationPosition == null || locationPosition.Count() != 2)
				continue;

			if (!ShouldIncludeType(locationType, include))
				continue;

			// Match Expansion's ChernarusPlus cleanup/special cases.
			if (worldNameLower == "chernarusplus")
			{
				if (locationClassName.Contains("ZalivGuba"))
					continue;

				if (locationClassName.Contains("Dubovo"))
					locationType = "Suburb";

				if (locationClassName.Contains("Novoselki"))
					locationType = "Suburb";

				if (locationClassName.Contains("Vysotovo"))
					locationType = "Suburb";
			}

			BDE_MapUI_BountyAreaZone zone = new BDE_MapUI_BountyAreaZone();
			zone.Map = worldNameLower;
			zone.Name = locationName;
			zone.Type = locationType;
			zone.ClassName = locationClassName;
			zone.PositionX = locationPosition[0];
			zone.PositionZ = locationPosition[1];
			zone.Radius = GetRadiusForLocationType(locationType);

			if (zone.Radius <= 0.0)
				continue;

			m_Zones.Insert(zone);
			loadedIndex++;
		}
	}

	private bool ShouldIncludeType(string locationType, int include)
	{
		if ((include & BDE_MapUI_LOC_CAMP) == 0 && locationType.Contains("Camp"))
			return false;

		if ((include & BDE_MapUI_LOC_HILL) == 0 && locationType.Contains("Hill"))
			return false;

		if ((include & BDE_MapUI_LOC_LOCAL) == 0 && locationType.Contains("Local"))
			return false;

		if ((include & BDE_MapUI_LOC_LOCALOFFICE) == 0 && locationType.Contains("LocalOffice"))
			return false;

		if ((include & BDE_MapUI_LOC_MARINE) == 0 && locationType.Contains("Marine"))
			return false;

		if ((include & BDE_MapUI_LOC_RAILROADSTATION) == 0 && locationType.Contains("RailroadStation"))
			return false;

		if ((include & BDE_MapUI_LOC_RUIN) == 0 && locationType.Contains("Ruin"))
			return false;

		if ((include & BDE_MapUI_LOC_SETTLEMENT) == 0 && IsSettlementType(locationType))
			return false;

		if ((include & BDE_MapUI_LOC_VIEWPOINT) == 0 && locationType.Contains("ViewPoint"))
			return false;

		return true;
	}

	private bool IsSettlementType(string locationType)
	{
		return locationType.Contains("Capital") || locationType.Contains("City") || locationType.Contains("Village");
	}

	private float GetRadiusForLocationType(string locationType)
	{
		if (locationType.Contains("Capital"))
			return 1000.0;

		if (locationType.Contains("City"))
			return 500.0;

		if (locationType.Contains("Village") || locationType.Contains("Suburb"))
			return 200.0;

		if (locationType.Contains("Camp"))
			return 100.0;

		if (locationType.Contains("Local"))
			return 100.0;

		return 100.0;
	}

	private BDE_MapUI_BountyAreaZone FindCurrentZone(vector position)
	{
		BDE_MapUI_BountyAreaZone nearestZone = null;
		float nearestDistanceSq = 999999999999.0;
		vector position2D = Vector(position[0], 0, position[2]);

		for (int i = 0; i < m_Zones.Count(); i++)
		{
			BDE_MapUI_BountyAreaZone zone = m_Zones.Get(i);
			if (!zone)
				continue;

			float distanceSq = vector.DistanceSq(position2D, zone.GetPosition2D());
			if (distanceSq <= nearestDistanceSq)
			{
				nearestDistanceSq = distanceSq;
				nearestZone = zone;
			}
		}

		if (nearestZone)
		{
			float radius = nearestZone.Radius;
			if (nearestDistanceSq <= radius * radius)
				return nearestZone;
		}

		return null;
	}
};
