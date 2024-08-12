//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "hl2mp_player.h"
#include "globalstate.h"
#include "game.h"
#include "gamerules.h"
#include "hl2mp_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "hl2mp_gamerules.h"
#include "KeyValues.h"
#include "team.h"
#include "weapon_hl2mpbase.h"
#include "grenade_satchel.h"
#include "eventqueue.h"
#include "gamestats.h"
#ifdef SDK2013CE
#include "tier0/vprof.h"
#include "bone_setup.h"
#endif // SDK2013CE
#ifdef SecobMod__SAVERESTORE
#include "filesystem.h"
#include "ammodef.h"
#endif //SecobMod__SAVERESTORE

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "ilagcompensationmanager.h"

#ifdef SDK2013CE
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif // SDK2013CE

#ifdef SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
Vector respawn_origin;
Vector pEntityOrigin;
ConVar sv_SecobMod__increment_killed("sv_SecobMod__increment_killed", "0", 0, "The level of Increment Killed as a convar.");
int PlayerDucking;
#endif //SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
int g_iLastCitizenModel = 0;
int g_iLastCombineModel = 0;

CBaseEntity* g_pLastCombineSpawn = NULL;
CBaseEntity* g_pLastRebelSpawn = NULL;
extern CBaseEntity* g_pLastSpawn;

#define HL2MP_COMMAND_MAX_RATE 0.3

#ifdef SDK2013CE
#define CYCLELATCH_UPDATE_INTERVAL	0.2f
#endif // SDK2013CE

void DropPrimedFragGrenade(CHL2MP_Player* pPlayer, CBaseCombatWeapon* pGrenade);

LINK_ENTITY_TO_CLASS(player, CHL2MP_Player);

LINK_ENTITY_TO_CLASS(info_player_combine, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_rebel, CPointEntity);

#ifdef SDK2013CE
extern void SendProxy_Origin(const SendProp* pProp, const void* pStruct, const void* pData, DVariant* pOut, int iElement, int objectID);

//Tony; this should ideally be added to dt_send.cpp
void* SendProxy_SendNonLocalDataTable(const SendProp* pProp, const void* pStruct, const void* pVarData, CSendProxyRecipients* pRecipients, int objectID)
{
	pRecipients->SetAllRecipients();
	pRecipients->ClearRecipient(objectID - 1);
	return (void*)pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER(SendProxy_SendNonLocalDataTable);


BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPLocalPlayerExclusive)
// send a hi-res origin to the local player for use in prediction
SendPropVector(SENDINFO(m_vecOrigin), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin),
SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
//	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),
	
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPNonLocalPlayerExclusive)
// send a lo-res origin to other players
SendPropVector(SENDINFO(m_vecOrigin), -1, SPROP_COORD_MP_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin),
SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN),
// Only need to latch cycle for other players
// If you increase the number of bits networked, make sure to also modify the code below and in the client class.
SendPropInt(SENDINFO(m_cycleLatch), 4, SPROP_UNSIGNED),
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
SendPropExclude("DT_BaseAnimating", "m_flPoseParameter"),
SendPropExclude("DT_BaseAnimating", "m_flPlaybackRate"),
SendPropExclude("DT_BaseAnimating", "m_nSequence"),
SendPropExclude("DT_BaseEntity", "m_angRotation"),
SendPropExclude("DT_BaseAnimatingOverlay", "overlay_vars"),

SendPropExclude("DT_BaseEntity", "m_vecOrigin"),

// playeranimstate and clientside animation takes care of these on the client
SendPropExclude("DT_ServerAnimationData", "m_flCycle"),
SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),

SendPropExclude("DT_BaseFlex", "m_flexWeight"),
SendPropExclude("DT_BaseFlex", "m_blinktoggle"),
SendPropExclude("DT_BaseFlex", "m_viewtarget"),

// Data that only gets sent to the local player.
SendPropDataTable("hl2mplocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPLocalPlayerExclusive), SendProxy_SendLocalDataTable),
// Data that gets sent to all other players
SendPropDataTable("hl2mpnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable),

SendPropEHandle(SENDINFO(m_hRagdoll)),
SendPropInt(SENDINFO(m_iSpawnInterpCounter), 4),
SendPropInt(SENDINFO(m_iPlayerSoundType), 3),

END_SEND_TABLE()
#else
IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN),
SendPropEHandle(SENDINFO(m_hRagdoll)),
SendPropInt(SENDINFO(m_iSpawnInterpCounter), 4),
SendPropInt(SENDINFO(m_iPlayerSoundType), 3),

SendPropExclude("DT_BaseAnimating", "m_flPoseParameter"),
SendPropExclude("DT_BaseFlex", "m_viewtarget"),

//	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
//	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

END_SEND_TABLE()
#endif // SDK2013CE

BEGIN_DATADESC(CHL2MP_Player)
END_DATADESC()

const char* g_ppszRandomCitizenModels[] =
{
	"models/humans/group03/male_01.mdl",
	"models/humans/group03/male_02.mdl",
	"models/humans/group03/female_01.mdl",
	"models/humans/group03/male_03.mdl",
	"models/humans/group03/female_02.mdl",
	"models/humans/group03/male_04.mdl",
	"models/humans/group03/female_03.mdl",
	"models/humans/group03/male_05.mdl",
	"models/humans/group03/female_04.mdl",
	"models/humans/group03/male_06.mdl",
	"models/humans/group03/female_06.mdl",
	"models/humans/group03/male_07.mdl",
	"models/humans/group03/female_07.mdl",
	"models/humans/group03/male_08.mdl",
	"models/humans/group03/male_09.mdl",
};

const char* g_ppszRandomCombineModels[] =
{
	"models/combine_soldier.mdl",
	"models/combine_soldier_prisonguard.mdl",
	"models/combine_super_soldier.mdl",
	"models/police.mdl",
};


#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define HL2MPPLAYER_PHYSDAMAGE_SCALE 4.0f

#pragma warning( disable : 4355 )

#ifdef SDK2013CE
CHL2MP_Player::CHL2MP_Player()
{
	//Tony; create our player animation state.
	m_PlayerAnimState = CreateHL2MPPlayerAnimState(this);
	UseClientSideAnimation();

	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

	m_bEnterObserver = false;
	m_bReady = false;

	m_cycleLatch = 0;
	m_cycleLatchTimer.Invalidate();

	BaseClass::ChangeTeam(0);

	//	UseClientSideAnimation();
}

CHL2MP_Player::~CHL2MP_Player(void)
{
	m_PlayerAnimState->Release();
}
#else
CHL2MP_Player::CHL2MP_Player() : m_PlayerAnimState(this)
{
	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

	m_bEnterObserver = false;
	m_bReady = false;

	BaseClass::ChangeTeam(0);

	//	UseClientSideAnimation();
}

CHL2MP_Player::~CHL2MP_Player(void)
{

}
#endif // SDK2013CE

void CHL2MP_Player::UpdateOnRemove(void)
{
	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	BaseClass::UpdateOnRemove();
}

void CHL2MP_Player::Precache(void)
{
	BaseClass::Precache();

	PrecacheModel("sprites/glow01.vmt");

	//Precache Citizen models
	int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCitizenModels[i]);

	//Precache Combine Models
	nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCombineModels[i]);

	PrecacheFootStepSounds();

	PrecacheScriptSound("NPC_MetroPolice.Die");
	PrecacheScriptSound("NPC_CombineS.Die");
	PrecacheScriptSound("NPC_Citizen.die");
}

