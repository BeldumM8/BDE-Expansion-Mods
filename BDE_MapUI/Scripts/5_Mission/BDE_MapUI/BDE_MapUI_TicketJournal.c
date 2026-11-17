// Shared immutable records avoid whole-conversation last-writer-wins loss.
class BDE_MapUI_TicketEvent
{
    int Format = 1;
    string ID;
    string TicketKey;
    string Kind;
    string OriginServer;
    string MapName;
    string ActorUID;
    string ActorName;
    string CreatedUTC;
    int Epoch;
    int Revision;
    bool IsAdmin;
    bool Migration;
    ref BDE_MapUI_Report TicketSnapshot;
    string Text;
    ref array<string> SeenIDs = new array<string>;
};

class BDE_MapUI_TicketJournal
{
    static const string SHARED = "$profile:/BDE/MapUI/Tickets";
    static const string LOCAL = "$profile:/BDE/MapUI/_Local/Tickets";
    static const string OUTBOX = "$profile:/BDE/MapUI/_Local/Tickets/PublishOutbox";
    protected ref BDE_MapUI_ReportStore m_Store = new BDE_MapUI_ReportStore;
    protected ref map<string, ref BDE_MapUI_TicketEvent> m_Events = new map<string, ref BDE_MapUI_TicketEvent>;
    protected ref map<string, ref BDE_MapUI_Report> m_ByKey = new map<string, ref BDE_MapUI_Report>;
    protected ref map<string, string> m_Seen = new map<string, string>;
    protected ref map<string, int> m_Revisions = new map<string, int>;
    protected ref map<string, bool> m_Deleted = new map<string, bool>;
    protected ref BDE_MapUI_TicketDiscord m_Discord;
    protected string m_ServerID;
    protected bool m_Ready;
    protected int m_LastReload;

    static string NewID()
    {
        int uuid[4];
        UUIDApi.Generate(uuid);
        return UUIDApi.FormatString(uuid);
    }

    static bool SafeKey(string key)
    {
        if (key.Length() < 1 || key.Length() > 100)
            return false;
        string allowed = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-";
        for (int i = 0; i < key.Length(); i++)
        {
            if (allowed.IndexOf(key.Substring(i, 1)) < 0)
                return false;
        }
        return true;
    }

