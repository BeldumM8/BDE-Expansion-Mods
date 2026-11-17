modded class AnalyticsManagerServer
{
	override protected void OnPlayerKilled(Man killer, EntityAI target)
	{
		super.OnPlayerKilled(killer, target);

		if (!g_Game || !g_Game.IsServer())
			return;

		if (!killer || !target)
			return;

		PlayerIdentity killerIdentity = killer.GetIdentity();
		if (!killerIdentity)
			return;

		string killerUID = killerIdentity.GetId();
		if (killerUID == "")
			return;

		// Expansion AI survivors are PlayerBase/eAIBase classes, so vanilla analytics can see them as "player" kills.
		// Keep AI kills separate for the Server Panel leaderboard and do not process them as bounty kills.
		string targetType = target.GetType();
		string targetTypeLower = targetType;
		targetTypeLower.ToLower();
		bool targetLooksLikeAI = targetTypeLower.Contains("eai_") || targetTypeLower.Contains("eaibase");

		Man victim;
		Class.CastTo(victim, target);

		PlayerIdentity victimIdentity = null;
		if (victim)
			victimIdentity = victim.GetIdentity();

		if (targetLooksLikeAI || !victimIdentity)
		{
			BDE_MapUI_StatsKillQueue.PushAIKill(killerUID, killerIdentity.GetName());
			return;
		}

		string victimUID = victimIdentity.GetId();
		if (victimUID == "")
			return;

		if (victimUID == killerUID)
			return;

		BDE_MapUI_BountyKillQueue.Push(victimUID, victimIdentity.GetName(), killerUID, killerIdentity.GetName());
	}
};
