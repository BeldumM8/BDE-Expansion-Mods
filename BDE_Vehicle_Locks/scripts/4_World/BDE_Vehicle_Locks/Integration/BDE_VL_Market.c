modded class ExpansionMarketModule
{
    override array<Object> Spawn(ExpansionMarketReserve reserve, PlayerBase player, inout EntityAI parent, bool includeAttachments = true, int skinIndex = -1, inout bool attachmentNotAttached = false, TStringIntMap spawnedAmounts = null)
    {
        array<Object> objects = super.Spawn(reserve, player, parent, includeAttachments, skinIndex, attachmentNotAttached, spawnedAmounts);

        if (!g_Game.IsServer() || !player)
            return objects;

        foreach (Object spawnedObject : objects)
        {
            ExpansionVehicle vehicle = ExpansionVehicle.Get(spawnedObject);
            if (vehicle)
                vehicle.BDE_VL_SetMarketOwner(player);
        }

        return objects;
    }
};

// BDE ownership replaces master keys. Include an owned vehicle even when the
// buyer has not driven it yet. Lock state does not prevent an authorized sale.
modded class ExpansionMarketPlayerInventory
{
    override array<EntityAI> GetNearbyDrivenVehicles(string className = "", int amount = -1)
    {
        array<EntityAI> vehicles = new array<EntityAI>();
        if (!m_Player || !m_Player.GetIdentity())
        {
            return vehicles;
        }
        string playerUID = m_Player.GetIdentity().GetId();

        string requestedClass = className;
        requestedClass.ToLower();
        auto node = ExpansionVehicle.s_All.m_Head;
        while (node)
        {
            ExpansionVehicle vehicle = node.m_Value;
            node = node.m_Next;
            if (!vehicle || vehicle.BDE_VL_GetMarketOwnerUID() != playerUID)
            {
                continue;
            }
            string vehicleClass = vehicle.GetType();
            vehicleClass.ToLower();
            if (requestedClass != "" && requestedClass != vehicleClass)
            {
                continue;
            }
            EntityAI entity = vehicle.GetEntity();
            if (!entity || entity.IsSetForDeletion() || vehicle.GetCrew().Count() > 0 || !IsVehicleNearby(entity))
            {
                continue;
            }
            if (vehicles.Find(entity) == -1)
            {
                vehicles.Insert(entity);
            }
        }
        if (amount > 0)
        {
            while (vehicles.Count() > amount)
            {
                vehicles.Remove(vehicles.Count() - 1);
            }
        }
        return vehicles;
    }
};
