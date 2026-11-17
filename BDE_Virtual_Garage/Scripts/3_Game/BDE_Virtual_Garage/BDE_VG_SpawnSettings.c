// Map-local spawn data. No native JSON deserializer is used on startup.
class BDE_VG_SpawnPosition
{
    vector Position;
    vector Direction;
};

class BDE_VG_SpawnSettings
{
    int Version = 1;
    ref array<ref BDE_VG_SpawnPosition> SpawnPositions = new array<ref BDE_VG_SpawnPosition>;
    protected static ref BDE_VG_SpawnSettings s_Instance;

    static BDE_VG_SpawnSettings Get()
    {
        return s_Instance;
    }

    static array<ref BDE_VG_SpawnPosition> ReadEntries(ExpansionJSONElementBase root)
    {
        array<ref BDE_VG_SpawnPosition> entries = new array<ref BDE_VG_SpawnPosition>;
        if (!root)
            return entries;
        ExpansionJSONElementBase list = root.Get("SpawnPositions");
        if (!list || !list.IsArray())
            return entries;

        for (int i = 0; i < list.Count(); i++)
        {
            ExpansionJSONElementBase node = list.Get(i);
            if (!node || !node.IsObject())
                continue;
            string positionText = BDE_VG_JSONIO.ReadString(node, "Position", string.Empty);
            string directionText = BDE_VG_JSONIO.ReadString(node, "Direction", string.Empty);
            if (positionText == string.Empty || directionText == string.Empty)
                continue;
            BDE_VG_SpawnPosition entry = new BDE_VG_SpawnPosition;
            entry.Position = positionText.ToVector();
            entry.Direction = directionText.ToVector();
            entries.Insert(entry);
        }
        return entries;
    }

    static bool LoadOrMigrate(array<ref BDE_VG_SpawnPosition> oldPositions)
    {
        if (s_Instance)
            return true;

        BDE_VG_SpawnSettings loaded = new BDE_VG_SpawnSettings;
        if (FileExist(BDE_VG_SPAWN_POSITIONS_FILE))
        {
            ExpansionJSON document = BDE_VG_JSONIO.ReadObject(BDE_VG_SPAWN_POSITIONS_FILE);
            if (!document)
            {
                Print("[BDE Virtual Garage] Invalid map-local SpawnPositions.json; existing file has NOT been overwritten.");
                return false;
            }
            loaded.Version = BDE_VG_JSONIO.ReadInt(document, "Version", 1);
            loaded.SpawnPositions = ReadEntries(document);
        }
        else
        {
            if (oldPositions)
            {
                foreach (BDE_VG_SpawnPosition oldEntry : oldPositions)
                {
                    if (oldEntry)
                        loaded.SpawnPositions.Insert(oldEntry);
                }
            }

            ExpansionJSONObject diskData = BDE_VG_JSONIO.MakeObject();
            BDE_VG_JSONIO.PutInt(diskData, "Version", 1);
            ExpansionJSONArray positions = new ExpansionJSONArray;
            foreach (BDE_VG_SpawnPosition entry : loaded.SpawnPositions)
            {
                ExpansionJSONObject item = BDE_VG_JSONIO.MakeObject();
                BDE_VG_JSONIO.PutString(item, "Position", entry.Position.ToString());
                BDE_VG_JSONIO.PutString(item, "Direction", entry.Direction.ToString());
                positions.Insert(item);
            }
            diskData.Set("SpawnPositions", positions);
            if (!BDE_VG_JSONIO.WriteObject(BDE_VG_SPAWN_POSITIONS_FILE, diskData))
            {
                Print("[BDE Virtual Garage] Could not create SpawnPositions.json; Settings.json is unchanged.");
                return false;
            }
        }

        s_Instance = loaded;
        Print("[BDE Virtual Garage] Loaded " + loaded.SpawnPositions.Count().ToString() + " map-local spawn position(s).");
        return true;
    }
};
