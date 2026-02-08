#include "ExtraManagerPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

using namespace vgui;

#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// =========================================================
// 版本维护数据结构 (在此添加新版本即可)
// =========================================================
struct VersionInfo_t {
    const char *szVersion;
    const char *szDescription;
};

static VersionInfo_t g_VersionData[] = {
    { "1.16",    "- 修复touch贴图问题\n- 保持64位稳定。" },
    { "1.17.26", "- 修复烟雾渲染\n- 修复touch触摸\n- 修改touch网格颜色\n- 添加gamepadui支持" },
    { "1.18.0",  "- 修复gamepadui问题\n- 支持entropyZero2模组\n- 完整支持png加载\n- 加入更多半条命2 20th更新特性" },
    { "1.18.3",  "- 修复gamepadui tab对不齐\n- 支持touch使用png作为贴图\n- 优化性能" },
    { "1.18.4",  "- 修复gamepadui高分辨ui错位问题。" }
};

// =========================================================
// ModCardPanel 实现
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title) 
    : BaseClass(parent, name) {   
    SetPaintBackgroundEnabled(true);
    SetPaintBorderEnabled(false);
    m_iMargin = PROPVAL(6); 

    m_pImage = new vgui::ImagePanel(this, "ModImage");
    m_pImage->SetShouldScaleImage(true);
    m_pImage->SetImage("default_mod_preview"); 
    
    m_pTitle = new vgui::Label(this, "ModTitle", title);
    // 关键点：确保 Label 背景透明，否则会遮挡我们绘制的半透明黑色
    m_pTitle->SetPaintBackgroundEnabled(false);      
    m_pTitle->SetFgColor(Color(255, 255, 255, 255));
    m_pTitle->SetContentAlignment(vgui::Label::a_center);

    int iImageSize = PROPVAL(120);
    int iLabelHeight = PROPVAL(36); 
    SetSize(iImageSize + m_iMargin, iImageSize + iLabelHeight + m_iMargin);
}

void ModCardPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);
    m_pTitle->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
}

void ModCardPanel::Paint() {
    BaseClass::Paint();
    int w, h;
    GetSize(w, h);
    int contentW = w - m_iMargin;
    int drawX = m_iMargin / 2;
    int labelH = PROPVAL(36);
    int labelY = h - labelH - (m_iMargin / 2);
    vgui::surface()->DrawSetColor(0, 0, 0, 150); 
    vgui::surface()->DrawFilledRect(drawX, labelY, drawX + contentW, labelY + labelH);
}

void ModCardPanel::PerformLayout() {
    BaseClass::PerformLayout();
    int w, h;
    GetSize(w, h);
    
    int contentW = w - m_iMargin;
    int drawX = m_iMargin / 2;
    int drawY = m_iMargin / 2;
    m_pImage->SetBounds(drawX, drawY, contentW, contentW);

    int labelH = PROPVAL(36);
    int labelY = h - labelH - (m_iMargin / 2);
    m_pTitle->SetBounds(drawX, labelY, contentW, labelH);
}

// =========================================================
// ExtraListPage 实现
// =========================================================
ExtraListPage::ExtraListPage(vgui::Panel *parent, const char *panelName) 
    : BaseClass(parent, panelName) {
    
    m_pModListPanel = new vgui::PanelListPanel(this, "ModListPanel");
    m_pModListPanel->SetFirstColumnWidth(0);
    m_pModListPanel->SetNumColumns(4); 
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(12));

    RefreshList();
}