    void BDE_MapUI_TicketJournal(BDE_MapUI_ReportStore legacy)
    {
        m_Ready = ExpansionStatic.MakeDirectoryRecursive(SHARED);
        m_Ready = ExpansionStatic.MakeDirectoryRecursive(OUTBOX) && m_Ready;
        m_Ready = ExpansionStatic.MakeDirectoryRecursive(LOCAL + "/Notified") && m_Ready;
        FileHandle file = OpenFile(LOCAL + "/ServerID.txt", FileMode.READ);
        if (file)
        {
            FGets(file, m_ServerID);
            CloseFile(file);
            m_ServerID.TrimInPlace();
        }
        if (!SafeKey(m_ServerID))
        {
            m_ServerID = NewID();
            file = OpenFile(LOCAL + "/ServerID.txt", FileMode.WRITE);
            if (file)
            {
                FPrintln(file, m_ServerID);
                CloseFile(file);
            }
            else
                m_Ready = false;
        }
        m_Discord = new BDE_MapUI_TicketDiscord(LOCAL, m_ServerID);
        Reload();
        if (m_Ready && legacy && legacy.Reports)
            ImportLegacy(legacy);
        Reload();
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, 30000, true);
    }

    bool Ready()
    {
        return m_Ready;
    }

    BDE_MapUI_ReportStore Store()
    {
        return m_Store;
    }

    BDE_MapUI_Report Find(string key)
    {
        return m_ByKey.Get(key);
    }

    protected BDE_MapUI_Report CopyHeader(BDE_MapUI_Report source)
    {
        BDE_MapUI_Report result = new BDE_MapUI_Report;
        result.ID = source.ID;
        result.TicketKey = source.TicketKey;
        result.SteamID = source.SteamID;
        result.IdentityID = source.IdentityID;
        result.PlayerName = source.PlayerName;
        result.Category = source.Category;
        result.Subject = source.Subject;
        result.Message = source.Message;
        result.CreatedUTC = source.CreatedUTC;
        result.WorldName = source.WorldName;
        result.Position = source.Position;
        result.Direction = source.Direction;
        result.CharacterType = source.CharacterType;
        result.Health = source.Health;
        result.Blood = source.Blood;
        result.Shock = source.Shock;
        result.Alive = source.Alive;
        if (source.Inventory)
            result.Inventory.Copy(source.Inventory);
        if (source.NearbyPlayers)
            result.NearbyPlayers.Copy(source.NearbyPlayers);
        result.HistoryInitialized = true;
        return result;
    }

    protected BDE_MapUI_TicketEvent MakeEvent(string key, string kind, string uid, string name)
    {
        BDE_MapUI_TicketEvent eventData = new BDE_MapUI_TicketEvent;
        eventData.ID = NewID();
        eventData.TicketKey = key;
        eventData.Kind = kind;
        eventData.OriginServer = m_ServerID;
        eventData.MapName = g_Game.GetWorldName();
        eventData.ActorUID = uid;
        eventData.ActorName = name;
        eventData.CreatedUTC = BDE_MapUI_PortalService.Timestamp();
        eventData.Epoch = CF_Date.Now(true).DateToEpoch();
        eventData.Revision = m_Revisions.Get(key) + 1;
        return eventData;
    }

    protected bool Valid(BDE_MapUI_TicketEvent eventData, string filename)
    {
        if (!eventData || eventData.Format != 1 || !SafeKey(eventData.ID) || !SafeKey(eventData.TicketKey))
            return false;
        if (filename != eventData.ID + ".json" || !SafeKey(eventData.OriginServer) || eventData.ActorUID == "" || eventData.Epoch <= 0 || eventData.Revision < 1)
            return false;
        if (eventData.Kind == "CREATE")
            return eventData.TicketSnapshot && eventData.TicketSnapshot.SteamID != "" && eventData.TicketSnapshot.TicketKey == eventData.TicketKey;
        if (eventData.Kind == "MESSAGE")
            return eventData.Text.Length() > 0 && eventData.Text.Length() <= 4000;
        if (eventData.Kind == "SEEN")
            return eventData.SeenIDs && eventData.SeenIDs.Count() <= 1000;
        return eventData.Kind == "CLOSE" || eventData.Kind == "DELETE";
    }

    protected void Scan(string directory, bool pending)
    {
        string filename;
        int attributes;
        FindFileHandle handle = CF.FindFileEx(directory + "/*.json", filename, attributes, FindFileFlags.ALL);
        if (!handle)
            return;
        bool found = true;
        while (found)
        {
            if (filename.Length() > 5)
            {
                string knownID = filename.Substring(0, filename.Length() - 5);
                if (m_Events.Contains(knownID))
                {
                    if (pending)
                        Publish(m_Events.Get(knownID));
                    found = FindNextFile(handle, filename, attributes);
                    continue;
                }
            }
            BDE_MapUI_TicketEvent eventData;
            string error;
            if (JsonFileLoader<BDE_MapUI_TicketEvent>.LoadFile(directory + "/" + filename, eventData, error) && Valid(eventData, filename))
            {
                m_Events.Set(eventData.ID, eventData);
                if (pending)
                    Publish(eventData);
            }
            // Partial Hive copies are retried on the next scan, never rewritten.
            found = FindNextFile(handle, filename, attributes);
        }
        CloseFindFile(handle);
    }

    protected bool Publish(BDE_MapUI_TicketEvent eventData)
    {
        string path = SHARED + "/" + eventData.ID + ".json";
        string error;
        BDE_MapUI_TicketEvent verify;
        if (!FileExist(path) && !JsonFileLoader<BDE_MapUI_TicketEvent>.SaveFile(path, eventData, error))
            return false;
        if (!JsonFileLoader<BDE_MapUI_TicketEvent>.LoadFile(path, verify, error) || !Valid(verify, eventData.ID + ".json"))
            return false;
        if (verify.ID != eventData.ID || verify.TicketKey != eventData.TicketKey || verify.Kind != eventData.Kind)
            return false;
        DeleteFile(OUTBOX + "/" + eventData.ID + ".json");
        return true;
    }

    protected bool Commit(BDE_MapUI_TicketEvent eventData)
    {
        if (!m_Ready)
            return false;
        string error;
        if (!JsonFileLoader<BDE_MapUI_TicketEvent>.SaveFile(OUTBOX + "/" + eventData.ID + ".json", eventData, error))
        {
            Print("[BDE_MapUI tickets] Could not save ticket event to local outbox.");
            return false;
        }
        BDE_MapUI_TicketEvent verify;
        if (!JsonFileLoader<BDE_MapUI_TicketEvent>.LoadFile(OUTBOX + "/" + eventData.ID + ".json", verify, error) || !Valid(verify, eventData.ID + ".json"))
            return false;
        m_Events.Set(eventData.ID, eventData);
        Publish(eventData);
        Reload();
        return true;
    }

    bool Create(BDE_MapUI_Report report)
    {
        report.TicketKey = NewID();
        BDE_MapUI_TicketEvent eventData = MakeEvent(report.TicketKey, "CREATE", report.SteamID, report.PlayerName);
        eventData.TicketSnapshot = CopyHeader(report);
        return Commit(eventData);
    }

    bool Reply(BDE_MapUI_Report report, PlayerIdentity sender, string message, bool admin)
    {
        BDE_MapUI_TicketEvent eventData = MakeEvent(report.TicketKey, "MESSAGE", sender.GetPlainId(), sender.GetName());
        eventData.Text = message;
        eventData.IsAdmin = admin;
        return Commit(eventData);
    }

    bool Change(BDE_MapUI_Report report, PlayerIdentity sender, string kind)
    {
        BDE_MapUI_TicketEvent eventData = MakeEvent(report.TicketKey, kind, sender.GetPlainId(), sender.GetName());
        return Commit(eventData);
    }

    bool Acknowledge(BDE_MapUI_Report report, PlayerIdentity sender, array<string> rendered)
    {
        if (!rendered || rendered.Count() > 1000)
            return false;
        BDE_MapUI_TicketEvent eventData = MakeEvent(report.TicketKey, "SEEN", sender.GetPlainId(), sender.GetName());
        foreach (string id : rendered)
        {
            if (report.ViewEventIDs.Find(id) >= 0 && !m_Seen.Contains(eventData.ActorUID + "|" + id) && eventData.SeenIDs.Find(id) < 0)
                eventData.SeenIDs.Insert(id);
        }
        if (eventData.SeenIDs.Count() == 0)
            return true;
        return Commit(eventData);
    }

    int Unread(string uid)
    {
        int count;
        foreach (BDE_MapUI_Report report : m_Store.Reports)
        {
            if (report.Closed)
                continue;
            foreach (string id : report.ViewEventIDs)
            {
                BDE_MapUI_TicketEvent eventData = m_Events.Get(id);
                if (eventData && eventData.ActorUID != uid && !m_Seen.Contains(uid + "|" + id))
                    count++;
            }
        }
        return count;
    }

    void Reload()
    {
        m_LastReload = CF_Date.Now(true).DateToEpoch();
        Scan(SHARED, false);
        Scan(OUTBOX, true);
        array<ref BDE_MapUI_TicketEvent> ordered = new array<ref BDE_MapUI_TicketEvent>;
        array<string> orderKeys = new array<string>;
        map<string, ref BDE_MapUI_TicketEvent> byOrder = new map<string, ref BDE_MapUI_TicketEvent>;
        foreach (string key, BDE_MapUI_TicketEvent current : m_Events)
        {
            string orderKey = current.Revision.ToStringLen(10) + "-" + current.Epoch.ToStringLen(10) + "-" + current.ID;
            orderKeys.Insert(orderKey);
            byOrder.Set(orderKey, current);
        }
        orderKeys.Sort();
        foreach (string sortedKey : orderKeys)
            ordered.Insert(byOrder.Get(sortedKey));
        m_ByKey.Clear();
        m_Seen.Clear();
        m_Revisions.Clear();
        m_Deleted.Clear();
        m_Store.Reports.Clear();
        m_Store.NextID = 1;
        foreach (BDE_MapUI_TicketEvent header : ordered)
        {
            m_Revisions.Set(header.TicketKey, Math.Max(m_Revisions.Get(header.TicketKey), header.Revision));
            if (header.Kind == "CREATE" && !m_ByKey.Contains(header.TicketKey))
            {
                BDE_MapUI_Report created = CopyHeader(header.TicketSnapshot);
                created.ViewEventIDs.Insert(header.ID);
                m_ByKey.Set(header.TicketKey, created);
                m_Store.Reports.Insert(created);
                m_Store.NextID = Math.Max(m_Store.NextID, created.ID + 1);
            }
            if (header.Kind == "SEEN")
            {
                foreach (string seenID : header.SeenIDs)
                    m_Seen.Set(header.ActorUID + "|" + seenID, header.CreatedUTC);
            }
        }
        foreach (BDE_MapUI_TicketEvent eventData : ordered)
        {
            BDE_MapUI_Report report = m_ByKey.Get(eventData.TicketKey);
            if (!report)
                continue;
            if (eventData.Kind == "MESSAGE")
            {
                BDE_MapUI_ReportMessage message = new BDE_MapUI_ReportMessage;
                message.EventID = eventData.ID;
                message.PlayerName = eventData.ActorName;
                message.Message = eventData.Text;
                message.CreatedUTC = eventData.CreatedUTC;
                message.IsAdmin = eventData.IsAdmin;
                message.MapName = eventData.MapName;
                report.Messages.Insert(message);
                report.ViewEventIDs.Insert(eventData.ID);
                if (message.IsAdmin)
                {
                    report.Response = message.Message;
                    report.RespondedBy = message.PlayerName;
                    report.RespondedUTC = message.CreatedUTC;
                    report.ResponseVersion++;
                    report.LastResponseKey = eventData.ID;
                }
            }
            else if (eventData.Kind == "CLOSE")
                report.Closed = true;
            else if (eventData.Kind == "DELETE")
                m_Deleted.Set(report.TicketKey, true);
        }
        for (int index = m_Store.Reports.Count() - 1; index >= 0; index--)
        {
            BDE_MapUI_Report entry = m_Store.Reports[index];
            if (m_Deleted.Contains(entry.TicketKey))
            {
                m_Store.Reports.Remove(index);
                m_ByKey.Remove(entry.TicketKey);
            }
            else if (entry.LastResponseKey != "")
                entry.ResponseReadUTC = m_Seen.Get(entry.SteamID + "|" + entry.LastResponseKey);
        }
        Deliver(ordered);
    }

    protected void Deliver(array<ref BDE_MapUI_TicketEvent> ordered)
    {
        foreach (BDE_MapUI_TicketEvent eventData : ordered)
        {
            if (eventData.Migration || (eventData.Kind != "CREATE" && eventData.Kind != "MESSAGE"))
                continue;
            string receipt = LOCAL + "/Notified/" + eventData.ID + ".txt";
            if (FileExist(SHARED + "/" + eventData.ID + ".json") && !FileExist(receipt))
            {
                FileHandle file = OpenFile(receipt, FileMode.WRITE);
                if (file)
                {
                    FPrintln(file, "1");
                    CloseFile(file);
                    array<Man> players = new array<Man>;
                    g_Game.GetPlayers(players);
                    foreach (Man player : players)
                    {
                        PlayerIdentity identity = player.GetIdentity();
                        if (identity && identity.GetPlainId() != eventData.ActorUID && BDE_MapUI_ConfigManager.Get().ReportAdminSteamIDs.Find(identity.GetPlainId()) >= 0)
                        {
                            string title = "Ticket reply";
                            if (eventData.Kind == "CREATE")
                                title = "New ticket submitted";
                            NotificationSystem.SendNotificationToPlayerIdentityExtended(identity, 8.0, title, eventData.ActorName + " | " + eventData.MapName, "set:dayz_gui image:iconInfo");
                        }
                    }
                }
            }
        }
        m_Discord.Process(ordered);
    }

    protected void ImportLegacy(BDE_MapUI_ReportStore legacy)
    {
        foreach (BDE_MapUI_Report old : legacy.Reports)
        {
            if (!old || old.SteamID == "")
                continue;
            string key = m_ServerID + "-legacy-" + old.ID.ToString();
            if (FileExist(LOCAL + "/" + key + "-imported.txt"))
                continue;
            old.EnsureHistory();
            old.TicketKey = key;
            BDE_MapUI_TicketEvent create = MakeEvent(key, "CREATE", old.SteamID, old.PlayerName);
            create.ID = key + "-create";
            create.Migration = true;
            create.TicketSnapshot = CopyHeader(old);
            if (!m_Events.Contains(create.ID) && !Commit(create))
            {
                m_Ready = false;
                return;
            }
            int revision = 1;
            foreach (BDE_MapUI_ReportMessage oldMessage : old.Messages)
            {
                if (!oldMessage || oldMessage.Message == "")
                    continue;
                BDE_MapUI_TicketEvent migrated = MakeEvent(key, "MESSAGE", old.SteamID, oldMessage.PlayerName);
                migrated.ID = key + "-message-" + revision.ToString();
                migrated.Revision = ++revision;
                migrated.CreatedUTC = oldMessage.CreatedUTC;
                migrated.MapName = old.WorldName;
                migrated.IsAdmin = oldMessage.IsAdmin;
                migrated.Text = oldMessage.Message;
                migrated.Migration = true;
                if (!m_Events.Contains(migrated.ID) && !Commit(migrated))
                {
                    m_Ready = false;
                    return;
                }
            }
            if (old.Closed)
            {
                BDE_MapUI_TicketEvent close = MakeEvent(key, "CLOSE", old.SteamID, old.PlayerName);
                close.ID = key + "-closed";
                close.Migration = true;
                if (!m_Events.Contains(close.ID) && !Commit(close))
                {
                    m_Ready = false;
                    return;
                }
            }
            FileHandle imported = OpenFile(LOCAL + "/" + key + "-imported.txt", FileMode.WRITE);
            if (imported)
            {
                FPrintln(imported, "1");
                CloseFile(imported);
            }
        }
    }

    void Tick()
    {
        Reload();
        BDE_MapUI_PortalService.Get().TicketHeartbeat();
    }

    void RefreshForRequest()
    {
        if (CF_Date.Now(true).DateToEpoch() - m_LastReload >= 10)
            Reload();
    }
};
