//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Grigori's personal shotgun (npc_monk)
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


bool sde_transmit;
extern ConVar sk_auto_reload_time;

//NEW DECLARE!!!!

class CWeaponAnnabelle : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponAnnabelle, CBaseHLCombatWeapon);
public:
	CWeaponAnnabelle(void);

	void PrimaryAttack(void);
	void SecondaryAttackWithNonInheritedName(void);
	void HoldIronsight(void);
	void ItemHolsterFrame(void);
	void ItemPostFrame(void);
	bool StartReload(void);
	bool Reload(void);
	void FillClip(void);
	void FinishReload(void);
	void CheckHolsterReload(void);
	void Pump(void);
	void DryFire(void);
	bool Deploy(void);


	virtual float GetFireRate(void) { return 1.5; };
	virtual float			GetMinRestTime() { return 1.0; }
	virtual float			GetMaxRestTime() { return 1.5; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	bool	m_bNeedPump;		// When emptied completely
	bool	m_bDelayedFire1;	// Fire primary when finished reloading
	bool	m_bDelayedFire2;	// Fire secondary when finished reloading
	bool	m_bInZoom;
	void	ToggleZoom(void);

public:

	void	Precache(void);

	//virtual bool	IsWeaponZoomed() { return m_bInZoom; }
	int CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone = vec3_origin;
		return cone;
	}

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 3; }
	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	DECLARE_ACTTABLE();

};

IMPLEMENT_SERVERCLASS_ST(CWeaponAnnabelle, DT_WeaponAnnabelle)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_annabelle, CWeaponAnnabelle);
#ifndef HL2MP
PRECACHE_WEAPON_REGISTER(weapon_annabelle);
#endif

BEGIN_DATADESC(CWeaponAnnabelle)
DEFINE_FIELD(m_bNeedPump, FIELD_BOOLEAN),
DEFINE_FIELD(m_bDelayedFire1, FIELD_BOOLEAN),
DEFINE_FIELD(m_bDelayedFire2, FIELD_BOOLEAN),
END_DATADESC()

acttable_t	CWeaponAnnabelle::m_acttable[] =
{
	{ ACT_IDLE_ANGRY, ACT_IDLE_ANGRY_SMG1, true },
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SHOTGUN, true },
	{ ACT_RELOAD, ACT_RELOAD_SMG1, true },
	{ ACT_WALK, ACT_WALK_RIFLE, true },
	{ ACT_WALK_AIM, ACT_WALK_AIM_RIFLE, true },
	{ ACT_WALK_CROUCH, ACT_WALK_CROUCH_RIFLE, false },
	{ ACT_WALK_CROUCH_AIM, ACT_WALK_CROUCH_AIM_RIFLE, false },
	{ ACT_RUN, ACT_RUN_RIFLE, true },
	{ ACT_RUN_AIM, ACT_RUN_AIM_RIFLE, true },
	{ ACT_RUN_CROUCH, ACT_RUN_CROUCH_RIFLE, false },
	{ ACT_RUN_CROUCH_AIM, ACT_RUN_CROUCH_AIM_RIFLE, false },
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_SHOTGUN, true },
	{ ACT_RELOAD_LOW, ACT_RELOAD_SMG1_LOW, false },
	{ ACT_GESTURE_RELOAD, ACT_GESTURE_RELOAD_SMG1, false },
};

IMPLEMENT_ACTTABLE(CWeaponAnnabelle);

