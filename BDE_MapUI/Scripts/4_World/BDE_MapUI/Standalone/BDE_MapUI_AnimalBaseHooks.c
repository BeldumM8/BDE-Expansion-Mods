modded class AnimalBase
{
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		if (!g_Game || !g_Game.IsServer())
			return;

		PlayerBase killerPlayer = BDE_MapUI_GetAnimalKillerPlayer(killer);
		if (!killerPlayer || !killerPlayer.GetIdentity())
			return;

		PlayerIdentity identity = killerPlayer.GetIdentity();
		BDE_MapUI_PointsAnimalKillQueue.Push(identity.GetId(), identity.GetName(), GetType());
	}

	protected PlayerBase BDE_MapUI_GetAnimalKillerPlayer(Object killer)
	{
		PlayerBase player = PlayerBase.Cast(killer);
		if (player)
			return player;

		EntityAI killerEntity = EntityAI.Cast(killer);
		if (killerEntity)
			return PlayerBase.Cast(killerEntity.GetHierarchyRootPlayer());

		return null;
	}
};
