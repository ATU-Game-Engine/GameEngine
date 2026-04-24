#include "../include/Gameplay/PlayerController.h"
#include "../include/Scene/GameObject.h"
#include "../include/Input/Input.h"
#include <btBulletDynamicsCommon.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

/**
 * @brief Constructs a PlayerController with a camera and optional physics query.
 *
 * @param cam   Pointer to the scene camera used for orbit and movement direction.
 * @param query Pointer to the PhysicsQuery used for ground detection raycasts.
 *              May be null; if so, isGrounded() will always return false and
 *              jumping will be disabled.
 */
PlayerController::PlayerController(Camera* cam, PhysicsQuery* query)
    : camera(cam), physicsQuery(query)
{
}


//  onStart - runs once when script is attached to the object

/**
 * @brief Lifecycle callback invoked once when the script is attached to its owner.
 *
 * Logs the owning object's name and emits a warning if no PhysicsQuery was
 * provided, since ground detection will be non-functional without it.
 */
void PlayerController::onStart()
{
    std::cout << "[PlayerController] Started on: " << owner->getName() << std::endl;

    if (!physicsQuery)
        std::cerr << "[PlayerController] Warning: no PhysicsQuery provided - isGrounded() will always return false" << std::endl;
}
// onUpdate - variable dt, runs every frame
// Handles: orbit camera rotation from mouse, camera position follow
// NOT movement - that lives in onFixedUpdate to stay in sync with physics
// handles stuff that doesnt require physics like camera rotation and input, so it can run every frame and feel smooth even if physics is running at a fixed tick rate

/**
 * @brief Per-frame update callback (variable timestep).
 *
 * Handles input and state that do not need to be synchronised with the physics
 * step, allowing them to run every rendered frame for maximum responsiveness:
 *
 *  - **Orbit camera rotation**: when the right mouse button is held, yaw and
 *    pitch are updated from mouse deltas. Pitch is clamped to [-20°, 80°] to
 *    prevent the camera flipping over the top or clipping through the ground.
 *  - **Camera position follow**: the camera is repositioned each frame to orbit
 *    around a pivot point slightly above the player's feet (eye-level).
 *
 * Deliberate omission: movement is NOT handled here — it lives in
 * onFixedUpdate() so it stays consistent with the physics simulation tick and
 * does not vary with frame rate.
 *
 * @param dt Elapsed time since the last frame, in seconds.
 */
void PlayerController::onUpdate(float dt)
{
    if (!camera || !owner) return;

    // Orbit camera: rotate when holding right mouse button
    if (Input::GetMouseDown(GLFW_MOUSE_BUTTON_RIGHT))
    {
        orbitYaw += Input::GetMouseDeltaX() * mouseSensitivity;
        orbitPitch -= Input::GetMouseDeltaY() * mouseSensitivity;

        // Clamp pitch so camera can't flip over the top or clip through ground
        if (orbitPitch > 80.0f) orbitPitch = 80.0f;
        if (orbitPitch < -20.0f) orbitPitch = -20.0f;
    }

    // Calculate orbit offset from yaw/pitch angles
    float yawRad = glm::radians(orbitYaw);
    float pitchRad = glm::radians(orbitPitch);

    glm::vec3 offset;
    offset.x = cos(pitchRad) * cos(yawRad);
    offset.y = sin(pitchRad);
    offset.z = cos(pitchRad) * sin(yawRad);
    offset *= cameraDistance;

    // Orbit pivot is slightly above player feet (eye level-ish)
    glm::vec3 pivotPos = owner->getPosition() + glm::vec3(0.0f, cameraHeight, 0.0f);

    camera->setPosition(pivotPos - offset);
    camera->setYaw(orbitYaw);
    camera->setPitch(orbitPitch);
}

//  onFixedUpdate - fixed 1/60s tick, runs in sync with physics
//  Handles: WASD movement, jumping
//  Movement goes here (not onUpdate) so it stays consistent with
//  the physics simulation step and doesn't vary with frame rate

