//========== Copyright (C) 2025, Team HL2SB++, All rights reserved. ===========//
//
// Purpose:
//
//===========================================================================//

#include "cbase.h"
#include "baseviewmodel_shared.h"


#if defined( CLIENT_DLL )
#define CHandViewModel C_HandViewModel
#endif

#ifdef CLIENT_DLL
ConVar c_hand( "c_hand", "models/weapons/c_arms_hev.mdl", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
ConVar c_handskin( "c_handskin", "0", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
static ConVar c_hand_enabled( "c_hand_enabled", "1", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_CLIENTDLL, "Toggle hand model (1=Enabled, 0=Disabled)" );
#endif

class CHandViewModel : public CBaseViewModel
{
	DECLARE_CLASS( CHandViewModel, CBaseViewModel );

public:
	DECLARE_NETWORKCLASS();

private:
};

LINK_ENTITY_TO_CLASS( hand_viewmodel, CHandViewModel );
IMPLEMENT_NETWORKCLASS_ALIASED( HandViewModel, DT_HandViewModel )

// For whatever reason the parent doesn't get sent
// And I don't really want to mess with BaseViewModel
// so now it does
BEGIN_NETWORK_TABLE( CHandViewModel, DT_HandViewModel )
#ifndef CLIENT_DLL
SendPropEHandle( SENDINFO_NAME( m_hMoveParent, moveparent ) ),
#else
RecvPropInt( RECVINFO_NAME( m_hNetworkMoveParent, moveparent ), 0, RecvProxy_IntToMoveParent ),
#endif
	END_NETWORK_TABLE()