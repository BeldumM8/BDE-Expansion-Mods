// Shared per-server heartbeats provide cluster-wide presence without one server
// setting every other server's player records offline during startup/shutdown.
class BDE_MapUI_HivePresence
{
    static const string DIRECTORY = "$profile:/BDE/MapUI/PlayerDatabase/_Presence";
    static const string IDENTITY = "$profile:/BDE/MapUI/_Local/ServerIdentity.txt";
    private string m_ServerID;
    private string m_MapName;
    private float m_Timer;
    private int m_CacheEpoch;
    private bool m_EmptyPublished;
    private ref map<string, ref BDE_MapUI_OnlinePlayerPresence> m_OnlineCache = new map<string, ref BDE_MapUI_OnlinePlayerPresence>;
    private ref map<string, ref BDE_MapUI_ServerPresence> m_Servers = new map<string, ref BDE_MapUI_ServerPresence>;

    void BDE_MapUI_HivePresence()
    {
        ExpansionStatic.MakeDirectoryRecursive(DIRECTORY);
        ExpansionStatic.MakeDirectoryRecursive("$profile:/BDE/MapUI/_Local");
        FileHandle file = OpenFile(IDENTITY, FileMode.READ);
        if (file)
        {
            FGets(file, m_ServerID);
            CloseFile(file);
            m_ServerID = m_ServerID.Trim();
        }
        if (!BDE_MapUI_HiveBountyEvents.SafeID(m_ServerID))
        {
            int uuid[4];
            UUIDApi.Generate(uuid);
            m_ServerID = UUIDApi.FormatString(uuid);
            file = OpenFile(IDENTITY, FileMode.WRITE);
            if (file)
            {
                FPrintln(file, m_ServerID);
                CloseFile(file);
            }
        }
        m_MapName = g_Game.GetWorldName();
        string displayName;
        if (g_Game.ConfigGetText("CfgWorlds " + m_MapName + " description", displayName) && displayName != "")
            m_MapName = displayName;
        Publish();
        Reload();
    }

    string ServerID()
    {
        return m_ServerID;
    }
    string MapName()
    {
        return m_MapName;
    }

    void Update(float timeslice)
    {
        m_Timer += timeslice;
        if (m_Timer >= 30.0)
        {
            m_Timer = 0;
            Publish();
            Reload();
        }
    }

