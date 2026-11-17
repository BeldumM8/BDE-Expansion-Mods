// Shared setup follows the vanilla inspect menu's item view and camera distance.
class BDE_MapUI_Preview
{
    static void SetItem(ItemPreviewWidget preview, EntityAI item)
    {
        if (!preview)
        {
            Print("[BDE_MapUI preview-v5] ERROR: missing item preview widget");
            return;
        }

        preview.SetItem(item);
        if (!item)
        {
            return;
        }

        preview.SetView(item.GetViewIndex());
        preview.SetModelPosition("0 0 1");
        preview.SetModelOrientation("0 0 0");
        preview.SetColor(ARGB(255, 255, 255, 255));
        preview.Update();
        Print("[BDE_MapUI preview-v5] Bind " + preview.GetName() + " item=" + item.GetType() + " view=" + item.GetViewIndex().ToString());
    }
}
