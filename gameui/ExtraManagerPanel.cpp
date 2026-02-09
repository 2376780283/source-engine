#include "ExtraManagerPanel.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "tier1/checksum_crc.h" // 用于路径哈希
#include "tier1/utlbuffer.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

// STB 库实现
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_resize.h"

using namespace vgui;

extern IFileSystem *g_pFullFileSystem;

#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// ========
// 辅助函数：加载 PNG 并返回 TextureID
// ========
static int CreatePNGTextureHelper(const char *szPath) {
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(szPath, "MOD", buf)) return -1;

    int width, height, channels;
    unsigned char *data = stbi_load_from_memory((unsigned char *)buf.Base(), buf.TellPut(), &width, &height, &channels, 4);
    if (!data) return -1;

    int targetW = 128; // 统一缩放大小
    int targetH = 128;
    unsigned char *resizedData = (unsigned char *)malloc(targetW * targetH * 4);
    int textureID = -1;

    if (resizedData) {
        if (stbir_resize_uint8(data, width, height, width * 4, resizedData, targetW, targetH, targetW * 4, 4)) {
            textureID = vgui::surface()->CreateNewTextureID(true);
            vgui::surface()->DrawSetTextureRGBA(textureID, resizedData, targetW, targetH, true, false);
        }
        free(resizedData);
    }

    stbi_image_free(data);
    return textureID;
}

// =========================================================
// 版本维护数据
// =========================================================
struct VersionInfo_t {
    const char *szVersion;
    const char *szDescription;
};

static VersionInfo_t g_VersionData[] = {
    {"1.18.4", "- Fixed GamePadUI alignment issues at high resolutions."},
    {"1.18.3", " (2025/08/14)\n- Fixed GamePadUI tab misalignment.\n- Added support for PNG textures in Touch UI.\n- Performance optimizations."},
    {"1.18.0", " (2025/01/26)\n- Fixed GamePadUI issues.\n- Added support for Entropy : Zero 2 mod.\n- Full support for PNG loading.\n- Integrated features "
               "from the HL2 20th Anniversary update."},
    {"1.17.26", " (2024/01/26)\n- Fixed smoke rendering and touch controls.\n- Fixed launcher issues for all ports.\n- Added GamePadUI support and touch grid "
                "color customization.\n- Enabled LTO (Link Time Optimization) for certain components."},
    {"1.17.25", " (2024/01/24)\n- Fixed crashes related to IsMapValid and spec_goto.\n- Resolved black screen and VSync issues after minimizing on Android.\n- "
                "Audio now runs in a separate thread.\n- Improved touch responsiveness."},
    {"1.16", " (2023/02/17)\n- Fixed touch texture issues and maintained 64-bit stability.\n- Added multi-threaded optimizations for the material system.\n- "
             "Unlocked -tickrate parameter for CSS, TF, and DOD.\n- Added Discord, GitHub, and Telegram buttons to main menu."},
    {"1.14", " (2022/09/19)\n- Fixed font issues for various languages and added Thai support.\n- Fixed touch button bugs (spawnmenu now works).\n- Fixed "
             "particle bugs in HL2."},
    {"1.13", " (2022/09/17)\n- Ported to 64-bit (Fixes 'Out of Memory' on 4GB+ RAM devices).\n- Added PBR (Physically Based Rendering) and VTF 7.5 support.\n- "
             "Added Chinese, Japanese, and Korean font support.\n- Fixed players sticking to physical props."},
    {"1.09", " (2022/03/02)\n- Fixed 'Black Textures' and all scenes in HL2 (Alyx, Dog, Eli).\n- Added voice recording with Opus codec support.\n- Fixed touch "
             "sensitivity in zoom (e.g., Crossbow)."}};

// =========================================================
// ModCardPanel 实现 (含延迟加载逻辑)
// =========================================================
ModCardPanel::ModCardPanel(vgui::Panel *parent, const char *name, const char *title) : BaseClass(parent, name) {
    m_nTextureID = -1;
    m_bAttemptedLoad = false;
    m_szImagePath[0] = '\0';

    SetPaintBackgroundEnabled(true);
    SetPaintBorderEnabled(false);
    SetMouseInputEnabled(true);
    m_iMargin = PROPVAL(6);

    m_clrBgNormal = Color(0, 0, 0, 0);
    m_clrBgHover = Color(89, 221, 242, 200);

    m_pImagePanelPlaceholder = new vgui::ImagePanel(this, "ModImage");
    m_pImagePanelPlaceholder->SetShouldScaleImage(true);
    m_pImagePanelPlaceholder->SetMouseInputEnabled(false);
    m_pImagePanelPlaceholder->SetVisible(false);

    m_pTitle = new vgui::Label(this, "ModTitle", title);
    m_pTitle->SetPaintBackgroundEnabled(false);
    m_pTitle->SetFgColor(Color(255, 255, 255, 255));
    m_pTitle->SetContentAlignment(vgui::Label::a_center);
    m_pTitle->SetMouseInputEnabled(false);

    int iImageSize = PROPVAL(120);
    int iLabelHeight = PROPVAL(36);
    SetSize(iImageSize + m_iMargin, iImageSize + iLabelHeight + m_iMargin);
}