    void Publish(string excludeUID = "", bool empty = false)
    {
        BDE_MapUI_ServerPresence snapshot = new BDE_MapUI_ServerPresence;
        snapshot.ServerID = m_ServerID;
        snapshot.MapName = m_MapName;
        snapshot.Epoch = CF_Date.Now(true).DateToEpoch();
        array<Man> players = new array<Man>;
        if (!empty)
            g_Game.GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity() || player.GetIdentity().GetId() == excludeUID)
                continue;
            BDE_MapUI_PresencePlayer entry = new BDE_MapUI_PresencePlayer;
            entry.UID = player.GetIdentity().GetId();
            entry.Name = player.GetIdentity().GetName();
            snapshot.Players.Insert(entry);
        }
        // Empty presence has no lease to renew. Keep the runtime cache clear,
        // but do not generate timestamp-only Hive traffic on an idle server.
        m_Servers.Set(m_ServerID, snapshot);
        m_CacheEpoch = 0;
        string ownPath = DIRECTORY + "/" + m_ServerID + ".json";
        string error;
        bool isEmpty = snapshot.Players.Count() == 0;
        if (isEmpty)
        {
            if (m_EmptyPublished)
                return;
            if (!FileExist(ownPath))
            {
                m_EmptyPublished = true;
                return;
            }
            // On restart an already-empty file needs no rewrite. A previous
            // populated file must be cleared once, even before anyone joins.
            BDE_MapUI_ServerPresence previous;
            if (JsonFileLoader<BDE_MapUI_ServerPresence>.LoadFile(ownPath, previous, error) && previous && previous.ServerID == m_ServerID && previous.Players && previous.Players.Count() == 0)
            {
                m_EmptyPublished = true;
                return;
            }
        }
        if (!JsonFileLoader<BDE_MapUI_ServerPresence>.SaveFile(ownPath, snapshot, error))
        {
            Print("[BDE_MapUI presence] Heartbeat save failed: " + error);
            return;
        }
        // Set only after successful persistence, so failed offline publication
        // retries and the last player's departure is not silently dropped.
        m_EmptyPublished = isEmpty;
    }

    void Reload()
    {
        m_CacheEpoch = 0;
        string name;
        int attributes;
        FindFileHandle handle = CF.FindFileEx(DIRECTORY + "/*.json", name, attributes, FindFileFlags.ALL);
        if (handle)
        {
            bool found = true;
            while (found)
            {
                if (!(attributes & FileAttr.DIRECTORY) && name.Length() == 41 && name.Substring(36, 5) == ".json")
                {
                    string id = name.Substring(0, 36);
                    if (id != m_ServerID && BDE_MapUI_HiveBountyEvents.SafeID(id))
                    {
                        BDE_MapUI_ServerPresence snapshot;
                        string error;
                        if (JsonFileLoader<BDE_MapUI_ServerPresence>.LoadFile(DIRECTORY + "/" + name, snapshot, error) && snapshot && snapshot.ServerID == id && snapshot.MapName != "" && snapshot.Epoch > 0 && snapshot.Players)
                            m_Servers.Set(id, snapshot);
                    }
                }
                found = FindNextFile(handle, name, attributes);
            }
            CloseFindFile(handle);
        }
        int now = CF_Date.Now(true).DateToEpoch();
        for (int i = m_Servers.Count() - 1; i >= 0; i--)
        {
            BDE_MapUI_ServerPresence current = m_Servers.GetElement(i);
            if (now - current.Epoch > 120 || current.Epoch > now + 120)
                m_Servers.Remove(m_Servers.GetKey(i));
        }
    }

    private void RefreshOnlineCache()
    {
        int now = CF_Date.Now(true).DateToEpoch();
        if (m_CacheEpoch == now)
            return;
        m_CacheEpoch = now;
        m_OnlineCache.Clear();
        ref map<string, ref BDE_MapUI_OnlinePlayerPresence> latest = m_OnlineCache;
        for (int i = 0; i < m_Servers.Count(); i++)
        {
            BDE_MapUI_ServerPresence snapshot = m_Servers.GetElement(i);
            if (now - snapshot.Epoch > 120 || snapshot.Epoch > now + 120)
                continue;
            foreach (BDE_MapUI_PresencePlayer player : snapshot.Players)
            {
                if (!player || player.UID == "")
                    continue;
                BDE_MapUI_OnlinePlayerPresence current;
                if (latest.Find(player.UID, current) && current.Epoch > snapshot.Epoch)
                    continue;
                BDE_MapUI_OnlinePlayerPresence entry = new BDE_MapUI_OnlinePlayerPresence;
                entry.UID = player.UID;
                entry.Name = player.Name;
                entry.ServerID = snapshot.ServerID;
                entry.MapName = snapshot.MapName;
                entry.Epoch = snapshot.Epoch;
                latest.Set(entry.UID, entry);
            }
        }
    }

    array<ref BDE_MapUI_OnlinePlayerPresence> OnlinePlayers()
    {
        RefreshOnlineCache();
        ref map<string, ref BDE_MapUI_OnlinePlayerPresence> latest = m_OnlineCache;
        ref array<ref BDE_MapUI_OnlinePlayerPresence> result = new array<ref BDE_MapUI_OnlinePlayerPresence>;
        for (int j = 0; j < latest.Count(); j++)
            result.Insert(latest.GetElement(j));
        return result;
    }

    bool Find(string uid, out BDE_MapUI_OnlinePlayerPresence presence)
    {
        RefreshOnlineCache();
        return m_OnlineCache.Find(uid, presence);
    }
};
