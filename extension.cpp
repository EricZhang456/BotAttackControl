#include "extension.h"

BotAttack g_BotAttack;
SMEXT_LINK(&g_BotAttack);

IGameConfig *g_pGameConf;
IForward *g_OnBotAttack;

#if !(SOURCE_ENGINE == SE_CSGO && defined PLATFORM_WINDOWS)
CDetour *dtrInSameTeam;
#endif
#if SOURCE_ENGINE == SE_CSGO
CDetour *dtrIsOtherEnemy;
#endif
CDetour *dtrOnAudibleEvent;
CDetour *dtrOnPlayerRadio;
CDetour *dtrOnPlayerDeath;

int g_iTeamOffset = -1;

inline int GetEntityTeamNum(CBaseEntity *pEntity)
{
	return *(int8_t *)((uint8_t *)pEntity + g_iTeamOffset);
}

inline void SetEntityTeamNum(CBaseEntity *pEntity, int teamNum)
{
	*(int8_t *)((uint8_t *)pEntity + g_iTeamOffset) = (int8_t)teamNum;
}

#if !(SOURCE_ENGINE == SE_CSGO && defined PLATFORM_WINDOWS)
DETOUR_DECL_MEMBER1(InSameTeam, bool, CBaseEntity *, pEntity)
{
	CBaseEntity *pBot = (CBaseEntity *)this;

	if (pBot != pEntity)
	{
		IGamePlayer *pPlayer1 = playerhelpers->GetGamePlayer(gamehelpers->EntityToBCompatRef(pBot));
		IGamePlayer *pPlayer2 = playerhelpers->GetGamePlayer(gamehelpers->EntityToBCompatRef(pEntity));

		if (pPlayer1 && pPlayer2)
		{
			if (pPlayer1->IsFakeClient())
				return !g_BotAttack.ShouldBotAttackPlayer(pBot, pEntity);
			if (pPlayer2->IsFakeClient())
				return !g_BotAttack.ShouldBotAttackPlayer(pEntity, pBot);
		}
	}

	return DETOUR_MEMBER_CALL(InSameTeam)(pEntity);
}
#endif

#if SOURCE_ENGINE == SE_CSGO
DETOUR_DECL_MEMBER1(IsOtherEnemy, bool, CCSPlayer *, pEntity)
{
	CBaseEntity *pBot = (CBaseEntity *)this;

	if (pBot != pEntity)
	{
		IGamePlayer *pPlayer1 = playerhelpers->GetGamePlayer(gamehelpers->EntityToBCompatRef(pBot));
		IGamePlayer *pPlayer2 = playerhelpers->GetGamePlayer(gamehelpers->EntityToBCompatRef(pEntity));

		if (pPlayer1 && pPlayer2)
		{
			if (pPlayer1->IsFakeClient())
				return g_BotAttack.ShouldBotAttackPlayer(pBot, pEntity);
			if (pPlayer2->IsFakeClient())
				return g_BotAttack.ShouldBotAttackPlayer(pEntity, pBot);
		}
	}

	return DETOUR_MEMBER_CALL(IsOtherEnemy)(pEntity);
}
#endif

