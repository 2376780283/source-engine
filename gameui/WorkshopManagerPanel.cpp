// WorkshopManagerPanel.cpp
#include "WorkshopManagerPanel.h"

#include "BasePanel.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "filesystem.h"
#include "tier0/memdbgon.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/TextEntry.h"

// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../thirdparty/stb/stb_image.h"
#include "../thirdparty/stb/stb_image_resize.h"

using namespace vgui;

extern IFileSystem *g_pFullFileSystem;

class ImagePanelPNG : public vgui::Panel {
    DECLARE_CLASS_SIMPLE(ImagePanelPNG, vgui::Panel);

public:
    enum DisplayMode {
        DISPLAY_CENTER,   // 居中保持比例
        DISPLAY_STRETCH,  // 拉伸填充
        DISPLAY_COVER     // 覆盖填充（保持比例但可能裁剪）
    };

    ImagePanelPNG(vgui::Panel *parent, const char *name, const char *pngPath, DisplayMode mode = DISPLAY_CENTER)
        : vgui::Panel(parent, name),
          m_textureID(-1),
          m_imgWidth(0),
          m_imgHeight(0),
          m_u0(0.0f),
          m_v0(0.0f),
          m_u1(1.0f),
          m_v1(1.0f),
          m_displayMode(mode) {
        LoadPNG(pngPath);
    }

    ~ImagePanelPNG() override {
        if (m_textureID != -1) {
            surface()->DestroyTextureID(m_textureID);
            m_textureID = -1;
        }
    }

    void SetDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    DisplayMode GetDisplayMode() const { return m_displayMode; }

    virtual void Paint() override {
        if (m_textureID == -1)
            return;

        int panelW, panelH;
        GetSize(panelW, panelH);

        surface()->DrawSetColor(255, 255, 255, 255);
        surface()->DrawSetTexture(m_textureID);

        int x = 0, y = 0;
        int drawW = panelW, drawH = panelH;

        switch (m_displayMode) {
            case DISPLAY_CENTER: {
                float panelAspect = (float)panelW / (float)panelH;
                float imgAspect = (float)m_imgWidth / (float)m_imgHeight;
                if (imgAspect > panelAspect) {
                    drawH = (int)((float)panelW / imgAspect);
                    y = (panelH - drawH) / 2;
                } else if (imgAspect < panelAspect) {
                    drawW = (int)((float)panelH * imgAspect);
                    x = (panelW - drawW) / 2;
                }
                break;
            }

            case DISPLAY_STRETCH:
                // 默认行为：拉伸填满
                break;

            case DISPLAY_COVER: {
                float panelAspect = (float)panelW / (float)panelH;
                float imgAspect = (float)m_imgWidth / (float)m_imgHeight;
                if (imgAspect > panelAspect) {
                    drawW = (int)((float)panelH * imgAspect);
                    x = (panelW - drawW) / 2;
                } else if (imgAspect < panelAspect) {
                    drawH = (int)((float)panelW / imgAspect);
                    y = (panelH - drawH) / 2;
                }
                break;
            }
        }

        // 使用 UV 修正的绘制
        surface()->DrawTexturedSubRect(x, y, x + drawW, y + drawH, m_u0, m_v0, m_u1, m_v1);
    }

private:
    void LoadPNG(const char *path) {
        char resolved[MAX_PATH];
        Q_strncpy(resolved, path, sizeof(resolved));
        if (!Q_stristr(resolved, "materials/"))
            Q_snprintf(resolved, sizeof(resolved), "materials/%s", path);

        FileHandle_t f = g_pFullFileSystem->Open(resolved, "rb");
        if (!f) {
            Warning("[ImagePanelPNG] Cannot open: %s\n", resolved);
            return;
        }

        int fileSize = g_pFullFileSystem->Size(f);
        CUtlMemory<unsigned char> buffer(0, fileSize);
        g_pFullFileSystem->Read(buffer.Base(), fileSize, f);
        g_pFullFileSystem->Close(f);

        int channels = 0;
        unsigned char *raw = stbi_load_from_memory(buffer.Base(), fileSize, &m_imgWidth, &m_imgHeight, &channels, STBI_rgb_alpha);
        if (!raw) {
            Warning("[ImagePanelPNG] Failed to decode PNG: %s\n", resolved);
            return;
        }

        const int maxSize = 4080;
        int outW = m_imgWidth, outH = m_imgHeight;
        if (outW > maxSize || outH > maxSize) {
            float scale = min((float)maxSize / outW, (float)maxSize / outH);
            outW = (int)(outW * scale);
            outH = (int)(outH * scale);
        }

        unsigned char *resized = new unsigned char[outW * outH * 4];
        stbir_resize_uint8(raw, m_imgWidth, m_imgHeight, 0, resized, outW, outH, 0, 4);

        m_textureID = surface()->CreateNewTextureID(true);
        surface()->DrawSetTextureRGBA(m_textureID, resized, outW, outH, false, false);

        m_imgWidth = outW;
        m_imgHeight = outH;

        delete[] resized;
        stbi_image_free(raw);

        // ✅ 半像素 UV 修正
        float texelU = 0.5f / (float)m_imgWidth;
        float texelV = 0.5f / (float)m_imgHeight;
        m_u0 = texelU;
        m_v0 = texelV;
        m_u1 = 1.0f - texelU;
        m_v1 = 1.0f - texelV;

        Warning("[ImagePanelPNG] Texture %s loaded: %dx%d (UV fixed)\n", resolved, outW, outH);
    }

private:
    int m_textureID;
    int m_imgWidth, m_imgHeight;
    float m_u0, m_v0, m_u1, m_v1;
    DisplayMode m_displayMode;
};

