modded class ActionGetInTransport
{
    override void OnEndServer(ActionData action_data)
    {
        super.OnEndServer(action_data);

        if (!action_data.m_Player || !action_data.m_Target)
            return;

        ExpansionVehicle vehicle = ExpansionVehicle.Get(action_data.m_Target.GetObject());
        if (!vehicle)
            return;

        int componentIndex = action_data.m_Target.GetComponentIndex();
        int seatIndex = vehicle.CrewPositionIndex(componentIndex);
        BDE_VL_MarkerManager.OnEnteredVehicle(action_data.m_Player, vehicle, seatIndex);
    }
};

modded class ActionGetOutTransport
{
    override void OnEndServer(ActionData action_data)
    {
        GetOutTransportActionData getOutData = GetOutTransportActionData.Cast(action_data);
        ExpansionVehicle vehicle;
        int seatIndex = -1;

        if (getOutData)
        {
            vehicle = ExpansionVehicle.Get(getOutData.m_Vehicle);
            seatIndex = getOutData.m_Expansion_SeatIdx;
        }

        super.OnEndServer(action_data);

        if (vehicle && action_data.m_Player)
            BDE_VL_MarkerManager.OnExitedVehicle(action_data.m_Player, vehicle, seatIndex);
    }
};
