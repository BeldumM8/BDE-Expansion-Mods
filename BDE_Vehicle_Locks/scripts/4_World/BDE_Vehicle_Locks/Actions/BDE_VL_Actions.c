class BDE_VL_ActionClaimVehicle : ActionInteractBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(3.0);
    }

    override string GetText()
    {
        return "Claim Vehicle";
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        if (!vehicle)
            return false;

        return !vehicle.BDE_VL_HasOwner() && !vehicle.HasKey();
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (vehicle)
            vehicle.BDE_VL_ClaimUnowned(action_data.m_Player);
    }
};

class BDE_VL_ActionClaimWithExpansionKey : ActionInteractBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(3.0);
    }

    override string GetText()
    {
        return "Claim with Expansion Key";
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        if (!vehicle || vehicle.BDE_VL_HasOwner() || !vehicle.HasKey())
            return false;

        ExpansionCarKey key = ExpansionCarKey.Cast(player.GetItemInHands());
        return key && !key.IsInherited(ExpansionCarAdminKey) && vehicle.IsPairedTo(key);
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (vehicle)
            vehicle.BDE_VL_ClaimWithExpansionKey(action_data.m_Player);
    }
};

class BDE_VL_ActionLockVehicle : ActionInteractBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(3.0);
    }

    override string GetText()
    {
        return "Lock Vehicle";
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        if (!vehicle)
            return false;

        return vehicle.BDE_VL_HasOwner() && !vehicle.IsLocked() && !vehicle.BDE_VL_HasCrew();
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (!vehicle)
            return;

        if (!vehicle.BDE_VL_CanAccess(action_data.m_Player))
        {
            ExpansionNotification("Vehicle Locks", "You are not the owner or a member of the owner's group.").Error(action_data.m_Player.GetIdentity());
            return;
        }

        if (vehicle.BDE_VL_HasCrew())
        {
            ExpansionNotification("Vehicle Locks", "The vehicle cannot be locked while someone is inside.").Error(action_data.m_Player.GetIdentity());
            return;
        }

        vehicle.BDE_VL_SetLocked(true);
    }
};

class BDE_VL_ActionUnlockVehicle : ActionInteractBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(3.0);
    }

    override string GetText()
    {
        return "Unlock Vehicle";
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        if (!vehicle)
            return false;

        return vehicle.BDE_VL_HasOwner() && vehicle.IsLocked();
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (!vehicle)
            return;

        if (!vehicle.BDE_VL_CanAccess(action_data.m_Player))
        {
            ExpansionNotification("Vehicle Locks", "You are not the owner or a member of the owner's group.").Error(action_data.m_Player.GetIdentity());
            return;
        }

        vehicle.BDE_VL_SetLocked(false);
    }
};

class BDE_VL_ActionAdminResetOwner : ActionInteractBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(3.0);
    }

    override string GetText()
    {
        return "[Admin] Reset Vehicle Owner";
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableAdminActions)
            return false;

        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        if (!vehicle || !vehicle.BDE_VL_HasOwner())
            return false;

        #ifdef JM_COT
        return GetCommunityOnlineToolsBase().IsActive(player);
        #else
        return false;
        #endif
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        #ifdef JM_COT
        if (!GetCommunityOnlineToolsBase().IsActive(action_data.m_Player))
            return;
        #else
        return;
        #endif

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (!vehicle)
            return;

        if (vehicle.HasKey())
            vehicle.ResetKeyPairing();

        vehicle.BDE_VL_ResetOwner();
        ExpansionNotification("Vehicle Ownership Reset", "This vehicle is now unclaimed.").Success(action_data.m_Player.GetIdentity());
    }
};

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);

        actions.Insert(BDE_VL_ActionClaimVehicle);
        actions.Insert(BDE_VL_ActionPickVehicleLock);
        actions.Insert(BDE_VL_ActionClaimWithExpansionKey);
        actions.Insert(BDE_VL_ActionLockVehicle);
        actions.Insert(BDE_VL_ActionUnlockVehicle);
        actions.Insert(BDE_VL_ActionAdminResetOwner);
    }
};