// ---------------- WorkshopListPage -----------------
WorkshopListPage::WorkshopListPage(Panel *parent, const char *panelName)
    : BaseClass(parent, panelName) {
    m_pFilterLabel = new Label(this, "FilterLabel", "Filter:");
    m_pSearchBox = new TextEntry(this, "SearchBox");
    m_pCategoryBox = new ComboBox(this, "CategoryBox", 5, false);
    m_pRefreshButton = new Button(this, "RefreshBtn", "Refresh", this, "refresh");

    m_pCategoryBox->AddItem("All", nullptr);
    m_pCategoryBox->AddItem("Maps", nullptr);
    m_pCategoryBox->AddItem("Models", nullptr);
    m_pCategoryBox->AddItem("Other", nullptr);

    m_pFolderList = new ListPanel(this, "FolderList");
    m_pFolderList->AddColumnHeader(0, "name", "Folder Name", 250);
    m_pFolderList->AddColumnHeader(1, "path", "Full Path", 370);

    PopulateFolderList();
}

void WorkshopListPage::PerformLayout() {
    BaseClass::PerformLayout();
    int wide, tall;
    GetSize(wide, tall);

    const int margin = 10;
    const int headerHeight = 30;
    const int buttonWidth = 80;
    const int inputHeight = 24;

    m_pFilterLabel->SetBounds(margin, margin + 2, 45, inputHeight);
    m_pSearchBox->SetBounds(margin + 50, margin, 200, inputHeight);
    m_pCategoryBox->SetBounds(margin + 260, margin, 150, inputHeight);
    m_pRefreshButton->SetBounds(wide - margin - buttonWidth, margin, buttonWidth, inputHeight);
    m_pFolderList->SetBounds(margin, margin + headerHeight, wide - 2 * margin, tall - headerHeight - 2 * margin);
}

