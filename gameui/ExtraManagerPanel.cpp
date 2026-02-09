#include "ExtraManagerPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "KeyValues.h"
#include "filesystem.h" 
#include "tier1/utlbuffer.h"

// STB 库宏定义
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_resize.h"

using namespace vgui;

extern IFileSystem *g_pFullFileSystem;

#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// =========================================================
// 版本维护数据结构
// =========================================================
struct VersionInfo_t {
    const char *szVersion;
    const char *szDescription;
};

static VersionInfo_t g_VersionData[] = {
    { "1.18.4",  "- Fixed GamePadUI alignment issues at high resolutions." },
    { "1.18.3",  " (2025/08/14)\n- Fixed GamePadUI tab misalignment.\n- Added support for PNG textures in Touch UI.\n- Performance optimizations." },
    { "1.18.0",  " (2025/01/26)\n- Fixed GamePadUI issues.\n- Added support for Entropy : Zero 2 mod.\n- Full support for PNG loading.\n- Integrated features from the HL2 20th Anniversary update." },
    { "1.17.26", " (2024/01/26)\n- Fixed smoke rendering and touch controls.\n- Fixed launcher issues for all ports.\n- Added GamePadUI support and touch grid color customization.\n- Enabled LTO (Link Time Optimization) for certain components." },
    { "1.17.25", " (2024/01/24)\n- Fixed crashes related to IsMapValid and spec_goto.\n- Resolved black screen and VSync issues after minimizing on Android.\n- Audio now runs in a separate thread.\n- Improved touch responsiveness." },
    { "1.16",    " (2023/02/17)\n- Fixed touch texture issues and maintained 64-bit stability.\n- Added multi-threaded optimizations for the material system.\n- Unlocked -tickrate parameter for CSS, TF, and DOD.\n- Added Discord, GitHub, and Telegram buttons to main menu." },
    { "1.14",    " (2022/09/19)\n- Fixed font issues for various languages and added Thai support.\n- Fixed touch button bugs (spawnmenu now works).\n- Fixed particle bugs in HL2." },
    { "1.13",    " (2022/09/17)\n- Ported to 64-bit (Fixes 'Out of Memory' on 4GB+ RAM devices).\n- Added PBR (Physically Based Rendering) and VTF 7.5 support.\n- Added Chinese, Japanese, and Korean font support.\n- Fixed players sticking to physical props." },
    { "1.09",    " (2022/03/02)\n- Fixed 'Black Textures' and all scenes in HL2 (Alyx, Dog, Eli).\n- Added voice recording with Opus codec support.\n- Fixed touch sensitivity in zoom (e.g., Crossbow)." }
};

// =========================================================
// ModCardPanel 实现
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title, int textureID) 
    : BaseClass(parent, name) {   
    m_nTextureID = textureID; // 修正：保存纹理ID

    SetPaintBackgroundEnabled(true);
    SetPaintBorderEnabled(false);
    SetMouseInputEnabled(true);
    m_iMargin = PROPVAL(6); 
    
    m_clrBgNormal = Color(0, 0, 0, 0);
    m_clrBgHover  = Color(89, 221, 242, 200);

    m_pImagePanelPlaceholder = new vgui::ImagePanel(this, "ModImage");
    m_pImagePanelPlaceholder->SetShouldScaleImage(true);    
    m_pImagePanelPlaceholder->SetMouseInputEnabled(false);
    m_pImagePanelPlaceholder->SetVisible(false); // 仅做坐标参考
    
    m_pTitle = new vgui::Label(this, "ModTitle", title);
    m_pTitle->SetPaintBackgroundEnabled(false);      
    m_pTitle->SetFgColor(Color(255, 255, 255, 255));
    m_pTitle->SetContentAlignment(vgui::Label::a_center);
    m_pTitle->SetMouseInputEnabled(false);

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
    int iMargin = PROPVAL(6);
    int contentW = w - iMargin;
    int drawX = iMargin / 2;
    int drawY = iMargin / 2;
    int imgSize = contentW; 
    if (m_nTextureID != -1 && vgui::surface()->IsTextureIDValid(m_nTextureID)) {
        vgui::surface()->DrawSetColor(255, 255, 255, 255);
        vgui::surface()->DrawSetTexture(m_nTextureID);
        vgui::surface()->DrawTexturedRect(drawX, drawY, drawX + imgSize, drawY + imgSize);
    } else {
        vgui::surface()->DrawSetColor(40, 40, 40, 255);
        vgui::surface()->DrawFilledRect(drawX, drawY, drawX + imgSize, drawY + imgSize);
    }
    int labelY = drawY + imgSize;
    int labelH = h - labelY - (iMargin / 2);
    vgui::surface()->DrawSetColor(0, 0, 0, 150);
    vgui::surface()->DrawFilledRect(drawX, labelY, drawX + contentW, labelY + labelH);
}

