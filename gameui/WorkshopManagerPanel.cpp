//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BasePanel.h"
#include "EngineInterface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include <vgui/ISystem.h>
#include "vgui_controls/RadioButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ControllerMap.h"
#include "filesystem.h"
#include "ModInfo.h"
#include "tier1/convar.h"
#include "GameUI_Interface.h"
#include "tier0/icommandline.h"
#include "vgui_controls/AnimationController.h"
#include "CommentaryExplanationDialog.h"
#include "vgui_controls/BitmapImagePanel.h"

#include <stdio.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;



//-----------------------------------------------------------------------------
// Purpose: new game chapter selection
//-----------------------------------------------------------------------------
// WorkshopManagerPanel::WorkshopManagerPanel(vgui::Panel *parent) : PropertyDialog(parent, "WorkshopManagerPanel"){
/*WorkshopManagerPanel::WorkshopManagerPanel(vgui::Panel* parent) : BaseClass(parent, "WorkshopManagerPanel") {
    SetSize(600, 400);
	SetBounds(0, 0, 600, 500);
	SetSizeable( false );
	SetTitle("Workshop Modlist", true);    
    
}*/

// 在文档1的构造函数中实现图片加载
WorkshopManagerPanel::WorkshopManagerPanel(vgui::Panel* parent) : BaseClass(parent, "WorkshopManagerPanel") 
{
    // 现有初始化代码
    SetSize(600, 400);
	SetBounds(0, 0, 600, 500);
	SetSizeable(false);
	SetTitle("Workshop Modlist", true);    

}

// 确保在析构函数中清理资源
WorkshopManagerPanel::~WorkshopManagerPanel()
{

}

void WorkshopManagerPanel::Activate( void )
{
	BaseClass::Activate();
}

void WorkshopManagerPanel::OnCommand(){}

void WorkshopManagerPanel::OnClose(){}
