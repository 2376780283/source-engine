//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/KeyRepeat.h"
#include "utlvector.h"


//-----------------------------------------------------------------------------
// Purpose: Handles starting a new game, skill and chapter selection
//-----------------------------------------------------------------------------
class WorkshopManagerPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( WorkshopManagerPanel, vgui::Frame );

public:
	WorkshopManagerPanel(vgui::Panel *parent );
	~WorkshopManagerPanel();

	virtual void	Activate( void );
	
	virtual void	OnCommand();
	virtual void	OnClose( void );
	

};


