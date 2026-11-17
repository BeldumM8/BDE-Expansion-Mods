modded class MissionBase
{
    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        if (id == BDE_VG_MENU_ID)
            return new BDE_VG_Menu;
        return super.CreateScriptedMenu(id);
    }
};
