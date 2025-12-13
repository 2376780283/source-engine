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

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../thirdparty/stb/stb_image.h"
#include "../thirdparty/stb/stb_image_resize.h"

using namespace vgui;

extern IFileSystem *g_pFullFileSystem;

// ====== 便利宏：统一比例缩放（使用时须保证在函数体内或合适作用域） ======
// 使用示例： int w = PROPVAL(300);
#ifndef PROPVAL
#define PROPVAL(x) (IsProportional() ? scheme()->GetProportionalScaledValueEx(GetScheme(), (x)) : (x))
#endif

// ======= ImagePanelPNG with Texture Cache & MaxSize control =======
class ImagePanelPNG : public vgui::Panel {
    DECLARE_CLASS_SIMPLE(ImagePanelPNG, vgui::Panel);

public:
    enum DisplayMode {
        DISPLAY_CENTER,
        DISPLAY_STRETCH,
        DISPLAY_COVER
    };

    ImagePanelPNG(vgui::Panel *parent, const char *name, const char *pngPath, DisplayMode mode = DISPLAY_CENTER)
        : vgui::Panel(parent, name),
          m_cacheIndex(-1),
          m_imgWidth(0),
          m_imgHeight(0),
          m_u0(0.0f), m_v0(0.0f), m_u1(1.0f), m_v1(1.0f),
          m_displayMode(mode) {
        SetPaintBackgroundEnabled(false);
        if (pngPath && pngPath[0]) {
            SetImage(pngPath);
        }
    }

    ~ImagePanelPNG() override {
        ReleaseCachedTexture();
    }

    void SetDisplayMode(DisplayMode mode) { m_displayMode = mode; InvalidateLayout(true); Repaint(); }
    DisplayMode GetDisplayMode() const { return m_displayMode; }

    // 更改图片（会使用缓存）
    void SetImage(const char *pngPath) {
        if (!pngPath) return;
        // 如果路径和当前缓存相同，直接返回
        if (m_cacheIndex >= 0 && m_cacheIndex < s_TextureCache.Count()) {
            if (!Q_stricmp(s_TextureCache[m_cacheIndex].path, pngPath)) {
                return;
            }
        }
        // 释放当前
        ReleaseCachedTexture();
        // 尝试从缓存获取或加载
        m_cacheIndex = AcquireTextureFromCache(pngPath, &m_imgWidth, &m_imgHeight);
        if (m_cacheIndex >= 0) {
            // 读取尺寸并计算 UV
            const CacheEntry &e = s_TextureCache[m_cacheIndex];
            m_imgWidth = e.width;
            m_imgHeight = e.height;
            UpdateUV();
            InvalidateLayout(true);
            Repaint();
        }
    }

    // 设置全局最大纹理尺寸（像素），影响后续加载。默认 2048。
    static void SetGlobalMaxTextureSize(int maxSize) {
        s_maxTextureSize = max(64, min(maxSize, 16384)); // 防御性限制
    }
    static int GetGlobalMaxTextureSize() { return s_maxTextureSize; }

