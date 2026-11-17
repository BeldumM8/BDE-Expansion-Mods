// Small widget helpers shared by the UI hooks.

class BDE_EGPS_WidgetUtils
{

    // Depth-first search by widget name.
    static Widget FindChildByName(Widget parent, string name)
    {
        if (!parent) return null;
        Widget child = parent.GetChildren();
        while (child)
        {
            if (child.GetName() == name) return child;
            Widget found = FindChildByName(child, name);
            if (found) return found;
            child = child.GetSibling();
        }
        return null;
    }
}
