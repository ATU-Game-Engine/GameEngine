#pragma once
#include "../Scene/Scene.h"
#include "../include/Rendering/DirectionalLight.h"

class Scene;

/// @brief Draws the Scene Manager ImGui panel. See SceneSavePanel.cpp for full documentation.
void DrawSceneSaveLoadPanel(Scene& scene, EngineMode engineMode, DirectionalLight& light, std::function<void()> onClearSelections = nullptr);