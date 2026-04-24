#include "../include/Saves/SceneSavePanel.h"

#include "../include/Scene/Scene.h"
#include "../External/imgui/core/imgui.h"
#include "../include/Rendering/DirectionalLight.h"
#include "../include/Core/Engine.h"

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

/*Scenes are stored as JSON files in:
../../assets/scenes/ */

/**
 * @brief Draws the Scene Manager ImGui panel.
 *
 * Provides save, load, rename, delete, and new-scene operations for JSON scene
 * files stored in ../../assets/scenes/. Most operations are disabled outside
 * Editor mode. onClearSelections is fired before load and new-scene to avoid
 * dangling pointers into the old scene.
 *
 * @param scene             Scene to save into or load from.
 * @param engineMode        Disables most buttons outside EngineMode::Editor.
 * @param light             Directional light whose state is saved and restored with the scene.
 * @param onClearSelections Called before load/new-scene to clear inspector selections. May be nullptr.
 */
void DrawSceneSaveLoadPanel(Scene& scene, EngineMode engineMode, DirectionalLight& light, std::function<void()> onClearSelections)
{
    // Buffer used when typing a name to SAVE a new scene
    static char sceneName[128] = "scene_test";
    // Cached list of scene filenames
    static std::vector<std::string> sceneFiles;
    // Currently selected scene in dropdown
    static int selectedIndex = -1;

    // Buffer used when typing a new name for RENAME
    static char renameBuffer[128] = "";

    // Folder where scene JSON files live
    const std::string sceneFolder = "../../assets/scenes/";

    ImGui::Begin("Scene Manager");

    static bool needsRefresh = true;

    // We scan the scene directory once when panel first opens.
    if (needsRefresh)
    {
        sceneFiles.clear();
        if (std::filesystem::exists(sceneFolder))
        {
            for (auto& entry : std::filesystem::directory_iterator(sceneFolder))
            {
                if (entry.path().extension() == ".json")
                {
                    sceneFiles.push_back(entry.path().stem().string());
                }
            }
        }

        needsRefresh = false;
    }
    if (selectedIndex >= (int)sceneFiles.size())
    {
        selectedIndex = -1;
    }

    // =========================
    // SAVE
    // =========================
    // User types a name and saves the current scene.
    // After saving we refresh the dropdown list.
    ImGui::InputText("Scene Name", sceneName, sizeof(sceneName));

    if (engineMode != EngineMode::Editor)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Save Scene"))
    {
        std::string fullPath = sceneFolder + std::string(sceneName) + ".json";
        scene.setLightState(light.getDirection(), light.getColor(), light.getIntensity());
        scene.saveToFile(fullPath);

        // refresh list
        sceneFiles.clear();
        if (std::filesystem::exists(sceneFolder))
        {
            for (auto& entry : std::filesystem::directory_iterator(sceneFolder))
            {
                if (entry.path().extension() == ".json")
                    sceneFiles.push_back(entry.path().stem().string());
            }
        }
    }

    if (engineMode != EngineMode::Editor)
    {
        ImGui::EndDisabled();   // ← THIS WAS MISSING
    }

    // =========================
    // NEW SCENE
    // =========================
    // Clears current scene and creates a fresh one with a ground plane.

    if (engineMode != EngineMode::Editor)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("New Scene"))
    {
        scene.clear();
        if (onClearSelections) {
            onClearSelections();
        }
        // Default ground plane
        scene.spawnObject(
            ShapeType::CUBE,
            glm::vec3(0.0f, -0.25f, 0.0f),
            glm::vec3(100.0f, 0.5f, 100.0f),
            0.0f,
            "Default"
        );

        selectedIndex = -1;

        std::cout << "New scene created\n";
    }

    if (engineMode != EngineMode::Editor)
    {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // =========================
    // DROPDOWN
    // =========================
    // Shows all available scenes found in the directory.
    if (!sceneFiles.empty())
    {
        std::vector<const char*> items;
        for (auto& s : sceneFiles)
            items.push_back(s.c_str());

        ImGui::Combo("Scenes", &selectedIndex, items.data(), items.size());
    }

    // =========================
    // RENAME
    // =========================
    // Renames selected scene file on disk.
    // Prevents overwriting an existing file.
    if (selectedIndex >= 0 && selectedIndex < sceneFiles.size())
    {
        ImGui::InputText("New Name", renameBuffer, sizeof(renameBuffer));

        if (engineMode != EngineMode::Editor)
            ImGui::BeginDisabled();

        if (ImGui::Button("Rename Scene"))
        {
            std::string oldPath = sceneFolder + sceneFiles[selectedIndex] + ".json";
            std::string newPath = sceneFolder + std::string(renameBuffer) + ".json";

            if (!std::filesystem::exists(newPath))
            {
                std::filesystem::rename(oldPath, newPath);
                sceneFiles[selectedIndex] = renameBuffer;
                std::cout << "Scene renamed\n";
            }
            else
            {
                std::cout << "Rename failed: file exists\n";
            }
        }

        if (engineMode != EngineMode::Editor)
            ImGui::EndDisabled();
    }

    // =========================
    // LOAD
    // =========================

    bool canLoad =
        (engineMode == EngineMode::Editor) &&
        (selectedIndex >= 0 && selectedIndex < sceneFiles.size());

    if (!canLoad)
        ImGui::BeginDisabled();

    if (ImGui::Button("Load Scene"))
    {
        std::string fullPath = sceneFolder + sceneFiles[selectedIndex] + ".json";
        if (onClearSelections) onClearSelections();
        if (scene.loadFromFile(fullPath))
        {
            glm::vec3 dir, col;
            float intensity;
            scene.getLightState(dir, col, intensity);
            light.setDirection(dir);
            light.setColor(col);
            light.setIntensity(intensity);
            
        }
    }

    if (!canLoad)
        ImGui::EndDisabled();

    // =========================
    // DELETE
    // =========================
    // Opens confirmation popup before removing scene file.
    // Prevents accidental deletion.
    if (selectedIndex >= 0 && selectedIndex < sceneFiles.size())
    {
        if (engineMode != EngineMode::Editor)
            ImGui::BeginDisabled();

        if (ImGui::Button("Delete Scene"))
            ImGui::OpenPopup("Confirm Delete");

        if (engineMode != EngineMode::Editor)
            ImGui::EndDisabled();
    }

    if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to delete this scene?");
        ImGui::Separator();

        // ---- Confirm deletion ----
        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            std::string fullPath = sceneFolder + sceneFiles[selectedIndex] + ".json";

            if (std::filesystem::exists(fullPath))
                std::filesystem::remove(fullPath);

            // Reset selection after deletion
            selectedIndex = -1;

            // Refresh scene list after deletion
            sceneFiles.clear();

            if (std::filesystem::exists(sceneFolder))
            {
                for (auto& entry : std::filesystem::directory_iterator(sceneFolder))
                {
                    if (entry.path().extension() == ".json")
                    {
                        sceneFiles.push_back(entry.path().stem().string());
                    }
                }
            }

            std::cout << "Scene deleted\n";

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // Cancel deletion
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}