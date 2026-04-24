#include "../include/Testing/TestUI.h"
#include "../External/imgui/core/imgui.h"
#include "../include/Core/GameTime.h"

/**
 * @brief Draws the Test Panel ImGui window.
 *
 * Displays delta time and FPS from GameTime. Shows a green PASS if FPS is
 * within [59, 61], red FAIL otherwise.
 */
void TestUI::draw()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Test Panel");

    ImGui::Text("Test Mode Active");

    ImGui::Separator();
    ImGui::Text("=== Timestep Test ===");

    ImGui::Text("Delta Time: %.4f", Time::GetDeltaTime());
    ImGui::Text("FPS: %.2f", Time::GetFPS());

    float fps = Time::GetFPS();

    if (fps >= 59.0f && fps <= 61.0f)
    {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: PASS (60 FPS)");
    }
    else
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: FAIL");
    }

    ImGui::End();
}