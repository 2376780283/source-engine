//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"

#ifdef GAME_DLL

#include "achievementmgr.h"
#include "baseachievement.h"

// Ep2-specific macro that sets game dir filter.  We need this because Ep1/Ep2/... share a binary so we need runtime check against running game.
#define DECLARE_EP2_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "snowdrop_escape", iPointValue, false )

#define DECLARE_EP2_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "snowdrop_escape", iPointValue, true )

// achievements which are won by a map event firing once
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_ACH_SDE_GOOD_END, "ACH_SDE_GOOD_END", 10 );
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_ACH_SDE_FLAWLESS_END, "ACH_SDE_FLAWLESS_END", 10 );
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_ACH_SDE_POLIBIUS, "ACH_SDE_POLIBIUS", 10 );
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_DOLL, "ACH_SDE_DOLL", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_WEAPON_WRENCH, "ACH_SDE_WEAPON_WRENCH", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_WEAPON_SOS, "ACH_SDE_WEAPON_SOS", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_WEAPON_357, "ACH_SDE_WEAPON_357", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_WEAPON_SMG, "ACH_SDE_WEAPON_SMG", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_BLUE_ROOM, "ACH_SDE_BLUE_ROOM", 10);
DECLARE_EP2_MAP_EVENT_ACHIEVEMENT(ACHIEVEMENT_ACH_SDE_LINK, "ACH_SDE_LINK", 10);

#endif // GAME_DLL