void ExtraListPage::RefreshList() {
    m_pModListPanel->DeleteAllItems();
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

    // --- 左侧面板 ---
    m_pLeftPanel = new vgui::EditablePanel(this, "LeftFloatingPanel");
    m_pLeftPanel->SetPaintBackgroundEnabled(true);
    m_pLeftPanel->SetBgColor(Color(0, 0, 0, 210)); 

    m_pTabSheet = new PropertySheet(m_pLeftPanel, "ExtraTabs");
    m_pModListPage = new ExtraListPage(m_pTabSheet, "ExtraListPage");
    m_pTabSheet->AddPage(m_pModListPage, "mods");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "preview items");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "developers");

    // --- 右侧面板 ---
    m_pRightPanel = new vgui::EditablePanel(this, "RightFloatingPanel");
    m_pRightPanel->SetPaintBackgroundEnabled(true);
    m_pRightPanel->SetBgColor(Color(0, 0, 0, 160)); 

    m_pDetailsLabel = new vgui::Label(m_pRightPanel, "DetailsLabel", "Information");
    m_pVersionTitleLabel = new vgui::Label(m_pRightPanel, "VersionTitleLabel", "Watch what new:");

    m_pDescriptionText = new vgui::RichText(m_pRightPanel, "DescriptionText");
    m_pDescriptionText->SetVerticalScrollbar(true);

    m_pVersionCombo = new vgui::ComboBox(m_pRightPanel, "VersionCombo", 6, false);
    m_pVersionCombo->AddActionSignalTarget(this);

    // 数据初始化
    InitVersionCombo();

    m_pRefreshButton = new vgui::Button(m_pRightPanel, "RefreshBtn", "刷新列表内容", this, "RefreshList");
    m_pCloseButton = new Button(this, "CloseBtn", "Close", this, "Close");

    // 默认选择第一个版本
    m_pVersionCombo->ActivateItemByRow(0);
}

void ExtraManagerPanel::InitVersionCombo() {
    if (!m_pVersionCombo) return;

    for (int i = 0; i < ARRAYSIZE(g_VersionData); i++) {
        m_pVersionCombo->AddItem(g_VersionData[i].szVersion, nullptr);
    }
}

// 响应版本切换显示特性
void ExtraManagerPanel::OnVersionSelected(vgui::Panel *panel) {
    if (panel == m_pVersionCombo) {
        char szText[64];
        m_pVersionCombo->GetText(szText, sizeof(szText));
        
        m_pDescriptionText->SetText(""); // 清空
        
        // 渲染标题
        m_pDescriptionText->InsertColorChange(Color(255, 210, 0, 255));
        m_pDescriptionText->InsertString("版本 ");
        m_pDescriptionText->InsertString(szText);
        m_pDescriptionText->InsertString(" 特性说明:\n\n");
        m_pDescriptionText->InsertColorChange(Color(255, 255, 255, 255));

        // 查找并插入对应的描述
        for (int i = 0; i < ARRAYSIZE(g_VersionData); i++) {
            if (!Q_strcmp(szText, g_VersionData[i].szVersion)) {
                m_pDescriptionText->InsertString(g_VersionData[i].szDescription);
                break;
            }
        }
    }
}

void ExtraManagerPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);

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

    if (m_pDetailsLabel) m_pDetailsLabel->SetFont(pScheme->GetFont("DefaultLarge", IsProportional()));
    if (m_pVersionTitleLabel) m_pVersionTitleLabel->SetFont(pScheme->GetFont("DefaultSmall", IsProportional()));
    if (m_pDescriptionText) m_pDescriptionText->SetFont(pScheme->GetFont("DefaultSmall", IsProportional()));
    if (m_pCloseButton) m_pCloseButton->SetFont(pScheme->GetFont("DefaultLarge", IsProportional()));
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

    // --- 右侧布局 ---
    int rInnerPad = PROPVAL(15);
    int currentY = rInnerPad;

    m_pDetailsLabel->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(30));
    currentY += PROPVAL(45);

    m_pVersionTitleLabel->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(15));
    currentY += PROPVAL(20);

    m_pVersionCombo->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(24));
    currentY += PROPVAL(35);

    int descH = panelH / 2.2;
    m_pDescriptionText->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), descH);

    int btnW = rightW - (rInnerPad * 2);
    int btnH = PROPVAL(35);
    m_pRefreshButton->SetBounds(rInnerPad, panelH - rInnerPad - btnH, btnW, btnH);

    int exitBtnW = PROPVAL(100);
    int exitBtnH = PROPVAL(30);
    m_pCloseButton->SetBounds(sw - iPadding - exitBtnW, sh - iPadding - exitBtnH, exitBtnW, exitBtnH);
}

void ExtraManagerPanel::OnCommand(const char *command) {
    if (!Q_stricmp(command, "Close")) {
        Close();
    } else if (!Q_stricmp(command, "RefreshList")) {
        if (m_pModListPage) {
            m_pModListPage->RefreshList();
        }
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