#include "cbase.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_window.h"

#include "touch.h"

extern ConVar touch_enable;
extern ConVar touch_pitch;
extern ConVar touch_yaw;

/*
========================================================
 CTouchImguiWindow
 - Drag ONLY title bar
========================================================
*/
class CTouchImguiWindow final : public IImguiWindow
{
public:
    CTouchImguiWindow()
        : IImguiWindow("touch", "Touch Controls")
    {
        // ⚠️ flags 必须在这里设置
        
    }

    bool Draw() override
    {
        ImGuiIO& io = ImGui::GetIO();

        // 仅首次设置大小
        ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_FirstUseEver);

        //==============================
        // 1️⃣ 标题栏拖动区域
        //==============================
        const float titleHeight = ImGui::GetFrameHeight();

        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // 放一个透明按钮覆盖标题栏
        ImGui::SetCursorScreenPos(winPos);
        ImGui::InvisibleButton(
            "##title_drag",
            ImVec2(winSize.x, titleHeight)
        );

        if (ImGui::IsItemActive())
        {
            ImVec2 pos = ImGui::GetWindowPos();
            pos.x += io.MouseDelta.x;
            pos.y += io.MouseDelta.y;
            ImGui::SetWindowPos(pos, ImGuiCond_Always);
        }

        //==============================
        // 2️⃣ 内容区域
        //==============================
        ImGui::Dummy(ImVec2(0, titleHeight + 4.0f));
        ImGui::Separator();

        ImGui::TextUnformatted("Touch Control Settings");
        ImGui::Spacing();

        bool enabled = touch_enable.GetBool();
        if (ImGui::Checkbox("Enable Touch", &enabled))
            touch_enable.SetValue(enabled ? 1 : 0);

        ImGui::Spacing();

        float pitch = touch_pitch.GetFloat();
        if (ImGui::SliderFloat("Pitch", &pitch, 10.f, 180.f))
            touch_pitch.SetValue(pitch);

        float yaw = touch_yaw.GetFloat();
        if (ImGui::SliderFloat("Yaw", &yaw, 10.f, 180.f))
            touch_yaw.SetValue(yaw);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text(
            "ImGui Input: %s",
            io.WantCaptureMouse ? "Captured" : "Pass-through"
        );

        return true;
    }
};

/*
========================================================
 Static registration
========================================================
*/
static CTouchImguiWindow g_TouchImguiWindow;