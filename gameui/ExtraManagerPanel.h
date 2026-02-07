#pragma once

#include "vgui_controls/Frame.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/RichText.h"
#include "vgui_controls/ComboBox.h"
#include "utlvector.h"

// ---------------------------------------------------------
// 模组卡片控件
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
    int m_iMargin; 
};

// ---------------------------------------------------------
// 列表页面
// ---------------------------------------------------------
class ExtraListPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(ExtraListPage, vgui::PropertyPage);
public:
    ExtraListPage(vgui::Panel *parent, const char *panelName);
    virtual void PerformLayout() override;
    void RefreshList(); // 模拟刷新逻辑

private:
    vgui::PanelListPanel *m_pModListPanel; 
};

// 占位页面
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
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme) override;

    // 响应下拉框改变
    MESSAGE_FUNC_PTR(OnVersionSelected, "TextChanged", panel);

private:
    // 版本数据初始化
    void InitVersionCombo();

    // 左侧面板组件
    vgui::EditablePanel *m_pLeftPanel;   
    vgui::PropertySheet *m_pTabSheet;
    ExtraListPage       *m_pModListPage;

    // 右侧面板组件
    vgui::EditablePanel *m_pRightPanel;  
    vgui::Label         *m_pDetailsLabel;
    vgui::Label         *m_pVersionTitleLabel; 
    vgui::RichText      *m_pDescriptionText;
    vgui::ComboBox      *m_pVersionCombo;
    vgui::Button        *m_pRefreshButton;

    vgui::Button        *m_pCloseButton;
};