    virtual void Paint() override {
        // draw background if desired
        if (m_cacheIndex < 0 || m_cacheIndex >= s_TextureCache.Count())
            return;

        const CacheEntry &e = s_TextureCache[m_cacheIndex];
        if (e.textureID < 0) return;

        int panelW, panelH;
        GetSize(panelW, panelH);
        if (panelW <= 0 || panelH <= 0) return;

        surface()->DrawSetColor(255, 255, 255, 255);
        surface()->DrawSetTexture(e.textureID);

        float x = 0.0f, y = 0.0f;
        float drawW = (float)panelW, drawH = (float)panelH;
        float panelAspect = (float)panelW / (float)panelH;
        float imgAspect = e.height > 0 ? (float)e.width / (float)e.height : 1.0f;

        switch (m_displayMode) {
            case DISPLAY_CENTER:
                if (imgAspect > panelAspect) {
                    drawW = (float)panelW;
                    drawH = drawW / imgAspect;
                    y = ((float)panelH - drawH) * 0.5f;
                } else {
                    drawH = (float)panelH;
                    drawW = drawH * imgAspect;
                    x = ((float)panelW - drawW) * 0.5f;
                }
                break;
            case DISPLAY_STRETCH:
                // do nothing
                break;
            case DISPLAY_COVER:
                if (imgAspect > panelAspect) {
                    drawH = (float)panelH;
                    drawW = drawH * imgAspect;
                    x = ((float)panelW - drawW) * 0.5f;
                } else {
                    drawW = (float)panelW;
                    drawH = drawW / imgAspect;
                    y = ((float)panelH - drawH) * 0.5f;
                }
                break;
        }

        int ix = (int)floorf(x + 0.5f);
        int iy = (int)floorf(y + 0.5f);
        int iwx = max(1, (int)floorf(drawW + 0.5f));
        int ihy = max(1, (int)floorf(drawH + 0.5f));

        surface()->DrawTexturedSubRect(ix, iy, ix + iwx, iy + ihy, s_tex_u0, s_tex_v0, s_tex_u1, s_tex_v1);
    }

private:
    // 简单缓存项
    struct CacheEntry {
        CUtlString path;   // materials/... 路径
        int textureID;
        int width;
        int height;
        int refCount;
        CacheEntry() : textureID(-1), width(0), height(0), refCount(0) {}
    };

    // 释放当前控件持有的引用
    void ReleaseCachedTexture() {
        if (m_cacheIndex >= 0 && m_cacheIndex < s_TextureCache.Count()) {
            CacheEntry &e = s_TextureCache[m_cacheIndex];
            e.refCount = max(0, e.refCount - 1);
            if (e.refCount == 0) {
                // 销毁纹理
                if (e.textureID != -1) {
                    surface()->DestroyTextureID(e.textureID);
                    e.textureID = -1;
                }
                // 从缓存中移除（保持简单：线性删除）
                s_TextureCache.Remove(m_cacheIndex);
                // 改变后无需调整其他索引（使用后续访问时重新查询）
            }
        }
        m_cacheIndex = -1;
    }

