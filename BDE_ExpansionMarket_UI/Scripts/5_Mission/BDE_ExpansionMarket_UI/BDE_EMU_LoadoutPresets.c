class BDE_EMU_SavedNode
{
    string SlotName;
    string ClassName;
    ref array<ref BDE_EMU_SavedNode> Children = new array<ref BDE_EMU_SavedNode>;
}

class BDE_EMU_SavedLoadout
{
    string Name;
    ref array<ref BDE_EMU_SavedNode> Nodes = new array<ref BDE_EMU_SavedNode>;
}

class BDE_EMU_PresetJob
{
    ref BDE_EMU_SavedNode Node;
    ref array<int> ParentPath = new array<int>;
}

class BDE_EMU_LoadoutPresets
{
    int Version = 1;
    ref array<ref BDE_EMU_SavedLoadout> Loadouts = new array<ref BDE_EMU_SavedLoadout>;
    ref array<ref BDE_EMU_SavedLoadout> Items = new array<ref BDE_EMU_SavedLoadout>;

    static BDE_EMU_LoadoutPresets Load(out bool writable)
    {
        BDE_EMU_LoadoutPresets data = new BDE_EMU_LoadoutPresets;
        writable = true;
        string path = "$profile:BDE/MarketUI/LoadoutPresets.json";
        if (FileExist(path))
        {
            string error;
            if (!JsonFileLoader<BDE_EMU_LoadoutPresets>.LoadFile(path, data, error) || !data)
            {
                Print("[BDE Loadout] Cannot read presets: " + error);
                writable = false;
                data = new BDE_EMU_LoadoutPresets;
                return data;
            }
        }
        // Append missing collections without resetting saved configurations.
        if (!data.Loadouts)
        {
            data.Loadouts = new array<ref BDE_EMU_SavedLoadout>;
        }
        if (!data.Items)
        {
            data.Items = new array<ref BDE_EMU_SavedLoadout>;
        }
        for (int loadoutIndex = data.Loadouts.Count() - 1; loadoutIndex >= 0; loadoutIndex--)
        {
            if (!data.Loadouts[loadoutIndex] || !data.Loadouts[loadoutIndex].Nodes)
            {
                data.Loadouts.Remove(loadoutIndex);
            }
        }
        for (int itemIndex = data.Items.Count() - 1; itemIndex >= 0; itemIndex--)
        {
            if (!data.Items[itemIndex] || !data.Items[itemIndex].Nodes)
            {
                data.Items.Remove(itemIndex);
            }
        }
        return data;
    }

    bool Save(out string error)
    {
        MakeDirectory("$profile:BDE");
        MakeDirectory("$profile:BDE/MarketUI");
        string path = "$profile:BDE/MarketUI/LoadoutPresets.json";
        string temporaryPath = "$profile:BDE/MarketUI/LoadoutPresets.pending.json";
        if (!JsonFileLoader<BDE_EMU_LoadoutPresets>.SaveFile(temporaryPath, this, error))
        {
            return false;
        }
        BDE_EMU_LoadoutPresets verified;
        if (!JsonFileLoader<BDE_EMU_LoadoutPresets>.LoadFile(temporaryPath, verified, error))
        {
            return false;
        }
        if (FileExist(path) && !CopyFile(path, "$profile:BDE/MarketUI/LoadoutPresets.backup.json"))
        {
            error = "Could not back up the existing presets.";
            return false;
        }
        if (!CopyFile(temporaryPath, path))
        {
            error = "Could not install the saved presets.";
            return false;
        }
        DeleteFile(temporaryPath);
        return true;
    }

    static BDE_EMU_SavedNode Capture(EntityAI parent, int slotId, int depth = 0)
    {
        BDE_EMU_SavedNode node = new BDE_EMU_SavedNode;
        node.SlotName = InventorySlots.GetSlotName(slotId);
        EntityAI item = parent.GetInventory().FindAttachment(slotId);
        if (!item)
        {
            return node;
        }
        node.ClassName = item.GetType();
        if (depth < 8)
        {
            for (int i = 0; i < item.GetInventory().GetAttachmentSlotsCount(); i++)
            {
                node.Children.Insert(Capture(item, item.GetInventory().GetAttachmentSlotId(i), depth + 1));
            }
        }
        return node;
    }
}