#if SOURCE_ENGINE == SE_CSS || defined PLATFORM_POSIX
DETOUR_DECL_MEMBER7(OnAudibleEvent, void, IGameEvent *, pEvent, CBasePlayer *, pPlayer, float, range, PriorityType, priority, bool, isHostile, bool, isFootstep, const Vector *, actualOrigin)
#elif SOURCE_ENGINE == SE_CSGO
DETOUR_DECL_MEMBER6(OnAudibleEvent, void, IGameEvent *, pEvent, CBasePlayer *, pPlayer, PriorityType, priority, bool, isHostile, bool, isFootstep, const Vector *, actualOrigin)
#endif
{
	CBaseEntity *pBot = (CBaseEntity *)this;

	if (!pPlayer || pPlayer == pBot)
	{
#if SOURCE_ENGINE == SE_CSS || defined PLATFORM_POSIX
		DETOUR_MEMBER_CALL(OnAudibleEvent)(pEvent, pPlayer, range, priority, isHostile, isFootstep, actualOrigin);
#elif SOURCE_ENGINE == SE_CSGO
		DETOUR_MEMBER_CALL(OnAudibleEvent)(pEvent, pPlayer, priority, isHostile, isFootstep, actualOrigin);
#endif
		return;
	}

	int origTeam1 = GetEntityTeamNum(pBot);
	int origTeam2 = GetEntityTeamNum(pPlayer);

	// Prepare for team check inside this function.
	SetEntityTeamNum(pBot, CS_TEAM_CT);
	SetEntityTeamNum(pPlayer, g_BotAttack.ShouldBotAttackPlayer(pBot, pPlayer) ? CS_TEAM_T : CS_TEAM_CT);

#if SOURCE_ENGINE == SE_CSS || defined PLATFORM_POSIX
	DETOUR_MEMBER_CALL(OnAudibleEvent)(pEvent, pPlayer, range, priority, isHostile, isFootstep, actualOrigin);
#elif SOURCE_ENGINE == SE_CSGO
	DETOUR_MEMBER_CALL(OnAudibleEvent)(pEvent, pPlayer, priority, isHostile, isFootstep, actualOrigin);
#endif

	// Revert teams to their original values.
	SetEntityTeamNum(pBot, origTeam1);
	SetEntityTeamNum(pPlayer, origTeam2);
}

DETOUR_DECL_MEMBER1(OnPlayerRadio, void, IGameEvent *, pEvent)
{
	CBaseEntity *pBot = (CBaseEntity *)this;
	CBaseEntity *pPlayer = gamehelpers->ReferenceToEntity(playerhelpers->GetClientOfUserId(pEvent->GetInt("userid")));

	if (!pPlayer || pPlayer == pBot)
	{
		DETOUR_MEMBER_CALL(OnPlayerRadio)(pEvent);
		return;
	}

	int origTeam1 = GetEntityTeamNum(pBot);
	int origTeam2 = GetEntityTeamNum(pPlayer);

	// Prepare for team check inside this function.
	SetEntityTeamNum(pBot, CS_TEAM_CT);
	SetEntityTeamNum(pPlayer, g_BotAttack.ShouldBotAttackPlayer(pBot, pPlayer) ? CS_TEAM_T : CS_TEAM_CT);

	DETOUR_MEMBER_CALL(OnPlayerRadio)(pEvent);

	// Revert teams to their original values.
	SetEntityTeamNum(pBot, origTeam1);
	SetEntityTeamNum(pPlayer, origTeam2);
}

DETOUR_DECL_MEMBER1(OnPlayerDeath, void, IGameEvent *, pEvent)
{
	CBaseEntity *pBot = (CBaseEntity *)this;
	CBaseEntity *pPlayer = gamehelpers->ReferenceToEntity(playerhelpers->GetClientOfUserId(pEvent->GetInt("userid")));

	if (!pPlayer || pPlayer == pBot)
	{
		DETOUR_MEMBER_CALL(OnPlayerDeath)(pEvent);
		return;
	}

	int origTeam1 = GetEntityTeamNum(pBot);
	int origTeam2 = GetEntityTeamNum(pPlayer);

	// Prepare for team check inside this function.
	SetEntityTeamNum(pBot, CS_TEAM_CT);
	SetEntityTeamNum(pPlayer, g_BotAttack.ShouldBotAttackPlayer(pBot, pPlayer) ? CS_TEAM_T : CS_TEAM_CT);

	DETOUR_MEMBER_CALL(OnPlayerDeath)(pEvent);

	// Revert teams to their original values.
	SetEntityTeamNum(pBot, origTeam1);
	SetEntityTeamNum(pPlayer, origTeam2);
}

bool BotAttack::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	// Gamedata.
	if (!gameconfs->LoadGameConfigFile("botattackcontrol.games", &g_pGameConf, error, maxlength))
		return false;

	g_OnBotAttack = g_pForwards->CreateForward("OnShouldBotAttackPlayer", ET_Event, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef);

	// Get team sendprop offset.
	sm_sendprop_info_t info;
	if (!gamehelpers->FindSendPropInfo("CBaseEntity", "m_iTeamNum", &info))
	{
		smutils->Format(error, maxlength, "Failed to find sendprop offset: CBaseEntity::m_iTeamNum");
		return false;
	}
	g_iTeamOffset = info.actual_offset;

	// Setup detours.
	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pGameConf);

