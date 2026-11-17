class CfgPatches
{
    class BDE_MapUI
    {
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "JM_CF_Scripts",
            "DayZExpansion_Core_Scripts",
            "DayZExpansion_Navigation_Scripts",
            "DayZExpansion_Groups_Scripts"
        };
        units[] = {};
        weapons[] = {};
    };
};

class CfgMods
{
    class BDE_MapUI
    {
        dir = "BDE_MapUI";
        name = "BDE Integrated Map UI";
        author = "BDE";
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                files[] = {"BDE_MapUI/Scripts/3_Game"};
            };
            class imageSets
            {
                files[] = {"BDE_MapUI/GUI/imageset/BDE_MapUI_Icons.imageset"};
            };
            class worldScriptModule
            {
                files[] = {"BDE_MapUI/Scripts/4_World"};
            };
            class missionScriptModule
            {
                files[] = {"BDE_MapUI/Scripts/5_Mission"};
            };
        };
    };
};
