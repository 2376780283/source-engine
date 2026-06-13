//========== Copyright (C) 2025, Team HL2SB++, All rights reserved. ===========//
//
// Purpose:
//
//===========================================================================//

#include "cbase.h"
#include "baseviewmodel_shared.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialproxy.h"

#if defined( CLIENT_DLL )
#define CHandViewModel C_HandViewModel
#endif

#ifdef CLIENT_DLL
ConVar c_handmodel( "c_handmodel", "default", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_CLIENTDLL );

ConVar playercolor_r( "playercolor_r", "0", FCVAR_USERINFO | FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar playercolor_g( "playercolor_g", "229", FCVAR_USERINFO | FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar playercolor_b( "playercolor_b", "238", FCVAR_USERINFO | FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

class PlayerColorProxy : public IMaterialProxy
{
public:
	virtual bool	   Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void	   OnBind( void *pC_BaseEntity );
	virtual void	   Release();
	virtual IMaterial *GetMaterial();

private:
	IMaterial	 *m_pMaterial;
	IMaterialVar *m_pResultVar;
};

bool PlayerColorProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_pResultVar = pMaterial->FindVar( "$color2", &foundVar, false );
	m_pMaterial = pMaterial;
	return foundVar;
}

void PlayerColorProxy::OnBind( void *pC_BaseEntity )
{
	if ( m_pResultVar )
	{
		float r = playercolor_r.GetFloat() / 255.0f;
		float g = playercolor_g.GetFloat() / 255.0f;
		float b = playercolor_b.GetFloat() / 255.0f;

		m_pResultVar->SetVecValue( r, g, b );
	}
}

void PlayerColorProxy::Release()
{
}

IMaterial *PlayerColorProxy::GetMaterial()
{
	return m_pMaterial;
}

EXPOSE_INTERFACE( PlayerColorProxy, IMaterialProxy, "PlayerColor" IMATERIAL_PROXY_INTERFACE_VERSION );
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