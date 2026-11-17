// Immutable state/announcement events are inside the synced PlayerDatabase tree.
// Receipts and retry outboxes are server-local and must never be Hive-synced.
class BDE_MapUI_HiveBountyEvents
{
    static const string EVENTS = "$profile:/BDE/MapUI/PlayerDatabase/_BountyEvents";
    static const string LOCAL = "$profile:/BDE/MapUI/_Local/BountyAnnouncements";
    static const string OUTBOX = "$profile:/BDE/MapUI/_Local/BountyAnnouncements/Outbox";
    private ref map<string, ref BDE_MapUI_HiveBountyEvent> m_Events = new map<string, ref BDE_MapUI_HiveBountyEvent>;
    private ref map<string, ref BDE_MapUI_HiveBountyEvent> m_Latest = new map<string, ref BDE_MapUI_HiveBountyEvent>;
    private ref map<string, ref BDE_MapUI_HiveBountyEvent> m_Pending = new map<string, ref BDE_MapUI_HiveBountyEvent>;
    private ref map<string, bool> m_Seen = new map<string, bool>;

    void BDE_MapUI_HiveBountyEvents()
    {
        ExpansionStatic.MakeDirectoryRecursive(EVENTS);
        ExpansionStatic.MakeDirectoryRecursive(OUTBOX);
        Scan(OUTBOX, true);
        Reload();
    }

    static bool SafeID(string id)
    {
        if (id.Length() != 36)
            return false;
        string allowed = "0123456789abcdefABCDEF-";
        for (int i = 0; i < id.Length(); i++)
        {
            if (allowed.IndexOf(id.Substring(i, 1)) < 0)
                return false;
        }
        return true;
    }

    static int CompareID(string firstID, string secondID)
    {
        int count = Math.Min(firstID.Length(), secondID.Length());
        for (int i = 0; i < count; i++)
        {
            int firstCharacter = firstID.Substring(i, 1).ToAscii();
            int secondCharacter = secondID.Substring(i, 1).ToAscii();
            if (firstCharacter != secondCharacter)
                return firstCharacter - secondCharacter;
        }
        return firstID.Length() - secondID.Length();
    }

    static bool Newer(BDE_MapUI_HiveBountyEvent eventData, int revision, int epoch, string id)
    {
        if (eventData.Revision != revision)
            return eventData.Revision > revision;
        if (eventData.Epoch != epoch)
            return eventData.Epoch > epoch;
        return CompareID(eventData.ID, id) > 0;
    }

    private array<ref BDE_MapUI_BountyContributor> CopyContributors(array<ref BDE_MapUI_BountyContributor> source)
    {
        ref array<ref BDE_MapUI_BountyContributor> result = new array<ref BDE_MapUI_BountyContributor>;
        if (source)
        {
            foreach (BDE_MapUI_BountyContributor entry : source)
            {
                if (!entry || entry.Amount <= 0)
                    continue;
                BDE_MapUI_BountyContributor copy = new BDE_MapUI_BountyContributor;
                copy.UID = entry.UID;
                copy.Name = entry.Name;
                copy.Amount = entry.Amount;
                result.Insert(copy);
            }
        }
        return result;
    }

    private BDE_MapUI_HiveBountyEvent ReadEvent(string path)
    {
        BDE_MapUI_HiveBountyEvent eventData;
        string error;
        if (!JsonFileLoader<BDE_MapUI_HiveBountyEvent>.LoadFile(path, eventData, error) || !eventData)
            return null;
        if (!SafeID(eventData.ID) || eventData.TargetUID == "" || eventData.Revision <= 0 || !eventData.Contributors)
            return null;
        if (eventData.Kind != "Placed" && eventData.Kind != "Claimed" && eventData.Kind != "Removed")
            return null;
        foreach (BDE_MapUI_BountyContributor entry : eventData.Contributors)
        {
            if (!entry || entry.UID == "" || entry.Amount <= 0)
                return null;
        }
        if (eventData.MapName == "" || eventData.ActorUID == "" || eventData.Amount <= 0 || eventData.Epoch <= 0)
            return null;
        int total;
        foreach (BDE_MapUI_BountyContributor contribution : eventData.Contributors)
            total += contribution.Amount;
        if (total != eventData.TotalAmount)
            return null;
        if (eventData.Kind == "Placed" && eventData.Contributors.Count() == 0)
            return null;
        if (eventData.Kind == "Claimed" && eventData.Contributors.Count() != 0)
            return null;
        return eventData;
    }

    private void RememberState(BDE_MapUI_HiveBountyEvent eventData)
    {
        BDE_MapUI_HiveBountyEvent current;
        if (!m_Latest.Find(eventData.TargetUID, current) || Newer(eventData, current.Revision, current.Epoch, current.ID))
            m_Latest.Set(eventData.TargetUID, eventData);
    }

    private void Accept(BDE_MapUI_HiveBountyEvent eventData)
    {
        m_Events.Set(eventData.ID, eventData);
        RememberState(eventData);
    }

    private void Scan(string directory, bool outbox)
    {
        string name;
        int attributes;
        FindFileHandle handle = CF.FindFileEx(directory + "/*.json", name, attributes, FindFileFlags.ALL);
        if (!handle)
            return;
        bool found = true;
        while (found)
        {
            if (!(attributes & FileAttr.DIRECTORY) && name.Length() == 41 && name.Substring(36, 5) == ".json")
            {
                string id = name.Substring(0, 36);
                if (SafeID(id) && (outbox || !m_Events.Contains(id)))
                {
                    BDE_MapUI_HiveBountyEvent eventData = ReadEvent(directory + "/" + name);
                    if (eventData && eventData.ID == id)
                    {
                        if (outbox)
                        {
                            m_Pending.Set(id, eventData);
                            RememberState(eventData);
                        }
                        else
                            Accept(eventData);
                    }
                }
            }
            found = FindNextFile(handle, name, attributes);
        }
        CloseFindFile(handle);
    }

