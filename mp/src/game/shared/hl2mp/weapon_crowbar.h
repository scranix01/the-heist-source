//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2MP_WEAPON_CROWBAR_H
#define HL2MP_WEAPON_CROWBAR_H
#pragma once


#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "weapon_hl2mpbasebasebludgeon.h"


#ifdef CLIENT_DLL
#define CWeaponCrowbar C_WeaponCrowbar
#endif

//-----------------------------------------------------------------------------
// CWeaponCrowbar
//-----------------------------------------------------------------------------

class CWeaponCrowbar : public CBaseHL2MPBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponCrowbar, CBaseHL2MPBludgeonWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#if !defined( CLIENT_DLL ) || defined( SDK2013CE )
	DECLARE_ACTTABLE();
#endif

	CWeaponCrowbar();

	float		GetRange( void );
	float		GetFireRate( void );

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );
	void		SecondaryAttack( void )	{	return;	}

	void		Drop( const Vector &vecVelocity );

	CWeaponCrowbar(const CWeaponCrowbar&);


	// Animation event
#ifndef CLIENT_DLL
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	//int WeaponMeleeAttack1Condition( float flDot, float flDist );
#endif

	//CWeaponCrowbar( const CWeaponCrowbar & );

private:
	// Animation event handlers
	//void HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
};


#endif // HL2MP_WEAPON_CROWBAR_H

