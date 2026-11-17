class BDE_VG_ActionOpenGarage : ActionInteractBase
{
    void BDE_VG_ActionOpenGarage()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_ATTACHITEM;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_Text = "Access BDE Virtual Garage";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(BDE_VG_ACCESS_ACTION_DISTANCE);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || player.GetCommand_Vehicle() || !target)
            return false;

        return ResolveAccessObject(target) != null;
    }

    protected Object ResolveAccessObject(ActionTarget target)
    {
        if (!target || !BDE_VG_Module.s_Instance)
        {
            return null;
        }

        Object targetObject = target.GetObject();
        if (BDE_VG_Module.s_Instance.IsGarageActionTarget(targetObject))
        {
            return targetObject;
        }

        Object parentObject = target.GetParent();
        if (parentObject != targetObject && BDE_VG_Module.s_Instance.IsGarageActionTarget(parentObject))
        {
            return parentObject;
        }

        return null;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);
        if (!action_data || !action_data.m_Target)
            return;

        Object targetObject = ResolveAccessObject(action_data.m_Target);
        BDE_VG_Module module;
        Class.CastTo(module,CF_ModuleCoreManager.Get(BDE_VG_Module));
        if (!module || !module.SetClientAccessObject(targetObject))
            return;

        g_Game.GetUIManager().EnterScriptedMenu(BDE_VG_MENU_ID,null);
    }
};

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);
        actions.Insert(BDE_VG_ActionOpenGarage);
    }
};
