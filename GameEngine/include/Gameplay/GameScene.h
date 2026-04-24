#pragma once
class Scene;
class Camera;
class Physics;

/// @brief Spawns the initial game objects and force generators into the scene.
void SetupGameScene(Scene& scene, Camera& camera, Physics& physics);

// Call this after loadFromFile() to re-attach scripts to loaded objects
/// @brief Registers all tag and trigger script bindings, then applies them to existing objects.
void SetupScripts(Scene& scene, Camera& camera, Physics& physics);