void WorkshopListPage::PopulateFolderList() {
    if (!m_pFolderList) return;

    m_pFolderList->RemoveAll();
    const char *gameDir = engine->GetGameDirectory();
    if (!gameDir || !gameDir[0]) return;

    char searchPath[MAX_PATH];
    FileFindHandle_t findHandle;

    // custom/
    Q_snprintf(searchPath, sizeof(searchPath), "%s/custom/*", gameDir);
    const char *pFileName = g_pFullFileSystem->FindFirst(searchPath, &findHandle);
    while (pFileName) {
        if (Q_strcmp(pFileName, ".") && Q_strcmp(pFileName, "..")) {
            char fullPath[MAX_PATH];
            Q_snprintf(fullPath, sizeof(fullPath), "%s/custom/%s", gameDir, pFileName);

            if (g_pFullFileSystem->IsDirectory(fullPath)) {
                KeyValues *kv = new KeyValues("item");
                kv->SetString("name", pFileName);
                kv->SetString("path", fullPath);
                m_pFolderList->AddItem(kv, 0, false, false);
                kv->deleteThis();
            }
        }
        pFileName = g_pFullFileSystem->FindNext(findHandle);
    }
    g_pFullFileSystem->FindClose(findHandle);

    // workshop content
    Q_snprintf(searchPath, sizeof(searchPath), "%s/../../../workshop/content/1583720/*", gameDir);
    pFileName = g_pFullFileSystem->FindFirst(searchPath, &findHandle);
    while (pFileName) {
        if (Q_strcmp(pFileName, ".") && Q_strcmp(pFileName, "..")) {
            char fullPath[MAX_PATH];
            Q_snprintf(fullPath, sizeof(fullPath), "%s/../../../workshop/content/1583720/%s", gameDir, pFileName);

            if (g_pFullFileSystem->IsDirectory(fullPath)) {
                KeyValues *kv = new KeyValues("item");
                kv->SetString("name", pFileName);
                kv->SetString("path", fullPath);
                m_pFolderList->AddItem(kv, 0, false, false);
                kv->deleteThis();
            }
        }
        pFileName = g_pFullFileSystem->FindNext(findHandle);
    }
    g_pFullFileSystem->FindClose(findHandle);
}

void WorkshopListPage::OnCommand(const char *command) {
    if (!Q_stricmp(command, "refresh"))
        PopulateFolderList();
    else
        BaseClass::OnCommand(command);
}

// ---------------- MDLViewerPanel -----------------
class MDLViewerPanel : public vgui::Panel {
    DECLARE_CLASS_SIMPLE(MDLViewerPanel, vgui::Panel);

   public:
    MDLViewerPanel(vgui::Panel *parent, const char *name) : BaseClass(parent, name) {
        m_pModelPath = nullptr;
        SetPaintBackgroundEnabled(true);
        SetBgColor(Color(50, 50, 50, 255));
    }

    void SetModel(const char *modelPath) {
        m_pModelPath = modelPath;
        Repaint();
    }

    virtual void Paint() override {
        BaseClass::Paint();
        if (m_pModelPath) DevMsg("Rendering model: %s\n", m_pModelPath);
    }

   private:
    const char *m_pModelPath;
};

// ---------------- ModelPreviewPage -----------------
class ModelPreviewPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(ModelPreviewPage, vgui::PropertyPage);

   public:
    ModelPreviewPage(vgui::Panel *parent, const char *panelName)
        : BaseClass(parent, panelName) {
        m_pModelList = new ListPanel(this, "ModelList");
        m_pModelList->AddColumnHeader(0, "name", "Model Name", 200);

        m_pPreviewButton = new Button(this, "PreviewBtn", "Preview", this, "preview");
        m_pOtherButton = new Button(this, "OtherBtn", "???", this, "other");

        PopulateModelList();
    }

    virtual void PerformLayout() override {
        BaseClass::PerformLayout();
        int wide, tall;
        GetSize(wide, tall);
        const int margin = 10;
        const int buttonHeight = 24;
        const int rightWidth = 300;

        m_pModelList->SetBounds(margin, margin, wide - rightWidth - 3 * margin, tall - 2 * margin);

        m_pPreviewButton->SetBounds(wide - rightWidth - margin, tall - margin - buttonHeight, 140, buttonHeight);
        m_pOtherButton->SetBounds(wide - rightWidth - margin + 160, tall - margin - buttonHeight, 140, buttonHeight);
    }

    virtual void OnCommand(const char *command) override {
        if (!Q_stricmp(command, "preview"))
            PreviewSelectedModel();
        else if (!Q_stricmp(command, "other"))
            return;
        else
            BaseClass::OnCommand(command);
    }

   private:
    ListPanel *m_pModelList;
    MDLViewerPanel *m_pMDLViewer;
    Button *m_pPreviewButton;
    Button *m_pOtherButton;

    void PopulateModelList() {
        m_pModelList->RemoveAll();
        const char *gameDir = engine->GetGameDirectory();
        if (!gameDir || !gameDir[0]) return;

        char searchPath[MAX_PATH];
        FileFindHandle_t findHandle;

        Q_snprintf(searchPath, sizeof(searchPath), "%s/models/*.mdl", gameDir);
        const char *pFileName = g_pFullFileSystem->FindFirst(searchPath, &findHandle);
        while (pFileName) {
            if (Q_strcmp(pFileName, ".") && Q_strcmp(pFileName, "..")) {
                KeyValues *kv = new KeyValues("item");
                kv->SetString("name", pFileName);
                m_pModelList->AddItem(kv, 0, false, false);
                kv->deleteThis();
            }
            pFileName = g_pFullFileSystem->FindNext(findHandle);
        }
        g_pFullFileSystem->FindClose(findHandle);
    }

    void PreviewSelectedModel() {
        int selected = m_pModelList->GetSelectedItem(0);
        if (selected < 0) return;

        KeyValues *kv = m_pModelList->GetItem(selected);
        const char *modelName = kv->GetString("name");

        char modelPath[MAX_PATH];
        Q_snprintf(modelPath, sizeof(modelPath), "models/%s", modelName);

        MDLViewerPanel *viewer = dynamic_cast<MDLViewerPanel *>(m_pMDLViewer);
        if (viewer) viewer->SetModel(modelPath);
    }
};

