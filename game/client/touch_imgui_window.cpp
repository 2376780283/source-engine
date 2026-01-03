#include "cbase.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_window.h"
#include "touch.h"

extern ConVar touch_enable;
extern ConVar touch_pitch;
extern ConVar touch_yaw;

// ---------------------------
// Material You 3 Dark - 莫奈黄
// ---------------------------
static ImVec4 g_accentColor = ImVec4(1.0f, 0.87f, 0.6f, 1.0f); // 莫奈黄
static ImVec4 g_darkBg      = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
static ImVec4 g_surface     = ImVec4(0.18f, 0.18f, 0.2f, 1.0f);

static Color g_EditGridColor(223, 244, 224, 50); // 默认网格颜色

// ---------------------------
// Utility: 安全缩放颜色
// ---------------------------
static ImVec4 ScaleColor(const ImVec4 &c, float f)
{
    return ImVec4(c.x * f, c.y * f, c.z * f, c.w);
}

// ---------------------------
// CTouchImguiFullScreen
// ---------------------------
class CTouchImguiFullScreen final : public IImguiWindow
{
public:
    CTouchImguiFullScreen()
        : IImguiWindow("touch_full", "Touch Controls Fullscreen")
        , selectedIndex(-1)
    {
        m_flags = ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoScrollbar;
    }

    bool Draw() override
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0)
            return false;

        const float margin = 20.0f;
        ImVec2 pos(margin, margin);
        ImVec2 size(io.DisplaySize.x - margin*2, io.DisplaySize.y - margin*2);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        // -------------------------------
        // Material You Dark 风格
        // -------------------------------
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 8.0f;
        style.GrabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;

        style.Colors[ImGuiCol_WindowBg]         = g_darkBg;
        style.Colors[ImGuiCol_FrameBg]          = g_surface;
        style.Colors[ImGuiCol_Button]           = ScaleColor(g_accentColor, 0.9f);
        style.Colors[ImGuiCol_ButtonHovered]    = g_accentColor;
        style.Colors[ImGuiCol_ButtonActive]     = ScaleColor(g_accentColor, 0.8f);
        style.Colors[ImGuiCol_Header]           = ScaleColor(g_accentColor, 0.9f);
        style.Colors[ImGuiCol_HeaderHovered]    = g_accentColor;
        style.Colors[ImGuiCol_HeaderActive]     = ScaleColor(g_accentColor, 0.8f);
        style.Colors[ImGuiCol_SliderGrab]       = ScaleColor(g_accentColor, 0.9f);
        style.Colors[ImGuiCol_SliderGrabActive] = ScaleColor(g_accentColor, 0.8f);

        // -------------------------------
        // Begin window
        // -------------------------------
        if (!ImGui::Begin(m_pTitle, nullptr, m_flags))
        {
            ImGui::End();
            return false;
        }

        float leftWidth = size.x * 0.35f;  // 左侧列表 35%
        float rightWidth = size.x - leftWidth - 10.0f; // 右侧剩余宽度

        // 左侧: Touch 按钮列表
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), true);
        ImGui::TextUnformatted("Touch Buttons");
        ImGui::Separator();

        if (gTouch.btns.Count() > 0)
        {
            ImGui::BeginChild("ButtonsList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (int i = 0; i < gTouch.btns.Count(); i++)
            {
                CTouchButton* btn = gTouch.btns[i];
                char label[128];
                snprintf(label, sizeof(label), "%s [%s]", btn->name, btn->command);

                if (ImGui::Selectable(label, selectedIndex == i))
                    selectedIndex = i;
            }
            ImGui::EndChild();
        }
        else
        {
            ImGui::TextDisabled("No buttons added.");
            selectedIndex = -1;
        }

        // 删除按钮
        if (selectedIndex >= 0 && selectedIndex < gTouch.btns.Count())
        {
            if (ImGui::Button("Delete Selected", ImVec2(-1, 0)))
            {
                gTouch.RemoveButton(gTouch.btns[selectedIndex]->name);
                selectedIndex = -1; // 安全重置
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // 右侧: 其他控件
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, 0), true);
        ImGui::TextUnformatted("Touch Control Settings");
        ImGui::Spacing();

        bool enabled = touch_enable.GetBool();
        if (ImGui::Checkbox("Enable Touch", &enabled))
            touch_enable.SetValue(enabled ? 1 : 0);

        float pitch = touch_pitch.GetFloat();
        if (ImGui::SliderFloat("Pitch", &pitch, 10.f, 180.f))
            touch_pitch.SetValue(pitch);

        float yaw = touch_yaw.GetFloat();
        if (ImGui::SliderFloat("Yaw", &yaw, 10.f, 180.f))
            touch_yaw.SetValue(yaw);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextUnformatted("Grid Color");
        static float color[4] = {
            gTouch.gridcolor.r / 255.f,
            gTouch.gridcolor.g / 255.f,
            gTouch.gridcolor.b / 255.f,
            gTouch.gridcolor.a / 255.f
        };

        if (ImGui::ColorEdit4("Grid Color Picker", color))
        {
            gTouch.gridcolor.r = (int)(color[0] * 255);
            gTouch.gridcolor.g = (int)(color[1] * 255);
            gTouch.gridcolor.b = (int)(color[2] * 255);
            gTouch.gridcolor.a = (int)(color[3] * 255);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // 添加按钮
        static char newName[64] = "";
        static char newCmd[64] = "";
        static float newX1 = 0.4f, newY1 = 0.4f, newX2 = 0.6f, newY2 = 0.6f;

        ImGui::InputText("Name", newName, sizeof(newName));
        ImGui::InputText("Command", newCmd, sizeof(newCmd));
        ImGui::InputFloat("X1", &newX1);
        ImGui::InputFloat("Y1", &newY1);
        ImGui::InputFloat("X2", &newX2);
        ImGui::InputFloat("Y2", &newY2);

        if (ImGui::Button("Add Button"))
        {
            rgba_t color(255, 255, 255, 155);
            gTouch.AddButton(newName, "", newCmd, newX1, newY1, newX2, newY2, color);
            newName[0] = newCmd[0] = 0;
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("ImGui Input: %s", io.WantCaptureMouse ? "Captured" : "Pass-through");

        ImGui::End();
        return true;
    }

private:
    ImGuiWindowFlags m_flags;
    int selectedIndex; // 当前选中按钮
};

// ---------------------------
// Static registration
// ---------------------------
static CTouchImguiFullScreen g_TouchImguiFullScreenWindow;