void CWeaponAnnabelle::Precache(void)
{
	CBaseCombatWeapon::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_SHOTGUN_FIRE:
	{
									  Vector vecShootOrigin, vecShootDir;
									  vecShootOrigin = pOperator->Weapon_ShootPosition();
									  CAI_BaseNPC *npc = pOperator->MyNPCPointer();
									  ASSERT(npc != NULL);
									  WeaponSound(SINGLE_NPC);
									  pOperator->DoMuzzleFlash();
									  m_iClip1 = m_iClip1 - 1;

									  vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);
									  pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);
	}
		break;

	default:
		CBaseCombatWeapon::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponAnnabelle::Deploy(void)
{
	Msg("SDE_SMG!_deploy\n");
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer)
		pPlayer->ShowCrosshair(true);
	DisplaySDEHudHint();

	bool return_value = BaseClass::Deploy();

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType))
	{
		m_bForbidIronsight = true; // to suppress ironsight during deploy in case the weapon is empty and the player has ammo 
	}							   // -> reload will be forced. Behavior of ironsightable weapons that don't bolt on deploy

	return return_value;
}
bool CWeaponAnnabelle::StartReload(void)
{
	//m_bExpSighted.SdeDisableIrons();
	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	// If shotgun totally emptied then a pump animation is needed
	if (m_iClip1 <= 0)
	{
		m_bNeedPump = true;
	}

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));

	if (j <= 0)
		return false;

	DevMsg("SDE m1garand debug: reloading started \n");
	SendWeaponAnim(ACT_SHOTGUN_RELOAD_START);

	// Make shotgun shell visible
	SetBodygroup(1, 0);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	m_bInReload = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponAnnabelle::Reload(void)
{

	DevMsg("SDE m1garand debug: reloading \n");
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer)
	{
		pPlayer->ShowCrosshair(true); // show crosshair to fix crosshair for reloading weapons in toggle ironsight

		if (m_iClip1 < 1)
		{
			bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
			if (fRet)
			{
				WeaponSound(RELOAD);
			}
			return fRet;
		}
		else
		{
			bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD_NOBOLD);
			if (fRet)
			{
				WeaponSound(RELOAD);
			}
			return fRet;
		}

	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::FinishReload(void)
{
	DevMsg("SDE m1garand debug: reloading ended \n");
	// Make shotgun shell invisible
	SetBodygroup(1, 1);

	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	FillClip(); // moved here to not set the ammo amount in the very reload start

	m_bForbidIronsight = true; // until reload animation finishes
	m_bInReload = false;

	// Finish reload animation
	SendWeaponAnim(ACT_SHOTGUN_RELOAD_FINISH);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::FillClip(void)
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return;
	DevMsg("SDE m1garand debug: fill clip, %f %f \n", Clip1(), GetMaxClip1());
	// Add them to the clip
	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0)
	{
		while (Clip1() < GetMaxClip1())
		{
			m_iClip1++;
			pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) == 0) break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play weapon pump anim
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::Pump(void)
{
	CBaseCombatCharacter *pOwner = GetOwner();
	DevMsg("SDE m1garand debug: pump \n");
	if (pOwner == NULL)
		return;

	m_bNeedPump = false;

	WeaponSound(SPECIAL1);

	// Finish reload animation
	SendWeaponAnim(ACT_SHOTGUN_PUMP);

	pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAnnabelle::ItemHolsterFrame(void)
{

	//#ifdef CLIENT_DLL

	//#endif
	// Must be player held
	if (GetOwner() && GetOwner()->IsPlayer() == false)
		return;

	// We can't be active
	if (GetOwner()->GetActiveWeapon() == this)
		return;

	// If it's been longer than three seconds, reload
	if ((gpGlobals->curtime - m_flHolsterTime) > sk_auto_reload_time.GetFloat())
	{
		// Reset the timer
		m_flHolsterTime = gpGlobals->curtime;

		if (GetOwner() == NULL)
			return;

		if (m_iClip1 == GetMaxClip1())
			return;

		// Just load the clip with no animations
		int ammoFill = MIN((GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount(GetPrimaryAmmoType()));

		GetOwner()->RemoveAmmo(ammoFill, GetPrimaryAmmoType());
		m_iClip1 += ammoFill;
	}
}
//added
void CWeaponAnnabelle::ToggleZoom(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	if (m_bInZoom)
	{
		if (pPlayer->SetFOV(this, 0, 0.2f))
		{
			m_bInZoom = false;
		}
	}
	else
	{
		if (pPlayer->SetFOV(this, 20, 0.1f))
		{
			m_bInZoom = true;
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponAnnabelle::CWeaponAnnabelle(void)
{
	m_bReloadsSingly = false;

	m_bNeedPump = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;

	m_fMinRange1 = 0.0;
	m_fMaxRange1 = 500;
	m_fMinRange2 = 0.0;
	m_fMaxRange2 = 200;
}

void CWeaponAnnabelle::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.1f;
		}

		return;
	}

	m_iPrimaryAttacks++;
	//gamestats->Event_WeaponFired(pPlayer, true, GetClassname());

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.4f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.4f;

	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	pPlayer->FireBullets(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);

	pPlayer->SetMuzzleFlashTime(gpGlobals->curtime + 0.5);

	//Disorient the player
	QAngle angles = pPlayer->GetLocalAngles();

	angles.x += random->RandomInt(-1, 1);
	angles.y += random->RandomInt(-1, 1);
	angles.z = 0;

	pPlayer->SnapEyeAngles(angles);

	pPlayer->ViewPunch(QAngle(-2, random->RandomFloat(-1, 1), 0));
	if (m_iClip1 >= 1)
	{
		//CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner());
		WeaponSound(SINGLE);
	}
	else
	{
		WeaponSound(EMPTY_SHOT);
	}
	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}

void CWeaponAnnabelle::HoldIronsight(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer->m_afButtonPressed & IN_IRONSIGHT)
	{
		EnableIronsights();
		pPlayer->ShowCrosshair(false);
	}
	if (pPlayer->m_afButtonReleased & IN_IRONSIGHT)
	{
		DisableIronsights();
		pPlayer->ShowCrosshair(true);
	}
}

void CWeaponAnnabelle::ItemPostFrame(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	if (m_bForbidIronsight && gpGlobals->curtime >= m_flNextPrimaryAttack)
		m_bForbidIronsight = false;

	if (!(m_bInReload || m_bForbidIronsight) && (m_iClip1 > 0 || (m_iClip1 <= 0 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)))
	{// if this weapon is ready or (empty && player has no ammo for it)

		// if the weapon is empty and the player has no ammo for it, perform default actions (e.g. switch to a usable weapon)
		// except attacks and reload
		if (m_iClip1 <= 0 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 && !((pOwner->m_nButtons & IN_ATTACK) ||
			(pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)))
		{
			BaseClass::ItemPostFrame();
		}

		// Allow  Ironsight
		HoldIronsight();

		if ((pOwner->m_afButtonPressed & IN_ATTACK) && gpGlobals->curtime >= m_flNextPrimaryAttack)
		{
			PrimaryAttack();
		}

		if ((pOwner->m_afButtonPressed & IN_ATTACK2) && gpGlobals->curtime >= m_flNextSecondaryAttack)
			// toggle zoom on powerful scoped weapon like vanilla HL2 crossbow
		{
			SecondaryAttackWithNonInheritedName(); // so that it cannot be called by BaseClass::ItemPostFrame() when unneeded
		}

		if ((pOwner->m_afButtonPressed & IN_RELOAD) && gpGlobals->curtime >= m_flNextPrimaryAttack)
		{
			Reload();
		}
	}
	else
		BaseClass::ItemPostFrame(); //reload
}

void CWeaponAnnabelle::SecondaryAttackWithNonInheritedName(void)
{
	//// Only the player fires this way so we can cast
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	ToggleIronsights();
	pOwner->ToggleCrosshair();


}