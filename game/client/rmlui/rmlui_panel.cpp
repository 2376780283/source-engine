// ==================================================================
// Den Urakolouy AKA URAKOLOUY5
// 2025
// 
// Feel free to use it as you want to use.
// Major code based on open source references from Source SDK.
// ==================================================================

// Include pre-include header to fix NULL macro conflicts
#include "rmlui_preinclude.h"

// Include RmlUI headers first to prevent Vector namespace conflicts
#include <RmlUi/Core.h>

// 强行解除引擎可能存在的宏污染
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif
#ifdef realloc
#undef realloc
#endif

#include "cbase.h"

// Fix NULL macro after basetypes.h has been included
#ifdef NULL
#undef NULL
#endif
#define NULL nullptr
#include "rmlui_panel.h"
#include "rmlui_manager.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

RmlUiPanel::RmlUiPanel()
{    
    SetParent(enginevgui->GetPanel(PANEL_INGAMESCREENS));

    // Set panel to full screen.
    int screenWide, screenTall;
    vgui::surface()->GetScreenSize(screenWide, screenTall);
    SetPos(0, 0);
    SetSize(screenWide, screenTall);
}

void RmlUiPanel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
    RmlUIManager::GetInstance()->OnScreenSizeChanged(iOldWide, iOldTall);
}

void RmlUiPanel::OnCursorMoved(int x, int y)
{
    RmlUIManager::GetInstance()->OnCursorMoved(x, y);
    BaseClass::OnCursorMoved(x, y);
}

void RmlUiPanel::OnMousePressed(vgui::MouseCode code)
{
    RmlUIManager::GetInstance()->OnMousePressed(code);
    BaseClass::OnMousePressed(code);
}

void RmlUiPanel::OnMouseDoublePressed(vgui::MouseCode code)
{

}

void RmlUiPanel::OnMouseReleased(vgui::MouseCode code)
{
    RmlUIManager::GetInstance()->OnMouseReleased(code);
    BaseClass::OnMouseReleased(code);
}

void RmlUiPanel::OnMouseWheeled(int delta)
{
    RmlUIManager::GetInstance()->OnMouseWheeled(delta);
    BaseClass::OnMouseWheeled(delta);
}

void RmlUiPanel::OnKeyCodePressed(vgui::KeyCode code)
{
    RmlUIManager::GetInstance()->OnKeyCodePressed(code);
    BaseClass::OnKeyCodePressed(code);
}

void RmlUiPanel::OnKeyCodeReleased(vgui::KeyCode code)
{
    RmlUIManager::GetInstance()->OnKeyCodeReleased(code);
    BaseClass::OnKeyCodeReleased(code);
}

void RmlUiPanel::OnKeyTyped(wchar_t unichar)
{
    RmlUIManager::GetInstance()->OnKeyTyped(unichar);
    BaseClass::OnKeyTyped(unichar);
}

void RmlUiPanel::Paint()
{
    // Render our rmlui here for this time
    RmlUIManager::GetInstance()->Render("main");
}

void RmlUiPanel::PaintBackground()
{
}
