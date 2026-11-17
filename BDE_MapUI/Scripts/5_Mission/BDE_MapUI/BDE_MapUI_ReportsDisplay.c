class BDE_MapUI_ReportsDisplay
{
    protected Widget m_Root;
    protected TextListboxWidget m_List;
    protected XComboBoxWidget m_Type;
    protected EditBoxWidget m_Subject;
    protected MultilineEditBoxWidget m_Message;
    protected MultilineEditBoxWidget m_Response;
    protected RichTextWidget m_DetailText;
    protected TextWidget m_Status;
    protected ref BDE_MapUI_Report m_Selected;
    protected bool m_AdminView;
    protected bool m_IsAdmin;
    protected bool m_ShowDetails;
    protected int m_Page;
    protected int m_SelectedID;
    protected string m_SelectedKey;
    protected int m_DeleteConfirmID;
    protected int m_FitPasses;

    void Init(Widget host)
    {
        m_Root = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/ReportsDisplay.layout", host);
        m_List = TextListboxWidget.Cast(m_Root.FindAnyWidget("ReportList"));
        m_Type = XComboBoxWidget.Cast(m_Root.FindAnyWidget("ReportType"));
        m_Subject = EditBoxWidget.Cast(m_Root.FindAnyWidget("ReportSubject"));
        m_Message = MultilineEditBoxWidget.Cast(m_Root.FindAnyWidget("ReportMessage"));
        m_Response = MultilineEditBoxWidget.Cast(m_Root.FindAnyWidget("ReportReply"));
        m_DetailText = RichTextWidget.Cast(m_Root.FindAnyWidget("ReportDetailText"));
        m_Status = TextWidget.Cast(m_Root.FindAnyWidget("ReportStatus"));
        WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_List, this, "SelectReport");
        m_Root.FindAnyWidget("ReportAdmin").Show(false);
    }

    void Show(bool adminInbox = false)
    {
        if (adminInbox)
            m_AdminView = true;
        bool compose = !m_AdminView && m_SelectedKey == "";
        m_Root.FindAnyWidget("ReportCompose").Show(compose);
        m_Type.ClearAll();
        foreach (string category : BDE_MapUI_ConfigManager.Get().ReportTypes)
        {
            m_Type.AddItem(category);
        }
        m_Type.SetCurrentItem(0);
        Send("LIST");
    }

    protected BDE_MapUI_PortalRequest MakeRequest(string action)
    {
        BDE_MapUI_PortalRequest request = new BDE_MapUI_PortalRequest();
        request.Action = action;
        request.ID = m_SelectedID;
        request.TicketKey = m_SelectedKey;
        request.AdminView = m_AdminView;
        request.Page = m_Page;
        return request;
    }

    protected void Send(string action)
    {
        BDE_MapUI_PortalService.Get().Request(MakeRequest(action));
    }

    void SelectReport()
    {
        int index = m_List.GetSelectedRow();
        if (index < 0)
        {
            return;
        }
        BDE_MapUI_ReportRow row;
        m_List.GetItemData(index, 0, row);
        if (!row)
        {
            return;
        }
        if (m_SelectedKey != row.TicketKey)
        {
            m_Response.SetText("");
        }
        m_SelectedID = row.ID;
        m_SelectedKey = row.TicketKey;
        m_DeleteConfirmID = 0;
        Send("DETAIL");
    }

    static string Escape(string value)
    {
        value.Replace("&", "&amp;");
        value.Replace("<", "&lt;");
        value.Replace(">", "&gt;");
        value.Replace("\n", "<br/>");
        return value;
    }

    void Receive(BDE_MapUI_PortalReply reply)
    {
        m_IsAdmin = reply.IsAdmin;
        m_Root.FindAnyWidget("ReportAdmin").Show(m_IsAdmin);
        if (reply.Action == "READ")
        {
            if (reply.Status != "")
                m_Status.SetText(reply.Status);
            return;
        }
        if (reply.Status != "")
        {
            m_Status.SetText(reply.Status);
        }
        if (reply.Action == "DETAIL")
        {
            if (!reply.Detail || reply.Detail.TicketKey != m_SelectedKey)
            {
                return;
            }
            m_Selected = reply.Detail;
            m_Root.FindAnyWidget("ReportCompose").Show(false);
            m_Root.FindAnyWidget("ReportDetail").Show(true);
            m_Root.FindAnyWidget("ReportAdminTools").Show(true);
            m_Root.FindAnyWidget("ReportClose").Show(m_AdminView && m_IsAdmin);
            m_Root.FindAnyWidget("ReportDelete").Show(m_AdminView && m_IsAdmin);
            m_Root.FindAnyWidget("ReportToggleDetails").Show(m_AdminView && m_IsAdmin);

            RenderConversation();
            // Acknowledge only the exact events rendered, never a later unseen reply.
            if (m_Root.IsVisibleHierarchy())
            {
                BDE_MapUI_PortalRequest read = MakeRequest("READ");
                read.Version = m_Selected.ResponseVersion;
                read.SeenIDs.Copy(m_Selected.ViewEventIDs);
                BDE_MapUI_PortalService.Get().Request(read);
            }
            return;
        }
        m_AdminView = reply.AdminView;
        m_Page = reply.Page;
        m_List.ClearItems();
        int selectedRow = -1;
        foreach (BDE_MapUI_ReportRow row : reply.Rows)
        {
            string status = "OPEN";
            if (row.Closed)
            {
                status = "CLOSED";
            }
            else if (row.PlayerReplied)
            {
                status = "PLAYER REPLY";
            }
            else if (row.HasResponse)
            {
                status = "REPLIED";
                if (row.ResponseRead)
                {
                    status = "READ";
                }
            }
            string label = "#" + row.ID.ToString() + " | " + status + " | " + row.Subject;
            if (m_AdminView)
            {
                label = "#" + row.ID.ToString() + " | " + status + " | " + row.PlayerName + " | " + row.Subject;
            }
            label += " | " + row.WorldName;
            int rowIndex = m_List.AddItem(label, row, 0);
            if (row.TicketKey == m_SelectedKey)
            {
                selectedRow = rowIndex;
            }
        }
        TextWidget.Cast(m_Root.FindAnyWidget("ReportPage")).SetText("Page " + (m_Page + 1).ToString());
        m_Root.FindAnyWidget("ReportPrev").Enable(m_Page > 0);
        m_Root.FindAnyWidget("ReportNext").Enable(reply.HasNext);
        m_Root.FindAnyWidget("ReportNew").Show(!m_AdminView);
        m_Root.FindAnyWidget("ReportAdminTools").Show(true);
        m_Root.FindAnyWidget("ReportClose").Show(m_AdminView && m_IsAdmin);
        m_Root.FindAnyWidget("ReportDelete").Show(m_AdminView && m_IsAdmin);
        m_Root.FindAnyWidget("ReportToggleDetails").Show(m_AdminView && m_IsAdmin);
        if (reply.Action == "SUBMIT" && reply.Status.IndexOf("Report #") == 0)
        {
            m_Subject.SetText("");
            m_Message.SetText("");
        }
        if (reply.Action == "DELETE" && reply.MessageSaved)
        {
            m_SelectedID = 0;
            m_SelectedKey = "";
            m_Selected = null;
            m_Root.FindAnyWidget("ReportDetail").Show(false);
        }
        if (selectedRow < 0 && m_AdminView && reply.Rows.Count() > 0)
        {
            selectedRow = 0;
            m_List.SelectRow(selectedRow);
            SelectReport();
        }
        else if (selectedRow >= 0)
        {
            m_List.SelectRow(selectedRow);
            if (m_Root.FindAnyWidget("ReportDetail").IsVisibleHierarchy())
                Send("DETAIL");
        }
        if (selectedRow < 0 && m_AdminView)
        {
            m_SelectedID = 0;
            m_SelectedKey = "";
            m_Selected = null;
            m_Root.FindAnyWidget("ReportDetail").Show(false);
        }
        if (reply.MessageSaved && reply.ReportKey == m_SelectedKey)
        {
            m_Response.SetText("");
        }
        if (reply.Action == "RESPOND" || reply.Action == "MESSAGE" || reply.Action == "CLOSE")
        {
            Send("DETAIL");
        }
    }

    protected void RenderConversation()
    {
        if (!m_Selected)
        {
            return;
        }
        m_Root.FindAnyWidget("ReportRespond").Enable(!m_Selected.Closed);
        m_Response.Enable(!m_Selected.Closed);
        string toggleLabel = "Show details";
        if (m_ShowDetails)
        {
            toggleLabel = "Hide details";
        }
        TextWidget.Cast(m_Root.FindAnyWidget("ReportToggleDetailsLabel")).SetText(toggleLabel);
        string state = "Open";
        if (m_Selected.Closed)
        {
            state = "Closed";
        }
        TextWidget.Cast(m_Root.FindAnyWidget("ReportOwnerTitle")).SetText(m_Selected.PlayerName + "'s Report");
        string detail = "<p><b>" + Escape(m_Selected.Category) + " | " + Escape(m_Selected.Subject) + "</b></p>";
        if (!m_AdminView || m_ShowDetails)
        {
            detail += "<p>#" + m_Selected.ID.ToString() + " | " + state + " | " + Escape(m_Selected.CreatedUTC) + "</p>";
        }
        detail += "<p><b>Submitted from map: " + Escape(m_Selected.WorldName) + "</b></p>";
        detail += "<p>Ticket key: " + Escape(m_Selected.TicketKey) + "</p>";
        detail += "<p>" + Escape(m_Selected.Message) + "</p>";
        m_Selected.EnsureHistory();
        foreach (BDE_MapUI_ReportMessage message : m_Selected.Messages)
        {
            if (!message)
            {
                continue;
            }
            detail += "<p><b>" + Escape(message.PlayerName) + "</b>";
            if (!m_AdminView || m_ShowDetails)
            {
                string role = "Player";
                if (message.IsAdmin)
                {
                    role = "Admin";
                }
                detail += " | " + role + " | " + Escape(message.CreatedUTC);
            }
            if (message.MapName != "")
                detail += " | " + Escape(message.MapName);
            detail += "<br/>" + Escape(message.Message) + "</p>";
        }
        if (m_Selected.Response == "")
        {
            detail += "<p>Waiting for Admin Response</p>";
        }
        if (m_AdminView && m_IsAdmin && m_ShowDetails)
        {
            string readState = "Not opened by player";
            if (m_Selected.ResponseReadUTC != "")
            {
                readState = "Opened: " + m_Selected.ResponseReadUTC;
            }
            detail += "<p>" + readState + "</p><h2>Player snapshot</h2><p>Steam64: " + m_Selected.SteamID;
            detail += "<br/>World: " + Escape(m_Selected.WorldName) + "<br/>Position: " + m_Selected.Position.ToString();
            detail += "<br/>Character: " + Escape(m_Selected.CharacterType) + "<br/>Health: " + m_Selected.Health.ToString() + " | Blood: " + m_Selected.Blood.ToString() + "</p>";
            detail += "<h2>Nearby players (100m)</h2><p>";
            foreach (string nearby : m_Selected.NearbyPlayers)
            {
                detail += Escape(nearby) + "<br/>";
            }
            detail += "</p><h2>Inventory</h2><p>";
            foreach (string item : m_Selected.Inventory)
            {
                detail += Escape(item) + "<br/>";
            }
            detail += "</p>";
        }
        m_DetailText.SetText(detail);
        ScrollWidget.Cast(m_Root.FindAnyWidget("ReportDetailScroll")).VScrollToPos01(0);
        m_FitPasses = 3;
    }

    void Poll()
    {
        if (!m_Root || !m_Root.IsVisibleHierarchy())
            return;
        Send("LIST");
    }

    void Update()
    {
        if (m_FitPasses > 0)
        {
            float width, height;
            m_DetailText.GetScreenSize(width, height);
            m_DetailText.SetFlags(WidgetFlags.HEXACTSIZE | WidgetFlags.VEXACTSIZE);
            m_DetailText.SetScreenSize(width, height);
            m_DetailText.Update();
            m_DetailText.SetScreenSize(width, Math.Max(100, m_DetailText.GetContentHeight() + 40));
            m_DetailText.Update();
            m_FitPasses--;
        }
    }

    bool OnClick(Widget w)
    {
        string name = w.GetName();
        BDE_MapUI_PortalRequest request;
        if (name == "ReportMine" || name == "ReportAdmin")
        {
            m_AdminView = name == "ReportAdmin" && m_IsAdmin;
            m_Page = 0;
            m_SelectedID = 0;
            m_SelectedKey = "";
            m_Response.SetText("");
            m_Root.FindAnyWidget("ReportDetail").Show(false);
            m_Root.FindAnyWidget("ReportCompose").Show(!m_AdminView);
            Send("LIST");
        }
        else if (name == "ReportToggleDetails")
        {
            m_ShowDetails = !m_ShowDetails;
            RenderConversation();
        }
        else if (name == "ReportRefresh")
        {
            Send("LIST");
            if (m_SelectedID > 0)
            {
                Send("DETAIL");
            }
        }
        else if (name == "ReportPrev" || name == "ReportNext")
        {
            if (name == "ReportNext")
            {
                m_Page++;
            }
            else
            {
                m_Page = Math.Max(0, m_Page - 1);
            }
            Send("LIST");
        }
        else if (name == "ReportNew")
        {
            m_SelectedID = 0;
            m_SelectedKey = "";
            m_Root.FindAnyWidget("ReportCompose").Show(true);
            m_Root.FindAnyWidget("ReportDetail").Show(false);
            m_Status.SetText("");
        }
        else if (name == "ReportSubmit")
        {
            int selected = m_Type.GetCurrentItem();
            array<string> types = BDE_MapUI_ConfigManager.Get().ReportTypes;
            if (selected < 0 || selected >= types.Count())
            {
                return true;
            }
            request = MakeRequest("SUBMIT");
            request.Category = types[selected];
            request.Subject = m_Subject.GetText();
            m_Message.GetText(request.Message);
            BDE_MapUI_PortalService.Get().Request(request);
        }
        else if (name == "ReportRespond" || name == "ReportClose" || name == "ReportDelete")
        {
            if (m_SelectedID <= 0 || (name != "ReportRespond" && (!m_IsAdmin || !m_AdminView)))
            {
                return true;
            }
            string action = "CLOSE";
            if (name == "ReportRespond")
            {
                action = "MESSAGE";
                if (m_AdminView && m_IsAdmin)
                {
                    action = "RESPOND";
                }
            }
            if (name == "ReportDelete")
            {
                if (m_DeleteConfirmID != m_SelectedID)
                {
                    m_DeleteConfirmID = m_SelectedID;
                    m_Status.SetText("Click Delete again to remove this ticket from all servers.");
                    return true;
                }
                action = "DELETE";
            }
            request = MakeRequest(action);
            m_Response.GetText(request.Message);
            BDE_MapUI_PortalService.Get().Request(request);
        }
        else
        {
            return false;
        }
        return true;
    }
};