void CHL2MP_Player::GiveAllItems(void)
{
	EquipSuit();

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(255, "AR2");
	CBasePlayer::GiveAmmo(5, "AR2AltFire");
	CBasePlayer::GiveAmmo(255, "SMG1");
	CBasePlayer::GiveAmmo(1, "smg1_grenade");
	CBasePlayer::GiveAmmo(255, "Buckshot");
	CBasePlayer::GiveAmmo(32, "357");
	CBasePlayer::GiveAmmo(3, "rpg_round");

	CBasePlayer::GiveAmmo(1, "grenade");
	CBasePlayer::GiveAmmo(2, "slam");

	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_stunstick");
	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_357");

	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_ar2");

	GiveNamedItem("weapon_shotgun");
	GiveNamedItem("weapon_frag");

	GiveNamedItem("weapon_crossbow");

	GiveNamedItem("weapon_rpg");

	GiveNamedItem("weapon_slam");

	GiveNamedItem("weapon_physcannon");

}

void CHL2MP_Player::GiveDefaultItems(void)
{
	EquipSuit();

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(45, "SMG1");
	CBasePlayer::GiveAmmo(1, "grenade");
	CBasePlayer::GiveAmmo(6, "Buckshot");
	CBasePlayer::GiveAmmo(6, "357");

	if (GetPlayerModelType() == PLAYER_SOUNDS_METROPOLICE || GetPlayerModelType() == PLAYER_SOUNDS_COMBINESOLDIER)
	{
		GiveNamedItem("weapon_stunstick");
	}
	else if (GetPlayerModelType() == PLAYER_SOUNDS_CITIZEN)
	{
		GiveNamedItem("weapon_crowbar");
	}

	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_frag");
	GiveNamedItem("weapon_physcannon");

	//SecobMod__Information: Still provide armour for a non-playerclass player.
	SetArmorValue(100);
	SetMaxArmorValue(200);

	const char* szDefaultWeaponName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_defaultweapon");

	CBaseCombatWeapon* pDefaultWeapon = Weapon_OwnsThisType(szDefaultWeaponName);

	if (pDefaultWeapon)
	{
		Weapon_Switch(pDefaultWeapon);
	}
	else
	{
		Weapon_Switch(Weapon_OwnsThisType("weapon_physcannon"));
	}
}

void CHL2MP_Player::PickDefaultSpawnTeam(void)
{
	if (GetTeamNumber() == 0)
	{
		if (HL2MPRules()->IsTeamplay() == false)
		{
			if (GetModelPtr() == NULL)
			{
				const char* szModelName = NULL;
				szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

				if (ValidatePlayerModel(szModelName) == false)
				{
					char szReturnString[512];

					Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel models/combine_soldier.mdl\n");
					engine->ClientCommand(edict(), szReturnString);
				}

				ChangeTeam(TEAM_UNASSIGNED);
			}
		}
		else
		{
			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			if (pCombine == NULL || pRebels == NULL)
			{
				ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
			}
			else
			{
				if (pCombine->GetNumPlayers() > pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_REBELS);
				}
				else if (pCombine->GetNumPlayers() < pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_COMBINE);
				}
				else
				{
					ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
				}
			}
		}
	}
}

#ifdef SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
//------------------------------------------------------------------------------
// A small wrapper around SV_Move that never clips against the supplied entity.
//------------------------------------------------------------------------------
static bool TestEntityPosition(CBasePlayer* pPlayer)
{
	trace_t	trace;
	UTIL_TraceEntity(pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), MASK_PLAYERSOLID, &trace);
	return (trace.startsolid == 0);
}

static int FindPassableSpace(CBasePlayer* pPlayer, const Vector& direction, float step, Vector& oldorigin)
{
	int i;
	for (i = 0; i < 100; i++)
	{
		Vector origin = pPlayer->GetAbsOrigin();
		VectorMA(origin, step, direction, origin);
		pPlayer->SetAbsOrigin(origin);
		if (TestEntityPosition(pPlayer))
		{
			VectorCopy(pPlayer->GetAbsOrigin(), oldorigin);
			return 1;
		}
	}
	return 0;
}
#endif //SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE

//-----------------------------------------------------------------------------
// Purpose: Sets HL2 specific defaults.
//-----------------------------------------------------------------------------
void CHL2MP_Player::Spawn(void)
{
#ifdef SecobMod__MULTIPLAYER_LEVEL_TRANSITIONS
	if (m_bTransition)
	{
		if (m_bTransitionTeleported)
			g_pGameRules->GetPlayerSpawnSpot(this);

		m_bTransition = false;
		m_bTransitionTeleported = false;

		return;
	}
#endif //SecobMod__MULTIPLAYER_LEVEL_TRANSITIONS

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	//PickDefaultSpawnTeam();

	BaseClass::Spawn();

	if (!IsObserver())
	{
#ifdef SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
		// Disengage from hierarchy
		SetParent(NULL);
		SetMoveType(MOVETYPE_NOCLIP);
		AddEFlags(EFL_NOCLIP_ACTIVE);

		//SecobMod__Information: Just in case our dynamic respawn code is going to spawn us in world geometery, or even a player or NPC, run these checks so if we are stuck we get moved to a good location to spawn.
		CPlayerState* pl2 = PlayerData();
		Assert(pl2);

		RemoveEFlags(EFL_NOCLIP_ACTIVE);
		SetMoveType(MOVETYPE_WALK);

		Vector oldorigin = GetAbsOrigin();
		if (!TestEntityPosition(this))
		{
			Vector forward, right, up;

			AngleVectors(pl2->v_angle, &forward, &right, &up);

			// Try to move into the world
			if (!FindPassableSpace(this, forward, 1, oldorigin))
			{
				if (!FindPassableSpace(this, right, 1, oldorigin))
				{
					if (!FindPassableSpace(this, right, -1, oldorigin))		// left
					{
						if (!FindPassableSpace(this, up, 1, oldorigin))	// up
						{
							if (!FindPassableSpace(this, up, -1, oldorigin))	// down
							{
								if (!FindPassableSpace(this, forward, -1, oldorigin))	// back
								{
									//Coudln't find the world, so we kill the player. In  some cases this is because of collision bugs when a player should be crouched but isnt.
									// we therefore set the player to be ducked before killing them, so that our PlayerDucking check can take care of fixing their spawn point automatically.
									// and we hope that the players next attempt at spawning is more successful than their last attempt.
									AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.
									SetCollisionBounds(VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX);
									AddFlag(FL_DUCKING);
									m_Local.m_bDucked = true;
									m_Local.m_bDucking = false;
									CommitSuicide();
								}
							}
						}
					}
				}
			}
			SetAbsOrigin(oldorigin);
		}

		//SecobMod__Information: Very rarely a player may still spawn in another player, this rarely happens, so as a fix we kill the player we're about to spawn on. We do at least warn them that they should move first.
		int MovedYet = 0;

	LoopSpot:
		CBaseEntity* ent = NULL;
		for (CEntitySphereQuery sphere(GetAbsOrigin(), 0.1); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
		{
			if (ent->IsPlayer() && ent != (this))
			{
				AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

				if (MovedYet == 0)
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "#BLOCKING_SPAWN");
				}
				MovedYet++;

				if (MovedYet >= 6)
				{
					//SecobMod__Information: We gave them a chance to move, they didn't take it so now we spawn the player and kill the other one.
					ent->TakeDamage(CTakeDamageInfo(GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 3000, DMG_GENERIC));
				}
				else
				{
					goto LoopSpot; //SecobMod__Information: We loop through the code until either there's no one in our way, or 6 (or more) calls have been made, in which case we kill the offending player.
				}
			}
			MovedYet = 0;
		}
#endif //SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE	

		pl.deadflag = false;
		RemoveSolidFlags(FSOLID_NOT_SOLID);

		RemoveEffects(EF_NODRAW);

		if (gpGlobals->maxClients > 1 && gpGlobals->deathmatch)
			GiveDefaultItems();
	}

