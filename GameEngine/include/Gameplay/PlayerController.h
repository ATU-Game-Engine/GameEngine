#pragma once

#include "../Scene/ScriptComponent.h"
#include "../Rendering/Camera.h"
#include "../include/Physics/PhysicsQuery.h"
#include <glm/glm.hpp>

/**
 * @brief Third-person player script. Handles orbit camera and physics-driven movement.
 *
 * Attach to a capsule GameObject with the "player" tag.
 * Camera rotation runs in onUpdate(); movement and jumping run in onFixedUpdate()
 * to stay in sync with the physics tick.
 */
class PlayerController : public ScriptComponent
{
public:
    float moveSpeed = 6.0f;
    float jumpForce = 6.0f;
    float cameraDistance = 6.0f;
    float cameraHeight = 2.5f;
    float mouseSensitivity = 0.1f;

    PlayerController(Camera* cam, PhysicsQuery* query);

    // --- ScriptComponent lifecycle ---
    void onStart()                    override;
    void onUpdate(float dt)           override;  // orbit camera, mouse input
    void onFixedUpdate(float fixedDt) override;  // movement, jumping (physics tick)
    void onDestroy()                  override;

private:
    Camera* camera = nullptr;
    PhysicsQuery* physicsQuery = nullptr;

    float orbitYaw = -90.0f;
    float orbitPitch = 15.0f;

    // Delegates to PhysicsQuery::isGrounded() 
    bool checkGrounded() const;
};