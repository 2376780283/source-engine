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
#include "utlmap.h"

// ---------------------------------------------------------
// 模组卡片控件：支持延迟加载
// ---------------------------------------------------------
class ModCardPanel : public vgui::EditablePanel {
    DECLARE_CLASS_SIMPLE(ModCardPanel, vgui::EditablePanel);
public:
    ModCardPanel(vgui::Panel *parent, const char *name, const char *title);
    
    void SetImagePath(const char *path);
    virtual void PerformLayout() override;
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme) override;    
    virtual void Paint() override;
    
    virtual void OnCursorEntered() override;
    virtual void OnCursorExited() override;
    virtual void OnMousePressed(vgui::MouseCode code) override;

private:
    vgui::ImagePanel *m_pImagePanelPlaceholder; 
    vgui::Label      *m_pTitle;
    
    Color m_clrBgNormal;
    Color m_clrBgHover;
    
    int m_iMargin; 
    int m_nTextureID; 
    char m_szImagePath[MAX_PATH];
    bool m_bAttemptedLoad; // 是否尝试过加载，防止失败后死循环
};

// ---------------------------------------------------------
// 列表页面：管理纹理生命周期
// ---------------------------------------------------------
class ExtraListPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(ExtraListPage, vgui::PropertyPage);
public:
    ExtraListPage(vgui::Panel *parent, const char *panelName);
    virtual ~ExtraListPage(); 

    virtual void PerformLayout() override;
    void RefreshList(); 

    // 提供给 ModCardPanel 调用的纹理加载接口
    int GetTextureForPath(const char *fullPath);

private:
    int CreateTextureFromPNG(const char *fullPath);
    void CleanUpTextures();

    vgui::PanelListPanel *m_pModListPanel; 

    // 纹理缓存：Key 是路径哈希或字符串，Value 是 TextureID
    CUtlMap<unsigned int, int> m_TextureCache; 
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

    MESSAGE_FUNC_PTR(OnVersionSelected, "TextChanged", panel);
    MESSAGE_FUNC_PARAMS( OnModCardSelected, "ModCardSelected", data );

private:
    void InitVersionCombo();

    vgui::EditablePanel *m_pLeftPanel;   
    vgui::PropertySheet *m_pTabSheet;
    ExtraListPage       *m_pModListPage;

    vgui::EditablePanel *m_pRightPanel;  
    vgui::Label         *m_pDetailsLabel;
    vgui::Label         *m_pVersionTitleLabel; 
    vgui::RichText      *m_pDescriptionText;
    vgui::ComboBox      *m_pVersionCombo;
    vgui::Button        *m_pRefreshButton;

    vgui::Button        *m_pCloseButton;
};