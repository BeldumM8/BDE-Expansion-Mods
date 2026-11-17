// Do NOT mod ExpansionMarketSettings here. Its JsonFileLoader<ExpansionMarketSettings>
// calls use "this" and Enforce's modded-class type rewriting can make that generic
// call fail with "Incompatible parameter 'this'" inside ExpansionMarketSettings.c.
// Use the official zone JSON files and public Market API instead.
class BDE_VG_TraderZones
{
    protected static ref array<ref ExpansionMarketTraderZone> s_LocalZones;

    static void Reload()
    {
        s_LocalZones = new array<ref ExpansionMarketTraderZone>;
        if (!g_Game || !g_Game.IsServer())
            return;

        if (!FileExist(EXPANSION_TRADER_ZONES_FOLDER))
            return;

        TStringArray files = ExpansionStatic.FindFilesInLocation(EXPANSION_TRADER_ZONES_FOLDER,".json",true);
        foreach (string fileName : files)
        {
            if (fileName.Length() <= 5)
                continue;
            string zoneName = fileName.Substring(0,fileName.Length() - 5);
            ExpansionMarketTraderZone zone = ExpansionMarketTraderZone.Load(zoneName);
            if (IsLocalZone(zone))
                s_LocalZones.Insert(zone);
        }
    }

    protected static bool IsLocalZone(ExpansionMarketTraderZone zone)
    {
        if (!zone || zone.Radius <= 0)
            return false;
        string name = zone.m_FileName;
        name.ToLower();
        // Expansion's stock-only "World" zone is not a physical trader location.
        if (name == "world" && zone.Radius >= 1000.0)
            return false;
        return true;
    }

    static ExpansionMarketTraderZone GetLocalTraderZoneByPosition(vector position)
    {
        if (!s_LocalZones)
            Reload();

        ExpansionMarketTraderZone bestZone;
        float bestDistance = -1.0;
        foreach (ExpansionMarketTraderZone zone : s_LocalZones)
        {
            float distance = vector.Distance(zone.Position,position);
            if (distance > zone.Radius)
                continue;
            if (!bestZone || zone.Radius < bestZone.Radius || (zone.Radius == bestZone.Radius && distance < bestDistance))
            {
                bestZone = zone;
                bestDistance = distance;
            }
        }

        // Also consider dynamically registered zones (AddMarketZone) not backed
        // by a file. The public API can return the catch-all "World" zone;
        // never use that fallback to make base terminals public trader terminals.
        ExpansionMarketTraderZone dynamicZone = GetExpansionSettings().GetMarket().GetTraderZoneByPosition(position);
        if (IsLocalZone(dynamicZone))
        {
            float dynamicDistance = vector.Distance(dynamicZone.Position,position);
            if (dynamicDistance <= dynamicZone.Radius && (!bestZone || dynamicZone.Radius < bestZone.Radius))
                bestZone = dynamicZone;
        }
        return bestZone;
    }
};