void ModCardPanel::PerformLayout() {
    BaseClass::PerformLayout();
    
    int w, h;
    GetSize(w, h);

    int iMargin = PROPVAL(6);
    int contentW = w - iMargin;
    int drawX = iMargin / 2;
    int drawY = iMargin / 2;
    m_pImagePanelPlaceholder->SetBounds(drawX, drawY, contentW, contentW);
    int imgX, imgY, imgW, imgH;
    m_pImagePanelPlaceholder->GetBounds(imgX, imgY, imgW, imgH);
  
    int labelY = imgH; 
    int labelH = PROPVAL(26); // 保持这样就好

    m_pTitle->SetBounds(drawX, labelY, contentW, labelH);
}


void ModCardPanel::OnCursorEntered() { SetBgColor(m_clrBgHover); }
void ModCardPanel::OnCursorExited() { SetBgColor(m_clrBgNormal); }

void ModCardPanel::OnMousePressed(vgui::MouseCode code) {
    if (code == MOUSE_LEFT) {
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
    m_pModListPanel->SetNumColumns(4); 
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(12));
}

ExtraListPage::~ExtraListPage() {
    CleanUpTextures();
}

void ExtraListPage::CleanUpTextures() {
    for (int i = 0; i < m_TextureIds.Count(); i++) {
        if (vgui::surface()->IsTextureIDValid(m_TextureIds[i])) {
            vgui::surface()->DeleteTextureByID(m_TextureIds[i]);
        }
    }
    m_TextureIds.RemoveAll();
}

int ExtraListPage::CreateTextureFromPNG(const char *fullPath) {
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(fullPath, "MOD", buf)) return -1;

    int width, height, channels;
    unsigned char *data = stbi_load_from_memory((unsigned char*)buf.Base(), buf.TellPut(), &width, &height, &channels, 4);
    if (!data) return -1;

    int targetW = 128; 
    int targetH = 128;
    unsigned char *resizedData = (unsigned char *)malloc(targetW * targetH * 4);
    if (!resizedData) {
        stbi_image_free(data);
        return -1;
    }

    if (!stbir_resize_uint8(data, width, height, width * 4, resizedData, targetW, targetH, targetW * 4, 4)) {
        stbi_image_free(data);
        free(resizedData);
        return -1;
    }

    int textureID = vgui::surface()->CreateNewTextureID(true);
    vgui::surface()->DrawSetTextureRGBA(textureID, resizedData, targetW, targetH, true, false);

    stbi_image_free(data);
    free(resizedData);
    return textureID;
}

void ExtraListPage::RefreshList() {
    m_pModListPanel->DeleteAllItems();
    CleanUpTextures();

    FileFindHandle_t findHandle;
    const char *pFileName = g_pFullFileSystem->FindFirst("custom/*", &findHandle);

    while (pFileName) {
        if (Q_strcmp(pFileName, ".") != 0 && Q_strcmp(pFileName, "..") != 0) {
            if (g_pFullFileSystem->FindIsDirectory(findHandle)) {
                char szIconPath[MAX_PATH];
                Q_snprintf(szIconPath, sizeof(szIconPath), "custom/%s/icon.png", pFileName);

                int textureID = -1;
                if (g_pFullFileSystem->FileExists(szIconPath, "MOD")) {
                    textureID = CreateTextureFromPNG(szIconPath);
                    if (textureID != -1) m_TextureIds.AddToTail(textureID);
                }

                ModCardPanel *pCard = new ModCardPanel(m_pModListPanel, pFileName, pFileName, textureID);
                vgui::Panel *pTarget = GetParent();
                while (pTarget && !dynamic_cast<ExtraManagerPanel*>(pTarget)) {
                    pTarget = pTarget->GetParent();
                }
                if (pTarget) pCard->AddActionSignalTarget(pTarget);
                
                m_pModListPanel->AddItem(nullptr, pCard);
            }
        }
        pFileName = g_pFullFileSystem->FindNext(findHandle);
    }
    g_pFullFileSystem->FindClose(findHandle);
}

