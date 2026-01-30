#include "ExtraManagerPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

using namespace vgui;

// 比例缩放宏
#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// =========================================================
// ModCardPanel 实现 (修复文字可见性)
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title) 
    : BaseClass(parent, name) {
    
    SetPaintBackgroundEnabled(false);
    
    m_iMargin = PROPVAL(8); 

    // 1. 创建图片
    m_pImage = new vgui::ImagePanel(this, "ModImage");
    m_pImage->SetShouldScaleImage(true);
    m_pImage->SetImage("default_mod_preview"); 
    
    // 2. 创建文字 Label
    m_pTitle = new vgui::Label(this, "ModTitle", title);
    m_pTitle->SetPaintBackgroundEnabled(true);      // 修复：关闭背景色防止遮挡字体底部
    m_pTitle->SetFgColor(Color(255, 255, 255, 255)); // 纯白文字
    m_pTitle->SetContentAlignment(vgui::Label::a_center);

    // 3. 关键修复：增加文字区域高度
    int iImageSize = PROPVAL(120);
    int iLabelHeight = PROPVAL(32); // 修复：从 24 增加到 32，防止文字溢出被截断
    
    // 设定 Panel 总尺寸
    SetSize(iImageSize + m_iMargin, iImageSize + iLabelHeight + m_iMargin);
}

void ModCardPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);
    // 使用稍小的字体以确保能完全装进高度内
    m_pTitle->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
}

void ModCardPanel::PerformLayout() {
    BaseClass::PerformLayout();
    
    int w, h;
    GetSize(w, h);

    // 实际绘制内容的宽度（减去边距）
    int contentW = w - m_iMargin;
    
    // 顶部和左侧留出间距
    int drawX = m_iMargin / 2;
    int drawY = m_iMargin / 2;

    // 图片设置为 contentW * contentW (1:1)
    m_pImage->SetBounds(drawX, drawY, contentW, contentW);
    
    // 文字位置：从图片底部开始，并占据剩余的所有高度
    // 修复：确保 Label 的高度足够大，不使用固定值，而是计算剩余空间
    int labelY = drawY + contentW;
    int labelH = h - labelY - (m_iMargin / 2);
    m_pTitle->SetBounds(drawX, labelY, contentW, labelH);
}

// =========================================================
// ExtraListPage 实现 (宫格布局刷新)
// =========================================================
ExtraListPage::ExtraListPage(vgui::Panel *parent, const char *panelName) 
    : BaseClass(parent, panelName) {
    
    m_pModListPanel = new vgui::PanelListPanel(this, "ModListPanel");
    m_pModListPanel->SetFirstColumnWidth(0);
    m_pModListPanel->SetNumColumns(3); // 设置为3列
    
    // 设置垂直缓冲区（行间距）
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(8));

    // 生成测试卡片
    for (int i = 0; i < 20; i++) {
        char szTitle[64];
        Q_snprintf(szTitle, sizeof(szTitle), "MOD %02d", i + 1);
        
        ModCardPanel *pCard = new ModCardPanel(m_pModListPanel, "ModCard", szTitle);
        
        // 第一个参数设为 nullptr，直接把面板加进去
        m_pModListPanel->AddItem(nullptr, pCard);
    }
}

void ExtraListPage::PerformLayout() {
    BaseClass::PerformLayout();
    
    int w, h;
    GetSize(w, h);
    int margin = PROPVAL(6);

    if (m_pModListPanel) {
        m_pModListPanel->SetBounds(margin, margin, w - (margin * 2), h - (margin * 2));
    }
}

// =========================================================
// ExtraManagerPanel 实现
// =========================================================
ExtraManagerPanel::ExtraManagerPanel(vgui::Panel *parent)
    : BaseClass(parent, "ExtraManagerPanel") {
    
    int w = PROPVAL(512);
    int h = PROPVAL(420);
    SetSize(w, h);
    SetSizeable(false);
    SetTitle("Extra Manager", true);
    MoveToCenterOfScreen();

    m_pTabSheet = new PropertySheet(this, "ExtraTabs");
    m_pTabSheet->AddPage(new ExtraListPage(m_pTabSheet, "ExtraListPage"), "Installed Mods");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "Model Preview");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "Developers");

    m_pCloseButton = new Button(this, "CloseBtn", "Close", this, "Close");
}

void ExtraManagerPanel::PerformLayout() {
    BaseClass::PerformLayout();
    int wide, tall;
    GetSize(wide, tall);

    int margin = PROPVAL(10);
    int topGap = PROPVAL(36);
    int bottomArea = PROPVAL(40);

    if (m_pTabSheet) {
        m_pTabSheet->SetBounds(margin, topGap, wide - (margin * 2), tall - topGap - bottomArea);
    }
    
    if (m_pCloseButton) {
        int btnW = PROPVAL(80);
        int btnH = PROPVAL(24);
        m_pCloseButton->SetBounds(wide - margin - btnW, tall - margin - btnH, btnW, btnH);
    }
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