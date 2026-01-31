#include "ExtraManagerPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "KeyValues.h"

using namespace vgui;

#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// =========================================================
// ModCardPanel 实现 (硬编码样式与交互增强版)
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title) 
    : BaseClass(parent, name) {
    
    // 初始化硬编码颜色
    m_clrBgNormal = Color(40, 40, 40, 150);
    m_clrBgHover  = Color(80, 80, 80, 200);
    m_clrText     = Color(255, 255, 255, 255);
    m_iPadding    = PROPVAL(6);

    // 启用背景绘制与鼠标交互
    SetPaintBackgroundEnabled(true);
    SetPaintBorderEnabled(true);
    SetBgColor(m_clrBgNormal);
    SetMouseInputEnabled(true);
    
    m_pImage = new vgui::ImagePanel(this, "ModImage");
    m_pImage->SetShouldScaleImage(true);
    m_pImage->SetImage("default_mod_preview"); 
    m_pImage->SetMouseInputEnabled(false); // 允许点击穿透

    m_pTitle = new vgui::Label(this, "ModTitle", title);
    m_pTitle->SetPaintBackgroundEnabled(false);      
    m_pTitle->SetFgColor(m_clrText);
    m_pTitle->SetContentAlignment(vgui::Label::a_center);
    m_pTitle->SetMouseInputEnabled(false); // 允许点击穿透

    UpdateIdealSize();
}

void ModCardPanel::UpdateIdealSize() {
    // 基于硬编码的间距和比例计算尺寸
    int iImageSize = PROPVAL(120);
    
    HFont hFont = m_pTitle->GetFont();
    if (hFont == INVALID_FONT) {
        hFont = scheme()->GetIScheme(GetScheme())->GetFont("DefaultVerySmall", IsProportional());
    }
    int iFontHeight = vgui::surface()->GetFontTall(hFont);
    
    // 总高度 = 图片高度 + 文字高度 + 3倍边距(上下+文字图片间)
    int iTotalWidth = iImageSize + (m_iPadding * 2);
    int iTotalHeight = iImageSize + iFontHeight + (m_iPadding * 3); 
    
    SetSize(iTotalWidth, iTotalHeight);
}

void ModCardPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);   
    
    HFont hFont = pScheme->GetFont("DefaultVerySmall", IsProportional());
    m_pTitle->SetFont(hFont);   
    
    // 强制设置边框外观
    SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));

    UpdateIdealSize();
}

void ModCardPanel::PerformLayout() {
    BaseClass::PerformLayout();    
    
    int w, h;
    GetSize(w, h);
    
    int contentW = w - (m_iPadding * 2);

    // 1. 图片放置在顶部内边距处
    m_pImage->SetBounds(m_iPadding, m_iPadding, contentW, contentW);   

    // 2. 文字位于图片底部下方
    int labelY = m_iPadding + contentW + PROPVAL(4);
    int labelH = h - labelY - m_iPadding;
    
    m_pTitle->SetBounds(m_iPadding, labelY, contentW, labelH);
}

// 鼠标进入事件
void ModCardPanel::OnCursorEntered() {
    SetBgColor(m_clrBgHover);
}

// 鼠标离开事件
void ModCardPanel::OnCursorExited() {
    SetBgColor(m_clrBgNormal);
}

// 点击事件处理
void ModCardPanel::OnMousePressed(vgui::MouseCode code) {
    if (code == MOUSE_LEFT) {
        // 向父容器发送被选中的消息，可携带卡片名或其他数据
        PostActionSignal(new KeyValues("ModCardSelected", "panelName", GetName()));
    }
}

// =========================================================
// ExtraListPage 实现
// =========================================================
ExtraListPage::ExtraListPage(vgui::Panel *parent, const char *panelName) 
    : BaseClass(parent, panelName) {
    
    m_pModListPanel = new vgui::PanelListPanel(this, "ModListPanel");
    m_pModListPanel->SetFirstColumnWidth(0);
    m_pModListPanel->SetNumColumns(3); 
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(8));

    // 填充示例卡片
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
// ExtraManagerPanel 实现
// =========================================================
ExtraManagerPanel::ExtraManagerPanel(vgui::Panel *parent)
    : BaseClass(parent, "ExtraManagerPanel") {
    
    int screenW, screenH;
    vgui::surface()->GetScreenSize(screenW, screenH);
    SetSize(screenW, screenH);

    SetTitle("", false);
    SetPaintBackgroundEnabled(false);
    SetPaintBorderEnabled(false);
    SetMoveable(false);
    SetSizeable(false);
    SetCloseButtonVisible(false);

    m_pLeftPanel = new vgui::EditablePanel(this, "LeftFloatingPanel");
    m_pLeftPanel->SetPaintBackgroundEnabled(true);
    m_pLeftPanel->SetPaintBorderEnabled(true);

    m_pRightPanel = new vgui::EditablePanel(this, "RightFloatingPanel");
    m_pRightPanel->SetPaintBackgroundEnabled(true);
    m_pRightPanel->SetPaintBorderEnabled(true);

    m_pTabSheet = new PropertySheet(m_pLeftPanel, "ExtraTabs");
    m_pTabSheet->AddPage(new ExtraListPage(m_pTabSheet, "ExtraListPage"), "MODS");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "PREVIEW");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "DEV");

    m_pCloseButton = new Button(this, "CloseBtn", "BACK", this, "Close");
}

void ExtraManagerPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);

    if (m_pLeftPanel) {
        m_pLeftPanel->SetPaintBackgroundEnabled(true);
        m_pLeftPanel->SetPaintBorderEnabled(true);
        m_pLeftPanel->SetBorder(pScheme->GetBorder("FrameBorder"));
        m_pLeftPanel->SetBgColor(Color(0, 0, 0, 200)); // 硬编码颜色
    }

    if (m_pRightPanel) {
        m_pRightPanel->SetPaintBackgroundEnabled(true);
        m_pRightPanel->SetPaintBorderEnabled(true);
        m_pRightPanel->SetBorder(pScheme->GetBorder("FrameBorder"));
        m_pRightPanel->SetBgColor(Color(0, 0, 0, 150)); // 硬编码颜色
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
    
    int leftW = (sw * 0.65) - (iPadding + iGap / 2);
    int rightW = sw - leftW - (iPadding * 2) - iGap;
    int panelH = sh - (iPadding * 2);

    m_pLeftPanel->SetBounds(iPadding, iPadding, leftW, panelH);
    m_pRightPanel->SetBounds(iPadding + leftW + iGap, iPadding, rightW, panelH);

    int tPadding = PROPVAL(12);
    m_pTabSheet->SetBounds(tPadding, tPadding, leftW - (tPadding * 2), panelH - (tPadding * 2));

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