#ifndef SDK2013CE
	SetNumAnimOverlays(3);
	ResetAnimation();
#endif // !SDK2013CE

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = HL2MPPLAYER_PHYSDAMAGE_SCALE;

	if (HL2MPRules()->IsIntermission())
	{
		AddFlag(FL_FROZEN);
	}
	else
	{
		RemoveFlag(FL_FROZEN);
	}

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	m_bReady = false;

#ifdef SDK2013CE
	m_cycleLatchTimer.Start(CYCLELATCH_UPDATE_INTERVAL);

	//Tony; do the spawn animevent
	DoAnimationEvent(PLAYERANIMEVENT_SPAWN);
#endif // SDK2013CE
}

void CHL2MP_Player::PickupObject(CBaseEntity* pObject, bool bLimitMassAndSize)
{
#ifdef SDK2013CE
	BaseClass::PickupObject(pObject, bLimitMassAndSize);
#endif
}

bool CHL2MP_Player::ValidatePlayerModel(const char* pModel)
{
	int iModels = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCitizenModels[i], pModel))
		{
			return true;
		}
	}

	iModels = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCombineModels[i], pModel))
		{
			return true;
		}
	}

	return false;
}

void CHL2MP_Player::SetPlayerTeamModel(void)
{
	const char* szModelName = NULL;
	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1 || ValidatePlayerModel(szModelName) == false)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		if (Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

			g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
			szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];
		}

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		if (!Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

			g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
			szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];
		}

		m_iModelType = TEAM_REBELS;
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetPlayerModel(void)
{
	const char* szModelName = NULL;
	const char* pszCurrentModelName = modelinfo->GetModelName(GetModel());

	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	if (ValidatePlayerModel(szModelName) == false)
	{
		char szReturnString[512];

		if (ValidatePlayerModel(pszCurrentModelName) == false)
		{
			pszCurrentModelName = "models/Combine_Soldier.mdl";
		}

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pszCurrentModelName);
		engine->ClientCommand(edict(), szReturnString);

		szModelName = pszCurrentModelName;
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

		g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
		szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

		g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
		szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];

		m_iModelType = TEAM_REBELS;
	}
	else
	{
		if (Q_strlen(szModelName) == 0)
		{
			szModelName = g_ppszRandomCitizenModels[0];
		}

		if (Q_stristr(szModelName, "models/human"))
		{
			m_iModelType = TEAM_REBELS;
		}
		else
		{
			m_iModelType = TEAM_COMBINE;
		}
	}

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetupPlayerSoundsByModel(const char* pModelName)
{
	if (Q_stristr(pModelName, "models/human"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	}
	else if (Q_stristr(pModelName, "police"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_METROPOLICE;
	}
	else if (Q_stristr(pModelName, "combine"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_COMBINESOLDIER;
	}
}

#ifndef SDK2013CE
void CHL2MP_Player::ResetAnimation(void)
{
	if (IsAlive())
	{
		SetSequence(-1);
		SetActivity(ACT_INVALID);

		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation(PLAYER_IDLE);
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && (GetFlags() & FL_ONGROUND))
			SetAnimation(PLAYER_WALK);
		else if (GetWaterLevel() > 1)
			SetAnimation(PLAYER_WALK);
	}
}
#endif // !SDK2013CE

bool CHL2MP_Player::Weapon_Switch(CBaseCombatWeapon* pWeapon, int viewmodelindex)
{
	bool bRet = BaseClass::Weapon_Switch(pWeapon, viewmodelindex);

#ifndef SDK2013CE
	if (bRet == true)
	{
		ResetAnimation();
	}
#endif // !SDK2013CE

	return bRet;
}

void CHL2MP_Player::PreThink(void)
{
#ifndef SDK2013CE
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if (vTempAngles[PITCH] > 180.0f)
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles(vTempAngles);
#endif // !SDK2013CE

	BaseClass::PreThink();
	State_PreThink();

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;
#ifndef SDK2013CE
	SetLocalAngles(vOldAngles);
#endif // !SDK2013CE
}

void CHL2MP_Player::PostThink(void)
{
	BaseClass::PostThink();

	if (GetFlags() & FL_DUCKING)
	{
		SetCollisionBounds(VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX);
	}

#ifndef SDK2013CE
	m_PlayerAnimState.Update();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();
#endif // !SDK2013CE

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles(angles);

#ifdef SDK2013CE
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();
	m_PlayerAnimState->Update(m_angEyeAngles[YAW], m_angEyeAngles[PITCH]);

	if (IsAlive() && m_cycleLatchTimer.IsElapsed())
	{
		m_cycleLatchTimer.Start(CYCLELATCH_UPDATE_INTERVAL);
		// Compress the cycle into 4 bits. Can represent 0.0625 in steps which is enough.
		m_cycleLatch.GetForModify() = 16 * GetCycle();
	}
#endif // SDK2013CE
}

void CHL2MP_Player::PlayerDeathThink()
{
	if (!IsObserver())
	{
		BaseClass::PlayerDeathThink();
	}
}

void CHL2MP_Player::FireBullets(const FireBulletsInfo_t& info)
{
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation(this, this->GetCurrentCommand());

	FireBulletsInfo_t modinfo = info;

	CWeaponHL2MPBase* pWeapon = dynamic_cast<CWeaponHL2MPBase*>(GetActiveWeapon());

	if (pWeapon)
	{
		modinfo.m_iPlayerDamage = modinfo.m_flDamage = pWeapon->GetHL2MPWpnData().m_iPlayerDamage;
	}

	NoteWeaponFired();

	BaseClass::FireBullets(modinfo);

	// Move other players back to history positions based on local player's lag
	lagcompensation->FinishLagCompensation(this);
}

void CHL2MP_Player::NoteWeaponFired(void)
{
	Assert(m_pCurrentCommand);
	if (m_pCurrentCommand)
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;
#ifdef SM_AI_FIXES
bool CHL2MP_Player::WantsLagCompensationOnEntity(const CBaseEntity* pEntity, const CUserCmd* pCmd, const CBitVec<MAX_EDICTS>* pEntityTransmitBits) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if (!(pCmd->buttons & IN_ATTACK) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5))
		return false;
	return BaseClass::WantsLagCompensationOnEntity(pEntity, pCmd, pEntityTransmitBits);
}
#else
bool CHL2MP_Player::WantsLagCompensationOnEntity(const CBasePlayer* pPlayer, const CUserCmd* pCmd, const CBitVec<MAX_EDICTS>* pEntityTransmitBits) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if (!(pCmd->buttons & IN_ATTACK) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5))
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if (pEntityTransmitBits && !pEntityTransmitBits->Get(pPlayer->entindex()))
		return false;

	const Vector& vMyOrigin = GetAbsOrigin();
	const Vector& vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if (vHisOrigin.DistTo(vMyOrigin) < maxDistance)
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors(pCmd->viewangles, &vForward);

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize(vDiff);

	float flCosAngle = 0.707107f;	// 45 degree angle
	if (vForward.Dot(vDiff) < flCosAngle)
		return false;

	return true;
}
#endif

Activity CHL2MP_Player::TranslateTeamActivity(Activity ActToTranslate)
{
	if (m_iModelType == TEAM_COMBINE)
		return ActToTranslate;

	if (ActToTranslate == ACT_RUN)
		return ACT_RUN_AIM_AGITATED;

	if (ActToTranslate == ACT_IDLE)
		return ACT_IDLE_AIM_AGITATED;

	if (ActToTranslate == ACT_WALK)
		return ACT_WALK_AIM_AGITATED;

	return ActToTranslate;
}