    private bool Publish(BDE_MapUI_HiveBountyEvent eventData)
    {
        string path = EVENTS + "/" + eventData.ID + ".json";
        BDE_MapUI_HiveBountyEvent verified = ReadEvent(path);
        string error;
        if (!verified)
        {
            if (!JsonFileLoader<BDE_MapUI_HiveBountyEvent>.SaveFile(path, eventData, error))
                return false;
            verified = ReadEvent(path);
        }
        if (!verified || verified.ID != eventData.ID || verified.TargetUID != eventData.TargetUID || verified.Revision != eventData.Revision)
            return false;
        Accept(verified);
        DeleteFile(OUTBOX + "/" + eventData.ID + ".json");
        return true;
    }

    void Reload()
    {
        for (int i = m_Pending.Count() - 1; i >= 0; i--)
        {
            string id = m_Pending.GetKey(i);
            if (Publish(m_Pending.GetElement(i)))
                m_Pending.Remove(id);
        }
        Scan(EVENTS, false);
    }

    void Queue(BDE_MapUI_PlayerRecord record, string kind, string actorUID, string actorName, int amount)
    {
        record.EnsureArrays();
        record.RecalculateBounty();
        BDE_MapUI_HiveBountyEvent eventData = new BDE_MapUI_HiveBountyEvent;
        int uuid[4];
        UUIDApi.Generate(uuid);
        eventData.ID = UUIDApi.FormatString(uuid);
        eventData.Kind = kind;
        eventData.TargetUID = record.UID;
        eventData.TargetName = record.Name;
        eventData.ActorUID = actorUID;
        eventData.ActorName = actorName;
        eventData.Amount = amount;
        eventData.TotalAmount = record.BountyAmount;
        eventData.Revision = record.BountyRevision + 1;
        eventData.Epoch = CF_Date.Now(true).DateToEpoch();
        eventData.MapName = g_Game.GetWorldName();
        string displayName;
        if (g_Game.ConfigGetText("CfgWorlds " + eventData.MapName + " description", displayName) && displayName != "")
            eventData.MapName = displayName;
        eventData.Contributors = CopyContributors(record.BountyContributors);
        record.BountyRevision = eventData.Revision;
        record.BountyRevisionEpoch = eventData.Epoch;
        record.BountyRevisionID = eventData.ID;
        RememberState(eventData);
        string error;
        if (!JsonFileLoader<BDE_MapUI_HiveBountyEvent>.SaveFile(OUTBOX + "/" + eventData.ID + ".json", eventData, error))
            Print("[BDE_MapUI bounties] Event outbox write failed: " + error);
        if (!Publish(eventData))
        {
            m_Pending.Set(eventData.ID, eventData);
            Print("[BDE_MapUI bounties] Hive event publication pending; retrying next reload: " + eventData.ID);
        }
    }

    void Apply(BDE_MapUI_PlayerRecord record)
    {
        BDE_MapUI_HiveBountyEvent current;
        if (!m_Latest.Find(record.UID, current))
            return;
        if (Newer(current, record.BountyRevision, record.BountyRevisionEpoch, record.BountyRevisionID) || current.ID == record.BountyRevisionID)
        {
            record.BountyContributors = CopyContributors(current.Contributors);
            record.BountyRevision = current.Revision;
            record.BountyRevisionEpoch = current.Epoch;
            record.BountyRevisionID = current.ID;
            record.RecalculateBounty();
        }
    }

    array<ref BDE_MapUI_HiveBountyEvent> Unseen()
    {
        ref array<ref BDE_MapUI_HiveBountyEvent> result = new array<ref BDE_MapUI_HiveBountyEvent>;
        for (int i = 0; i < m_Events.Count(); i++)
        {
            BDE_MapUI_HiveBountyEvent eventData = m_Events.GetElement(i);
            if (eventData.Kind == "Removed" || m_Seen.Contains(eventData.ID))
                continue;
            string seenPath = LOCAL + "/" + eventData.ID + ".seen";
            if (FileExist(seenPath))
            {
                FileHandle receipt = OpenFile(seenPath, FileMode.READ);
                string savedID;
                bool valid = receipt && FGets(receipt, savedID) >= 0;
                if (receipt)
                    CloseFile(receipt);
                if (valid && savedID.Trim() == eventData.ID)
                {
                    m_Seen.Set(eventData.ID, true);
                    continue;
                }
            }
            int insertAt = result.Count();
            for (int j = 0; j < result.Count(); j++)
            {
                if (eventData.Revision < result[j].Revision || (eventData.Revision == result[j].Revision && eventData.Epoch < result[j].Epoch))
                {
                    insertAt = j;
                    break;
                }
            }
            result.InsertAt(eventData, insertAt);
        }
        return result;
    }

    bool MarkAnnounced(BDE_MapUI_HiveBountyEvent eventData)
    {
        string path = LOCAL + "/" + eventData.ID + ".seen";
        FileHandle file = OpenFile(path, FileMode.WRITE);
        if (!file)
            return false;
        FPrintln(file, eventData.ID);
        CloseFile(file);
        file = OpenFile(path, FileMode.READ);
        string savedID;
        bool valid = file && FGets(file, savedID) >= 0;
        if (file)
            CloseFile(file);
        if (!valid || savedID.Trim() != eventData.ID)
        {
            DeleteFile(path);
            return false;
        }
        m_Seen.Set(eventData.ID, true);
        return true;
    }
};