void ModCardPanel::SetImagePath(const char *path) {
    if (path) { Q_strncpy(m_szImagePath, path, sizeof(m_szImagePath)); }
}

void ModCardPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);
    m_pTitle->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
}

void ModCardPanel::Paint() {
    BaseClass::Paint();

    // --- 性能优化：延迟加载逻辑 ---
    if (m_nTextureID == -1 && !m_bAttemptedLoad && m_szImagePath[0] != '\0') {
        // 向上寻找 ExtraListPage 以调用其缓存加载器
        vgui::Panel *pPage = GetParent();
        while (pPage && !dynamic_cast<ExtraListPage *>(pPage)) { pPage = pPage->GetParent(); }

        if (pPage) {
            ExtraListPage *pListPage = static_cast<ExtraListPage *>(pPage);
            m_nTextureID = pListPage->GetTextureForPath(m_szImagePath);
            m_bAttemptedLoad = true; // 无论成功失败，只尝试一次，避免每帧磁盘访问
        }
    }

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
        // 加载中或无图：绘制深灰色占位背景
        vgui::surface()->DrawSetColor(30, 30, 30, 255);
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

void ModCardPanel::OnCursorEntered() {
    SetBgColor(m_clrBgHover);
}
void ModCardPanel::OnCursorExited() {
    SetBgColor(m_clrBgNormal);
}

void ModCardPanel::OnMousePressed(vgui::MouseCode code) {
    if (code == MOUSE_LEFT) { PostActionSignal(new KeyValues("ModCardSelected", "panelName", GetName())); }
}

// =========================================================
// DevItemPanel 实现
// =========================================================
DevItemPanel::DevItemPanel(vgui::Panel *parent, const char *name, const char *nick, const char *desc, int nTextureID) : BaseClass(parent, name) {
    m_nTextureID = nTextureID;
    
    SetPaintBackgroundEnabled(true);
    SetBgColor(Color(0, 0, 0, 100));

    // 依然保留图标容器位置，但不使用 ImagePanel 的图片加载功能
    m_pIcon = new vgui::ImagePanel(this, "DevIcon");
    m_pIcon->SetShouldScaleImage(true);
    m_pIcon->SetVisible(false); // 隐藏它，我们自己在 Paint 里画

    m_pNameLabel = new vgui::Label(this, "DevName", nick);
    m_pNameLabel->SetFgColor(Color(255, 210, 0, 255));

    m_pDescLabel = new vgui::Label(this, "DevDesc", desc);
    m_pDescLabel->SetFgColor(Color(200, 200, 200, 255));
    m_pDescLabel->SetContentAlignment(vgui::Label::a_northwest);

    SetSize(PROPVAL(300), PROPVAL(64));
}

void DevItemPanel::Paint() {
    BaseClass::Paint();

    // 手动绘制 PNG 头像
    if (m_nTextureID != -1 && vgui::surface()->IsTextureIDValid(m_nTextureID)) {
        int ix, iy, iw, ih;
        m_pIcon->GetBounds(ix, iy, iw, ih);
        
        vgui::surface()->DrawSetColor(255, 255, 255, 255);
        vgui::surface()->DrawSetTexture(m_nTextureID);
        vgui::surface()->DrawTexturedRect(ix, iy, ix + iw, iy + ih);
    }

    // 绘制底部装饰线
    vgui::surface()->DrawSetColor(255, 255, 255, 10);
    vgui::surface()->DrawFilledRect(0, GetTall() - 1, GetWide(), GetTall());
}

void DevItemPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);
    m_pNameLabel->SetFont(pScheme->GetFont("DefaultBold", IsProportional()));
    m_pDescLabel->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
}

void DevItemPanel::PerformLayout() {
    BaseClass::PerformLayout();
    int w, h;
    GetSize(w, h);
    int iPadding = PROPVAL(8);
    int iIconSize = h - (iPadding * 2);    
    m_pIcon->SetBounds(iPadding, iPadding, iIconSize, iIconSize);    
    int iTextX = iPadding * 2 + iIconSize;
    int iTextW = w - iTextX - iPadding;
    m_pNameLabel->SetBounds(iTextX, iPadding, iTextW, PROPVAL(20));
    m_pDescLabel->SetBounds(iTextX, iPadding + PROPVAL(22), iTextW, h - iPadding * 2 - PROPVAL(22));
}