extern ConVar hl2_normspeed;

#ifndef SDK2013CE
// Set the activity based on an event or current state
void CHL2MP_Player::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;

	float speed;

	speed = GetAbsVelocity().Length2D();


	// bool bRunning = true;

	//Revisit!
/*	if ( ( m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) )
	{
		if ( speed > 1.0f && speed < hl2_normspeed.GetFloat() - 20.0f )
		{
			bRunning = false;
		}
	}*/

	if (GetFlags() & (FL_FROZEN | FL_ATCONTROLS))
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	Activity idealActivity = ACT_HL2MP_RUN;

	// This could stand to be redone. Why is playerAnim abstracted from activity? (sjb)
	if (playerAnim == PLAYER_JUMP)
	{
		idealActivity = ACT_HL2MP_JUMP;
	}
	else if (playerAnim == PLAYER_DIE)
	{
		if (m_lifeState == LIFE_ALIVE)
		{
			return;
		}
	}
	else if (playerAnim == PLAYER_ATTACK1)
	{
		if (GetActivity() == ACT_HOVER ||
			GetActivity() == ACT_SWIM ||
			GetActivity() == ACT_HOP ||
			GetActivity() == ACT_LEAP ||
			GetActivity() == ACT_DIESIMPLE)
		{
			idealActivity = GetActivity();
		}
		else
		{
			idealActivity = ACT_HL2MP_GESTURE_RANGE_ATTACK;
		}
	}
	else if (playerAnim == PLAYER_RELOAD)
	{
		idealActivity = ACT_HL2MP_GESTURE_RELOAD;
	}
	else if (playerAnim == PLAYER_IDLE || playerAnim == PLAYER_WALK)
	{
		if (!(GetFlags() & FL_ONGROUND) && GetActivity() == ACT_HL2MP_JUMP)	// Still jumping
		{
			idealActivity = GetActivity();
		}
		/*
		else if ( GetWaterLevel() > 1 )
		{
			if ( speed == 0 )
				idealActivity = ACT_HOVER;
			else
				idealActivity = ACT_SWIM;
		}
		*/
		else
		{
			if (GetFlags() & FL_DUCKING)
			{
				if (speed > 0)
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if (speed > 0)
				{
					/*
					if ( bRunning == false )
					{
						idealActivity = ACT_WALK;
					}
					else
					*/
					{
						idealActivity = ACT_HL2MP_RUN;
					}
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
		}

		idealActivity = TranslateTeamActivity(idealActivity);
	}

	if (idealActivity == ACT_HL2MP_GESTURE_RANGE_ATTACK)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));

		// FIXME: this seems a bit wacked
		Weapon_SetActivity(Weapon_TranslateActivity(ACT_RANGE_ATTACK1), 0);

		return;
	}
	else if (idealActivity == ACT_HL2MP_GESTURE_RELOAD)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));
		return;
	}
	else
	{
		SetActivity(idealActivity);

		animDesired = SelectWeightedSequence(Weapon_TranslateActivity(idealActivity));

		if (animDesired == -1)
		{
			animDesired = SelectWeightedSequence(idealActivity);

			if (animDesired == -1)
			{
				animDesired = 0;
			}
		}

		// Already using the desired animation?
		if (GetSequence() == animDesired)
			return;

		m_flPlaybackRate = 1.0;
		ResetSequence(animDesired);
		SetCycle(0);
		return;
	}

	// Already using the desired animation?
	if (GetSequence() == animDesired)
		return;

	//Msg( "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	ResetSequence(animDesired);
	SetCycle(0);
}
#endif // !SDK2013CE

extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CHL2MP_Player::BumpWeapon(CBaseCombatWeapon* pWeapon)
{
	CBaseCombatCharacter* pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if (!IsAllowedToPickupWeapons())
		return false;

	if (pOwner || !Weapon_CanUse(pWeapon) || !g_pGameRules->CanHavePlayerItem(this, pWeapon))
	{
		if (gEvilImpulse101)
		{
			UTIL_Remove(pWeapon);
		}
		return false;
	}

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if (!pWeapon->FVisible(this, MASK_SOLID) && !(GetFlags() & FL_NOTARGET))
	{
		return false;
	}

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType());

	if (bOwnsWeaponAlready == true)
	{
		//If we have room for the ammo, then "take" the weapon too.
		if (Weapon_EquipAmmoOnly(pWeapon))
		{
			pWeapon->CheckRespawn();

			UTIL_Remove(pWeapon);
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();
#ifdef SDK2013CE
	pWeapon->AddSolidFlags(FSOLID_NOT_SOLID);
	pWeapon->AddEffects(EF_NODRAW);
#endif
	Weapon_Equip(pWeapon);

	return true;
}

void CHL2MP_Player::ChangeTeam(int iTeam)
{
	/*	if ( GetNextTeamChangeTime() >= gpGlobals->curtime )
		{
			char szReturnString[128];
			Q_snprintf( szReturnString, sizeof( szReturnString ), "Please wait %d more seconds before trying to switch teams again.\n", (int)(GetNextTeamChangeTime() - gpGlobals->curtime) );

			ClientPrint( this, HUD_PRINTTALK, szReturnString );
			return;
		}*/

	bool bKill = false;

	if (HL2MPRules()->IsTeamplay() != true && iTeam != TEAM_SPECTATOR)
	{
		//don't let them try to join combine or rebels during deathmatch.
		iTeam = TEAM_UNASSIGNED;
	}

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (iTeam != GetTeamNumber() && GetTeamNumber() != TEAM_UNASSIGNED)
		{
			bKill = true;
		}
	}

	BaseClass::ChangeTeam(iTeam);

	m_flNextTeamChangeTime = gpGlobals->curtime + TEAM_CHANGE_INTERVAL;

	if (HL2MPRules()->IsTeamplay() == true)
	{
		SetPlayerTeamModel();
	}
	else
	{
		SetPlayerModel();
	}

	if (iTeam == TEAM_SPECTATOR)
	{
		RemoveAllItems(true);

		State_Transition(STATE_OBSERVER_MODE);
	}

	if (bKill == true)
	{
		CommitSuicide();
	}
}

bool CHL2MP_Player::HandleCommand_JoinTeam(int team)
{
	if (!GetGlobalTeam(team) || team == 0)
	{
		Warning("HandleCommand_JoinTeam( %d ) - invalid team index.\n", team);
		return false;
	}

	if (team == TEAM_SPECTATOR)
	{
		// Prevent this is the cvar is set
		if (!mp_allowspectators.GetInt())
		{
			ClientPrint(this, HUD_PRINTCENTER, "#Cannot_Be_Spectator");
			return false;
		}

		if (GetTeamNumber() != TEAM_UNASSIGNED && !IsDead())
		{
			m_fNextSuicideTime = gpGlobals->curtime;	// allow the suicide to work

			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			IncrementFragCount(1);
		}

		ChangeTeam(TEAM_SPECTATOR);

		return true;
	}
	else
	{
		StopObserverMode();
		State_Transition(STATE_ACTIVE);
	}

	// Switch their actual team...
	ChangeTeam(team);

	return true;
}

bool CHL2MP_Player::ClientCommand(const CCommand& args)
{
	if (FStrEq(args[0], "spectate"))
	{
		if (ShouldRunRateLimitedCommand(args))
		{
			// instantly join spectators
			HandleCommand_JoinTeam(TEAM_SPECTATOR);
		}
		return true;
	}
	else if (FStrEq(args[0], "jointeam"))
	{
		if (args.ArgC() < 2)
		{
			Warning("Player sent bad jointeam syntax\n");
		}

		if (ShouldRunRateLimitedCommand(args))
		{
			int iTeam = atoi(args[1]);
			HandleCommand_JoinTeam(iTeam);
		}
		return true;
	}
	else if (FStrEq(args[0], "joingame"))
	{
		return true;
	}

	return BaseClass::ClientCommand(args);
}

