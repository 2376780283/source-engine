    //========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "vgui_controls/Frame.h"
#include "vgui_controls/KeyRepeat.h"
#include "utlvector.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ControllerMap.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/RadioButton.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"

//
// 第一页：模组列表
//
class ModelPreviewPage; 
class ImagePanelPNG; 
   
class WorkshopListPage : public vgui::PropertyPage
{
    DECLARE_CLASS_SIMPLE(WorkshopListPage, vgui::PropertyPage);

public:
    WorkshopListPage(vgui::Panel *parent, const char *panelName);
    void PopulateFolderList();
    void PerformLayout();

protected:
    virtual void OnCommand(const char *command) override;

private:
    vgui::ListPanel *m_pFolderList;
    vgui::TextEntry *m_pSearchBox;
    vgui::ComboBox  *m_pCategoryBox;
    vgui::Button    *m_pRefreshButton;
    vgui::Label     *m_pFilterLabel;
};


//
// 主窗口：Workshop 管理器
//
class WorkshopManagerPanel : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(WorkshopManagerPanel, vgui::Frame);

public:
    WorkshopManagerPanel(vgui::Panel *parent);
    ~WorkshopManagerPanel();

    virtual void Activate() override;
    virtual void OnCommand(const char *command) override;
    virtual void OnClose() override;
    virtual void PerformLayout() override;

private:
    vgui::PropertySheet *m_pTabSheet;   // 标签页容器
    WorkshopListPage    *m_pListPage;   // 第一页
    ModelPreviewPage *m_pModelPreviewPage;  // mdl页
    vgui::PropertyPage *m_pDevPage;

    vgui::Button        *m_pCloseButton; // 关闭按钮
};


