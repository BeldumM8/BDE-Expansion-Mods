class BDE_MapUI_PointsStoreDisplay
{
    private Widget m_RootWidget;
    private TextListboxWidget m_TaskList;
    private TextListboxWidget m_StoreList;
    private ButtonWidget m_DailyButton;
    private ButtonWidget m_WeeklyButton;
    private ButtonWidget m_PurchaseButton;
    private ButtonWidget m_RefreshButton;
    private ButtonWidget m_GetVIPButton;
    private TextWidget m_TaskStatusText;
    private TextWidget m_StoreStatusText;
    private TextWidget m_BalanceText;
    private TextWidget m_VIPText;
    private TextWidget m_ActionStatusText;
    private MultilineTextWidget m_SelectedItemText;
    private MultilineTextWidget m_TaskDescriptionText;
    private ItemPreviewWidget m_ItemPreview;
    private EntityAI m_PreviewItem;

    private ref BDE_MapUI_PointsStoreSyncData m_SyncData;
    private int m_CurrentCycleFilter;
    private string m_SelectedItemID;
    private string m_SelectedTaskID;
    private string m_LastPreviewClassName;

    void BDE_MapUI_PointsStoreDisplay()
    {
        m_CurrentCycleFilter = BDE_MapUI_POINTS_CYCLE_DAILY;
        m_SelectedItemID = "";
        m_SelectedTaskID = "";
        m_LastPreviewClassName = "";
    }

    void Init(Widget rootWidget)
    {
        Widget parent = rootWidget.FindAnyWidget("Tab_3");
        if (!parent)
            return;

        Widget oldStaticScroll = rootWidget.FindAnyWidget("ScrollWidget3");
        if (oldStaticScroll)
            oldStaticScroll.Show(false);

        m_RootWidget = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/PointsStoreDisplay.layout", parent);
        if (!m_RootWidget)
            return;

        m_TaskList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsTaskList"));
        m_StoreList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsStoreList"));
        m_DailyButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsDailyButton"));
        m_WeeklyButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsWeeklyButton"));
        m_PurchaseButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsPurchaseButton"));
        m_RefreshButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsRefreshButton"));
        m_GetVIPButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsGetVIPButton"));
        m_TaskStatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsTaskStatus"));
        m_StoreStatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsStoreStatus"));
        m_BalanceText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsBalanceText"));
        m_VIPText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsVIPText"));
        m_ActionStatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsActionStatus"));
        m_SelectedItemText = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsSelectedItemText"));
        m_TaskDescriptionText = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsTaskDescription"));
        m_ItemPreview = ItemPreviewWidget.Cast(m_RootWidget.FindAnyWidget("BDE_PointsItemPreview"));

        if (m_StoreList)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_StoreList, this, "OnStoreItemSelected");
        if (m_TaskList)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_TaskList, this, "OnTaskSelected");

        ApplyFilterButtonColors();
        SetStatus("", BDE_MapUI_DisplayColors.WHITE);
        ClearTaskDescription();
        ClearSelectedItem();
    }

    void OnMenuShow()
    {
        RequestData();
    }

    void OnMenuHide()
    {
        DeletePreviewItem();
    }

    void OnTabShown()
    {
        RequestData();
    }

    void Update(float timeslice)
    {
    }

    bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return false;

        if (w == m_DailyButton)
        {
            m_CurrentCycleFilter = BDE_MapUI_POINTS_CYCLE_DAILY;
            ApplyFilterButtonColors();
            RenderTasks();
            return true;
        }
        if (w == m_WeeklyButton)
        {
            m_CurrentCycleFilter = BDE_MapUI_POINTS_CYCLE_WEEKLY;
            ApplyFilterButtonColors();
            RenderTasks();
            return true;
        }
        if (w == m_RefreshButton)
        {
            RequestData();
            return true;
        }
        if (w == m_PurchaseButton)
        {
            SendPurchaseRequest();
            return true;
        }
        if (w == m_GetVIPButton)
        {
            OpenBuyMeACoffee();
            return true;
        }

        return false;
    }

    private void RequestData()
    {
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestPointsStoreData", NULL, true, NULL);
    }

    private void SendPurchaseRequest()
    {
        if (m_SelectedItemID == "")
        {
            SetStatus("Select an item first.", BDE_MapUI_DisplayColors.YELLOW);
            return;
        }

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_PurchasePointsStoreItem", new Param1<string>(m_SelectedItemID), true, NULL);
    }

    void ReceivePointsStoreData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref BDE_MapUI_PointsStoreSyncData> data;
        if (!ctx.Read(data))
        {
            SetStatus("Points store data read failed. Version mismatch?", BDE_MapUI_DisplayColors.RED);
            return;
        }

        m_SyncData = data.param1;
        RenderAll();
    }

    void ReceivePointsStoreAction(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param2<bool, string> data;
        if (!ctx.Read(data))
        {
            SetStatus("Points store action read failed.", BDE_MapUI_DisplayColors.RED);
            return;
        }

        if (data.param1)
            SetStatus(data.param2, BDE_MapUI_DisplayColors.GREEN);
        else
            SetStatus(data.param2, BDE_MapUI_DisplayColors.RED);

        RequestData();
    }

    void OnStoreItemSelected()
    {
        if (!m_StoreList)
            return;

        int selectedIndex = m_StoreList.GetSelectedRow();
        if (selectedIndex < 0)
            return;

        BDE_MapUI_PointStoreItem item;
        m_StoreList.GetItemData(selectedIndex, 0, item);
        if (!item)
            return;

        m_SelectedItemID = item.ItemID;
        RenderSelectedItem(item);
    }

    void OnTaskSelected()
    {
        if (!m_TaskList)
            return;

        int selectedIndex = m_TaskList.GetSelectedRow();
        if (selectedIndex < 0)
            return;

        BDE_MapUI_PointTask task;
        m_TaskList.GetItemData(selectedIndex, 0, task);
        if (!task)
            return;

        m_SelectedTaskID = task.TaskID;
        RenderSelectedTask(task);
    }

    private void OpenBuyMeACoffee()
    {
        BDE_MapUI_Config panelConfig = BDE_MapUI_ConfigManager.Get();
        if (panelConfig && panelConfig.VIPURL != "")
        {
            g_Game.OpenURL(panelConfig.VIPURL);
            return;
        }

        SetStatus("BuyMeACoffee link is not configured.", BDE_MapUI_DisplayColors.YELLOW);
    }

    private void RenderAll()
    {
        RenderBalance();
        RenderTasks();
        RenderStore();
    }

    private void RenderBalance()
    {
        int points = 0;
        if (m_SyncData && m_SyncData.PlayerData)
            points = m_SyncData.PlayerData.Points;

        if (m_BalanceText)
            m_BalanceText.SetText(points.ToString() + " Points");

        if (m_VIPText)
        {
            if (m_SyncData && m_SyncData.IsVIP)
            {
                m_VIPText.SetText("VIP Rewards x" + m_SyncData.VIPMultiplier.ToString());
                m_VIPText.SetColor(BDE_MapUI_DisplayColors.YELLOW);
            }
            else
            {
                m_VIPText.SetText("Standard Rewards");
                m_VIPText.SetColor(BDE_MapUI_DisplayColors.WHITE);
            }
        }

        if (m_GetVIPButton)
        {
            if (m_SyncData && m_SyncData.IsVIP)
                m_GetVIPButton.Show(false);
            else
                m_GetVIPButton.Show(true);
        }
    }

    private void RenderTasks()
    {
        if (!m_TaskList)
            return;

        m_TaskList.ClearItems();
        if (!m_SyncData || !m_SyncData.Config || !m_SyncData.Config.Tasks)
        {
            SetTaskStatus("No task config loaded.", BDE_MapUI_DisplayColors.YELLOW);
            ClearTaskDescription();
            return;
        }

        int count = 0;
        int selectedIndex = -1;
        BDE_MapUI_PointTask firstTask = null;
        for (int i = 0; i < m_SyncData.Config.Tasks.Count(); i++)
        {
            BDE_MapUI_PointTask task = m_SyncData.Config.Tasks.Get(i);
            if (!task || task.Cycle != m_CurrentCycleFilter)
                continue;

            task.EnsureDefaults();
            BDE_MapUI_PointTaskState state = GetTaskState(task.TaskID);
            int progress = 0;
            bool complete = false;
            if (state)
            {
                progress = state.Progress;
                complete = state.Completed || state.Paid;
            }
            if (complete)
                continue;

            if (progress > task.Target)
                progress = task.Target;

            int row = m_TaskList.AddItem(BDETaskLabel(task.Name), task, 0);
            m_TaskList.SetItem(row, "| " + progress.ToString() + "/" + task.Target.ToString(), task, 1);
            m_TaskList.SetItem(row, "| " + GetDisplayAward(task.Points).ToString() + "P", task, 2);
            if (!firstTask)
                firstTask = task;
            if (m_SelectedTaskID != "" && task.TaskID == m_SelectedTaskID)
                selectedIndex = count;
            count++;
        }

        SetTaskStatus("Tasks loaded: " + count.ToString(), BDE_MapUI_DisplayColors.WHITE);

        if (selectedIndex >= 0)
        {
            m_TaskList.SelectRow(selectedIndex);
            RenderSelectedTask(FindTask(m_SelectedTaskID));
        }
        else if (firstTask)
        {
            m_SelectedTaskID = firstTask.TaskID;
            m_TaskList.SelectRow(0);
            RenderSelectedTask(firstTask);
        }
        else
        {
            m_SelectedTaskID = "";
            ClearTaskDescription();
        }
    }

    private void RenderStore()
    {
        if (!m_StoreList)
            return;

        m_StoreList.ClearItems();
        if (!m_SyncData || !m_SyncData.Config || !m_SyncData.Config.StoreItems)
        {
            SetStoreStatus("No store config loaded.", BDE_MapUI_DisplayColors.YELLOW);
            return;
        }

        int selectedIndex = -1;
        BDE_MapUI_PointStoreItem firstItem = null;
        for (int i = 0; i < m_SyncData.Config.StoreItems.Count(); i++)
        {
            BDE_MapUI_PointStoreItem item = m_SyncData.Config.StoreItems.Get(i);
            if (!item)
                continue;

            item.EnsureDefaults();
            int row = m_StoreList.AddItem(item.Price.ToString() + "P", item, 0);
            m_StoreList.SetItem(row, "| " + item.Name, item, 1);
            if (!firstItem)
                firstItem = item;

            if (m_SelectedItemID != "" && item.ItemID == m_SelectedItemID)
                selectedIndex = i;
        }

        SetStoreStatus("Items loaded: " + m_SyncData.Config.StoreItems.Count().ToString(), BDE_MapUI_DisplayColors.WHITE);

        if (selectedIndex >= 0)
        {
            m_StoreList.SelectRow(selectedIndex);
            BDE_MapUI_PointStoreItem selected = m_SyncData.Config.StoreItems.Get(selectedIndex);
            RenderSelectedItem(selected);
        }
        else if (firstItem)
        {
            m_SelectedItemID = firstItem.ItemID;
            m_StoreList.SelectRow(0);
            RenderSelectedItem(firstItem);
        }
        else
        {
            ClearSelectedItem();
        }
    }

    private void RenderSelectedItem(BDE_MapUI_PointStoreItem item)
    {
        if (!item)
        {
            ClearSelectedItem();
            return;
        }

        string text = item.Name + "\n";
        text += item.Category + "\n";
        text += "Cost: " + item.Price.ToString() + " points\n";
        text += "Quantity: " + item.Quantity.ToString();

        if (m_SelectedItemText)
            m_SelectedItemText.SetText(text);

        UpdateItemPreview(item.ClassName);
    }

    private void ClearSelectedItem()
    {
        m_SelectedItemID = "";
        if (m_SelectedItemText)
            m_SelectedItemText.SetText("Select a store item.");
        DeletePreviewItem();
    }

    private void RenderSelectedTask(BDE_MapUI_PointTask task)
    {
        if (!task)
        {
            ClearTaskDescription();
            return;
        }

        task.EnsureDefaults();
        BDE_MapUI_PointTaskState state = GetTaskState(task.TaskID);
        int progress = 0;
        if (state)
            progress = state.Progress;
        if (progress > task.Target)
            progress = task.Target;

        string cycleName = task.GetCycleName();
        string text = task.Name + " (" + cycleName + ")\n";
        if (task.Description != "")
            text += task.Description + "\n";
        text += "Progress: " + progress.ToString() + "/" + task.Target.ToString() + " | Reward: " + GetDisplayAward(task.Points).ToString() + " points";

        if (m_TaskDescriptionText)
        {
            m_TaskDescriptionText.SetColor(BDE_MapUI_DisplayColors.WHITE);
            m_TaskDescriptionText.SetText(text);
        }
    }

    private void ClearTaskDescription()
    {
        if (m_TaskDescriptionText)
        {
            m_TaskDescriptionText.SetColor(BDE_MapUI_DisplayColors.WHITE);
            m_TaskDescriptionText.SetText("Select a task to see what to do.");
        }
    }

    private BDE_MapUI_PointTask FindTask(string taskID)
    {
        if (taskID == "" || !m_SyncData || !m_SyncData.Config || !m_SyncData.Config.Tasks)
            return null;

        for (int i = 0; i < m_SyncData.Config.Tasks.Count(); i++)
        {
            BDE_MapUI_PointTask task = m_SyncData.Config.Tasks.Get(i);
            if (task && task.TaskID == taskID)
                return task;
        }
        return null;
    }

    private string BDETaskLabel(string label)
    {
        if (label.IndexOf("Daily ") == 0)
        {
            return label.Substring(6, label.Length() - 6);
        }
        if (label.IndexOf("Weekly ") == 0)
        {
            return label.Substring(7, label.Length() - 7);
        }
        return label;
    }

    private void UpdateItemPreview(string className)
    {
        if (!m_ItemPreview)
            return;

        if (className == "")
        {
            DeletePreviewItem();
            return;
        }

        if (m_LastPreviewClassName == className && m_PreviewItem)
            return;

        DeletePreviewItem();
        m_PreviewItem = EntityAI.Cast(g_Game.CreateObjectEx(className, "0 0 0", ECE_LOCAL));
        if (m_PreviewItem)
        {
            BDE_MapUI_Preview.SetItem(m_ItemPreview, m_PreviewItem);
            m_ItemPreview.SetModelPosition("0 0 0.65");
            m_LastPreviewClassName = className;
        }
        else
        {
            Print("[BDE_MapUI preview-v4] ERROR: cannot create store preview " + className);
        }
    }

    private void DeletePreviewItem()
    {
        if (m_ItemPreview)
            m_ItemPreview.SetItem(null);

        if (m_PreviewItem)
        {
            g_Game.ObjectDelete(m_PreviewItem);
            m_PreviewItem = null;
        }

        m_LastPreviewClassName = "";
    }

    private BDE_MapUI_PointTaskState GetTaskState(string taskID)
    {
        if (!m_SyncData || !m_SyncData.PlayerData || !m_SyncData.PlayerData.TaskStates)
            return null;

        for (int i = 0; i < m_SyncData.PlayerData.TaskStates.Count(); i++)
        {
            BDE_MapUI_PointTaskState state = m_SyncData.PlayerData.TaskStates.Get(i);
            if (state && state.TaskID == taskID)
                return state;
        }
        return null;
    }

    private int GetDisplayAward(int basePoints)
    {
        if (m_SyncData && m_SyncData.IsVIP && m_SyncData.Config)
            return Math.Floor(basePoints * m_SyncData.VIPMultiplier);
        return basePoints;
    }

    private void ApplyFilterButtonColors()
    {
        SetButtonColor(m_DailyButton, m_CurrentCycleFilter == BDE_MapUI_POINTS_CYCLE_DAILY);
        SetButtonColor(m_WeeklyButton, m_CurrentCycleFilter == BDE_MapUI_POINTS_CYCLE_WEEKLY);
    }

    private void SetButtonColor(ButtonWidget button, bool active)
    {
        if (!button)
            return;

        if (active)
            button.SetColor(BDE_MapUI_DisplayColors.ACTIVE_BUTTON_COLOR);
        else
            button.SetColor(BDE_MapUI_DisplayColors.NORMAL_BUTTON_COLOR);
    }

    private void SetStatus(string message, int color)
    {
        if (m_ActionStatusText)
        {
            m_ActionStatusText.SetText(message);
            m_ActionStatusText.SetColor(color);
        }
    }

    private void SetTaskStatus(string message, int color)
    {
        if (m_TaskStatusText)
        {
            m_TaskStatusText.SetText(message);
            m_TaskStatusText.SetColor(color);
        }
    }

    private void SetStoreStatus(string message, int color)
    {
        if (m_StoreStatusText)
        {
            m_StoreStatusText.SetText(message);
            m_StoreStatusText.SetColor(color);
        }
    }
};