void ExtraListPage::PerformLayout() {
    BaseClass::PerformLayout();
    int w, h;
    GetSize(w, h);
    int margin = PROPVAL(8);
    m_pModListPanel->SetBounds(margin, margin, w - (margin * 2), h - (margin * 2));
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
    m_pTabSheet = new PropertySheet(m_pLeftPanel, "ExtraTabs");
    m_pModListPage = new ExtraListPage(m_pTabSheet, "ExtraListPage");
    m_pTabSheet->AddPage(m_pModListPage, "mods");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "preview items");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "developers");

    m_pRightPanel = new vgui::EditablePanel(this, "RightFloatingPanel");
    m_pDetailsLabel = new vgui::Label(m_pRightPanel, "DetailsLabel", "Information");
    m_pVersionTitleLabel = new vgui::Label(m_pRightPanel, "VersionTitleLabel", "Watch what new:");
    m_pDescriptionText = new vgui::RichText(m_pRightPanel, "DescriptionText");
    m_pVersionCombo = new vgui::ComboBox(m_pRightPanel, "VersionCombo", 6, false);
    m_pVersionCombo->AddActionSignalTarget(this);

    InitVersionCombo();

    m_pRefreshButton = new vgui::Button(m_pRightPanel, "RefreshBtn", "#GameUI_Refresh", this, "RefreshList");
    m_pCloseButton = new Button(this, "CloseBtn", "#GameUI_Close", this, "Close");

    m_pVersionCombo->ActivateItemByRow(0);
}

void ExtraManagerPanel::OnModCardSelected( KeyValues *data ) {
    if ( !data ) return;
    const char *pPanelName = data->GetString( "panelName", "" );
    if ( m_pDescriptionText ) {
        m_pDescriptionText->SetText( "" );
        m_pDescriptionText->InsertColorChange( Color( 255, 255, 255, 255 ) );
        m_pDescriptionText->InsertString( "Selected Mod: " );
        m_pDescriptionText->InsertString( pPanelName );
    }
}

void ExtraManagerPanel::InitVersionCombo() {
    for (int i = 0; i < (int)ARRAYSIZE(g_VersionData); i++) {
        m_pVersionCombo->AddItem(g_VersionData[i].szVersion, nullptr);
    }
}

void ExtraManagerPanel::OnVersionSelected(vgui::Panel *panel) {
    if (panel == m_pVersionCombo) {
        char szText[64];
        m_pVersionCombo->GetText(szText, sizeof(szText));
        m_pDescriptionText->SetText(""); 
        m_pDescriptionText->InsertColorChange(Color(255, 210, 0, 255));
        m_pDescriptionText->InsertString("version ");
        m_pDescriptionText->InsertString(szText);
        m_pDescriptionText->InsertString(" feature:\n\n");
        m_pDescriptionText->InsertColorChange(Color(255, 255, 255, 255));

        for (int i = 0; i < (int)ARRAYSIZE(g_VersionData); i++) {
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
    int iPadding = PROPVAL(20), iGap = PROPVAL(20);       
    int leftW = (sw * 0.65) - (iPadding + iGap / 2);
    int rightW = sw - leftW - (iPadding * 2) - iGap;
    int panelH = sh - (iPadding * 2);

    m_pLeftPanel->SetBounds(iPadding, iPadding, leftW, panelH);
    m_pRightPanel->SetBounds(iPadding + leftW + iGap, iPadding, rightW, panelH);

    int tPadding = PROPVAL(12);
    m_pTabSheet->SetBounds(tPadding, tPadding, leftW - (tPadding * 2), panelH - (tPadding * 2));
    
    int rInnerPad = PROPVAL(15), currentY = rInnerPad;
    m_pDetailsLabel->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(30));
    currentY += PROPVAL(45);
    m_pVersionTitleLabel->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(15));
    currentY += PROPVAL(20);
    m_pVersionCombo->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), PROPVAL(24));
    currentY += PROPVAL(35);
    m_pDescriptionText->SetBounds(rInnerPad, currentY, rightW - (rInnerPad * 2), panelH / 2.2);

    int btnW = PROPVAL(90), btnH = PROPVAL(24); 
    int btnY = panelH - rInnerPad - btnH;
    m_pRefreshButton->SetBounds(rInnerPad, btnY, btnW + PROPVAL(20), btnH); 
    m_pCloseButton->SetBounds(sw - iPadding - rInnerPad - btnW, sh - iPadding - rInnerPad - btnH, btnW, btnH);
}

void ExtraManagerPanel::OnCommand(const char *command) {
    if (!Q_stricmp(command, "Close")) Close();
    else if (!Q_stricmp(command, "RefreshList") && m_pModListPage) m_pModListPage->RefreshList();
    else BaseClass::OnCommand(command);
}

void ExtraManagerPanel::Activate() {
    BaseClass::Activate();
    if (m_pModListPage) m_pModListPage->RefreshList();
}

void ExtraManagerPanel::OnClose() {
    BaseClass::OnClose();
    MarkForDeletion();
}