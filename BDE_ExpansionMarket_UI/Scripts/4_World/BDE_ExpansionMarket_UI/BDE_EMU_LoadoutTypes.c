class BDE_EMU_LoadoutPlacement
{
    string Name = "Loadout Manager";
    string ClassName = "";
    string Loadout = "";
    vector Position;
    vector Orientation;
    bool SpawnEntity = true;
    bool Enabled = true;
}

class BDE_EMU_LoadoutSettings
{
    int Version = 2;
    ref array<ref BDE_EMU_LoadoutPlacement> Stations = new array<ref BDE_EMU_LoadoutPlacement>;
}

class BDE_EMU_LoadoutStation
{
    int Low;
    int High;
    string Name;
}

class BDE_EMU_LoadoutOffer
{
    int Kind; // 0 purchase, 1 empty, 2 keep owned item
    int TraderLow;
    int TraderHigh;
    int ItemID;
    string TraderName;
    string CategoryName;
    string ClassName;
    string PriceText;
    int Price;
    ref BDE_EMU_AmmoLine StartingAmmo;
    int SalePrice;
    int NetPrice;
    int ReplacedLow;
    int ReplacedHigh;
    string NetPriceText;
    string ReplacementText;
    int Stock;
    int RequiredRP;
    int TraderRequiredRP;
    bool CanBuy;
    string Reason;
}

class BDE_EMU_LoadoutUtil
{
    static string ConfigPath(string type)
    {
        if (GetGame().ConfigIsExisting("CfgWeapons " + type))
        {
            return "CfgWeapons " + type;
        }
        if (GetGame().ConfigIsExisting("CfgMagazines " + type))
        {
            return "CfgMagazines " + type;
        }
        return "CfgVehicles " + type;
    }

    static string DisplayName(string type)
    {
        string name;
        GetGame().ConfigGetText(ConfigPath(type) + " displayName", name);
        if (name == "")
        {
            return type;
        }
        return Widget.TranslateString(name);
    }

    static bool FitsSlot(string type, int slot)
    {
        TStringArray slots = new TStringArray;
        string path = ConfigPath(type) + " inventorySlot";
        if (GetGame().ConfigGetType(path) == CT_ARRAY)
        {
            GetGame().ConfigGetTextArray(path, slots);
        }
        else if (GetGame().ConfigGetType(path) == CT_STRING)
        {
            string singleSlot;
            GetGame().ConfigGetText(path, singleSlot);
            slots.Insert(singleSlot);
        }
        foreach (string name: slots)
        {
            if (InventorySlots.GetSlotIdFromString(name) == slot)
            {
                return true;
            }
        }
        return false;
    }

    static bool HasSlot(EntityAI parent, int slot)
    {
        if (!parent || !parent.GetInventory())
        {
            return false;
        }
        for (int i = 0; i < parent.GetInventory().GetAttachmentSlotsCount(); i++)
        {
            if (parent.GetInventory().GetAttachmentSlotId(i) == slot)
            {
                return true;
            }
        }
        return false;
    }

    // Only the player's equipped hierarchy is editable, never ground/cargo items.
    static bool IsEquippedParent(PlayerBase player, EntityAI parent)
    {
        EntityAI current = parent;
        for (int depth = 0; current && depth < 16; depth++)
        {
            if (current == player)
            {
                return true;
            }
            InventoryLocation location = new InventoryLocation;
            if (!current.GetInventory().GetCurrentInventoryLocation(location) || location.GetType() != InventoryLocationType.ATTACHMENT)
            {
                return false;
            }
            current = location.GetParent();
        }
        return false;
    }
}

class BDE_EMU_LoadoutTargetCondition: CCTBase
{
    override bool Can(PlayerBase player, ActionTarget target)
    {
        if (!target || !target.GetParentOrObject())
        {
            return false;
        }
        return vector.DistanceSq(player.GetPosition(), target.GetParentOrObject().GetPosition()) <= 16;
    }
}

