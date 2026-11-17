class BDE_MapUI_PortalService
{
    static const string RPC = "BDE_MapUI_Portal";
    static const string REPORTS_PATH = "$profile:/BDE/MapUI/Reports.json";
    static const string CHANGES_PATH = "$profile:/BDE/MapUI/ChangelogState.json";
    protected static ref BDE_MapUI_PortalService s_Instance;
    protected ref BDE_MapUI_ReportStore m_Reports;
    protected ref BDE_MapUI_TicketJournal m_Tickets;
    protected ref BDE_MapUI_ChangelogState m_Changes;
    protected bool m_ReportsReady = true;
    protected bool m_ChangesReady = true;
    protected ref map<string, int> m_LastSubmission = new map<string, int>();
    protected ref map<string, int> m_LastRequest = new map<string, int>();

    static BDE_MapUI_PortalService Get()
    {
        if (!s_Instance)
        {
            s_Instance = new BDE_MapUI_PortalService();
        }
        return s_Instance;
    }

    void BDE_MapUI_PortalService()
    {
        if (g_Game.IsServer())
        {
            m_Reports = new BDE_MapUI_ReportStore();
            m_Changes = new BDE_MapUI_ChangelogState();
            if (FileExist(REPORTS_PATH))
            {
                string reportsError;
                m_ReportsReady = JsonFileLoader<BDE_MapUI_ReportStore>.LoadFile(REPORTS_PATH, m_Reports, reportsError);
                if (!m_ReportsReady)
                {
                    Print("[BDE_MapUI reports] " + reportsError);
                }
            }
            if (FileExist(CHANGES_PATH))
            {
                string changesError;
                m_ChangesReady = JsonFileLoader<BDE_MapUI_ChangelogState>.LoadFile(CHANGES_PATH, m_Changes, changesError);
                if (!m_ChangesReady)
                {
                    Print("[BDE_MapUI changelog] " + changesError);
                }
            }
            if (!m_Reports || !m_Reports.Reports)
            {
                m_ReportsReady = false;
            }
            if (m_ReportsReady)
            {
                foreach (BDE_MapUI_Report existing : m_Reports.Reports)
                {
                    if (existing)
                    {
                        existing.EnsureHistory();
                        m_Reports.NextID = Math.Max(m_Reports.NextID, existing.ID + 1);
                    }
                }
            }
            if (!m_Changes || !m_Changes.SeenSteamIDs)
            {
                m_ChangesReady = false;
            }
            if (m_ChangesReady)
            {
                SnapshotChangelog();
            }
            if (m_ReportsReady)
            {
                m_Tickets = new BDE_MapUI_TicketJournal(m_Reports);
                m_Reports = m_Tickets.Store();
                m_ReportsReady = m_Tickets.Ready();
            }
            GetRPCManager().AddRPC(RPC, "RPC_Request", this, SingeplayerExecutionType.Server);
        }
        else
        {
            GetRPCManager().AddRPC(RPC, "RPC_Reply", this, SingeplayerExecutionType.Client);
        }
    }

    static string Timestamp()
    {
        int year, month, day, hour, minute, second;
        GetYearMonthDayUTC(year, month, day);
        GetHourMinuteSecondUTC(hour, minute, second);
        return year.ToString() + "-" + month.ToStringLen(2) + "-" + day.ToStringLen(2) + " " + hour.ToStringLen(2) + ":" + minute.ToStringLen(2) + ":" + second.ToStringLen(2) + " UTC";
    }

    static bool IsChangelog(BDE_MapUI_TabConfig tab)
    {
        if (!tab)
        {
            return false;
        }
        string name = tab.Label;
        name.ToLower();
        return name == "changelog" || name == "change log";
    }

    protected void SnapshotChangelog()
    {
        array<string> lines = new array<string>();
        string legacyContent;
        foreach (BDE_MapUI_TabConfig tab : BDE_MapUI_ConfigManager.Get().Tabs)
        {
            if (IsChangelog(tab) && tab.Content)
            {
                legacyContent = tab.Heading;
                foreach (string line : tab.Content)
                {
                    lines.Insert(line);
                    legacyContent += "\n" + line;
                }
                break;
            }
        }

        bool changed;
        if (m_Changes.SnapshotVersion < 2)
        {
            // Preserve acknowledgements when upgrading the previous heading/text snapshot.
            changed = m_Changes.Content != legacyContent;
        }
        else if (!m_Changes.ContentLines || m_Changes.ContentLines.Count() != lines.Count())
        {
            changed = true;
        }
        else
        {
            for (int i = 0; i < lines.Count(); i++)
            {
                if (m_Changes.ContentLines[i] != lines[i])
                {
                    changed = true;
                    break;
                }
            }
        }
        if (changed)
        {
            m_Changes.Revision++;
            m_Changes.SeenSteamIDs.Clear();
        }
        m_Changes.Revision = Math.Max(1, m_Changes.Revision);
        m_Changes.SnapshotVersion = 2;
        m_Changes.ContentLines = lines;
        m_Changes.Content = "";
        foreach (string contentLine : lines)
        {
            m_Changes.Content += contentLine;
        }
        m_Changes.SnapshotUTC = Timestamp();
        m_ChangesReady = SaveChanges();
        if (m_ChangesReady)
        {
            RemoveLegacyChangelogSnapshots();
            Print("[BDE_MapUI changelog] Content changed=" + changed.ToString() + "; revision=" + m_Changes.Revision.ToString() + "; seen players=" + m_Changes.SeenSteamIDs.Count().ToString());
        }
    }

    protected void RemoveLegacyChangelogSnapshots()
    {
        string fileName;
        int fileAttr;
        array<string> obsolete = new array<string>();
        FindFileHandle handle = CF.FindFileEx(BDE_MAPUI_CONFIG_DIR + "/ChangelogRevision_*.json", fileName, fileAttr, FindFileFlags.ALL);
        if (!handle)
        {
            return;
        }
        bool found = true;
        while (found)
        {
            if (fileName.IndexOf("ChangelogRevision_") == 0)
            {
                obsolete.Insert(BDE_MAPUI_CONFIG_DIR + "/" + fileName);
            }
            found = FindNextFile(handle, fileName, fileAttr);
        }
        CloseFindFile(handle);
        foreach (string obsoletePath : obsolete)
        {
            DeleteFile(obsoletePath);
        }
    }

    protected bool SaveChanges()
    {
        string error;
        if (!JsonFileLoader<BDE_MapUI_ChangelogState>.SaveFile(CHANGES_PATH, m_Changes, error))
        {
            Print("[BDE_MapUI changelog] " + error);
            return false;
        }
        return true;
    }

    void Request(BDE_MapUI_PortalRequest request)
    {
        GetRPCManager().SendRPC(RPC, "RPC_Request", new Param1<ref BDE_MapUI_PortalRequest>(request), true, NULL);
    }

    protected BDE_MapUI_Report FindReport(int id, string key)
    {
        if (!m_Tickets || !BDE_MapUI_TicketJournal.SafeKey(key))
            return null;
        return m_Tickets.Find(key);
    }

    void TicketHeartbeat()
    {
        if (!g_Game.IsServer() || !m_Tickets)
            return;
        array<Man> players = new array<Man>;
        g_Game.GetPlayers(players);
        foreach (Man player : players)
        {
            PlayerIdentity identity = player.GetIdentity();
            if (identity && BDE_MapUI_ConfigManager.Get().ReportAdminSteamIDs.Find(identity.GetPlainId()) >= 0)
            {
                BDE_MapUI_PortalReply reply = new BDE_MapUI_PortalReply;
                reply.Action = "TICKET_STATUS";
                reply.IsAdmin = true;
                Send(identity, reply);
            }
        }
    }

    protected string Submit(PlayerIdentity sender, BDE_MapUI_PortalRequest request)
    {
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        string uid = sender.GetPlainId();
        int now = g_Game.GetTime();
        if (m_LastSubmission.Contains(uid) && now - m_LastSubmission.Get(uid) < cfg.ReportCooldownSeconds * 1000)
        {
            return "Please wait before submitting another report.";
        }
        request.Subject.TrimInPlace();
        request.Message.TrimInPlace();
        if (cfg.ReportTypes.Find(request.Category) < 0 || request.Subject.Length() < 3 || request.Subject.Length() > 100 || request.Message.Length() < 10 || request.Message.Length() > 4000)
        {
            return "Select a report type. Use a 3-100 character subject and 10-4000 character description.";
        }
        int open;
        foreach (BDE_MapUI_Report previous : m_Reports.Reports)
        {
            if (previous && previous.SteamID == uid && !previous.Closed)
            {
                open++;
            }
        }
        if (open >= cfg.MaxOpenReportsPerPlayer)
        {
            return "You have reached the open report limit. Wait for an admin to close an existing report.";
        }
        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (!player)
        {
            return "Player is not available. Try again after spawning.";
        }
        BDE_MapUI_Report report = new BDE_MapUI_Report();
        report.ID = m_Reports.NextID++;
        report.SteamID = uid;
        report.IdentityID = sender.GetId();
        report.PlayerName = sender.GetName();
        report.Category = request.Category;
        report.Subject = request.Subject;
        report.Message = request.Message;
        report.CreatedUTC = Timestamp();
        g_Game.GetWorldName(report.WorldName);
        report.Position = player.GetPosition();
        report.Direction = player.GetDirection();
        report.CharacterType = player.GetType();
        report.Health = player.GetHealth("", "Health");
        report.Blood = player.GetHealth("", "Blood");
        report.Shock = player.GetHealth("", "Shock");
        report.Alive = player.IsAlive();
        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
        foreach (EntityAI item : items)
        {
            if (item && report.Inventory.Count() < 150)
            {
                report.Inventory.Insert(item.GetType() + " | health=" + item.GetHealth("", "Health").ToString());
            }
        }
        array<Man> players = new array<Man>();
        g_Game.GetPlayers(players);
        foreach (Man nearby : players)
        {
            if (nearby != player && nearby.GetIdentity() && vector.Distance(nearby.GetPosition(), report.Position) <= 100)
            {
                report.NearbyPlayers.Insert(nearby.GetIdentity().GetName() + " | " + nearby.GetIdentity().GetPlainId() + " | " + nearby.GetPosition().ToString());
            }
        }
        if (!m_Tickets.Create(report))
        {
            return "Report storage failed. Contact an admin; the server log has details.";
        }
        m_LastSubmission.Set(uid, now);
        TicketHeartbeat();
        return "Report #" + report.ID.ToString() + " submitted (saved; Hive publication retries automatically).";
    }

    void RPC_Request(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !g_Game.IsServer() || !sender)
        {
            return;
        }
        Param1<ref BDE_MapUI_PortalRequest> payload;
        if (!ctx.Read(payload) || !payload || !payload.param1)
        {
            return;
        }
        BDE_MapUI_PortalRequest request = payload.param1;
        if (request.Action != "STATUS" && request.Action != "CHANGELOG_READ" && request.Action != "GROUP_PLAYERS" && request.Action != "SUBMIT" && request.Action != "LIST" && request.Action != "DETAIL" && request.Action != "READ" && request.Action != "RESPOND" && request.Action != "MESSAGE" && request.Action != "CLOSE" && request.Action != "DELETE")
        {
            return;
        }
        string uid = sender.GetPlainId();
        if (uid == "")
        {
            return;
        }
        // Bound requests per action without dropping the detail/read acknowledgement pair.
        string throttleKey = uid + ":" + request.Action;
        int now = g_Game.GetTime();
        if (m_LastRequest.Contains(throttleKey) && now - m_LastRequest.Get(throttleKey) < 200)
        {
            return;
        }
        m_LastRequest.Set(throttleKey, now);
        if (m_Tickets)
            m_Tickets.RefreshForRequest();
        BDE_MapUI_PortalReply reply = new BDE_MapUI_PortalReply();
        reply.Action = request.Action;
        reply.IsAdmin = BDE_MapUI_ConfigManager.Get().ReportAdminSteamIDs.Find(uid) >= 0;
        reply.AdminView = request.AdminView && reply.IsAdmin;
        if (m_ChangesReady)
        {
            reply.ChangelogRevision = m_Changes.Revision;
            reply.ChangelogPending = m_Changes.Content != "" && m_Changes.SeenSteamIDs.Find(uid) < 0;
        }
        if (request.Action == "GROUP_PLAYERS")
        {
            array<Man> players = new array<Man>();
            g_Game.GetPlayers(players);
            foreach (Man player : players)
            {
                if (player.GetIdentity() && player.GetIdentity().GetId() != sender.GetId())
                {
                    BDE_MapUI_GroupPlayer groupPlayer = new BDE_MapUI_GroupPlayer();
                    groupPlayer.ID = player.GetIdentity().GetId();
                    groupPlayer.Name = player.GetIdentity().GetName();
                    reply.GroupPlayers.Insert(groupPlayer);
                }
            }
            Send(sender, reply);
            return;
        }
        if (request.Action == "STATUS")
        {
            Send(sender, reply);
            return;
        }
        if (request.Action == "CHANGELOG_READ")
        {
            if (m_ChangesReady && request.Version == m_Changes.Revision && m_Changes.SeenSteamIDs.Find(uid) < 0)
            {
                m_Changes.SeenSteamIDs.Insert(uid);
                if (!SaveChanges())
                {
                    m_Changes.SeenSteamIDs.RemoveItem(uid);
                }
                else
                {
                    Print("[BDE_MapUI changelog] Saved acknowledgement for " + uid + "; revision=" + m_Changes.Revision.ToString());
                }
            }
            return;
        }
        if (!m_ReportsReady)
        {
            reply.Status = "Report storage is unavailable. Ask an admin to check the server log.";
            Send(sender, reply);
            return;
        }
        if (request.Action == "SUBMIT")
        {
            reply.Status = Submit(sender, request);
        }
        BDE_MapUI_Report report = FindReport(request.ID, request.TicketKey);
        bool ownsReport = report && report.SteamID == uid;
        if (request.Action == "DETAIL" || request.Action == "READ")
        {
            if (report && (ownsReport || reply.IsAdmin))
            {
                if (request.Action == "READ")
                {
                    if (!m_Tickets.Acknowledge(report, sender, request.SeenIDs))
                        reply.Status = "Read acknowledgement could not be saved.";
                    report = FindReport(request.ID, request.TicketKey);
                }
                reply.Detail = report;
                // Detailed metadata is visible only to the author and admins.
            }
            else
            {
                reply.Status = "Report is unavailable.";
            }
            Send(sender, reply);
            return;
        }
        if (request.Action == "MESSAGE" || request.Action == "RESPOND")
        {
            reply.ReportID = request.ID;
            reply.ReportKey = request.TicketKey;
            bool adminMessage = request.Action == "RESPOND";
            if (!report || (adminMessage && !reply.IsAdmin) || (!adminMessage && !ownsReport))
            {
                reply.Status = "Report unavailable or access denied.";
            }
            else if (report.Closed)
            {
                reply.Status = "This report is closed. Create a new report if you need more help.";
            }
            else
            {
                request.Message.TrimInPlace();
                report.EnsureHistory();
                if (request.Message.Length() < 1 || request.Message.Length() > 4000)
                {
                    reply.Status = "Message must contain 1-4000 characters.";
                }
                else if (report.Messages.Count() >= 200)
                {
                    reply.Status = "This conversation is full. Please create a follow-up report.";
                }
                else
                {
                    reply.MessageSaved = m_Tickets.Reply(report, sender, request.Message, adminMessage);
                    if (reply.MessageSaved)
                    {
                        reply.Status = "Message saved. Hive publication retries automatically.";
                        TicketHeartbeat();
                    }
                    else
                        reply.Status = "Message could not be saved. Your draft has been kept.";
                }
            }
        }
        if (request.Action == "CLOSE" || request.Action == "DELETE")
        {
            if (!reply.IsAdmin || !report)
            {
                reply.Status = "Admin access required or report unavailable.";
            }
            else
            {
                reply.MessageSaved = m_Tickets.Change(report, sender, request.Action);
                if (reply.MessageSaved)
                {
                    reply.Status = "Ticket " + request.Action + " saved.";
                    TicketHeartbeat();
                }
                else
                    reply.Status = "Ticket change could not be saved.";
            }
        }
        int page = Math.Max(0, request.Page);
        page = Math.Min(page, m_Reports.Reports.Count() / 20);
        int matching;
        for (int i = m_Reports.Reports.Count() - 1; i >= 0; i--)
        {
            BDE_MapUI_Report entry = m_Reports.Reports[i];
            if (!entry || (!reply.AdminView && entry.SteamID != uid))
            {
                continue;
            }
            if (matching >= page * 20 && reply.Rows.Count() < 20)
            {
                BDE_MapUI_ReportRow row = new BDE_MapUI_ReportRow();
                row.ID = entry.ID;
                row.TicketKey = entry.TicketKey;
                row.WorldName = entry.WorldName;
                row.PlayerName = entry.PlayerName;
                row.Category = entry.Category;
                row.Subject = entry.Subject;
                row.Closed = entry.Closed;
                row.HasResponse = entry.Response != "";
                if (entry.Messages && entry.Messages.Count() > 0)
                {
                    BDE_MapUI_ReportMessage lastMessage = entry.Messages[entry.Messages.Count() - 1];
                    row.PlayerReplied = lastMessage && !lastMessage.IsAdmin;
                }
                row.ResponseRead = entry.ResponseReadUTC != "";
                reply.Rows.Insert(row);
            }
            matching++;
        }
        reply.Page = page;
        reply.HasNext = matching > (page + 1) * 20;
        Send(sender, reply);
    }

    protected void Send(PlayerIdentity sender, BDE_MapUI_PortalReply reply)
    {
        if (!m_ReportsReady && reply.Action != "STATUS" && reply.Action != "GROUP_PLAYERS")
        {
            reply.Status = "Report storage failed. Contact an admin; the server log has details.";
        }
        reply.IsAdmin = BDE_MapUI_ConfigManager.Get().ReportAdminSteamIDs.Find(sender.GetPlainId()) >= 0;
        if (reply.IsAdmin && m_Tickets)
            reply.AdminUnreadCount = m_Tickets.Unread(sender.GetPlainId());
        GetRPCManager().SendRPC(RPC, "RPC_Reply", new Param1<ref BDE_MapUI_PortalReply>(reply), true, sender);
    }

    void RPC_Reply(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
        {
            return;
        }
        Param1<ref BDE_MapUI_PortalReply> data;
        if (!ctx.Read(data) || !data || !data.param1)
        {
            return;
        }
        ExpansionMapMenu menu = ExpansionMapMenu.Cast(g_Game.GetUIManager().GetMenu());
        if (menu)
        {
            menu.BDE_MapUI_ReceivePortal(data.param1);
        }
    }
};
