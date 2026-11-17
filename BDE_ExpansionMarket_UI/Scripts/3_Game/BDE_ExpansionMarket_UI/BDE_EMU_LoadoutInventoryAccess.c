// Only the loadout manager's synchronous attachment operations enter this scope.
class BDE_EMU_LoadoutInventoryAccess
{
    static EntityAI Root;

    static void Begin(EntityAI entity)
    {
        Root = entity;
        while (Root && Root.GetHierarchyParent())
        {
            Root = Root.GetHierarchyParent();
        }
    }

    static void End()
    {
        Root = null;
    }
}

