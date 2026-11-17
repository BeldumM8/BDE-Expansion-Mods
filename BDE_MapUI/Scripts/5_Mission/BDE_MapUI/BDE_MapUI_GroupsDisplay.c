class BDE_MapUI_GroupsDisplay
{
    protected Widget m_Root;
    protected ExpansionPartyModule m_Module;
    protected TextListboxWidget m_Members;
    protected TextListboxWidget m_Online;
    protected TextListboxWidget m_Invites;
    protected TextWidget m_Status;
    protected ref array<CheckBoxWidget> m_Permissions = new array<CheckBoxWidget>();
    protected ref array<int> m_PermissionBits = new array<int>();
    protected string m_SelectedMember;
    protected string m_LastSnapshot;
    protected string m_ConfirmAction;
    protected float m_Elapsed;

    void Init(Widget host)
    {
        m_Root = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/GroupsDisplay.layout", host);
        m_Module = ExpansionPartyModule.Cast(CF_ModuleCoreManager.Get(ExpansionPartyModule));
        m_Members = TextListboxWidget.Cast(m_Root.FindAnyWidget("GroupMembers"));
        m_Online = TextListboxWidget.Cast(m_Root.FindAnyWidget("GroupOnline"));
        m_Invites = TextListboxWidget.Cast(m_Root.FindAnyWidget("GroupInvites"));
        m_Status = TextWidget.Cast(m_Root.FindAnyWidget("GroupStatus"));
        m_Permissions.Insert(CheckBoxWidget.Cast(m_Root.FindAnyWidget("GroupPermEdit")));
        m_Permissions.Insert(CheckBoxWidget.Cast(m_Root.FindAnyWidget("GroupPermInvite")));
        m_Permissions.Insert(CheckBoxWidget.Cast(m_Root.FindAnyWidget("GroupPermKick")));
        m_Permissions.Insert(CheckBoxWidget.Cast(m_Root.FindAnyWidget("GroupPermDelete")));
        m_Permissions.Insert(CheckBoxWidget.Cast(m_Root.FindAnyWidget("GroupPermMoney")));
        m_PermissionBits.Insert(ExpansionPartyPlayerPermissions.CAN_EDIT);
        m_PermissionBits.Insert(ExpansionPartyPlayerPermissions.CAN_INVITE);
        m_PermissionBits.Insert(ExpansionPartyPlayerPermissions.CAN_KICK);
        m_PermissionBits.Insert(ExpansionPartyPlayerPermissions.CAN_DELETE);
        m_PermissionBits.Insert(ExpansionPartyPlayerPermissions.CAN_WITHDRAW_MONEY);
        WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_Members, this, "SelectMember");
    }

    void Show()
    {
        m_ConfirmAction = "";
        m_LastSnapshot = "";
        RefreshPlayers();
        RefreshParty();
    }

    protected void RefreshPlayers()
    {
        BDE_MapUI_PortalRequest request = new BDE_MapUI_PortalRequest();
        request.Action = "GROUP_PLAYERS";
        BDE_MapUI_PortalService.Get().Request(request);
    }

    void Receive(BDE_MapUI_PortalReply reply)
    {
        BDE_MapUI_GroupPlayer previous;
        if (m_Online.GetSelectedRow() >= 0)
        {
            m_Online.GetItemData(m_Online.GetSelectedRow(), 0, previous);
        }
        m_Online.ClearItems();
        int selectedRow = -1;
        foreach (BDE_MapUI_GroupPlayer player : reply.GroupPlayers)
        {
            int row = m_Online.AddItem(player.Name, player, 0);
            if (previous && previous.ID == player.ID)
            {
                selectedRow = row;
            }
        }
        if (reply.GroupPlayers.Count() > 0)
        {
            m_Online.SelectRow(Math.Max(0, selectedRow));
        }
    }

    void Update(float timeslice)
    {
        m_Elapsed += timeslice;
        if (m_Elapsed >= 1)
        {
            m_Elapsed = 0;
            RefreshParty();
        }
    }

    protected void RefreshParty()
    {
        if (!m_Module)
        {
            m_Status.SetText("Expansion Groups is unavailable.");
            return;
        }
        ExpansionPartyData party = m_Module.GetParty();
        array<ref ExpansionPartyInviteData> invites = m_Module.GetPartyInvites();
        string snapshot = "No group";
        if (party)
        {
            snapshot = party.GetPartyName() + "|" + party.GetOwnerUID();
            foreach (ExpansionPartyPlayerData member : party.GetPlayers())
            {
                snapshot += "|" + member.UID + member.Name + member.Permissions.ToString();
            }
        }
        if (invites)
        {
            foreach (ExpansionPartyInviteData invite : invites)
            {
                snapshot += "|" + invite.partyID.ToString() + invite.partyName;
            }
        }
        if (snapshot == m_LastSnapshot)
        {
            return;
        }
        m_LastSnapshot = snapshot;
        m_ConfirmAction = "";
        m_Members.ClearItems();
        m_Invites.ClearItems();
        string heading = "You are not in a group";
        ExpansionPartyPlayerData self;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (party)
        {
            heading = party.GetPartyName() + " | Leader: " + party.GetOwnerName();
            if (player && player.GetIdentity())
            {
                self = party.GetPlayer(player.GetIdentity().GetId());
            }
            foreach (ExpansionPartyPlayerData entry : party.GetPlayers())
            {
                string name = entry.Name;
                if (entry.UID == party.GetOwnerUID())
                {
                    name += " (Leader)";
                }
                int row = m_Members.AddItem(name, entry, 0);
                if (entry.UID == m_SelectedMember)
                {
                    m_Members.SelectRow(row);
                }
            }
        }
        if (invites)
        {
            foreach (ExpansionPartyInviteData invitation : invites)
            {
                m_Invites.AddItem(invitation.partyName, invitation, 0);
            }
        }
        TextWidget.Cast(m_Root.FindAnyWidget("GroupHeading")).SetText(heading);
        m_Root.FindAnyWidget("GroupCreate").Enable(!party);
        m_Root.FindAnyWidget("GroupLeave").Enable(party != null);
        m_Root.FindAnyWidget("GroupInvite").Enable(self && self.CanInvite());
        m_Root.FindAnyWidget("GroupKick").Enable(self && self.CanKick());
        m_Root.FindAnyWidget("GroupDissolve").Enable(self && self.CanDelete());
        m_Root.FindAnyWidget("GroupSavePermissions").Enable(self && self.CanEdit());
        if (party && party.GetPlayers().Count() > 0 && m_Members.GetSelectedRow() < 0)
        {
            m_Members.SelectRow(0);
        }
        SelectMember();
    }

    void SelectMember()
    {
        m_ConfirmAction = "";
        ExpansionPartyPlayerData member;
        if (m_Members.GetSelectedRow() >= 0)
        {
            m_Members.GetItemData(m_Members.GetSelectedRow(), 0, member);
        }
        m_SelectedMember = "";
        int bits;
        if (member)
        {
            m_SelectedMember = member.UID;
            bits = member.Permissions;
        }
        for (int i = 0; i < m_Permissions.Count(); i++)
        {
            m_Permissions[i].SetChecked((bits & m_PermissionBits[i]) != 0);
        }
    }

    bool OnClick(Widget w)
    {
        string name = w.GetName();
        if (!m_Module)
        {
            return false;
        }
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player || !player.GetIdentity())
        {
            return false;
        }
        if (name == "GroupRefresh")
        {
            RefreshPlayers();
            m_LastSnapshot = "";
            RefreshParty();
            return true;
        }
        if (name == "GroupCreate")
        {
            string groupName = EditBoxWidget.Cast(m_Root.FindAnyWidget("GroupName")).GetText();
            string tag = EditBoxWidget.Cast(m_Root.FindAnyWidget("GroupTag")).GetText();
            groupName.TrimInPlace();
            tag.TrimInPlace();
            if (groupName == "")
            {
                m_Status.SetText("Enter a group name and tag.");
                return true;
            }
            m_Module.CreateParty(groupName, tag);
        }
        else if (name == "GroupInvite")
        {
            BDE_MapUI_GroupPlayer online;
            if (m_Online.GetSelectedRow() >= 0)
            {
                m_Online.GetItemData(m_Online.GetSelectedRow(), 0, online);
            }
            if (!online)
            {
                m_Status.SetText("Select an online player first.");
                return true;
            }
            m_Module.InvitePlayer(online.ID);
        }
        else if (name == "GroupAccept" || name == "GroupDecline")
        {
            ExpansionPartyInviteData invitation;
            if (m_Invites.GetSelectedRow() >= 0)
            {
                m_Invites.GetItemData(m_Invites.GetSelectedRow(), 0, invitation);
            }
            if (!invitation)
            {
                m_Status.SetText("Select an invitation first.");
                return true;
            }
            if (name == "GroupAccept")
            {
                m_Module.AcceptInvite(invitation.partyID);
            }
            else
            {
                m_Module.DeclineInvite(invitation.partyID);
            }
        }
        else if (name == "GroupLeave" || name == "GroupDissolve" || name == "GroupKick")
        {
            if (name == "GroupKick" && m_SelectedMember == "")
            {
                m_Status.SetText("Select a member first.");
                return true;
            }
            string confirmation = name + m_SelectedMember;
            if (m_ConfirmAction != confirmation)
            {
                m_ConfirmAction = confirmation;
                m_Status.SetText("Click the same button again to confirm this group change.");
                return true;
            }
            m_ConfirmAction = "";
            if (name == "GroupLeave")
            {
                m_Module.RemovePartyMember(player.GetIdentity().GetId(), true);
            }
            else if (name == "GroupDissolve")
            {
                m_Module.DissolveParty();
            }
            else
            {
                m_Module.RemovePartyMember(m_SelectedMember);
            }
        }
        else if (name == "GroupSavePermissions")
        {
            if (m_SelectedMember == "")
            {
                m_Status.SetText("Select a member first.");
                return true;
            }
            int bits;
            for (int i = 0; i < m_Permissions.Count(); i++)
            {
                if (m_Permissions[i].IsChecked())
                {
                    bits |= m_PermissionBits[i];
                }
            }
            m_Module.UpdatePermissions(m_SelectedMember, bits);
        }
        else
        {
            return false;
        }
        m_Status.SetText("Request sent. Expansion checks group permissions and settings.");
        return true;
    }
};
