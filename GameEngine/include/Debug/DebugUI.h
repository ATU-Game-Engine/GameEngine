#pragma once

#include "DebugUIContext.h"
#include "../External/imgui/core/imgui.h"
// Draws all debug/editor UI using ImGui.
// Reads engine state from DebugUIContext and issues debug commands.
// Does not own rendering or engine systems.

/**
 * @brief Draws all editor UI panels each frame via ImGui.
 *
 * Reads engine state from DebugUIContext and issues commands through it.
 * Does not own any engine systems.
 */
class DebugUI
{
public:
    void draw(DebugUIContext& context);

private:
    bool layoutBuilt = false;
    void buildDefaultLayout(ImGuiID dockspaceID, ImGuiViewport* viewport);
};