class BDE_EMU_ActionLoadoutManager: ActionInteractBase
{
    void BDE_EMU_ActionLoadoutManager()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_ATTACHITEM;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_Text = "Open Loadout Manager";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new BDE_EMU_LoadoutTargetCondition;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || player.IsRestrained() || !target || !ExpansionMarketModule.GetInstance())
        {
            return false;
        }
        return ExpansionMarketModule.GetInstance().BDE_LM_IsStation(target.GetParentOrObject());
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);
        if (action_data.m_Target)
        {
            Print("[BDE Loadout] Client interaction executed.");
            ExpansionMarketModule.SI_BDELoadoutOpen.Invoke(action_data.m_Target.GetParentOrObject());
        }
    }
}

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);
        actions.Insert(BDE_EMU_ActionLoadoutManager);
    }
}

modded class PlayerBase
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(BDE_EMU_ActionLoadoutManager);
    }
}

// Slot paths start at the player. No client entity or price is trusted by checkout.
class BDE_EMU_LoadoutChange
{
    ref array<int> Slots = new array<int>;
    ref BDE_EMU_LoadoutOffer Offer;
}

class BDE_EMU_LoadoutCommit
{
    ref BDE_EMU_LoadoutChange Change;
    EntityAI OldItem;
    EntityAI Purchased;
    ExpansionTraderObjectBase Trader;
    ExpansionMarketItem Item;
    ref ExpansionMarketSell Sale;
}

class BDE_EMU_LoadoutPaths
{
    static EntityAI Resolve(EntityAI root, array<int> slots, int count)
    {
        EntityAI current = root;
        for (int index = 0; current && index < count; index++)
        {
            current = current.GetInventory().FindAttachment(slots[index]);
        }
        return current;
    }

    static bool Prefix(array<int> prefix, array<int> path)
    {
        if (prefix.Count() > path.Count())
        {
            return false;
        }
        for (int i = 0; i < prefix.Count(); i++)
        {
            if (prefix[i] != path[i])
            {
                return false;
            }
        }
        return true;
    }
}

class BDE_EMU_LoadoutQuoteContext
{
    static EntityAI Item;
}

class BDE_EMU_AmmoLine
{
    string ClassName;
    string Bullet;
    int Count;
    int Price;
    int TraderLow;
    int TraderHigh;
    int ItemID;
    int Stock;
}

class BDE_EMU_AmmoChange
{
    ref array<int> Slots = new array<int>;
    string MagazineClassName;
    int Mode; // 1 fill, 2 remove, 3 configured starting ammunition
    int PurchaseItemID;
    int PurchaseTraderLow;
    int PurchaseTraderHigh;
    int Low;
    int High;
    string OriginalSignature;
    int Price;
    ref array<ref BDE_EMU_AmmoLine> Lines = new array<ref BDE_EMU_AmmoLine>;
}

class BDE_EMU_AmmoCommit
{
    ref BDE_EMU_AmmoChange Change;
    Magazine MagazineEntity;
    bool SnapshotTaken;
    ref array<string> Bullets = new array<string>;
    ref array<float> Damage = new array<float>;
}

class BDE_EMU_AmmoUtil
{
    static string Signature(Magazine magazine)
    {
        if (!magazine)
        {
            return "";
        }
        string signature = magazine.GetAmmoCount().ToString();
        for (int i = 0; i < magazine.GetAmmoCount(); i++)
        {
            float damage;
            string bullet;
            magazine.GetCartridgeAtIndex(i, damage, bullet);
            signature += "/" + bullet + ":" + damage.ToString();
        }
        return signature;
    }
}

class BDE_EMU_CheckoutPlan
{
    int Token;
    int Expires;
    int Total;
    string Fingerprint;
    Object Station;
    PlayerBase Buyer;
    ExpansionMarketTraderZone Zone;
    ExpansionMarketTrader Currency;
    ExpansionMarketItem Payout;
    ref array<ref BDE_EMU_LoadoutCommit> Equipment;
    ref array<ref BDE_EMU_AmmoCommit> Ammunition;
    ref ExpansionMarketReserve Hold;
}