    // 在缓存中查找，若不存在则加载并加入缓存
// 在 ImagePanelPNG::AcquireTextureFromCache 的定义内，替换整个函数体为下面内容：
static int AcquireTextureFromCache(const char *requestedPath, int *outW, int *outH) {
    if (!requestedPath || !requestedPath[0]) return -1;

    // 规范化路径（确保带 materials/ 前缀）
    char resolved[MAX_PATH];
    Q_strncpy(resolved, requestedPath, sizeof(resolved));
    if (!Q_stristr(resolved, "materials/")) {
        Q_snprintf(resolved, sizeof(resolved), "materials/%s", requestedPath);
    }

    // 先查缓存（线性查找）
    for (int i = 0; i < s_TextureCache.Count(); ++i) {
        if (!Q_stricmp(s_TextureCache[i].path.Get(), resolved)) {
            s_TextureCache[i].refCount++;
            if (outW) *outW = s_TextureCache[i].width;
            if (outH) *outH = s_TextureCache[i].height;
            return i;
        }
    }

    // 打开文件
    FileHandle_t f = g_pFullFileSystem->Open(resolved, "rb");
    if (!f) {
        Warning("[ImagePanelPNG] Cannot open: %s\n", resolved);
        return -1;
    }
    int fileSize = g_pFullFileSystem->Size(f);
    if (fileSize <= 0) {
        g_pFullFileSystem->Close(f);
        Warning("[ImagePanelPNG] empty file: %s\n", resolved);
        return -1;
    }

    CUtlMemory<unsigned char> buffer(0, fileSize);
    g_pFullFileSystem->Read(buffer.Base(), fileSize, f);
    g_pFullFileSystem->Close(f);

    int imgW = 0, imgH = 0, channels = 0;
    unsigned char *raw = stbi_load_from_memory(buffer.Base(), fileSize, &imgW, &imgH, &channels, STBI_rgb_alpha);
    if (!raw) {
        Warning("[ImagePanelPNG] stbi load failed: %s\n", resolved);
        return -1;
    }

    // 决定是否缩放到受限尺寸，优先使用全局限制
    int maxSize = s_maxTextureSize;
    // 使用局部整数名（避免与 outW/outH 指针冲突）
    int w = imgW, h = imgH;
    if (w > maxSize || h > maxSize) {
        float scale = min((float)maxSize / (float)w, (float)maxSize / (float)h);
        w = max(1, (int)(w * scale));
        h = max(1, (int)(h * scale));
    }

    unsigned char *uploadBuf = nullptr;
    bool resized = false;

    if (w != imgW || h != imgH) {
        // 需要 resize —— 分配 resized buffer，然后释放原始 raw
        uploadBuf = new unsigned char[w * h * 4];
        stbir_resize_uint8(raw, imgW, imgH, 0, uploadBuf, w, h, 0, 4);
        // 原始 raw 是由 stbi 分配，必须用 stbi_image_free 释放
        stbi_image_free(raw);
        raw = nullptr;
        resized = true; // uploadBuf 需用 delete[]
    } else {
        // 不需要 resize —— 直接使用 raw（stbi 分配），并用 stbi_image_free 释放
        uploadBuf = raw;
        raw = nullptr;
        resized = false; // uploadBuf 需用 stbi_image_free
    }

    // 创建纹理 ID 并上传（surface 会内部复制数据）
    int texID = surface()->CreateNewTextureID(true);
    surface()->DrawSetTextureRGBA(texID, uploadBuf, w, h, false, false);

    // 根据分配方式正确释放 uploadBuf
    if (resized) {
        delete[] uploadBuf;
    } else {
        // uploadBuf 来自 stbi_load，使用 stbi_image_free 释放
        if (uploadBuf) stbi_image_free(uploadBuf);
    }

    // 把 texture 信息加入缓存（使用局部 w/h）
    CacheEntry entry;
    entry.path = resolved;
    entry.textureID = texID;
    entry.width = w;
    entry.height = h;
    entry.refCount = 1;

    int newIndex = s_TextureCache.AddToTail(entry);

    // 将尺寸写回调用者（如果传入非空指针）
    if (outW) *outW = entry.width;
    if (outH) *outH = entry.height;

    // UV 半像素修正（全局缓存通用）
    UpdateGlobalUV(entry.width, entry.height);

    return newIndex;
}

    // 更新当前对象的 UV（基于全局静态值）
    void UpdateUV() {
        // s_tex_* 已由 Acquire 设置（全局）
    }

    // 静态，供全局 UV 使用（纹理尺寸变化时更新）
    static void UpdateGlobalUV(int texW, int texH) {
        if (texW <= 0 || texH <= 0) {
            s_tex_u0 = 0.0f; s_tex_v0 = 0.0f; s_tex_u1 = 1.0f; s_tex_v1 = 1.0f;
            return;
        }
        float texelU = 0.5f / (float)texW;
        float texelV = 0.5f / (float)texH;
        s_tex_u0 = texelU;
        s_tex_v0 = texelV;
        s_tex_u1 = 1.0f - texelU;
        s_tex_v1 = 1.0f - texelV;
    }

private:
    int m_cacheIndex; // 在 s_TextureCache 中的索引
    int m_imgWidth, m_imgHeight;
    float m_u0, m_v0, m_u1, m_v1;
    DisplayMode m_displayMode;

    // 静态缓存数据
    static CUtlVector<CacheEntry> s_TextureCache;
    static int s_maxTextureSize;
    static float s_tex_u0, s_tex_v0, s_tex_u1, s_tex_v1;
};

