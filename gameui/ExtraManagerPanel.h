#pragma once

#include "vgui_controls/Frame.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/PanelListPanel.h"
#include "utlvector.h"

// ---------------------------------------------------------
// 模组卡片控件：增加内边距来实现宫格 Margin 效果
// ---------------------------------------------------------
class ModCardPanel : public vgui::EditablePanel {
    DECLARE_CLASS_SIMPLE(ModCardPanel, vgui::EditablePanel);
public:
    ModCardPanel(vgui::Panel *parent, const char *name, const char *title);
    virtual void PerformLayout() override;
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme) override;

private:
    vgui::ImagePanel *m_pImage;
    vgui::Label      *m_pTitle;
    
    // 用于内部间距的变量
    int m_iMargin; 
};

// ---------------------------------------------------------
// 第一页：Installed Mods
// ---------------------------------------------------------
class ExtraListPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(ExtraListPage, vgui::PropertyPage);
public:
    ExtraListPage(vgui::Panel *parent, const char *panelName);
    virtual void PerformLayout() override;

private:
    vgui::PanelListPanel *m_pModListPanel; 
};

// ---------------------------------------------------------
// 其他页面
// ---------------------------------------------------------
class ModelPreviewPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(ModelPreviewPage, vgui::PropertyPage);
public:
    ModelPreviewPage(vgui::Panel *parent, const char *panelName) : BaseClass(parent, panelName) {}
};

class DevPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(DevPage, vgui::PropertyPage);
public:
    DevPage(vgui::Panel *parent, const char *panelName) : BaseClass(parent, panelName) {}
};

// ---------------------------------------------------------
// 主窗口
// ---------------------------------------------------------
class ExtraManagerPanel : public vgui::Frame {
    DECLARE_CLASS_SIMPLE(ExtraManagerPanel, vgui::Frame);
public:
    ExtraManagerPanel(vgui::Panel *parent);
    virtual ~ExtraManagerPanel() {}

    virtual void Activate() override;
    virtual void OnCommand(const char *command) override;
    virtual void OnClose() override;
    virtual void PerformLayout() override;

private:
    vgui::PropertySheet *m_pTabSheet;
    vgui::Button        *m_pCloseButton;
};