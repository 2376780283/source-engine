下面是一份可直接放进仓库的 README.md 示例，内容针对 Source SDK 2013 + source-2013-imgui 的实际使用方式编写，偏向工程/Mod 开发者视角，不讲泛泛概念，直接可落地。
Dear ImGui for Source SDK 2013
This project integrates Dear ImGui into Source SDK 2013, providing a modern immediate-mode UI system for debugging tools, developer panels, and in-game utilities.
It is designed to work natively with Source’s Material System, VGUI input pipeline, filesystem, and memory allocator, without relying on external graphics APIs.
Features
Native Source Material System rendering (IMaterial / IMesh)
VGUI-based input capture overlay (mouse + keyboard)
Plugin-style ImGui window system
Console command integration
Hi-DPI scaling support
Fully compatible with Source filesystem (VPK / mod paths)
Requirements
Source SDK 2013 (client DLL)
C++17 compatible compiler (same as SDK toolchain)
ImGui (already included as third-party in this repo)
Integration Overview
The integration is split into three layers:
Copy code

ImGui Core                (thirdparty/imgui)
Renderer Backend          (imgui_impl_source.cpp)
System / Window Manager   (imgui_system.cpp)
You normally do not need to modify the renderer backend.
Most usage happens through IImguiSystem and IImguiWindow.
Step 1: Add to Your Mod Project
Option A: Git Submodule (recommended)
Copy code
Bash
git submodule add https://github.com/Source-SDK-Resources/source-2013-imgui.git src/imgui
Option B: Manual Copy
Copy the following directories into your client project:
Copy code

imgui/
thirdparty/imgui/
Step 2: Update VPC Files
Add ImGui sources to your client VPC (e.g. client_base.vpc):
Copy code
Vpc
$Folder "ImGui"
{
    $Files
    {
        "$SRCDIR/imgui/imgui_system.cpp"
        "$SRCDIR/imgui/imgui_impl_source.cpp"
        "$SRCDIR/imgui/imgui_window.cpp"
    }
}
Make sure the thirdparty/imgui files are also included.
Step 3: Initialize ImGui System
ImGui must be initialized once in the client DLL.
Typical location:
CHLClient::Init()
or your client DLL initialization code
Copy code
Cpp
#include "imgui_system.h"

extern IImguiSystem* g_pImguiSystem;

void InitClient()
{
    g_pImguiSystem->Init();
}
Shutdown on unload:
Copy code
Cpp
void ShutdownClient()
{
    g_pImguiSystem->Shutdown();
}
Step 4: Creating an ImGui Window
ImGui windows are implemented by inheriting from IImguiWindow.
Example Window
Copy code
Cpp
#include "imgui_window.h"
#include "../thirdparty/imgui/imgui.h"

class CExampleImguiWindow : public IImguiWindow
{
public:
    const char* GetName() const override { return "example"; }
    const char* GetWindowTitle() const override { return "Example Window"; }

    bool Draw() override
    {
        ImGui::Text("Hello from ImGui!");
        return true;
    }
};
Register the Window
Register your window once (static or during startup):
Copy code
Cpp
static CExampleImguiWindow g_ExampleWindow;
REGISTER_IMGUI_WINDOW( g_ExampleWindow );
The window is now available to the ImGui system.
Step 5: Showing the Window
Via Console
Copy code
Text
imgui_show example
Supports auto-completion.
Via Code
Copy code
Cpp
auto* window = g_pImguiSystem->FindWindow("example");
if (window)
{
    window->SetDraw(true);
    g_pImguiSystem->SetWindowVisible(window, true, true);
}
Input Handling
ImGui input is handled through a transparent VGUI overlay panel.
Toggle Input (Mouse Cursor)
Copy code
Text
imgui_toggle_input
or bindable commands:
Copy code
Text
+imgui_input
-imgui_input
Menu Bar (Built-in)
Copy code
Text
imgui_toggle_menu
The menu bar includes:
Window list
Debug windows (Demo / Metrics)
Scaling (Hi-DPI)
Two ConVars control scaling:
Copy code
Text
imgui_display_scale   // framebuffer scale (Hi-DPI)
imgui_font_scale      // font scale
Example:
Copy code
Text
imgui_display_scale 1.5
imgui_font_scale 1.25
Rendering Details
Rendering uses UnlitGeneric materials
Font atlas is uploaded as a procedural texture
ImDrawData is converted into IMesh triangles
Scissor rects are used for clipping
You do not need to manage render state manually.
File IO Support
ImGui file operations (ini, fonts) are routed through:
Copy code
Cpp
g_pFullFileSystem
This ensures compatibility with:
VPK files
Mod directories
Source search paths
Debugging Tools
Built-in ImGui debug windows:
Copy code
Text
imgui_show_demo
Or via menu bar:
Demo Window
Metrics Window
Common Pitfalls
Do not call ImGui::NewFrame() yourself
The system handles this internally.
Do not create your own ImGui context
Use the one managed by IImguiSystem.
ImGui rendering happens during VGUI Paint, not in HUD paint hooks.
License
MIT License
Copyright (c) 2023 Strata Source Contributors
Summary
This project provides a production-ready Dear ImGui integration for Source SDK 2013, suitable for:
Developer tools
Debug overlays
In-game editors
Live diagnostics
It is designed to coexist cleanly with Source’s rendering, input, memory, and console systems.