#if !(SOURCE_ENGINE == SE_CSGO && defined PLATFORM_WINDOWS)
	dtrInSameTeam = DETOUR_CREATE_MEMBER(InSameTeam, "CBaseEntity::InSameTeam");
	if (!dtrInSameTeam)
	{
		smutils->Format(error, maxlength, "Detour failed: CBaseEntity::InSameTeam");
		return false;
	}
#endif

#if SOURCE_ENGINE == SE_CSGO
	dtrIsOtherEnemy = DETOUR_CREATE_MEMBER(IsOtherEnemy, "CCSPlayer::IsOtherEnemy");
	if (!dtrIsOtherEnemy)
	{
		smutils->Format(error, maxlength, "Detour failed: CCSPlayer::IsOtherEnemy");
		return false;
	}
#endif

	dtrOnAudibleEvent = DETOUR_CREATE_MEMBER(OnAudibleEvent, "CCSBot::OnAudibleEvent");
	if (!dtrOnAudibleEvent)
	{
		smutils->Format(error, maxlength, "Detour failed: CCSBot::OnAudibleEvent");
		return false;
	}

	dtrOnPlayerRadio = DETOUR_CREATE_MEMBER(OnPlayerRadio, "CCSBot::OnPlayerRadio");
	if (!dtrOnPlayerRadio)
	{
		smutils->Format(error, maxlength, "Detour failed: CCSBot::OnPlayerRadio");
		return false;
	}

	dtrOnPlayerDeath = DETOUR_CREATE_MEMBER(OnPlayerDeath, "CCSBot::OnPlayerDeath");
	if (!dtrOnPlayerDeath)
	{
		smutils->Format(error, maxlength, "Detour failed: CCSBot::OnPlayerDeath");
		return false;
	}
	
#if !(SOURCE_ENGINE == SE_CSGO && defined PLATFORM_WINDOWS)
	dtrInSameTeam->EnableDetour();
#endif
#if SOURCE_ENGINE == SE_CSGO
	dtrIsOtherEnemy->EnableDetour();
#endif
	dtrOnAudibleEvent->EnableDetour();
	dtrOnPlayerRadio->EnableDetour();
	dtrOnPlayerDeath->EnableDetour();

	return true;
}

void BotAttack::SDK_OnUnload()
{
#if !(SOURCE_ENGINE == SE_CSGO && defined PLATFORM_WINDOWS)
	if (dtrInSameTeam)
	{
		dtrInSameTeam->Destroy();
		dtrInSameTeam = NULL;
	}
#endif
#if SOURCE_ENGINE == SE_CSGO
	if (dtrIsOtherEnemy)
	{
		dtrIsOtherEnemy->Destroy();
		dtrIsOtherEnemy = NULL;
	}
#endif
	if (dtrOnAudibleEvent)
	{
		dtrOnAudibleEvent->Destroy();
		dtrOnAudibleEvent = NULL;
	}
	if (dtrOnPlayerRadio)
	{
		dtrOnPlayerRadio->Destroy();
		dtrOnPlayerRadio = NULL;
	}
	if (dtrOnPlayerDeath)
	{
		dtrOnPlayerDeath->Destroy();
		dtrOnPlayerDeath = NULL;
	}

	g_pForwards->ReleaseForward(g_OnBotAttack);

	gameconfs->CloseGameConfigFile(g_pGameConf);
}

bool BotAttack::ShouldBotAttackPlayer(CBaseEntity *pBot, CBaseEntity *pPlayer)
{
	int team1 = GetEntityTeamNum(pBot);
	int team2 = GetEntityTeamNum(pPlayer);

	cell_t result = (team1 != team2);
	g_OnBotAttack->PushCell(gamehelpers->EntityToBCompatRef(pBot));
	g_OnBotAttack->PushCell(gamehelpers->EntityToBCompatRef(pPlayer));
	g_OnBotAttack->PushCellByRef(&result);

	cell_t retValue = Pl_Continue;
	g_OnBotAttack->Execute(&retValue);

	if (retValue == Pl_Changed)
		return !!result;
	
	return (team1 != team2);
}
