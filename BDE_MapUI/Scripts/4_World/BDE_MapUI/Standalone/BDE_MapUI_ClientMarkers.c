// Personal markers are client-owned Expansion data, not Hive player records.
// Use separate native persistence buckets for each endpoint AND world. Keep
// old unscoped buckets intact; their coordinates cannot safely be migrated.
#ifndef SERVER
#ifdef EXPANSIONMODNAVIGATION
modded class ExpansionMarkerModule
{
    // Expansion's m_CurrentData is intentionally weak. Normal collections are
    // owned by m_AllData; the temporary fallback needs its own strong owner.
    protected ref ExpansionMarkerClientData m_BDE_TemporaryMarkers;

    override void OnMissionLoaded(Class sender, CF_EventArgs args)
    {
        super.OnMissionLoaded(sender, args);
        if (!IsMissionClient())
            return;

        string address;
        int port;
        string worldName = g_Game.GetWorldName();
        worldName.ToLower();
        if (!g_Game.GetHostAddress(address, port) || address == string.Empty || worldName == string.Empty)
        {
            // Never reuse the previous connection's personal collection if
            // connection identity is temporarily unavailable.
            m_BDE_TemporaryMarkers = new ExpansionMarkerClientData;
            m_CurrentData = m_BDE_TemporaryMarkers;
            Print("[BDE_MapUI markers] Connection identity unavailable; using temporary empty personal markers.");
            return;
        }

        string namespaceID = "BDE_MARKERS_1|" + address + "|" + worldName;
        ref ExpansionMarkerClientData scopedData;
        foreach (ExpansionMarkerClientData candidate : m_AllData)
        {
            if (candidate && candidate.Equals(namespaceID, port))
            {
                scopedData = candidate;
                break;
            }
        }
        bool fresh = !scopedData;
        if (fresh)
        {
            scopedData = new ExpansionMarkerClientData;
            scopedData.m_IP = namespaceID;
            scopedData.m_Port = port;
            m_AllData.Insert(scopedData);
        }
        m_CurrentData = scopedData;
        m_BDE_TemporaryMarkers = null;
#ifdef EXPANSIONMODGROUPS
        // A fresh scope must not inherit a hidden party category from another
        // server. Subsequent user changes to the visibility controls are kept.
        if (fresh)
            SetVisibility(ExpansionMapMarkerType.PARTY, EXPANSION_MARKER_VIS_WORLD | EXPANSION_MARKER_VIS_MAP);
#endif
        SaveLocalServerMarkers();
        Refresh();
        Print("[BDE_MapUI markers] Using connection-local markers for " + worldName + "; personal=" + scopedData.PersonalCount().ToString());
    }
};

#ifdef EXPANSIONMODGROUPS
modded class ExpansionMarkerClientData
{
    protected ref array<string> m_BDE_CheckedPartyMarkers;

    override void OnRefresh()
    {
        super.OnRefresh();
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player || !player.IsPlayerLoaded() || !m_PartyModule || !m_PartyModule.GetParty())
            return;
        ExpansionPartyData party = m_PartyModule.GetParty();
        if (!party.m_MarkersSynced)
            return;
        if (!m_BDE_CheckedPartyMarkers)
            m_BDE_CheckedPartyMarkers = new array<string>;
        foreach (ExpansionMarkerData marker : party.GetAllMarkers())
        {
            if (!marker)
                continue;
            string markerID = marker.GetUID();
            ExpansionMarkerClientInfo clientInfo = m_MarkerInfo_Party.Get(markerID);
            if (!clientInfo)
            {
                clientInfo = new ExpansionMarkerClientInfo(markerID);
                m_MarkerInfo_Party.Insert(markerID, clientInfo);
            }
            // Apply the receiving member's own defaults immediately, including
            // the first refresh. Do not depend on the creator's local state.
            marker.ApplyVisibility(clientInfo.GetVisibility());
            if (m_BDE_CheckedPartyMarkers.Find(markerID) < 0)
            {
                m_BDE_CheckedPartyMarkers.Insert(markerID);
                if (!marker.IsMapVisible() || (m_MarkerModule && !m_MarkerModule.IsMapVisible(ExpansionMapMarkerType.PARTY)))
                    Print("[BDE_MapUI markers] Received party marker hidden by local visibility: " + markerID);
                if (marker.GetIconName() == string.Empty)
                {
                    marker.SetIcon(ExpansionIcons.Get("Map Marker"));
                    Print("[BDE_MapUI markers] Applied fallback icon to party marker " + markerID);
                }
            }
        }
    }
};
#endif
#endif
#endif
