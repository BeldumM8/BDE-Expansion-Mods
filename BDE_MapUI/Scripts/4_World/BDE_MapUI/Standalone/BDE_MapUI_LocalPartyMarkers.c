// Persistence overrides are dedicated-server code. Clients keep Expansion's
// original party class and receive local markers through its normal network API.
#ifdef SERVER
// Group membership remains in Expansion's normal Hive-synced group file.
// Party markers live only in this server's separate, unsynced folder.
class BDE_MapUI_LocalPartyMarkers
{
    static const string DIRECTORY = "$profile:/BDE/MapUI/PartyMarkers";
    static const int FORMAT_VERSION = 1;

    static bool EnsureDirectory()
    {
        return ExpansionStatic.MakeDirectoryRecursive(DIRECTORY);
    }

    static string GetPath(string groupKey)
    {
        return DIRECTORY + "/" + groupKey + ".bin";
    }

    static bool Read(string path, string groupKey, out array<ref ExpansionMarkerData> markers)
    {
        markers = new array<ref ExpansionMarkerData>;
        FileSerializer file = new FileSerializer;
        if (!file.Open(path, FileMode.READ))
            return false;

        int format;
        int markerVersion;
        string savedKey;
        int count;
        bool valid = file.Read(format) && format == FORMAT_VERSION;
        valid = valid && file.Read(markerVersion) && markerVersion > 0 && markerVersion <= EXPANSION_VERSION_MAP_MARKER_SAVE;
        valid = valid && file.Read(savedKey) && savedKey == groupKey;
        valid = valid && file.Read(count) && count >= 0 && count <= 100000;
        ref array<string> markerIDs = new array<string>;
        for (int index = 0; valid && index < count; index++)
        {
            string markerUID;
            valid = file.Read(markerUID) && markerUID != string.Empty && markerIDs.Find(markerUID) < 0;
            if (!valid)
                break;
            ExpansionMarkerData marker = ExpansionMarkerData.Create(ExpansionMapMarkerType.PARTY, markerUID);
            valid = marker && marker.OnStoreLoad(file, markerVersion);
            if (valid)
            {
                markers.Insert(marker);
                markerIDs.Insert(markerUID);
            }
        }
        string footer;
        valid = valid && file.Read(footer) && footer == "BDE_PARTY_MARKERS_END";
        file.Close();
        return valid;
    }

    static bool Save(string groupKey, array<ref ExpansionMarkerData> markers)
    {
        if (!EnsureDirectory())
            return false;
        string path = GetPath(groupKey);
        string pending = path + ".pending";
        FileSerializer file = new FileSerializer;
        if (!file.Open(pending, FileMode.WRITE))
            return false;
        file.Write(FORMAT_VERSION);
        file.Write(EXPANSION_VERSION_MAP_MARKER_SAVE);
        file.Write(groupKey);
        file.Write(markers.Count());
        foreach (ExpansionMarkerData marker : markers)
        {
            if (!marker)
            {
                file.Close();
                return false;
            }
            file.Write(marker.GetUID());
            marker.OnStoreSave(file);
        }
        file.Write("BDE_PARTY_MARKERS_END");
        file.Close();

        ref array<ref ExpansionMarkerData> verified;
        if (!Read(pending, groupKey, verified) || verified.Count() != markers.Count())
            return false;
        if (FileExist(path))
        {
            string backup = path + ".bak";
            if (FileExist(backup) && !DeleteFile(backup))
                return false;
            if (!CopyFile(path, backup))
                return false;
        }
        if (!CopyFile(pending, path) || !Read(path, groupKey, verified) || verified.Count() != markers.Count())
            return false;
        DeleteFile(pending);
        return true;
    }
};

