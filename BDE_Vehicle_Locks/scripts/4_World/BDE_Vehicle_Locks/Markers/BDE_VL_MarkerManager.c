class BDE_VL_MarkerManager
{
    static void SendMarker(PlayerIdentity identity, vector position)
    {
        if (!identity)
            return;

        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings)
            return;

        if (!settings.EnableVehicleMarkers)
            position = vector.Zero;

        GetRPCManager().SendRPC(BDE_VL_RPC_NAMESPACE,BDE_VL_RPC_MARKER_UPDATE,new Param4<vector, string, string, int>(position, settings.MarkerName, settings.MarkerIcon, settings.MarkerColor),true,identity);
    }

    static bool HasMarkerAccess(PlayerBase player, ExpansionVehicle vehicle)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleMarkers || !player || !player.GetIdentity() || !vehicle)
            return false;

        if (settings.MarkerRequiresVehicleAccess && !vehicle.BDE_VL_CanAccess(player))
            return false;

        return true;
    }

    static bool CanCreateMarker(PlayerBase player, ExpansionVehicle vehicle, int seatIndex)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !HasMarkerAccess(player, vehicle))
            return false;

        if (settings.MarkerDriverOnly && seatIndex != DayZPlayerConstants.VEHICLESEAT_DRIVER)
            return false;

        return true;
    }

    static void OnEnteredVehicle(PlayerBase player, ExpansionVehicle vehicle, int seatIndex)
    {
        if (!HasMarkerAccess(player, vehicle))
            return;

        string vehiclePersistentID = vehicle.GetPersistentIDString();
        string lastVehiclePersistentID = player.BDE_VL_GetLastVehiclePersistentID();

        if (lastVehiclePersistentID == vehiclePersistentID)
            SendMarker(player.GetIdentity(), vector.Zero);

        if (!BDE_VL_Settings.Get().MarkerDriverOnly || seatIndex == DayZPlayerConstants.VEHICLESEAT_DRIVER)
            player.BDE_VL_SetLastVehiclePersistentID(vehiclePersistentID);
    }

    static void OnExitedVehicle(PlayerBase player, ExpansionVehicle vehicle, int seatIndex)
    {
        if (!CanCreateMarker(player, vehicle, seatIndex))
            return;

        player.BDE_VL_SetLastVehiclePersistentID(vehicle.GetPersistentIDString());
        SendMarker(player.GetIdentity(), vehicle.GetPosition());
    }

    static void RestoreMarker(PlayerBase player)
    {
        if (!player || !player.GetIdentity())
            return;

        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (!settings || !settings.EnableVehicleMarkers)
        {
            SendMarker(player.GetIdentity(), vector.Zero);
            return;
        }

        ExpansionVehicle currentVehicle = ExpansionVehicle.Get(player);
        if (currentVehicle)
        {
            SendMarker(player.GetIdentity(), vector.Zero);
            return;
        }

        ExpansionVehicle vehicle = ExpansionVehicle.BDE_VL_FindByPersistentID(player.BDE_VL_GetLastVehiclePersistentID());
        if (!vehicle)
        {
            SendMarker(player.GetIdentity(), vector.Zero);
            return;
        }

        if (settings.MarkerRequiresVehicleAccess && !vehicle.BDE_VL_CanAccess(player))
        {
            SendMarker(player.GetIdentity(), vector.Zero);
            return;
        }

        SendMarker(player.GetIdentity(), vehicle.GetPosition());
    }
};
