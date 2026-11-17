class BDE_MapUI_ReportMessage
{
    string EventID;
    string MapName;
    string PlayerName;
    string Message;
    string CreatedUTC;
    bool IsAdmin;
};

class BDE_MapUI_Report
{
    string TicketKey;
    string LastResponseKey;
    ref array<string> ViewEventIDs = new array<string>;
    int ID;
    string SteamID;
    string IdentityID;
    string PlayerName;
    string Category;
    string Subject;
    string Message;
    string CreatedUTC;
    string WorldName;
    vector Position;
    vector Direction;
    string CharacterType;
    float Health;
    float Blood;
    float Shock;
    bool Alive;
    ref array<string> Inventory = new array<string>();
    ref array<string> NearbyPlayers = new array<string>();
    string Response;
    string RespondedBy;
    string RespondedUTC;
    int ResponseVersion;
    string ResponseReadUTC;
    bool Closed;
    bool HistoryInitialized;
    ref array<ref BDE_MapUI_ReportMessage> Messages = new array<ref BDE_MapUI_ReportMessage>();

    void EnsureHistory()
    {
        if (!Messages)
        {
            Messages = new array<ref BDE_MapUI_ReportMessage>();
        }
        if (!HistoryInitialized)
        {
            if (Response != "")
            {
                BDE_MapUI_ReportMessage legacy = new BDE_MapUI_ReportMessage();
                legacy.PlayerName = RespondedBy;
                legacy.Message = Response;
                legacy.CreatedUTC = RespondedUTC;
                legacy.IsAdmin = true;
                Messages.Insert(legacy);
            }
            HistoryInitialized = true;
        }
    }
};

class BDE_MapUI_ReportStore
{
    int NextID = 1;
    ref array<ref BDE_MapUI_Report> Reports = new array<ref BDE_MapUI_Report>();
};

class BDE_MapUI_ChangelogState
{
    int SnapshotVersion;
    ref array<string> ContentLines = new array<string>();
    int Revision;
    string Content;
    string SnapshotUTC;
    ref array<string> SeenSteamIDs = new array<string>();
};

class BDE_MapUI_ReportRow
{
    string TicketKey;
    string WorldName;
    int ID;
    string PlayerName;
    string Category;
    string Subject;
    bool Closed;
    bool HasResponse;
    bool PlayerReplied;
    bool ResponseRead;
};

class BDE_MapUI_PortalRequest
{
    string TicketKey;
    ref array<string> SeenIDs = new array<string>;
    string Action;
    int ID;
    int Version;
    int Page;
    bool AdminView;
    string Category;
    string Subject;
    string Message;
};

class BDE_MapUI_PortalReply
{
    int AdminUnreadCount;
    string ReportKey;
    string Action;
    string Status;
    bool MessageSaved;
    int ReportID;
    bool IsAdmin;
    bool AdminView;
    bool ChangelogPending;
    int ChangelogRevision;
    int Page;
    bool HasNext;
    ref array<ref BDE_MapUI_ReportRow> Rows = new array<ref BDE_MapUI_ReportRow>();
    ref BDE_MapUI_Report Detail;
    ref array<ref BDE_MapUI_GroupPlayer> GroupPlayers = new array<ref BDE_MapUI_GroupPlayer>();
};

class BDE_MapUI_GroupPlayer
{
    string ID;
    string Name;
};