/**
 * @brief Fixed-timestep update callback (physics tick, ~60 Hz).
 *
 * Handles all movement that must stay in sync with Bullet's simulation step:
 *
 *  - **WASD movement**: builds a movement direction in camera-relative XZ space
 *    (the Y component is zeroed so looking up/down does not affect ground speed).
 *    Diagonal inputs are normalised to prevent faster-than-intended movement.
 *    Rather than setting velocity directly, a correction force is derived from
 *    the delta between the target and current horizontal velocities, giving
 *    responsive acceleration while still being physics-friendly.
 *  - **Jumping**: a central impulse is applied when Space is pressed and
 *    checkGrounded() confirms the player is on the ground. An impulse is used
 *    (rather than a force) so the full jump energy is applied in a single tick.
 *
 * The rigid body is explicitly kept awake each tick to prevent Bullet from
 * sleeping it mid-game.
 *
 * @param fixedDt The fixed physics timestep, in seconds (typically 1/60).
 */
void PlayerController::onFixedUpdate(float fixedDt)
{
    if (!owner || !owner->hasPhysics()) return;

    btRigidBody* body = owner->getRigidBody();
    if (!body) return;

    // Keep body awake so physics doesn't put it to sleep mid-game
    body->activate(true);

    // Build movement directions relative to camera's current facing
    // Flatten to XZ plane so looking up/down doesn't affect movement speed
    glm::vec3 forward = glm::normalize(glm::vec3(camera->getFront().x, 0.0f, camera->getFront().z));
    glm::vec3 right = glm::normalize(glm::vec3(camera->getRight().x, 0.0f, camera->getRight().z));

    glm::vec3 moveDir(0.0f);
    if (Input::GetKeyDown(GLFW_KEY_W)) moveDir += forward;
    if (Input::GetKeyDown(GLFW_KEY_S)) moveDir -= forward;
    if (Input::GetKeyDown(GLFW_KEY_A)) moveDir -= right;
    if (Input::GetKeyDown(GLFW_KEY_D)) moveDir += right;

    // Normalize so diagonal movement isn't faster than straight movement
    if (glm::length(moveDir) > 0.01f)
        moveDir = glm::normalize(moveDir);

    // Apply horizontal velocity while preserving vertical (gravity still applies)
    btVector3 currentVel = body->getLinearVelocity();
    glm::vec3 currentHorizontal(currentVel.x(), 0.0f, currentVel.z());

    // Target horizontal velocity from input
    glm::vec3 targetHorizontal = moveDir * moveSpeed;


    float accelerationStrength = 15.0f;
    glm::vec3 velocityDelta = targetHorizontal - currentHorizontal;
    glm::vec3 correctionForce = velocityDelta * accelerationStrength;

    body->applyCentralForce(btVector3(
        correctionForce.x,
        0.0f,
        correctionForce.z
    ));


    // Jump - only if actually on the ground (PhysicsQuery raycast check)
    if (Input::GetKeyPressed(GLFW_KEY_SPACE) && checkGrounded())
    {
        // applyCentralImpulse(force applied from the center of the object to avoid rotation) adds to existing velocity rather than overriding it
        body->applyCentralImpulse(btVector3(0.0f, jumpForce, 0.0f));
    }
}

/**
 * @brief Lifecycle callback invoked when the script's owner is destroyed.
 *
 * Logs the destruction event. Override to add any cleanup logic.
 */
void PlayerController::onDestroy()
{
    std::cout << "[PlayerController] Destroyed" << std::endl;
}

// use physics query to check if player is grounded by raycasting down from their position
// instead of relying on Bullet's contact points which can be unreliable and cause "sticky" feeling when trying to jump

/**
 * @brief Determines whether the player is currently standing on the ground.
 *
 * Casts a short ray downward from the player's world position using PhysicsQuery.
 * A raycast is preferred over Bullet's contact point callbacks because contact
 * points can be unreliable on curved/compound surfaces and may produce a
 * "sticky" feeling when attempting to jump near geometry edges.
 *
 * @return true if the ray hits solid geometry within the capsule's ground margin.
 *         Always returns false if physicsQuery or owner are null.
 */
bool PlayerController::checkGrounded() const
{
    if (!physicsQuery || !owner) return false;

    // Cast a short ray downward from the player's position
    // 1.1f gives a small margin below the capsule's bottom
    return physicsQuery->isGrounded(owner->getPosition(), 1.1f);
}