void CHL2MP_Player::CheatImpulseCommands(int iImpulse)
{
	switch (iImpulse)
	{
	case 101:
	{
		if (sv_cheats->GetBool())
		{
			GiveAllItems();
		}
	}
	break;

	default:
		BaseClass::CheatImpulseCommands(iImpulse);
	}
}

bool CHL2MP_Player::ShouldRunRateLimitedCommand(const CCommand& args)
{
	int i = m_RateLimitLastCommandTimes.Find(args[0]);
	if (i == m_RateLimitLastCommandTimes.InvalidIndex())
	{
		m_RateLimitLastCommandTimes.Insert(args[0], gpGlobals->curtime);
		return true;
	}
	else if ((gpGlobals->curtime - m_RateLimitLastCommandTimes[i]) < HL2MP_COMMAND_MAX_RATE)
	{
		// Too fast.
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

void CHL2MP_Player::CreateViewModel(int index /*=0*/)
{
	Assert(index >= 0 && index < MAX_VIEWMODELS);

	if (GetViewModel(index))
		return;

	CPredictedViewModel* vm = (CPredictedViewModel*)CreateEntityByName("predicted_viewmodel");
	if (vm)
	{
		vm->SetAbsOrigin(GetAbsOrigin());
		vm->SetOwner(this);
		vm->SetIndex(index);
		DispatchSpawn(vm);
		vm->FollowEntity(this, false);
		m_hViewModel.Set(index, vm);
	}
}

bool CHL2MP_Player::BecomeRagdollOnClient(const Vector& force)
{
	return true;
}

// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CHL2MPRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS(CHL2MPRagdoll, CBaseAnimatingOverlay);
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle(CBaseEntity, m_hPlayer);	// networked entity handle 
	CNetworkVector(m_vecRagdollVelocity);
	CNetworkVector(m_vecRagdollOrigin);
};

LINK_ENTITY_TO_CLASS(hl2mp_ragdoll, CHL2MPRagdoll);

IMPLEMENT_SERVERCLASS_ST_NOBASE(CHL2MPRagdoll, DT_HL2MPRagdoll)
SendPropVector(SENDINFO(m_vecRagdollOrigin), -1, SPROP_COORD),
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropModelIndex(SENDINFO(m_nModelIndex)),
SendPropInt(SENDINFO(m_nForceBone), 8, 0),
SendPropVector(SENDINFO(m_vecForce), -1, SPROP_NOSCALE),
SendPropVector(SENDINFO(m_vecRagdollVelocity))
END_SEND_TABLE()


void CHL2MP_Player::CreateRagdollEntity(void)
{
	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	// If we already have a ragdoll, don't make another one.
	CHL2MPRagdoll* pRagdoll = dynamic_cast<CHL2MPRagdoll*>(m_hRagdoll.Get());

	if (!pRagdoll)
	{
		// create a new one
		pRagdoll = dynamic_cast<CHL2MPRagdoll*>(CreateEntityByName("hl2mp_ragdoll"));
	}

	if (pRagdoll)
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->SetAbsOrigin(GetAbsOrigin());
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2MP_Player::FlashlightIsOn(void)
{
	return IsEffectActive(EF_DIMLIGHT);
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOn(void)
{
	if (flashlight.GetInt() > 0 && IsAlive())
	{
		AddEffects(EF_DIMLIGHT);
		EmitSound("HL2Player.FlashlightOn");
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOff(void)
{
	RemoveEffects(EF_DIMLIGHT);

	if (IsAlive())
	{
		EmitSound("HL2Player.FlashlightOff");
	}
}

void CHL2MP_Player::Weapon_Drop(CBaseCombatWeapon* pWeapon, const Vector* pvecTarget, const Vector* pVelocity)
{
	//Drop a grenade if it's primed.
	if (GetActiveWeapon())
	{
		CBaseCombatWeapon* pGrenade = Weapon_OwnsThisType("weapon_frag");

		if (GetActiveWeapon() == pGrenade)
		{
			if ((m_nButtons & IN_ATTACK) || (m_nButtons & IN_ATTACK2))
			{
				DropPrimedFragGrenade(this, pGrenade);
				return;
			}
		}
	}

	BaseClass::Weapon_Drop(pWeapon, pvecTarget, pVelocity);
}


void CHL2MP_Player::DetonateTripmines(void)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() == this)
		{
			g_EventQueue.AddEvent(pSatchel, "Explode", 0.20, this, this);
		}
	}

	// Play sound for pressing the detonator
	EmitSound("Weapon_SLAM.SatchelDetonate");
}

void CHL2MP_Player::Event_Killed(const CTakeDamageInfo& info)
{
	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce(m_vecTotalBulletForce);

#ifndef SDK2013CE
	SetNumAnimOverlays(0);
#endif // !SDK2013CE

#ifdef SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
	sv_SecobMod__increment_killed.SetValue(sv_SecobMod__increment_killed.GetInt() + 1);
#endif //SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity();

#ifdef SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE
	//SecobMod__Information: When a player is killed and if there's a ragdoll (there always is, even if it gets removed instantly) then we either get the position of our next (other) nearest player (because GetNearestPlayer would return ourselves) and set it to be the vector labelled respawn_origin or we just use the position of our ragdolls first spawn if no players are alive.
	CBasePlayer* pPlayer = UTIL_GetOtherNearestPlayer(GetAbsOrigin());
	if (m_hRagdoll)
	{
		if (pPlayer == NULL || pPlayer == (this))
		{
			respawn_origin = m_hRagdoll->GetAbsOrigin();
		}
		else
		{
			respawn_origin = pPlayer->GetAbsOrigin();
		}
	}

	if (GetFlags() & FL_DUCKING)
	{
		//We we're killed while ducking, this could be in a vent or enclosed area, so set an int so the dynamic respawn code can set you ducked on spawn.
		PlayerDucking = 1;
	}
#endif //SecobMod__ENABLE_DYNAMIC_PLAYER_RESPAWN_CODE

	DetonateTripmines();

	BaseClass::Event_Killed(subinfo);

	if (info.GetDamageType() & DMG_DISSOLVE)
	{
		if (m_hRagdoll)
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
		}
	}

#ifdef SecobMod__BARNACLES_CAN_SWALLOW_PLAYERS
	if ((info.GetDamageType() & (DMG_ALWAYSGIB | DMG_LASTGENERICFLAG | DMG_CRUSH)) == (DMG_ALWAYSGIB | DMG_LASTGENERICFLAG | DMG_CRUSH))
	{
		if (m_hRagdoll)
		{
			UTIL_RemoveImmediate(m_hRagdoll);
		}
	}
#endif

	CBaseEntity* pAttacker = info.GetAttacker();

	if (pAttacker)
	{
		int iScoreToAdd = 1;

		if (pAttacker == this)
		{
			iScoreToAdd = -1;
		}

		GetGlobalTeam(pAttacker->GetTeamNumber())->AddScore(iScoreToAdd);
	}

	FlashlightTurnOff();

	m_lifeState = LIFE_DEAD;

	RemoveEffects(EF_NODRAW);	// still draw player body
	StopZooming();
}

int CHL2MP_Player::OnTakeDamage(const CTakeDamageInfo& inputInfo)
{
	//return here if the player is in the respawn grace period vs. slams.
	if (gpGlobals->curtime < m_flSlamProtectTime && (inputInfo.GetDamageType() == DMG_BLAST))
		return 0;

	m_vecTotalBulletForce += inputInfo.GetDamageForce();

	gamestats->Event_PlayerDamage(this, inputInfo);

	return BaseClass::OnTakeDamage(inputInfo);
}

void CHL2MP_Player::DeathSound(const CTakeDamageInfo& info)
{
	if (m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving())
		return;

	char szStepSound[128];

	Q_snprintf(szStepSound, sizeof(szStepSound), "%s.Die", GetPlayerModelSoundPrefix());

	const char* pModelName = STRING(GetModelName());

	CSoundParameters params;
	if (GetParametersForSound(szStepSound, params, pModelName) == false)
		return;

	Vector vecOrigin = GetAbsOrigin();

	CRecipientFilter filter;
	filter.AddRecipientsByPAS(vecOrigin);

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound(filter, entindex(), ep);
}


CBaseEntity* CHL2MP_Player::EntSelectSpawnPoint(void)
{
	return BaseClass::EntSelectSpawnPoint();
#if 0
	CBaseEntity* pSpot = NULL;
	CBaseEntity* pLastSpawnPoint = g_pLastSpawn;
	edict_t* player = edict();
	const char* pSpawnpointName = "info_player_deathmatch";

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			pSpawnpointName = "info_player_combine";
			pLastSpawnPoint = g_pLastCombineSpawn;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			pSpawnpointName = "info_player_rebel";
			pLastSpawnPoint = g_pLastRebelSpawn;
		}

		if (gEntList.FindEntityByClassname(NULL, pSpawnpointName) == NULL)
		{
			pSpawnpointName = "info_player_deathmatch";
			pLastSpawnPoint = g_pLastSpawn;
		}
	}

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for (int i = random->RandomInt(1, 5); i > 0; i--)
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	if (!pSpot)  // skip over the null point
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);

	CBaseEntity* pFirstSpot = pSpot;

	do
	{
		if (pSpot)
		{
			// check if pSpot is valid
			if (g_pGameRules->IsSpawnPointValid(pSpot, this))
			{
				if (pSpot->GetLocalOrigin() == vec3_origin)
				{
					pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
					continue;
				}

				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	} while (pSpot != pFirstSpot); // loop if we're not back to the start

	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	if (pSpot)
	{
		CBaseEntity* ent = NULL;
		for (CEntitySphereQuery sphere(pSpot->GetAbsOrigin(), 128); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
		{
			// if ent is a client, kill em (unless they are ourselves)
			if (ent->IsPlayer() && !(ent->edict() == player))
				ent->TakeDamage(CTakeDamageInfo(GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 300, DMG_GENERIC));
		}
		goto ReturnSpot;
	}

#ifdef SDK2013CE
	// If startspot is set, (re)spawn there.
	if (!gpGlobals->startspot || !strlen(STRING(gpGlobals->startspot)))
	{
#endif
		if (!pSpot)
		{
			pSpot = gEntList.FindEntityByClassname(pSpot, "info_player_start");

			if (pSpot)
				goto ReturnSpot;
		}
#ifdef SDK2013CE
	}
	else
	{
		pSpot = gEntList.FindEntityByName(NULL, gpGlobals->startspot);
		if (pSpot)
			goto ReturnSpot;
	}
#endif

ReturnSpot:

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			g_pLastCombineSpawn = pSpot;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			g_pLastRebelSpawn = pSpot;
		}
	}

	g_pLastSpawn = pSpot;

	m_flSlamProtectTime = gpGlobals->curtime + 0.5;

	return pSpot;
#endif
}