class DevPage : public vgui::PropertyPage {
    DECLARE_CLASS_SIMPLE(DevPage, vgui::PropertyPage);

   public:
    DevPage(vgui::Panel *parent, const char *name)
        : BaseClass(parent, name) {
        // 开发者列表
        m_pDevList = new ListPanel(this, "DevList");
        m_pDevList->AddColumnHeader(0, "name", "Name", 200);
        m_pDevList->AddColumnHeader(1, "role", "Role", 150);
        m_pDevList->AddActionSignalTarget(this);  // <-- 让 OnItemSelected 能接收事件

        // 默认显示的图像
        m_pDevImage = new ImagePanelPNG(this, "info_icon", "vgui/devs/default.png", ImagePanelPNG::DISPLAY_COVER);

        // 打开网页按钮
        m_pDevDummyBtn = new Button(this, "DevBtn", "Open Developer Url", this, "dev_dummy");

        // 填入开发者列表
        const char *devNames[] = {
            "nillerusr",
            "ER2",
            "ItzVladik",
            "ZZHlife_PixelZ", 
            "KonuriMaki_MaikJava"};

        for (int i = 0; i < ARRAYSIZE(devNames); i++) {
            KeyValues *kv = new KeyValues("item");
            kv->SetString("name", devNames[i]);
            kv->SetString("role", "Developers");
            m_pDevList->AddItem(kv, 0, false, false);
            kv->deleteThis();
        }

        m_pDevList->InvalidateLayout(true);
        m_pDevList->Repaint();
    }

    virtual void PerformLayout() override {
        BaseClass::PerformLayout();

        int wide, tall;
        GetSize(wide, tall);

        const int margin = 10;
        const int rightWidth = 300;
        const int buttonHeight = 24;

        // 左侧列表
        m_pDevList->SetBounds(margin, margin, wide - rightWidth - 3 * margin, tall - 2 * margin - buttonHeight);

        // 右侧区域
        int rightX = wide - rightWidth - margin;
        int rightY = margin;
        int rightH = tall - 2 * margin - buttonHeight;
        int rightW = rightWidth;

        // ✅ 方形显示：取最小边
        int squareSize = min(rightW, rightH);

        // ✅ 居中放置
        int imgX = rightX + (rightW - squareSize) / 2;
        int imgY = rightY + (rightH - squareSize) / 2;

        // 设置图片区域
        if (m_pDevImage)
            m_pDevImage->SetBounds(imgX, imgY, squareSize, squareSize);

        // 按钮位置不变
        m_pDevDummyBtn->SetBounds(
            wide - rightWidth - margin,
            tall - margin - buttonHeight,
            140,
            buttonHeight);
    }

