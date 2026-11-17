class CfgPatches
{
    class BDE_Virtual_Garage
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "JM_CF_Scripts",
            "DayZExpansion_Core_Scripts",
            "DayZExpansion_Objects_TemporaryOwnedContainer",
            "DayZExpansion_Vehicles_Scripts",
            "DayZExpansion_Groups_Scripts",
            "DayZExpansion_Garage_Scripts",
            "DayZExpansion_Market_Scripts",
            "BDE_Vehicle_Locks",
            "Parking_Meter"
        };
    };
};

class CfgMods
{
    class BDE_Virtual_Garage
    {
        storageVersion = 1;
        dir = "BDE_Virtual_Garage";
        name = "BDE Virtual Garage";
        credits = "Beldum_M8";
        author = "Beldum_M8";
        authorID = "0";
        version = "2.10.0";
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"BDE_Virtual_Garage/Scripts/3_Game"};
            };

            class worldScriptModule
            {
                value = "";
                files[] = {"BDE_Virtual_Garage/Scripts/4_World"};
            };

            class missionScriptModule
            {
                value = "";
                files[] = {"BDE_Virtual_Garage/Scripts/5_Mission"};
            };
        };
    };
};
