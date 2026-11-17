class BDE_VL_RaidActionData: ActionData
{
    int BDE_StartTime;
    float BDE_Duration;
    float BDE_SuccessChance;
    float BDE_HealthEase;
    bool BDE_Started;
    bool BDE_Finished;
    bool BDE_Charged;
}

class BDE_VL_ActionPickVehicleLockCB: ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        ExpansionVehicle vehicle = ExpansionVehicle.Get(m_ActionData.m_Target.GetParentOrObject());
        float duration = 300;
        if (vehicle)
        {
            duration = Math.Max(1, vehicle.BDE_VL_GetRaidSeconds());
        }
        BDE_VL_RaidActionData data = BDE_VL_RaidActionData.Cast(m_ActionData);
        if (data && vehicle)
        {
            // Snapshot this attempt: repairs/damage during it affect the next attempt.
            data.BDE_Duration = duration;
            if (GetGame().IsServer())
            {
                data.BDE_SuccessChance = vehicle.BDE_VL_GetRaidChance();
                data.BDE_HealthEase = vehicle.BDE_VL_GetRaidHealthEase();
            }
        }
        m_ActionData.m_ActionComponent = new CAContinuousTime(duration);
    }
}

class BDE_VL_ActionPickVehicleLock: ActionContinuousBase
{
    void BDE_VL_ActionPickVehicleLock()
    {
        m_CallbackClass = BDE_VL_ActionPickVehicleLockCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "Pick vehicle lock";
    }

    override ActionData CreateActionData()
    {
        return new BDE_VL_RaidActionData();
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINonRuined;
        m_ConditionTarget = new CCTCursor;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!target || !item || !item.IsKindOf("Lockpick") || item.IsRuined())
        {
            return false;
        }
        if (!m_ConditionTarget.Can(player, target))
        {
            return false;
        }
        ExpansionVehicle vehicle = ExpansionVehicle.Get(target.GetParentOrObject());
        return vehicle && vehicle.BDE_VL_CanRaid() && vehicle.IsLocked();
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);
        BDE_VL_RaidActionData data = BDE_VL_RaidActionData.Cast(action_data);
        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        if (data && vehicle && ActionCondition(action_data.m_Player, action_data.m_Target, action_data.m_MainItem))
        {
            data.BDE_StartTime = GetGame().GetTime();
            if (data.BDE_Duration <= 0)
            {
                data.BDE_Duration = vehicle.BDE_VL_GetRaidSeconds();
                data.BDE_SuccessChance = vehicle.BDE_VL_GetRaidChance();
                data.BDE_HealthEase = vehicle.BDE_VL_GetRaidHealthEase();
            }
            data.BDE_Started = true;
        }
    }

    void BDE_ChargeAttempt(BDE_VL_RaidActionData data)
    {
        if (!data || !data.BDE_Started || data.BDE_Charged || !data.m_MainItem)
        {
            return;
        }
        data.BDE_Charged = true;
        float damage = data.m_MainItem.GetMaxHealth("", "Health") * BDE_VL_RaidConfig.Get().LockpickDamagePercent / 100;
        data.m_MainItem.AddHealth("", "Health", -damage);
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        BDE_VL_RaidActionData data = BDE_VL_RaidActionData.Cast(action_data);
        if (!data || !data.BDE_Started || data.BDE_Finished)
        {
            return;
        }
        // Server timing is authoritative even if a client alters its progress component.
        if (GetGame().GetTime() - data.BDE_StartTime + 100 < data.BDE_Duration * 1000)
        {
            return;
        }
        data.BDE_Finished = true;
        if (!ActionCondition(action_data.m_Player, action_data.m_Target, action_data.m_MainItem))
        {
            BDE_ChargeAttempt(data);
            return;
        }
        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetParentOrObject());
        BDE_VL_LockDifficulty family = BDE_VL_RaidConfig.GetFamily(vehicle.GetType());
        float chance = data.BDE_SuccessChance;
        bool success = Math.RandomFloat(0, 100) < chance;
        PlayerIdentity identity = action_data.m_Player.GetIdentity();
        if (success)
        {
            vehicle.BDE_VL_SetLocked(false);
            if (identity)
            {
                ExpansionNotification("Vehicle Lockpick", "Lockpick succeeded. The vehicle is now unlocked.").Success(identity);
            }
            else
            {
                action_data.m_Player.MessageStatus("Lockpick succeeded. The vehicle is now unlocked.");
            }
        }
        else
        {
            if (identity)
            {
                ExpansionNotification("Vehicle Lockpick", "Lockpick failed. The vehicle remains locked.").Error(identity);
            }
            else
            {
                action_data.m_Player.MessageStatus("Lockpick failed. The vehicle remains locked.");
            }
        }
        BDE_ChargeAttempt(data);
        Print("[BDE Vehicle Locks] Lockpick vehicle=" + vehicle.GetType() + " family=" + family.BaseClass + " seconds=" + data.BDE_Duration.ToString() + " healthEase=" + data.BDE_HealthEase.ToString() + " chance=" + chance.ToString() + " success=" + success.ToString());
    }

    override void OnEndServer(ActionData action_data)
    {
        // Cancelled/interrupted attempts also consume one attempt's tool health.
        BDE_ChargeAttempt(BDE_VL_RaidActionData.Cast(action_data));
        super.OnEndServer(action_data);
    }
}

modded class Lockpick
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(BDE_VL_ActionPickVehicleLock);
    }
}
