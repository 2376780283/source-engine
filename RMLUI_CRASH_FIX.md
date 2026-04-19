# RmlUI 菜单进游戏后崩溃 - 修复说明

## 问题原因

菜单在主界面正常，但进入游戏再打开菜单会崩溃。原因是使用了复杂的上下文管理容器，造成指针失效和生命周期混乱。

## 修复方案

**简化设计 - 直接用单个指针管理主菜单上下文**

### 1. 简化头文件

**文件**: `rmlui_manager.h`

```cpp
class RmlUIManager {
private:
    static RmlUIManager* instance;
    RmlUiPanel* rmlPanel;
    Rml::Context* mainContext;  // 直接指针，明确管理
    
    RmlUIManager();
    void LoadFontFaces();

public:
    static RmlUIManager* GetInstance();
    void Init();
    void Render();              // 无参数，直接渲染主菜单
    void Shutdown();
    
    // ... 输入处理方法
};
```

### 2. 初始化时创建上下文

**文件**: `rmlui_manager.cpp` Init() 方法

```cpp
void RmlUIManager::Init()
{
    Rml::SetRenderInterface(&renderInterface);
    Rml::SetSystemInterface(&systemInterface);
    Rml::SetFileInterface(&fileInterface);
    Rml::Initialise();
    Rml::Factory::RegisterEventListenerInstancer(&g_MenuEventInstancer);
    LoadFontFaces();

    if (g_pFullFileSystem->FileExists("rmlui/mainmenu.rml", "MOD"))
    {
        rmlPanel = new RmlUiPanel();
        CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
        if (gameUIFactory)
        {
            IGameUI* m_pGameUI = (IGameUI*)gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
            m_pGameUI->SetMainMenuOverride(rmlPanel->GetVPanel());
        }
        
        // 直接创建并保存上下文指针
        mainContext = Rml::CreateContext("main", Rml::Vector2i(ScreenWidth(), ScreenHeight()));
        if (mainContext)
        {
            Rml::ElementDocument* document = mainContext->LoadDocument("rmlui/mainmenu.rml");
            if (document)
                document->Show();
        }
    }	
}
```

### 3. 简化渲染

**文件**: `rmlui_manager.cpp` Render() 方法

```cpp
void RmlUIManager::Render()
{
    if (!mainContext)
        return;

    renderInterface.BeginFrame();
    mainContext->Update();
    mainContext->Render();
    renderInterface.EndFrame();
}
```

### 4. 清晰的关闭流程

**文件**: `rmlui_manager.cpp` Shutdown() 方法

```cpp
void RmlUIManager::Shutdown()
{
    CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
    if (gameUIFactory)
    {
        IGameUI* m_pGameUI = (IGameUI*)gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
        m_pGameUI->SetMainMenuOverride(NULL);
    }

    if (mainContext)
    {
        Rml::RemoveContext("main");
        mainContext = nullptr;
    }

    Rml::Shutdown();

    if (rmlPanel)
    {
        rmlPanel->DeletePanel();
        rmlPanel = nullptr;
    }
}
```

### 5. 所有输入处理方法统一使用 mainContext

```cpp
void RmlUIManager::OnMousePressed(vgui::MouseCode code)
{
    if (!mainContext)
        return;
    mainContext->ProcessMouseButtonDown(GetMouseButtonIndex(code), GetKeyModifiers());
}

// 同样的模式用于其他输入方法...
```

## 修改的文件

- `game/client/rmlui/rmlui_manager.h` - 使用单个 Rml::Context* 替代 map
- `game/client/rmlui/rmlui_manager.cpp` - 简化所有方法实现
- `game/client/rmlui/rmlui_panel.cpp` - Render() 改为无参数调用

## 优势

1. **简单清晰** - 无容器，直接指针，易于理解和维护
2. **内存安全** - 指针生命周期明确，不存在野指针
3. **稳定可靠** - 进入游戏再打开菜单不会崩溃
4. **性能最优** - 无额外的 map 查询开销

## 测试

1. 启动游戏，主菜单显示正常
2. 进入游戏地图
3. 在游戏中打开菜单（如有快捷键）- 应该正常显示
4. 关闭菜单，游戏继续运行 - 无崩溃
