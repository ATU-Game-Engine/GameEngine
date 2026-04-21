#include "../include/Debug/DebugUI.h"
#include "../include/UI/InspectorPanel.h"
#include "../include/UI/SpawnPanel.h"
#include "../include/UI/LightingPanel.h"
#include "../include/UI/ModelImporterPanel.h"
#include "../include/UI/ConstraintCreatorPanel.h"
#include "../include/UI/TriggerEditorPanel.h"
#include "../include/UI/ForceGeneratorPanel.h"
#include "../include/UI/PointLightPanel.h"
#include "../External/imgui/core/imgui.h"
#include "../External/imgui/core/imgui_internal.h"

void DebugUI::draw(DebugUIContext& context)
{
    // ============================
    // CREATE DOCKSPACE FIRST
    // ============================
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoDocking;
    dockspace_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    dockspace_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    dockspace_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
   
    ImGui::Begin("DockSpace", nullptr, dockspace_flags);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!layoutBuilt)
    {
        layoutBuilt = true;
        buildDefaultLayout(dockspace_id, viewport);
    }

    ImGui::End();


   
    // Stats Panel

    // Displays read-only performance and physics information.
    // This panel does NOT modify engine state — it only reflects it.
    // All data comes from DebugUIContext, keeping UI decoupled from systems.
    ImGui::Begin("Stats");

    // Frame timing information
    ImGui::Text("FPS: %.1f", context.time.fps);
    ImGui::Text("Delta Time: %.4f s", context.time.deltaTime);

    ImGui::Separator();

    // Physics debug information
    ImGui::Text("Rigid Bodies: %d", context.physics.rigidBodyCount);
    ImGui::Text("Physics Enabled: %s", context.physics.physicsEnabled ? "Yes" : "No");

    ImGui::Separator();
    if (ImGui::Button("Reset Layout"))
        layoutBuilt = false;   // triggers buildDefaultLayout next frame

    ImGui::End();

  

    DrawInspectorPanel(context);
    DrawConstraintCreatorPanel(context);
    DrawModelImporterPanel(context);
    DrawLightingPanel(context);
    DrawSpawnPanel(context);
    DrawTriggerEditorPanel(context);
    DrawForceGeneratorPanel(context);
    DrawPointLightPanel(context);
}
void DebugUI::buildDefaultLayout(ImGuiID dockspaceID, ImGuiViewport* viewport)
{
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID,
        ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

    // ── Primary splits ───────────────────────────────────────────────────────
    ImGuiID dockLeft, dockCenter, dockRight, dockBottom;

    // Carve left sidebar (20 % of total width)
    ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.20f,
        &dockLeft, &dockCenter);

    // Carve right sidebar (25 % of remaining width)
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f,
        &dockRight, &dockCenter);

    // Carve bottom strip (30 % of remaining height)
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.30f,
        &dockBottom, &dockCenter);

    // ── Left sidebar — Inspector top, Stats bottom ────────────────────────────
    ImGuiID dockLeftTop, dockLeftBottom;
    ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.12f,
        &dockLeftBottom, &dockLeftTop);

    ImGui::DockBuilderDockWindow("Inspector", dockLeftTop);
    ImGui::DockBuilderDockWindow("Stats", dockLeftBottom);

    // ── Right sidebar — Spawn and Model Importer share the node (become tabs) ─
    ImGui::DockBuilderDockWindow("Spawn", dockRight);
    ImGui::DockBuilderDockWindow("Model Importer", dockRight);

    // ── Bottom strip — all tool panels share the node (tabs) ─────────────────
    ImGui::DockBuilderDockWindow("Constraint Creator", dockBottom);
    ImGui::DockBuilderDockWindow("Trigger Editor", dockBottom);
    ImGui::DockBuilderDockWindow("Force Generators", dockBottom);
    ImGui::DockBuilderDockWindow("Point Lights", dockBottom);
    ImGui::DockBuilderDockWindow("Lighting", dockBottom);
    ImGui::DockBuilderDockWindow("Scene Manager", dockBottom);

    ImGui::DockBuilderFinish(dockspaceID);
}