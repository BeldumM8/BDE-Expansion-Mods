class BDE_VG_ExclusiveConsent
{
    string Token;
    string Signature;
    EntityAI Vehicle;
    EntityAI Access;
    bool Repair;
    int Expires;
};

class BDE_VG_ExclusivePolicy
{
    protected static ref BDE_VG_ExclusiveList s_Items;
    protected static ref BDE_VG_ExclusiveList s_Vehicles;

    static bool LoadList(string path, out BDE_VG_ExclusiveList settings)
    {
        if (!FileExist(path))
        {
            ref BDE_VG_ExclusiveList defaults = new BDE_VG_ExclusiveList;
            settings = defaults;
            string error;
            if (!JsonFileLoader<BDE_VG_ExclusiveList>.SaveFile(path, settings, error))
            {
                BDE_VG_Logger.Log("ERROR creating exclusive config " + path + ": " + error, true);
                return false;
            }
            return true;
        }
        string text;
        ExpansionJSON document = BDE_VG_JSONIO.ReadObjectWithText(path, text);
        if (!document || !document.Get("ClassNames").IsArray() || BDE_VG_JSONIO.ReadInt(document,"Version",0) != 1)
        {
            BDE_VG_Logger.Log("ERROR exclusive config needs Version 1 and ClassNames array: " + path, true);
            return false;
        }
        string readError;
        ref BDE_VG_ExclusiveList loaded;
        if (!ExpansionJsonFileParser<BDE_VG_ExclusiveList>.ReadFromString(text, loaded, readError) || !loaded || loaded.Version != 1 || !loaded.ClassNames)
        {
            BDE_VG_Logger.Log("ERROR invalid exclusive config; cross-server storage blocked: " + path + " " + readError, true);
            return false;
        }
        settings = loaded;
        return true;
    }

    static bool Reload()
    {
        // Server-local policy. Missing files are created; malformed files are
        // preserved and block cross-server storage, never interpreted as empty.
        bool itemsOK = LoadList(BDE_VG_PROFILE_DIRECTORY + "/ServerExclusiveItems.json", s_Items);
        bool vehiclesOK = LoadList(BDE_VG_PROFILE_DIRECTORY + "/ServerExclusiveVehicles.json", s_Vehicles);
        return itemsOK && vehiclesOK;
    }

    static bool Matches(EntityAI entity, BDE_VG_ExclusiveList settings)
    {
        if (!entity || !settings)
            return false;
        string typeName = entity.GetType();
        typeName.ToLower();
        foreach (string entry : settings.ClassNames)
        {
            string exact = entry;
            exact.ToLower();
            if (entry != string.Empty && (typeName == exact || (settings.MatchInheritedClasses && entity.IsKindOf(entry))))
                return true;
        }
        return false;
    }

    static bool VehicleBlocked(EntityAI vehicle)
    {
        return Matches(vehicle, s_Vehicles);
    }

    static void Scan(EntityAI vehicle, out array<EntityAI> restricted, out array<string> rows, out string signature)
    {
        ref array<EntityAI> foundItems = new array<EntityAI>;
        ref array<string> foundRows = new array<string>;
        restricted = foundItems;
        rows = foundRows;
        signature = string.Empty;
        array<EntityAI> inventory = new array<EntityAI>;
        vehicle.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        map<string, int> counts = new map<string, int>;
        map<string, string> names = new map<string, string>;
        array<string> identities = new array<string>;
        foreach (EntityAI item : inventory)
        {
            if (item == vehicle || !Matches(item, s_Items))
                continue;
            restricted.Insert(item);
            string typeName = item.GetType();
            counts.Set(typeName, counts.Get(typeName) + 1);
            names.Set(typeName, item.GetDisplayName());
            string identity = item.ToString() + "|" + typeName;
            ItemBase quantityItem = ItemBase.Cast(item);
            if (quantityItem)
                identity += "|" + quantityItem.GetQuantity().ToString();
            identities.Insert(identity);
        }
        identities.Sort();
        foreach (string objectID : identities)
            signature += objectID + "\n";
        array<string> classes = counts.GetKeyArray();
        classes.Sort();
        foreach (string className : classes)
            rows.Insert(counts.Get(className).ToString() + "x " + names.Get(className) + "  [" + className + "]");
    }

    static bool MoveRestricted(PlayerBase player, EntityAI vehicle, array<EntityAI> restricted)
    {
        ExpansionTemporaryOwnedContainer container;
        array<EntityAI> roots = new array<EntityAI>;
        foreach (EntityAI item : restricted)
        {
            bool contained = false;
            EntityAI ancestor = item.GetHierarchyParent();
            while (ancestor && ancestor != vehicle)
            {
                if (restricted.Find(ancestor) >= 0)
                {
                    contained = true;
                    break;
                }
                ancestor = ancestor.GetHierarchyParent();
            }
            if (!contained)
                roots.Insert(item);
        }
        foreach (EntityAI root : roots)
        {
            bool moved = false;
            // Like trader overflow, create another owned container if the
            // previous one is full. Never clone, delete or drop an item.
            for (int attempt = 0; attempt < 2 && !moved; attempt++)
            {
                if (!container)
                {
                    container = ExpansionTemporaryOwnedContainer.Cast(g_Game.CreateObjectEx("ExpansionTemporaryOwnedContainer", player.GetPosition(), ECE_PLACE_ON_SURFACE));
                    if (!container)
                        return false;
                    player.Expansion_SetTemporaryOwnedContainer(container);
                }
                container.ExpansionSetCanReceiveItems(true);
                moved = container.GetInventory().TakeEntityToCargo(InventoryMode.SERVER, root);
                container.ExpansionSetCanReceiveItems(false);
                if (moved)
                    moved = root.GetHierarchyParent() == container;
                if (!moved)
                {
                    if (container.IsEmpty())
                    {
                        g_Game.ObjectDelete(container);
                        container = null;
                        break; // The item cannot fit even an empty container.
                    }
                    container = null;
                }
            }
            if (!moved)
                return false;
        }
        if (roots.Count() > 0)
            ExpansionItemSpawnHelper.TemporaryStorageNotification(player.GetIdentity());
        return true;
    }
};
