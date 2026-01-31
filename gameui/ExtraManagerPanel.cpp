#include "ExtraManagerPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

using namespace vgui;

#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// =========================================================
// ModCardPanel 实现 (修复文字挤压与可见性)
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title) 
    : BaseClass(parent, name) {
    
    // 子卡片不需要背景，由 PanelListPanel 处理
    SetPaintBackgroundEnabled(false);
    SetPaintBorderEnabled(false);
    m_iMargin = PROPVAL(6); 

    m_pImage = new vgui::ImagePanel(this, "ModImage");
    m_pImage->SetShouldScaleImage(true);
    m_pImage->SetImage("default_mod_preview"); 
    
    m_pTitle = new vgui::Label(this, "ModTitle", title);
    m_pTitle->SetPaintBackgroundEnabled(false);      
    m_pTitle->SetFgColor(Color(255, 255, 255, 255));
    m_pTitle->SetContentAlignment(vgui::Label::a_center);

    // 预留足够的高度给文字 (36px)
    int iImageSize = PROPVAL(120);
    int iLabelHeight = PROPVAL(36); 
    SetSize(iImageSize + m_iMargin, iImageSize + iLabelHeight + m_iMargin);
}

void ModCardPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);
    m_pTitle->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
}

void ModCardPanel::PerformLayout() {
    BaseClass::PerformLayout();
    
    int w, h;
    GetSize(w, h);
    int contentW = w - m_iMargin;
    int drawX = m_iMargin / 2;
    int drawY = m_iMargin / 2;

    m_pImage->SetBounds(drawX, drawY, contentW, contentW);
    
    // 修复：Label 占据底部剩余空间，防止文字被截断
    int labelY = drawY + contentW + PROPVAL(4);
    int labelH = h - labelY;
    m_pTitle->SetBounds(drawX, labelY, contentW, labelH);
}

// =========================================================
// ExtraListPage 实现
// =========================================================
ExtraListPage::ExtraListPage(vgui::Panel *parent, const char *panelName) 
    : BaseClass(parent, panelName) {
    
    m_pModListPanel = new vgui::PanelListPanel(this, "ModListPanel");
    m_pModListPanel->SetFirstColumnWidth(0);
    m_pModListPanel->SetNumColumns(3); 
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(12));

    for (int i = 0; i < 12; i++) {
        char szTitle[64];
        Q_snprintf(szTitle, sizeof(szTitle), "MOD ITEM %02d", i + 1);
        ModCardPanel *pCard = new ModCardPanel(m_pModListPanel, "ModCard", szTitle);
        m_pModListPanel->AddItem(nullptr, pCard);
    }
}

void ExtraListPage::PerformLayout() {
    BaseClass::PerformLayout();
    int w, h;
    GetSize(w, h);
    int margin = PROPVAL(8);
    if (m_pModListPanel) {
        m_pModListPanel->SetBounds(margin, margin, w - (margin * 2), h - (margin * 2));
    }
}