CON_COMMAND(timeleft, "prints the time remaining in the match")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());

	int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();

	if (iTimeRemaining == 0)
	{
		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "This game has no timelimit.");
		}
		else
		{
			Msg("* No Time Limit *\n");
		}
	}
	else
	{
		int iMinutes, iSeconds;
		iMinutes = iTimeRemaining / 60;
		iSeconds = iTimeRemaining % 60;

		char minutes[8];
		char seconds[8];

		Q_snprintf(minutes, sizeof(minutes), "%d", iMinutes);
		Q_snprintf(seconds, sizeof(seconds), "%2.2d", iSeconds);

		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "Time left in map: %s1:%s2", minutes, seconds);
		}
		else
		{
			Msg("Time Remaining:  %s:%s\n", minutes, seconds);
		}
	}
}

void CHL2MP_Player::Reset()
{
	ResetDeathCount();
	ResetFragCount();
}

bool CHL2MP_Player::IsReady()
{
	return m_bReady;
}

void CHL2MP_Player::SetReady(bool bReady)
{
	m_bReady = bReady;
}

void CHL2MP_Player::CheckChatText(char* p, int bufsize)
{
	//Look for escape sequences and replace

	char* buf = new char[bufsize];
	int pos = 0;

	// Parse say text for escape sequences
	for (char* pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize - 1; pSrc++)
	{
		// copy each char across
		buf[pos] = *pSrc;
		pos++;
	}

	buf[pos] = '\0';

	// copy buf back into p
	Q_strncpy(p, buf, bufsize);

	delete[] buf;

	const char* pReadyCheck = p;

	HL2MPRules()->CheckChatForReadySignal(this, pReadyCheck);
}

void CHL2MP_Player::State_Transition(HL2MPPlayerState newState)
{
	State_Leave();
	State_Enter(newState);
}


void CHL2MP_Player::State_Enter(HL2MPPlayerState newState)
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo(newState);

	// Initialize the new state.
	if (m_pCurStateInfo && m_pCurStateInfo->pfnEnterState)
		(this->*m_pCurStateInfo->pfnEnterState)();
}


void CHL2MP_Player::State_Leave()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState)
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


void CHL2MP_Player::State_PreThink()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnPreThink)
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}


CHL2MPPlayerStateInfo* CHL2MP_Player::State_LookupInfo(HL2MPPlayerState state)
{
	// This table MUST match the 
	static CHL2MPPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CHL2MP_Player::State_Enter_ACTIVE, NULL, &CHL2MP_Player::State_PreThink_ACTIVE },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CHL2MP_Player::State_Enter_OBSERVER_MODE,	NULL, &CHL2MP_Player::State_PreThink_OBSERVER_MODE }
	};

	for (int i = 0; i < ARRAYSIZE(playerStateInfos); i++)
	{
		if (playerStateInfos[i].m_iPlayerState == state)
			return &playerStateInfos[i];
	}

	return NULL;
}

bool CHL2MP_Player::StartObserverMode(int mode)
{
	//we only want to go into observer mode if the player asked to, not on a death timeout
	if (m_bEnterObserver == true)
	{
		VPhysicsDestroyObject();
		return BaseClass::StartObserverMode(mode);
	}
	return false;
}

void CHL2MP_Player::StopObserverMode()
{
	m_bEnterObserver = false;
	BaseClass::StopObserverMode();
}

void CHL2MP_Player::State_Enter_OBSERVER_MODE()
{
	int observerMode = m_iObserverLastMode;
	if (IsNetClient())
	{
		const char* pIdealMode = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_spec_mode");
		if (pIdealMode)
		{
			observerMode = atoi(pIdealMode);
			if (observerMode <= OBS_MODE_FIXED || observerMode > OBS_MODE_ROAMING)
			{
				observerMode = m_iObserverLastMode;
			}
		}
	}
	m_bEnterObserver = true;
	StartObserverMode(observerMode);
}

void CHL2MP_Player::State_PreThink_OBSERVER_MODE()
{
	// Make sure nobody has changed any of our state.
	//	Assert( GetMoveType() == MOVETYPE_FLY );
	Assert(m_takedamage == DAMAGE_NO);
	Assert(IsSolidFlagSet(FSOLID_NOT_SOLID));
	//	Assert( IsEffectActive( EF_NODRAW ) );

	// Must be dead.
	Assert(m_lifeState == LIFE_DEAD);
	Assert(pl.deadflag);
}