// =========================================================
// DevPage 实现
// =========================================================
DevPage::DevPage(vgui::Panel *parent, const char *panelName) : BaseClass(parent, panelName) {
    m_pDevList = new vgui::PanelListPanel(this, "DevList");
    m_pDevList->SetFirstColumnWidth(0);

    PopulateDevList();
}

void DevPage::PopulateDevList() {
    m_pDevList->DeleteAllItems();
    
    struct DevData_t {
        const char *name;
        const char *desc;
        const char *iconPath;
    };

    DevData_t devs[] = {
        {"nillerusr", "port leader", "vgui/social/gabe.png"},
        {"er2", "programming", "vgui/social/my_avatar.png"},
        {"itz", "programming", "vgui/social/default_dev.png"}
        {"zzh", "programming", "vgui/social/default_dev.png"}
    };

    for (int i = 0; i < ARRAYSIZE(devs); i++) {
        // 调用我们刚刚定义的 Helper
        int textureID = CreatePNGTextureHelper(devs[i].iconPath);

        // 使用更新后的构造函数
        DevItemPanel *pItem = new DevItemPanel(m_pDevList, "dev_item", devs[i].name, devs[i].desc, textureID);
        m_pDevList->AddItem(nullptr, pItem);
    }
}

void DevPage::PerformLayout() {
    BaseClass::PerformLayout();
    int w, h;
    GetSize(w, h);
    int margin = PROPVAL(10);
    int listW = w * 0.7;
    m_pDevList->SetBounds(margin, margin, listW, h - (margin * 2));
}

// =========================================================
// ExtraListPage 实现 (含纹理缓存)
// =========================================================
ExtraListPage::ExtraListPage(vgui::Panel *parent, const char *panelName) : BaseClass(parent, panelName) {
    m_pModListPanel = new vgui::PanelListPanel(this, "ModListPanel");
    m_pModListPanel->SetFirstColumnWidth(0);
    m_pModListPanel->SetNumColumns(4);
    m_pModListPanel->SetVerticalBufferPixels(PROPVAL(12));

    m_TextureCache.SetLessFunc(DefLessFunc(unsigned int));
}

ExtraListPage::~ExtraListPage() {
    CleanUpTextures();
}

void ExtraListPage::CleanUpTextures() {
    FOR_EACH_MAP(m_TextureCache, i) {
        int id = m_TextureCache[i];
        if (vgui::surface()->IsTextureIDValid(id)) { vgui::surface()->DeleteTextureByID(id); }
    }
    m_TextureCache.RemoveAll();
}

int ExtraListPage::GetTextureForPath(const char *fullPath) {
    if (!fullPath || !fullPath[0]) return -1;

    // 使用 CRC 计算路径哈希作为 Key
    CRC32_t hash;
    CRC32_Init(&hash);
    CRC32_ProcessBuffer(&hash, fullPath, Q_strlen(fullPath));
    CRC32_Final(&hash);

    int index = m_TextureCache.Find(hash);
    if (index != m_TextureCache.InvalidIndex()) { return m_TextureCache[index]; }

    // 缓存中没有，执行实时加载
    int newID = CreateTextureFromPNG(fullPath);
    if (newID != -1) { m_TextureCache.Insert(hash, newID); }
    return newID;
}

int ExtraListPage::CreateTextureFromPNG(const char *fullPath) {
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(fullPath, "MOD", buf)) return -1;

    int width, height, channels;
    // 使用 stb_image 解码
    unsigned char *data = stbi_load_from_memory((unsigned char *)buf.Base(), buf.TellPut(), &width, &height, &channels, 4);
    if (!data) return -1;

    // 性能优化：统一缩放到 128x128 节省显存
    int targetW = 128;
    int targetH = 128;
    unsigned char *resizedData = (unsigned char *)malloc(targetW * targetH * 4);
    if (resizedData) {
        if (stbir_resize_uint8(data, width, height, width * 4, resizedData, targetW, targetH, targetW * 4, 4)) {
            int textureID = vgui::surface()->CreateNewTextureID(true);
            vgui::surface()->DrawSetTextureRGBA(textureID, resizedData, targetW, targetH, true, false);
            stbi_image_free(data);
            free(resizedData);
            return textureID;
        }
        free(resizedData);
    }

    stbi_image_free(data);
    return -1;
}

