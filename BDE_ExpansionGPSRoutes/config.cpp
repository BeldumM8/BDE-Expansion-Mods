class CfgPatches
{
    class BDE_ExpansionGPSRoutes
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "RoadGraph_Core",
            "DayZExpansion_Core_Scripts",
            "DayZExpansion_Navigation_Scripts"
        };
        author = "BDE / Return";
        version = "1.0.1";
    };
};

class CfgMods
{
    class BDE_ExpansionGPSRoutes
    {
        dir = "BDE_ExpansionGPSRoutes";
        picture = "";
        action = "";
        hideName = 0;
        hidePicture = 0;
        name = "BDE Expansion GPS Routes";
        credits = "Original GPS route logic by Return. Expansion port by BDE.";
        author = "BDE";
        authorID = "0";
        version = "1.0.1";
        extra = 0;
        type = "mod";
        inputs = "BDE_ExpansionGPSRoutes/inputs.xml";
        dependencies[] = { "Game", "World", "Mission" };
        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = { "BDE_ExpansionGPSRoutes/scripts/3_Game" };
            };
            class worldScriptModule
            {
                value = "";
                files[] = { "BDE_ExpansionGPSRoutes/scripts/4_World" };
            };
            class missionScriptModule
            {
                value = "";
                files[] = { "BDE_ExpansionGPSRoutes/scripts/5_Mission" };
            };
        };
    };
};