void CHL2MP_Player::State_Enter_ACTIVE()
{
	SetMoveType(MOVETYPE_WALK);

	// md 8/15/07 - They'll get set back to solid when they actually respawn. If we set them solid now and mp_forcerespawn
	// is false, then they'll be spectating but blocking live players from moving.
	// RemoveSolidFlags( FSOLID_NOT_SOLID );

	m_Local.m_iHideHUD = 0;
}


void CHL2MP_Player::State_PreThink_ACTIVE()
{
	//we don't really need to do anything here. 
	//This state_prethink structure came over from CS:S and was doing an assert check that fails the way hl2dm handles death
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2MP_Player::CanHearAndReadChatFrom(CBasePlayer* pPlayer)
{
	// can always hear the console unless we're ignoring all chat
	if (!pPlayer)
		return false;

	return true;
}

#ifdef SDK2013CE
//-----------------------------------------------------------------------------
// Purpose: multiplayer does not do autoaiming.
//-----------------------------------------------------------------------------
Vector CHL2MP_Player::GetAutoaimVector(float flScale)
{
	//No Autoaim
	Vector	forward;
	AngleVectors(EyeAngles() + m_Local.m_vecPunchAngle, &forward);
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Do nothing multiplayer_animstate takes care of animation.
// Input  : playerAnim - 
//-----------------------------------------------------------------------------
void CHL2MP_Player::SetAnimation(PLAYER_ANIM playerAnim)
{
	return;
}

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //
class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS(CTEPlayerAnimEvent, CBaseTempEntity);
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent(const char* name) : CBaseTempEntity(name)
	{
	}

	CNetworkHandle(CBasePlayer, m_hPlayer);
	CNetworkVar(int, m_iEvent);
	CNetworkVar(int, m_nData);
};

IMPLEMENT_SERVERCLASS_ST_NOBASE(CTEPlayerAnimEvent, DT_TEPlayerAnimEvent)
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropInt(SENDINFO(m_iEvent), Q_log2(PLAYERANIMEVENT_COUNT) + 1, SPROP_UNSIGNED),
SendPropInt(SENDINFO(m_nData), 32)
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent("PlayerAnimEvent");

void TE_PlayerAnimEvent(CBasePlayer* pPlayer, PlayerAnimEvent_t event, int nData)
{
	CPVSFilter filter((const Vector&)pPlayer->EyePosition());

	//Tony; use prediction rules.
	filter.UsePredictionRules();

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create(filter, 0);
}


void CHL2MP_Player::DoAnimationEvent(PlayerAnimEvent_t event, int nData)
{
	m_PlayerAnimState->DoAnimationEvent(event, nData);
	TE_PlayerAnimEvent(this, event, nData);	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: Override setup bones so that is uses the render angles from
//			the HL2MP animation state to setup the hitboxes.
//-----------------------------------------------------------------------------
void CHL2MP_Player::SetupBones(matrix3x4_t* pBoneToWorld, int boneMask)
{
	VPROF_BUDGET("CHL2MP_Player::SetupBones", VPROF_BUDGETGROUP_SERVER_ANIM);

	// Get the studio header.
	Assert(GetModelPtr());
	CStudioHdr* pStudioHdr = GetModelPtr();
	if (!pStudioHdr)
		return;

	Vector pos[MAXSTUDIOBONES];
	Quaternion q[MAXSTUDIOBONES];

	// Adjust hit boxes based on IK driven offset.
	Vector adjOrigin = GetAbsOrigin() + Vector(0, 0, m_flEstIkOffset);

	// FIXME: pass this into Studio_BuildMatrices to skip transforms
	CBoneBitList boneComputed;
	if (m_pIk)
	{
		m_iIKCounter++;
		m_pIk->Init(pStudioHdr, GetAbsAngles(), adjOrigin, gpGlobals->curtime, m_iIKCounter, boneMask);
		GetSkeleton(pStudioHdr, pos, q, boneMask);

		m_pIk->UpdateTargets(pos, q, pBoneToWorld, boneComputed);
		CalculateIKLocks(gpGlobals->curtime);
		m_pIk->SolveDependencies(pos, q, pBoneToWorld, boneComputed);
	}
	else
	{
		GetSkeleton(pStudioHdr, pos, q, boneMask);
	}

	CBaseAnimating* pParent = dynamic_cast<CBaseAnimating*>(GetMoveParent());
	if (pParent)
	{
		// We're doing bone merging, so do special stuff here.
		CBoneCache* pParentCache = pParent->GetBoneCache();
		if (pParentCache)
		{
			BuildMatricesWithBoneMerge(
				pStudioHdr,
				m_PlayerAnimState->GetRenderAngles(),
				adjOrigin,
				pos,
				q,
				pBoneToWorld,
				pParent,
				pParentCache);

			return;
		}
	}

	Studio_BuildMatrices(
		pStudioHdr,
		m_PlayerAnimState->GetRenderAngles(),
		adjOrigin,
		pos,
		q,
		-1,
		GetModelScale(), // Scaling
		pBoneToWorld,
		boneMask);
}
#endif // SDK2013CE


void CHL2MP_Player::SetArmorValue(int value)
{
	BaseClass::SetArmorValue(value);
	m_iArmor = value;
}


// Armour Settings.
void CHL2MP_Player::SetMaxArmorValue(int MaxArmorValue)
{
	m_iMaxArmor = MaxArmorValue;
}

//-----------------------------------------------------------------------------
void CHL2MP_Player::IncrementArmorValue(int nCount, int nMaxValue)
{
	nMaxValue = m_iMaxArmor;
	BaseClass::IncrementArmorValue(nCount, nMaxValue);
}


#ifdef SecobMod__SAVERESTORE
void CHL2MP_Player::SaveTransitionFile(void)
{
	FileHandle_t hFile = g_pFullFileSystem->Open("transition.cfg", "w");

	if (hFile == FILESYSTEM_INVALID_HANDLE)
	{
		Warning("Invalid filesystem handle \n");
		CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
		g_pFullFileSystem->WriteFile("cfg/transition.cfg", "MOD", buf);
		return;
	}
	else
	{
		// Iterate all active players
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CHL2MP_Player* pPlayerMP = ToHL2MPPlayer(UTIL_PlayerByIndex(i));
			if (pPlayerMP == NULL)
			{
				//SecobMod__Information: If we're a listen server then the host is both a server and a client. As a server they return NULL so we return 
				g_pFullFileSystem->Close(hFile);
				return;
			}
			int HealthValue = pPlayerMP->m_iHealth;
			int ArmourValue = pPlayerMP->m_iArmor;
			int WeaponSlot = 0;


			//Set the weapon slot back to 0 for cleanliness and ease of use in hl2mp_client.cpps restore code.
			WeaponSlot = 0;
			Msg("Saving cfg file...\n");
			//Get this persons steam ID.
			//Also write on a new line a { and use the SteamID as our heading!.
			char tmpSteamid[32];
			Q_snprintf(tmpSteamid, sizeof(tmpSteamid), "\"%s\"\n""{\n", engine->GetPlayerNetworkIDString(pPlayerMP->edict()));

			//Write this persons steam ID to our file.
			g_pFullFileSystem->Write(&tmpSteamid, strlen(tmpSteamid), hFile);

			//Get their Health
			char data3[32];
			Q_snprintf(data3, sizeof(data3), "\"Health" "\" ");
			char data4[32];
			Q_snprintf(data4, sizeof(data4), "\"%i\"\n", HealthValue);
			//Write this persons Health to the file.
			g_pFullFileSystem->Write(&data3, strlen(data3), hFile);
			g_pFullFileSystem->Write(&data4, strlen(data4), hFile);

			//Get their Armour
			char data5[32];
			Q_snprintf(data5, sizeof(data5), "\"Armour" "\" ");
			char data6[32];
			Q_snprintf(data6, sizeof(data6), "\"%i\"\n", ArmourValue);
			//Write this persons Armour to the file.
			g_pFullFileSystem->Write(&data5, strlen(data5), hFile);
			g_pFullFileSystem->Write(&data6, strlen(data6), hFile);


			//Go through the players inventory to find out their weapons and ammo.
			CBaseCombatWeapon* pCheck;

			//This is our player. This is set because currently this section is in TakeDamage of hl2mp_player.cpp
			CBasePlayer* pPlayer = ToBasePlayer(pPlayerMP);
			const char* weaponName = "";
			weaponName = pPlayer->GetActiveWeapon()->GetClassname();

			//Get their current weapon so we can attempt to switch to it on spawning.
			char ActiveWepPre[32];
			Q_snprintf(ActiveWepPre, sizeof(ActiveWepPre), "\n""\"ActiveWeapon\" ");
			//Write our weapon.
			g_pFullFileSystem->Write(&ActiveWepPre, strlen(ActiveWepPre), hFile);
			char ActiveWep[32];
			Q_snprintf(ActiveWep, sizeof(ActiveWep), "\"%s\"\n", weaponName);
			//Write our weapon.
			g_pFullFileSystem->Write(&ActiveWep, strlen(ActiveWep), hFile);


			for (int i = 0; i < WeaponCount(); ++i)
			{
				pCheck = GetWeapon(i);
				if (!pCheck)
					continue;

				//Create a temporary int for both primary and secondary clip ammo counts.
				int TempPrimaryClip = pPlayer->GetAmmoCount(pCheck->GetPrimaryAmmoType());
				int TempSecondaryClip = pPlayer->GetAmmoCount(pCheck->GetSecondaryAmmoType());

				//Creaye a temporary int for both primary and seconday clip ammo TYPES.
				int ammoIndex_Pri = pCheck->GetPrimaryAmmoType();
				int ammoIndex_Sec = pCheck->GetSecondaryAmmoType();

				int ammoPrimaryClipLeft = pCheck->Clip1();
				int ammoSecondaryClipLeft = pCheck->Clip2();

				//Get out weapons classname and get our text set up.
				char pCheckWep[32];
				Q_snprintf(pCheckWep, sizeof(pCheckWep), "\n\"Weapon_%i\" \"%s\"\n", WeaponSlot, pCheck->GetClassname());
				//Write our weapon.
				g_pFullFileSystem->Write(&pCheckWep, strlen(pCheckWep), hFile);

				if (TempPrimaryClip >= 1)
				{
					//Get out weapons primary clip and get our text set up.
					char PrimaryClip[32];
					Q_snprintf(PrimaryClip, sizeof(PrimaryClip), "\n\"Weapon_%i_PriClip\" \"%i\"\n", WeaponSlot, TempPrimaryClip);
					//Now write our weapons primary clip count.
					g_pFullFileSystem->Write(&PrimaryClip, strlen(PrimaryClip), hFile);
					//Get out weapons primary clip ammo type.
					if (ammoIndex_Pri != -1)
					{
						char PrimaryWeaponClipAmmoType[32];
						Q_snprintf(PrimaryWeaponClipAmmoType, sizeof(PrimaryWeaponClipAmmoType), "\"Weapon_%i_PriClipAmmo\" ", WeaponSlot);

						char PrimaryClipAmmoType[32];
						if (GetAmmoDef()->GetAmmoOfIndex(ammoIndex_Pri)->pName)
						{
							Q_snprintf(PrimaryClipAmmoType, sizeof(PrimaryClipAmmoType), "\"%s\"\n", GetAmmoDef()->GetAmmoOfIndex(ammoIndex_Pri)->pName);
						}
						else
						{
							Q_snprintf(PrimaryClipAmmoType, sizeof(PrimaryClipAmmoType), "\n");
						}

						char PrimaryClipAmmoLeft[32];
						Q_snprintf(PrimaryClipAmmoLeft, sizeof(PrimaryClipAmmoLeft), "\"Weapon_%i_PriClipAmmoLeft\" \"%i\"\n", WeaponSlot, ammoPrimaryClipLeft);
						//Now write our weapons primary clip count and how much of the clip is left.
						g_pFullFileSystem->Write(&PrimaryWeaponClipAmmoType, strlen(PrimaryWeaponClipAmmoType), hFile);
						g_pFullFileSystem->Write(&PrimaryClipAmmoType, strlen(PrimaryClipAmmoType), hFile);
						g_pFullFileSystem->Write(&PrimaryClipAmmoLeft, strlen(PrimaryClipAmmoLeft), hFile);
					}
				}

				if (TempSecondaryClip >= 1)
				{
					//Get out weapons secondary clip and get our text set up.
					char SecondaryClip[32];
					Q_snprintf(SecondaryClip, sizeof(SecondaryClip), "\n\"Weapon_%i_SecClip\" \"%i\"\n", WeaponSlot, TempSecondaryClip);
					//Now write our weapons secondary clip count.
					g_pFullFileSystem->Write(&SecondaryClip, strlen(SecondaryClip), hFile);
					//Get out weapons secondary clip ammo type.
					if (ammoIndex_Sec != -1)
					{
						char SecondaryWeaponClipAmmoType[32];
						Q_snprintf(SecondaryWeaponClipAmmoType, sizeof(SecondaryWeaponClipAmmoType), "\"Weapon_%i_SecClipAmmo\" ", WeaponSlot);

						char SecondaryClipAmmoType[32];
						if (GetAmmoDef()->GetAmmoOfIndex(ammoIndex_Pri)->pName)
						{
							Q_snprintf(SecondaryClipAmmoType, sizeof(SecondaryClipAmmoType), "\"%s\"\n", GetAmmoDef()->GetAmmoOfIndex(ammoIndex_Sec)->pName);
						}
						else
						{
							Q_snprintf(SecondaryClipAmmoType, sizeof(SecondaryClipAmmoType), "\n");
						}

						char SecondaryClipAmmoLeft[32];
						Q_snprintf(SecondaryClipAmmoLeft, sizeof(SecondaryClipAmmoLeft), "\"Weapon_%i_SecClipAmmoLeft\" \"%i\"\n", WeaponSlot, ammoSecondaryClipLeft);
						//Now write our weapons primary clip count.
						g_pFullFileSystem->Write(&SecondaryWeaponClipAmmoType, strlen(SecondaryWeaponClipAmmoType), hFile);
						g_pFullFileSystem->Write(&SecondaryClipAmmoType, strlen(SecondaryClipAmmoType), hFile);
						g_pFullFileSystem->Write(&SecondaryClipAmmoLeft, strlen(SecondaryClipAmmoLeft), hFile);
					}
				}

				//Now increase our weapon slot number for the next weapon (if needed).
				WeaponSlot++;
			}

			//Also write on a new line a } to close off this Players section. Now that we're done with all weapons.
			char SecClose[32];
			Q_snprintf(SecClose, sizeof(SecClose), "\n\n}\n\n", NULL);
			g_pFullFileSystem->Write(&SecClose, strlen(SecClose), hFile);
		}

		//Close the file. Important or changes don't get saved till the exe closes which we don't want.
		g_pFullFileSystem->Close(hFile);
	}
}
#endif //SecobMod__SAVERESTORE