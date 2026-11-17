class CfgPatches
{
    class BDE_ExpansionMarket_UI
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Scripts",
            "DayZExpansion_Vehicles_Scripts",
            "JM_CF_Scripts",
            "DayZExpansion_Core_Scripts",
            "DayZExpansion_Market_Scripts",
            "DayZExpansion_Market_GUI"
        };
    };
};

class CfgMods
{
    class BDE_ExpansionMarket_UI
    {
        dir = "BDE_ExpansionMarket_UI";
        name = "BDE Expansion Market UI";
        author = "BDE";
        type = "mod";
        dependencies[] =
        {
            "Game",
            "World",
            "Mission"
        };
        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] =
                {
                    "BDE_ExpansionMarket_UI/Scripts/3_Game"
                };
            };
            class worldScriptModule
            {
                value = "";
                files[] =
                {
                    "BDE_ExpansionMarket_UI/Scripts/4_World"
                };
            };
            class missionScriptModule
            {
                value = "";
                files[] =
                {
                    "BDE_ExpansionMarket_UI/Scripts/5_Mission"
                };
            };
        };
    };
};
