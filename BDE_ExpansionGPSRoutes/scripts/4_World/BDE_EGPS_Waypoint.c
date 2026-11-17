// Route waypoint copied from an Expansion marker.
// Position and name are cached so the route survives marker deletion/refresh.

class BDE_EGPS_Waypoint
{
    protected string m_MarkerUid;
    protected int    m_MarkerType;
    protected vector m_CachedPos;
    protected string m_CachedName;

    void BDE_EGPS_Waypoint(string uid, int type, vector pos, string name)
    {
        m_MarkerUid  = uid;
        m_MarkerType = type;
        m_CachedPos  = pos;
        m_CachedName = name;
    }

    string GetUid()         { return m_MarkerUid; }
    int    GetMarkerType()  { return m_MarkerType; }
    vector GetPos()         { return m_CachedPos; }
    string GetName()        { return m_CachedName; }

    string ToDebugString()
    {
        return string.Format("Waypoint[uid=%1 type=%2 pos=%3 name='%4']",
            m_MarkerUid, m_MarkerType, m_CachedPos.ToString(), m_CachedName);
    }
}