modded class ExpansionPartyData
{

    // Compare persistent shared data only: runtime players and local markers
    // must never trigger a Hive reload or be copied from another server.
    string BDE_HiveGroupSignature()
    {
        array<string> values = new array<string>;
        values.Insert(PartyID.ToString());
        values.Insert(PartyName);
        values.Insert(PartyTag);
        values.Insert(OwnerUID);
        values.Insert(OwnerName);
        values.Insert(MoneyDeposited.ToString());
        values.Insert(LastMemberLeaveTimestamp.ToString());
        values.Insert(m_BDE_LocalMarkerKey);
        values.Insert(Players.Count().ToString());
        foreach (ExpansionPartyPlayerData member : Players)
        {
            if (!member)
                return "";
            values.Insert(member.UID);
            values.Insert(member.Name);
            values.Insert(member.Permissions.ToString());
            values.Insert(member.GetColor().ToString());
        }
        values.Insert(Invites.Count().ToString());
        foreach (ExpansionPartyInviteData invitation : Invites)
        {
            if (!invitation)
                return "";
            values.Insert(invitation.partyID.ToString());
            values.Insert(invitation.partyName);
            values.Insert(invitation.UID);
        }
        JsonSerializer serializer = new JsonSerializer;
        string result;
        if (!serializer.WriteToString(values, false, result))
            return "";
        return result;
    }

    bool BDE_HiveSameIdentity(ExpansionPartyData incoming)
    {
        return m_BDE_LocalMarkerKey != "" && m_BDE_LocalMarkerKey == incoming.m_BDE_LocalMarkerKey;
    }

    void BDE_HiveApply(ExpansionPartyData incoming)
    {
        PartyName = incoming.PartyName;
        PartyTag = incoming.PartyTag;
        OwnerUID = incoming.OwnerUID;
        OwnerName = incoming.OwnerName;
        MoneyDeposited = incoming.MoneyDeposited;
        LastMemberLeaveTimestamp = incoming.LastMemberLeaveTimestamp;
        // Keep the party object, local marker collection and player records
        // for retained members alive. Other mods may hold these references.
        array<ref ExpansionPartyPlayerData> refreshed = new array<ref ExpansionPartyPlayerData>;
        foreach (ExpansionPartyPlayerData sourceMember : incoming.Players)
        {
            ExpansionPartyPlayerData retained = GetPlayer(sourceMember.UID);
            if (!retained)
                retained = new ExpansionPartyPlayerData(this);
            retained.UID = sourceMember.UID;
            retained.Name = sourceMember.Name;
            retained.Permissions = sourceMember.Permissions;
            retained.SetColor(sourceMember.GetColor());
            refreshed.Insert(retained);
        }
        Players = refreshed;
        Invites.Clear();
        foreach (ExpansionPartyInviteData sourceInvite : incoming.Invites)
        {
            ExpansionPartyInviteData copiedInvite = new ExpansionPartyInviteData;
            copiedInvite.partyID = sourceInvite.partyID;
            copiedInvite.partyName = sourceInvite.partyName;
            copiedInvite.UID = sourceInvite.UID;
            Invites.Insert(copiedInvite);
        }
        InitMaps();
    }

    // A fresh party gets a unique key, so reusing Expansion's numeric party ID
    // cannot attach an old party's local markers to a new party.
    protected string m_BDE_LocalMarkerKey;
    protected bool m_BDE_LocalMarkerLoadFailed;

    protected void BDE_EnsureLocalMarkerKey()
    {
        if (m_BDE_LocalMarkerKey != string.Empty)
            return;
        int uuid[4];
        UUIDApi.Generate(uuid);
        m_BDE_LocalMarkerKey = UUIDApi.FormatString(uuid);
    }

    override void Save()
    {
        if (!IsMissionHost())
            return;
        BDE_EnsureLocalMarkerKey();
        if (!m_BDE_LocalMarkerLoadFailed && !BDE_MapUI_LocalPartyMarkers.Save(m_BDE_LocalMarkerKey, Markers))
            Print("[BDE_MapUI groups] Local marker save failed; pending data retained for party " + PartyID.ToString());
        super.Save();
    }

    override void OnStoreSave(ParamsWriteContext ctx)
    {
        BDE_EnsureLocalMarkerKey();
        // Leave Expansion's membership/permissions/bank format intact, but write
        // a zero-length marker collection. Network OnSend is a separate method
        // and continues to send this server's actual markers to its players.
        ref array<ref ExpansionMarkerData> localMarkers = Markers;
        Markers = new array<ref ExpansionMarkerData>;
        super.OnStoreSave(ctx);
        Markers = localMarkers;
        // Extension contains identity only, never marker text or coordinates.
        ctx.Write("BDE_LOCAL_PARTY_MARKERS_1");
        ctx.Write(m_BDE_LocalMarkerKey);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;
        // Existing unscoped markers are deliberately not migrated: this change
        // is intended for the user's full wipe.
        Markers.Clear();
        string extension;
        string groupKey;
        if (ctx.Read(extension) && extension == "BDE_LOCAL_PARTY_MARKERS_1" && ctx.Read(groupKey))
        {
            if (groupKey.Length() != 36 || groupKey.IndexOf("/") >= 0 || groupKey.IndexOf("\\") >= 0 || groupKey.IndexOf(":") >= 0 || groupKey.IndexOf("..") >= 0)
                return false;
            m_BDE_LocalMarkerKey = groupKey;
        }
        else
        {
            BDE_EnsureLocalMarkerKey();
            Print("[BDE_MapUI groups] Pre-wipe group has no local marker key; imported markers were ignored.");
        }

        string path = BDE_MapUI_LocalPartyMarkers.GetPath(m_BDE_LocalMarkerKey);
        ref array<ref ExpansionMarkerData> loadedMarkers;
        if (FileExist(path + ".pending") && BDE_MapUI_LocalPartyMarkers.Read(path + ".pending", m_BDE_LocalMarkerKey, loadedMarkers))
            Markers = loadedMarkers;
        else if (FileExist(path))
        {
            if (!BDE_MapUI_LocalPartyMarkers.Read(path, m_BDE_LocalMarkerKey, loadedMarkers))
            {
                m_BDE_LocalMarkerLoadFailed = true;
                Print("[BDE_MapUI groups] Local markers unreadable; file preserved: " + path);
            }
            else
                Markers = loadedMarkers;
        }
        else if (FileExist(path + ".pending"))
        {
            m_BDE_LocalMarkerLoadFailed = true;
            Print("[BDE_MapUI groups] Pending local markers unreadable; file preserved: " + path + ".pending");
        }
        // ExpansionPartyModule calls InitMaps after this returns, rebuilding
        // MarkersMap from the local collection before sending any group data.
        return true;
    }

    override void Delete()
    {
        if (!IsMissionHost())
            return;
        super.Delete();
        if (m_BDE_LocalMarkerKey == string.Empty)
            return;
        string path = BDE_MapUI_LocalPartyMarkers.GetPath(m_BDE_LocalMarkerKey);
        if (FileExist(path))
            DeleteFile(path);
        if (FileExist(path + ".pending"))
            DeleteFile(path + ".pending");
        if (FileExist(path + ".bak"))
            DeleteFile(path + ".bak");
    }
};

#endif