// ========== 静态成员定义 ==========
CUtlVector<ImagePanelPNG::CacheEntry> ImagePanelPNG::s_TextureCache;
int ImagePanelPNG::s_maxTextureSize = 2048;
float ImagePanelPNG::s_tex_u0 = 0.0f;
float ImagePanelPNG::s_tex_v0 = 0.0f;
float ImagePanelPNG::s_tex_u1 = 1.0f;
float ImagePanelPNG::s_tex_v1 = 1.0f;
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

    int margin = PROPVAL(10);
    int headerHeight = PROPVAL(30);
    int buttonWidth = PROPVAL(80);
    int inputHeight = PROPVAL(24);

    m_pFilterLabel->SetBounds(margin, margin + 2, PROPVAL(45), inputHeight);
    m_pSearchBox->SetBounds(margin + PROPVAL(50), margin, PROPVAL(200), inputHeight);
    m_pCategoryBox->SetBounds(margin + PROPVAL(260), margin, PROPVAL(150), inputHeight);
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
        int margin = PROPVAL(10);
        int buttonHeight = PROPVAL(24);
        int rightWidth = PROPVAL(300);

        m_pModelList->SetBounds(margin, margin, wide - rightWidth - 3 * margin, tall - 2 * margin);

        m_pPreviewButton->SetBounds(wide - rightWidth - margin, tall - margin - buttonHeight, PROPVAL(140), buttonHeight);
        m_pOtherButton->SetBounds(wide - rightWidth - margin + PROPVAL(160), tall - margin - buttonHeight, PROPVAL(140), buttonHeight);
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

// ---------------- DevPage -----------------
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

        // 默认显示的图像（使用 COVER 以填充方形）
        m_pDevImage = new ImagePanelPNG(this, "info_icon", "vgui/devs/zzh.png", ImagePanelPNG::DISPLAY_COVER);

        // 打开网页按钮
        m_pDevDummyBtn = new Button(this, "DevBtn", "Open Developer Url", this, "dev_dummy");

        // 填入开发者列表
        const char *devNames[] = {
            "nill",
            "er",
            "ltz",
            "zzh",
            "maik"};

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

        int margin = PROPVAL(10);
        int rightWidth = PROPVAL(300);
        int buttonHeight = PROPVAL(24);

        // 左侧列表
        m_pDevList->SetBounds(margin, margin, wide - rightWidth - 3 * margin, tall - 2 * margin - buttonHeight);

        // 右侧区域尺寸
        int rightX = wide - rightWidth - margin;
        int rightY = margin;
        int rightH = tall - 2 * margin - buttonHeight;
        int rightW = rightWidth;

        // 保证图片为方形，使用较小边作为尺寸，并居中放置
        int squareSize = min(rightW, rightH);
        int imgX = rightX + (rightW - squareSize) / 2;
        int imgY = rightY + (rightH - squareSize) / 2;

        if (m_pDevImage)
            m_pDevImage->SetBounds(imgX, imgY, squareSize, squareSize);

        // 按钮位于右侧底部
        m_pDevDummyBtn->SetBounds(rightX, tall - margin - buttonHeight, PROPVAL(140), buttonHeight);
    }

    // 当列表中某一行被点击
    MESSAGE_FUNC_INT(OnItemSelected, "ItemSelected", itemID) {
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
    // 基础设计尺寸（宽 x 高）
    int baseW = 750;
    int baseH = 610;

    int scaledW = PROPVAL(baseW);
    int scaledH = PROPVAL(baseH);

    // 限制窗口不超过屏幕尺寸（保留一些边距）
    int screenW = 1024, screenH = 768;
    surface()->GetScreenSize(screenW, screenH);
    int maxW = max(200, screenW - PROPVAL(50));
    int maxH = max(200, screenH - PROPVAL(50));
    scaledW = min(scaledW, maxW);
    scaledH = min(scaledH, maxH);

    // 修正：SetBounds 参数顺序为 (x, y, wide, tall)
    SetBounds(0, 0, scaledW, scaledH);
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

    int margin = PROPVAL(10);
    int topGap = PROPVAL(30);

    if (m_pTabSheet) m_pTabSheet->SetBounds(margin, topGap, wide - 2 * margin, tall - topGap - PROPVAL(30));
    if (m_pCloseButton) m_pCloseButton->SetBounds(wide - PROPVAL(90), tall - PROPVAL(34), PROPVAL(80), PROPVAL(24));
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

