#pragma once

#include "TimeDebugView.h"
#include "PhysicsDebugView.h"
#include "SceneDebugCommands.h"
#include "LightingDebugCommands.h"
#include "ConstraintDebugView.h"
#include "ConstraintDebugCommands.h"
#include "TriggerDebugView.h"       
#include "TriggerDebugCommands.h"  
#include "Scene/GameObject.h"

/**
 * @brief Aggregates all read-only views and command interfaces passed to DebugUI each frame.
 *
 * Populated by the engine at the start of each frame and passed into DebugUI::draw().
 * DebugUI reads state from the view structs and issues requests through the command structs.
 * It does not own any of the systems these reference.
 */
struct DebugUIContext
{
    TimeDebugView time;
    PhysicsDebugView physics;
    SceneDebugCommands scene;
    LightingDebugCommands lighting;
    ConstraintDebugView constraints;
    ConstraintDebugCommands constraintCommands;
    TriggerDebugView triggers;          
    TriggerDebugCommands triggerCommands;
    // Currently selected object (set by Engine picking).
    // DebugUI may display/edit it, but does not own it.
    GameObject* selectedObject = nullptr;

    std::function<void()> applyTriggerScripts;

};