// =========================================================
// ExtraManagerPanel 实现 (修复属性传递问题)
// =========================================================
ExtraManagerPanel::ExtraManagerPanel(vgui::Panel *parent)
    : BaseClass(parent, "ExtraManagerPanel") {
    
    // 1. 设置主面板全屏
    int screenW, screenH;
    vgui::surface()->GetScreenSize(screenW, screenH);
    SetSize(screenW, screenH);

    // 2. 仅对父面板设置：不渲染、不显示标题、不可交互
    SetTitle("", false);
    SetPaintBackgroundEnabled(false);
    SetPaintBorderEnabled(false);
    SetMoveable(false);
    SetSizeable(false);
    SetCloseButtonVisible(false);

    // 3. 创建左侧子面板
    m_pLeftPanel = new vgui::EditablePanel(this, "LeftFloatingPanel");
    // 关键修复：显式开启子面板的背景绘制
    m_pLeftPanel->SetPaintBackgroundEnabled(true);
    m_pLeftPanel->SetPaintBorderEnabled(true);
    m_pLeftPanel->SetBgColor(Color(0, 0, 0, 210)); 

    // 4. 创建右侧子面板
    m_pRightPanel = new vgui::EditablePanel(this, "RightFloatingPanel");
    // 关键修复：显式开启子面板的背景绘制
    m_pRightPanel->SetPaintBackgroundEnabled(true);
    m_pRightPanel->SetPaintBorderEnabled(true);
    m_pRightPanel->SetBgColor(Color(0, 0, 0, 160)); 

    // 5. 组装 TabSheet 到左面板
    m_pTabSheet = new PropertySheet(m_pLeftPanel, "ExtraTabs");
    m_pTabSheet->AddPage(new ExtraListPage(m_pTabSheet, "ExtraListPage"), "MODS");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "PREVIEW");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "DEV");

    // 退出按钮直接放在主面板
    m_pCloseButton = new Button(this, "CloseBtn", "BACK", this, "Close");
}

void ExtraManagerPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    // 调用父类会刷新 Scheme，但我们需要覆盖它对子面板可能产生的隐式影响
    BaseClass::ApplySchemeSettings(pScheme);

    // 修复：强制设置子面板的外观，不让它们继承父面板的 PaintEnabled(false)
    if (m_pLeftPanel) {
        m_pLeftPanel->SetPaintBackgroundEnabled(true);
        m_pLeftPanel->SetPaintBorderEnabled(true);
        m_pLeftPanel->SetBorder(pScheme->GetBorder("FrameBorder"));
        m_pLeftPanel->SetBgColor(pScheme->GetColor("Frame.BgColor", Color(0, 0, 0, 200)));
    }

    if (m_pRightPanel) {
        m_pRightPanel->SetPaintBackgroundEnabled(true);
        m_pRightPanel->SetPaintBorderEnabled(true);
        m_pRightPanel->SetBorder(pScheme->GetBorder("FrameBorder"));
        m_pRightPanel->SetBgColor(pScheme->GetColor("Frame.BgColor", Color(0, 0, 0, 150)));
    }

    if (m_pCloseButton) {
        m_pCloseButton->SetFont(pScheme->GetFont("DefaultLarge", IsProportional()));
    }
}

void ExtraManagerPanel::PerformLayout() {
    BaseClass::PerformLayout();

    int sw, sh;
    GetSize(sw, sh);

    int iPadding = PROPVAL(20);   
    int iGap = PROPVAL(20);       
    
    // 布局计算：左 65%，右 35% 左右
    int leftW = (sw * 0.65) - (iPadding + iGap / 2);
    int rightW = sw - leftW - (iPadding * 2) - iGap;
    int panelH = sh - (iPadding * 2);

    m_pLeftPanel->SetBounds(iPadding, iPadding, leftW, panelH);
    m_pRightPanel->SetBounds(iPadding + leftW + iGap, iPadding, rightW, panelH);

    // TabSheet 填充左面板
    int tPadding = PROPVAL(12);
    m_pTabSheet->SetBounds(tPadding, tPadding, leftW - (tPadding * 2), panelH - (tPadding * 2));

    // 关闭按钮
    int btnW = PROPVAL(100);
    int btnH = PROPVAL(30);
    m_pCloseButton->SetBounds(sw - iPadding - btnW, sh - iPadding - btnH, btnW, btnH);
}

void ExtraManagerPanel::OnCommand(const char *command) {
    if (!Q_stricmp(command, "Close")) {
        Close();
    } else {
        BaseClass::OnCommand(command);
    }
}

void ExtraManagerPanel::Activate() {
    BaseClass::Activate();
    MoveToFront();
    RequestFocus();
}

void ExtraManagerPanel::OnClose() {
    BaseClass::OnClose();
    SetVisible(false);
    MarkForDeletion();
}