void ExtraListPage::RefreshList() {
    m_pModListPanel->DeleteAllItems();
    // 注意：此处不主动 CleanUpTextures 以保持缓存。
    // 如果需要强制刷新物理资源，可手动调用 CleanUpTextures。

    FileFindHandle_t findHandle;
    const char *pFileName = g_pFullFileSystem->FindFirst("custom/*", &findHandle);

    while (pFileName) {
        if (Q_strcmp(pFileName, ".") != 0 && Q_strcmp(pFileName, "..") != 0) {
            if (g_pFullFileSystem->FindIsDirectory(findHandle)) {
                char szIconPath[MAX_PATH];
                Q_snprintf(szIconPath, sizeof(szIconPath), "custom/%s/icon.png", pFileName);

                // 只创建面板，不在此处加载图片 I/O
                ModCardPanel *pCard = new ModCardPanel(m_pModListPanel, pFileName, pFileName);
                if (g_pFullFileSystem->FileExists(szIconPath, "MOD")) { pCard->SetImagePath(szIconPath); }

                vgui::Panel *pTarget = GetParent();
                while (pTarget && !dynamic_cast<ExtraManagerPanel *>(pTarget)) { pTarget = pTarget->GetParent(); }
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
ExtraManagerPanel::ExtraManagerPanel(vgui::Panel *parent) : BaseClass(parent, "ExtraManagerPanel") {

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

    m_pTabSheet->AddPage(m_pModListPage, "MODS");
    m_pTabSheet->AddPage(new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage"), "PREVIEW");
    m_pTabSheet->AddPage(new DevPage(m_pTabSheet, "DevPage"), "CREDITS");

    m_pRightPanel = new vgui::EditablePanel(this, "RightFloatingPanel");
    m_pDetailsLabel = new vgui::Label(m_pRightPanel, "DetailsLabel", "Information");
    m_pVersionTitleLabel = new vgui::Label(m_pRightPanel, "VersionTitleLabel", "Update History:");
    m_pDescriptionText = new vgui::RichText(m_pRightPanel, "DescriptionText");
    m_pVersionCombo = new vgui::ComboBox(m_pRightPanel, "VersionCombo", 6, false);
    m_pVersionCombo->AddActionSignalTarget(this);

    InitVersionCombo();

    m_pRefreshButton = new vgui::Button(m_pRightPanel, "RefreshBtn", "#GameUI_Refresh", this, "RefreshList");
    m_pCloseButton = new Button(this, "CloseBtn", "#GameUI_Close", this, "Close");

    m_pVersionCombo->ActivateItemByRow(0);
}

void ExtraManagerPanel::OnModCardSelected(KeyValues *data) {
    if (!data) return;
    const char *pPanelName = data->GetString("panelName", "");
    if (m_pDescriptionText) {
        m_pDescriptionText->SetText("");
        m_pDescriptionText->InsertColorChange(Color(0, 255, 128, 255));
        m_pDescriptionText->InsertString(">>> SELECTED MOD: ");
        m_pDescriptionText->InsertString(pPanelName);
        m_pDescriptionText->InsertString("\n\nStatus: Locally installed.");
    }
}

void ExtraManagerPanel::InitVersionCombo() {
    for (int i = 0; i < (int)ARRAYSIZE(g_VersionData); i++) { m_pVersionCombo->AddItem(g_VersionData[i].szVersion, nullptr); }
}

void ExtraManagerPanel::OnVersionSelected(vgui::Panel *panel) {
    if (panel == m_pVersionCombo) {
        char szText[64];
        m_pVersionCombo->GetText(szText, sizeof(szText));
        m_pDescriptionText->SetText("");
        m_pDescriptionText->InsertColorChange(Color(255, 210, 0, 255));
        m_pDescriptionText->InsertString("Version ");
        m_pDescriptionText->InsertString(szText);
        m_pDescriptionText->InsertString(" Features:\n\n");
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

    int btnW = PROPVAL(110), btnH = PROPVAL(28);
    int btnY = panelH - rInnerPad - btnH;
    m_pRefreshButton->SetBounds(rInnerPad, btnY, btnW, btnH);
    m_pCloseButton->SetBounds(sw - iPadding - rInnerPad - btnW, sh - iPadding - rInnerPad - btnH, btnW, btnH);
}

void ExtraManagerPanel::OnCommand(const char *command) {
    if (!Q_stricmp(command, "Close"))
        Close();
    else if (!Q_stricmp(command, "RefreshList") && m_pModListPage)
        m_pModListPage->RefreshList();
    else
        BaseClass::OnCommand(command);
}

void ExtraManagerPanel::Activate() {
    BaseClass::Activate();
    if (m_pModListPage) m_pModListPage->RefreshList();
}

void ExtraManagerPanel::OnClose() {
    BaseClass::OnClose();
    MarkForDeletion();
}