    // 当列表中某一行被点击
    MESSAGE_FUNC_INT(OnItemSelected, "ItemSelected", itemID) {
        // 获取当前选中项的实际索引
        int selected = m_pDevList->GetSelectedItem(0);
        if (selected < 0)
            return;

        KeyValues *kv = m_pDevList->GetItem(selected);
        if (!kv) return;

        const char *devName = kv->GetString("name");
        if (!devName || !devName[0])
            return;

        // 替换非法字符
        char safeName[MAX_PATH];
        Q_strncpy(safeName, devName, sizeof(safeName));
        for (char *p = safeName; *p; ++p) {
            if (*p == '/' || *p == '\\' || *p == ' ')
                *p = '_';
        }

        char imgPath[MAX_PATH];
        Q_snprintf(imgPath, sizeof(imgPath), "vgui/devs/%s.png", safeName);

        // 如果当前已经显示这个图片，就不重新加载
        static char lastLoaded[MAX_PATH] = "";
        if (!Q_stricmp(lastLoaded, imgPath))
            return;
        Q_strncpy(lastLoaded, imgPath, sizeof(lastLoaded));

        ReplaceDevImage(imgPath);
    }

   private:
    ListPanel *m_pDevList;
    ImagePanelPNG *m_pDevImage;
    Button *m_pDevDummyBtn;

    void ReplaceDevImage(const char *path) {
        if (m_pDevImage) {
            m_pDevImage->MarkForDeletion();  // 删除旧控件
            m_pDevImage = nullptr;
        }

        m_pDevImage = new ImagePanelPNG(this, "DevImage", path, ImagePanelPNG::DISPLAY_COVER);
        InvalidateLayout(true);
        Repaint();
    }
};

// ---------------- WorkshopManagerPanel -----------------
WorkshopManagerPanel::WorkshopManagerPanel(vgui::Panel *parent)
    : BaseClass(parent, "WorkshopManagerPanel") {
    int m_z_high = 750;
    int m_z_wide = 610;    
	if (IsProportional())
	{
		m_z_wide = scheme()->GetProportionalScaledValueEx(GetScheme(), m_z_wide);
		m_z_high = scheme()->GetProportionalScaledValueEx(GetScheme(), m_z_high);
	}
	SetBounds(0, 0, m_z_high, m_z_wide);           
    SetSizeable(false);
    SetTitle("Workshop tools", true);

    m_pTabSheet = new PropertySheet(this, "WorkshopTabs");
    m_pListPage = new WorkshopListPage(m_pTabSheet, "WorkshopListPage");
    m_pTabSheet->AddPage(m_pListPage, "Installed Mods");

    m_pModelPreviewPage = new ModelPreviewPage(m_pTabSheet, "ModelPreviewPage");
    m_pTabSheet->AddPage(m_pModelPreviewPage, "Model Preview");

    vgui::PropertyPage *settingsPage = new PropertyPage(m_pTabSheet, "SettingsPage");
    new Label(settingsPage, "lblInfo", "Settings (Coming Soon)");
    m_pTabSheet->AddPage(settingsPage, "Settings");

    m_pDevPage = new DevPage(m_pTabSheet, "DevPage");
    m_pTabSheet->AddPage(m_pDevPage, "Developers");

    m_pCloseButton = new Button(this, "CloseBtn", "Close", this, "Close");
}

WorkshopManagerPanel::~WorkshopManagerPanel() {}

void WorkshopManagerPanel::PerformLayout() {
    BaseClass::PerformLayout();
    int wide, tall;
    GetSize(wide, tall);

    if (m_pTabSheet) m_pTabSheet->SetBounds(10, 30, wide - 20, tall - 60);
    if (m_pCloseButton) m_pCloseButton->SetBounds(wide - 90, tall - 34, 80, 24);
}

void WorkshopManagerPanel::OnCommand(const char *command) {
    if (!Q_stricmp(command, "Close"))
        Close();
    else
        BaseClass::OnCommand(command);
}

void WorkshopManagerPanel::Activate() {
    BaseClass::Activate();
    MoveToFront();
    RequestFocus();
}

void WorkshopManagerPanel::OnClose() {
    BaseClass::OnClose();
    SetVisible(false);
}