class CfgPatches
{
    class BDE_Vehicle_Locks
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
            "DayZExpansion_Vehicles_Scripts",
            "DayZExpansion_Groups_Scripts",
            "DayZExpansion_Navigation_Scripts",
            "DayZExpansion_Market_Scripts"
        };
    };
};

class CfgMods
{
    class BDE_Vehicle_Locks
    {
        storageVersion = 1;
        dir = "BDE_Vehicle_Locks";
        picture = "";
        action = "";
        hideName = 0;
        hidePicture = 0;
        name = "BDE Vehicle Locks";
        credits = "Beldum_M8";
        author = "Beldum_M8";
        authorID = "0";
        version = "1.0.6";
        extra = 0;
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"BDE_Vehicle_Locks/scripts/3_Game"};
            };

            class worldScriptModule
            {
                value = "";
                files[] = {"BDE_Vehicle_Locks/scripts/4_World"};
            };

            class missionScriptModule
            {
                value = "";
                files[] = {"BDE_Vehicle_Locks/scripts/5_Mission"};
            };
        };
    };
};
