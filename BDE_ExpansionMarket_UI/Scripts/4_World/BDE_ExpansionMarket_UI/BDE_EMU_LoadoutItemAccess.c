modded class ItemBase
{
    override bool AreChildrenAccessible()
    {
        if (super.AreChildrenAccessible())
        {
            return true;
        }
        if (!BDE_EMU_LoadoutInventoryAccess.Root)
        {
            return false;
        }
        EntityAI current = this;
        InventoryLocation location = new InventoryLocation;
        while (current)
        {
            if (current.GetInventory().GetCurrentInventoryLocation(location) && location.IsValid())
            {
                if (location.GetType() == InventoryLocationType.CARGO || location.GetType() == InventoryLocationType.PROXYCARGO)
                {
                    return false;
                }
            }
            if (current == BDE_EMU_LoadoutInventoryAccess.Root)
            {
                return true;
            }
            current = current.GetHierarchyParent();
        }
        return false;
    }
}
