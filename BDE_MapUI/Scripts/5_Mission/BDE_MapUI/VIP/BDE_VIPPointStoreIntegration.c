// Per-tier integration with the BDE MapUI Point Store.
// MapUI provides overridable hooks so VIP can be the single source of truth
// for whether a player is VIP and which reward multiplier their tier receives.

modded class BDE_MapUI_PointsStoreManager
{
    override protected float GetVIPMultiplier(string uid)
    {
        if (uid == "")
            return 1.0;

        string plainId = "";
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity())
                continue;
            if (player.GetIdentity().GetId() == uid)
            {
                plainId = player.GetIdentity().GetPlainId();
                break;
            }
        }

        BDE_VIPManager manager = BDE_VIPManager.Get();
        manager.Init();
        BDE_VIPTier tier = manager.GetPlayerTierByIDs(uid, plainId);
        if (!tier || tier.PointStoreMultiplier < 1.0)
            return 1.0;
        return tier.PointStoreMultiplier;
    }

    override protected bool IsVIP(string uid)
    {
        if (uid == "")
            return false;

        string plainId = "";
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity())
                continue;
            if (player.GetIdentity().GetId() == uid)
            {
                plainId = player.GetIdentity().GetPlainId();
                break;
            }
        }

        BDE_VIPManager manager = BDE_VIPManager.Get();
        manager.Init();
        return manager.GetPlayerEntryByIDs(uid, plainId) != null;
    }
}
