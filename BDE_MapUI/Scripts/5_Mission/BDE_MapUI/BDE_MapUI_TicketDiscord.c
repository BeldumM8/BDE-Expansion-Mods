// Never part of the configuration sent to clients.
class BDE_MapUI_TicketDiscordConfig
{
    int Version = 1;
    bool Enabled = false;
    string WebhookURL;
    string Username = "BDE Tickets";
};

class BDE_MapUI_TicketDiscordMentions
{
    ref array<string> parse = new array<string>;
};

class BDE_MapUI_TicketDiscordPayload
{
    string username;
    string content;
    ref BDE_MapUI_TicketDiscordMentions allowed_mentions = new BDE_MapUI_TicketDiscordMentions;
    ref array<ref BDE_MapUI_TicketDiscordEmbed> embeds = new array<ref BDE_MapUI_TicketDiscordEmbed>;
};

class BDE_MapUI_TicketDiscordEmbed
{
    string title;
    string description;
    int color = 3447003;
};

class BDE_MapUI_TicketDiscordCallback : RestCallback
{
    BDE_MapUI_TicketDiscord Owner;
    string EventID;

    override void OnSuccess(string data, int dataSize)
    {
        Owner.Completed(EventID, true);
    }

    override void OnError(int errorCode)
    {
        Owner.Completed(EventID, false);
    }

    override void OnTimeout()
    {
        Owner.Completed(EventID, false);
    }
};

class BDE_MapUI_TicketDiscord
{
    protected string m_Local;
    protected string m_ServerID;
    protected string m_InFlight;
    protected int m_NextAttempt;
    protected int m_Failures;
    protected int m_Started;
    protected ref BDE_MapUI_TicketDiscordCallback m_Callback;

    void BDE_MapUI_TicketDiscord(string localDirectory, string serverID)
    {
        m_Local = localDirectory;
        m_ServerID = serverID;
        ExpansionStatic.MakeDirectoryRecursive(m_Local + "/DiscordSent");
        string path = m_Local + "/Discord.json";
        if (!FileExist(path))
        {
            BDE_MapUI_TicketDiscordConfig defaults = new BDE_MapUI_TicketDiscordConfig;
            string error;
            if (!JsonFileLoader<BDE_MapUI_TicketDiscordConfig>.SaveFile(path, defaults, error))
                Print("[BDE_MapUI tickets] Could not create server-local Discord settings.");
        }
    }

    static string Clip(string text, int limit)
    {
        if (text.Length() > limit)
            return text.Substring(0, limit) + "...";
        return text;
    }

    void Process(array<ref BDE_MapUI_TicketEvent> ordered)
    {
        int now = CF_Date.Now(true).DateToEpoch();
        if (m_InFlight != "" && now - m_Started > 180)
            Completed(m_InFlight, false);
        if (m_InFlight != "" || now < m_NextAttempt)
            return;
        BDE_MapUI_TicketDiscordConfig config;
        string error;
        if (!JsonFileLoader<BDE_MapUI_TicketDiscordConfig>.LoadFile(m_Local + "/Discord.json", config, error) || !config || config.Version != 1 || !config.Enabled)
            return;
        string baseURL = "https://discord.com/";
        string prefix = "https://discord.com/api/webhooks/";
        if (config.WebhookURL.IndexOf(prefix) != 0)
        {
            Print("[BDE_MapUI tickets] Discord URL must use https://discord.com/api/webhooks/. No request sent.");
            m_NextAttempt = now + 300;
            return;
        }
        foreach (BDE_MapUI_TicketEvent eventData : ordered)
        {
            if (eventData.Migration || eventData.OriginServer != m_ServerID || (eventData.Kind != "CREATE" && eventData.Kind != "MESSAGE"))
                continue;
            if (FileExist(m_Local + "/DiscordSent/" + eventData.ID + ".txt") || !FileExist(BDE_MapUI_TicketJournal.SHARED + "/" + eventData.ID + ".json"))
                continue;
            string kind = "Player reply";
            string body = eventData.Text;
            string subject = "Ticket reply";
            if (eventData.Kind == "CREATE")
            {
                kind = "New ticket";
                subject = eventData.TicketSnapshot.Category + " | " + eventData.TicketSnapshot.Subject;
                body = eventData.TicketSnapshot.Message;
            }
            else if (eventData.IsAdmin)
                kind = "Admin reply";
            BDE_MapUI_TicketDiscordPayload payload = new BDE_MapUI_TicketDiscordPayload;
            payload.username = Clip(config.Username, 70);
            if (payload.username == "")
                payload.username = "BDE Tickets";
            payload.content = kind + " | " + Clip(eventData.MapName, 80) + " | " + Clip(eventData.ActorName, 80);
            payload.content += "\nTicket: " + eventData.TicketKey + "\nEvent: " + eventData.ID + "\n" + eventData.CreatedUTC;
            BDE_MapUI_TicketDiscordEmbed embed = new BDE_MapUI_TicketDiscordEmbed;
            embed.title = Clip(subject, 240);
            embed.description = Clip(body, 4000);
            payload.embeds.Insert(embed);
            JsonSerializer serializer = new JsonSerializer;
            string json;
            if (!serializer.WriteToString(payload, false, json))
                return;
            string request = config.WebhookURL.Substring(baseURL.Length(), config.WebhookURL.Length() - baseURL.Length());
            if (request.IndexOf("?") >= 0)
                request += "&wait=true";
            else
                request += "?wait=true";
            RestApi api = GetRestApi();
            if (!api)
                api = CreateRestApi();
            if (!api)
                return;
            RestContext context = api.GetRestContext(baseURL);
            if (!context)
                return;
            m_Callback = new BDE_MapUI_TicketDiscordCallback;
            m_Callback.Owner = this;
            m_Callback.EventID = eventData.ID;
            m_InFlight = eventData.ID;
            m_Started = now;
            context.SetHeader("application/json");
            int state = context.POST(m_Callback, request, json);
            if (state >= ERestResultState.EREST_ERROR)
                Completed(eventData.ID, false);
            return;
        }
    }

    void Completed(string id, bool success)
    {
        if (id != m_InFlight)
            return;
        if (success)
        {
            FileHandle file = OpenFile(m_Local + "/DiscordSent/" + id + ".txt", FileMode.WRITE);
            if (file)
            {
                FPrintln(file, "1");
                CloseFile(file);
                m_Failures = 0;
            }
            else
                success = false;
        }
        int delay = 3;
        if (!success)
        {
            m_Failures++;
            delay = Math.Min(900, 60 * m_Failures);
            Print("[BDE_MapUI tickets] Discord delivery failed; event retained for retry. Event=" + id);
        }
        m_NextAttempt = CF_Date.Now(true).DateToEpoch() + delay;
        m_InFlight = "";
        // Never print response bodies or the webhook URL/token.
    }
};
