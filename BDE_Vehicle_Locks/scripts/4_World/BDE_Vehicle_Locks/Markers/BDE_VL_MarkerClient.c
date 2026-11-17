class BDE_VL_MarkerClient
{
    protected static ExpansionMarkerModule s_MarkerModule;
    protected static string s_CurrentMarkerName = BDE_VL_MARKER_INTERNAL_NAME;

    static void CreateMarker(vector position, string markerName, string markerIcon, int markerColor)
    {
        if (!g_Game.IsClient() || position == vector.Zero)
            return;

        DeleteMarker(markerName);

        if (!CF_Modules<ExpansionMarkerModule>.Get(s_MarkerModule))
            return;

        s_CurrentMarkerName = markerName;

        ExpansionPersonalMarkerData marker = new ExpansionPersonalMarkerData();
        marker.SetName(markerName);
        marker.SetIcon(markerIcon);
        marker.SetColor(markerColor);
        marker.SetPosition(position);
        marker.Set3D(true);
        marker.SetLockState(true);
        s_MarkerModule.CreateMarker(marker);
    }

    static void DeleteMarker(string markerName = "")
    {
        if (!g_Game.IsClient())
            return;

        if (!CF_Modules<ExpansionMarkerModule>.Get(s_MarkerModule))
            return;

        ExpansionMarkerClientData data = s_MarkerModule.GetData();
        if (!data)
            return;

        auto personalMarkers = data.PersonalGet();
        if (!personalMarkers)
            return;

        if (markerName == string.Empty)
            markerName = s_CurrentMarkerName;

        for (int i = personalMarkers.Count() - 1; i >= 0; i--)
        {
            ExpansionMarkerData marker = personalMarkers[i];
            if (!marker)
                continue;

            string currentName = marker.GetName();
            if (currentName == markerName || currentName == s_CurrentMarkerName || currentName == BDE_VL_MARKER_INTERNAL_NAME)
                data.PersonalRemove(i);
        }

        s_MarkerModule.Refresh();
        s_CurrentMarkerName = markerName;
    }
};
