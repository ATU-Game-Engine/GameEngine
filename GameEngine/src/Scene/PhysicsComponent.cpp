#include "../include/Scene/PhysicsComponent.h"
#include "../include/Scene/TransformComponent.h"

/**
 * @brief Writes the physics simulation result back into the scene transform.
 *
 * Called once per frame after the Bullet physics step to keep the
 * GameObject's visible transform in sync with where the simulation moved
 * the rigid body. Position and rotation are both extracted from Bullet's
 * motion state, which interpolates between physics ticks for smooth rendering.
 *
 * This should only ever flow physics → transform. To move an object
 * by setting its transform directly (e.g. from the editor or a script),
 * use syncFromTransform() instead.
 *
 * @param transform  The TransformComponent to overwrite with the
 *                   rigid body's current world position and rotation.
 *                   Scale is left untouched.
 */
void PhysicsComponent::syncToTransform(TransformComponent& transform) {
    if (!rigidBody) return;

    // Get the world transform from Bullet physics
    btTransform trans;
    rigidBody->getMotionState()->getWorldTransform(trans);

    // Extract position
    btVector3 origin = trans.getOrigin();
    transform.setPosition(glm::vec3(origin.x(), origin.y(), origin.z()));

    // Extract rotation
    btQuaternion rot = trans.getRotation();
    transform.setRotation(glm::quat(rot.w(), rot.x(), rot.y(), rot.z()));
}

/**
 * @brief Teleports the rigid body to match the current scene transform.
 *
 * Overrides the physics simulation state so the body's world position and
 * rotation match the values stored in @p transform. Both the motion state
 * and the rigid body itself are updated to prevent a one-frame lag where
 * Bullet would still report the old position.
 *
 * The body is woken from sleep after the update so the change takes effect
 * immediately — without this, a sleeping body would ignore the new transform
 * until something else disturbed it.
 *
 * Typical use cases:
 *   - Respawning or teleporting an object to a fixed spawn point.
 *   - Dragging an object in the editor while the simulation is paused.
 *   - Snapping a physics object to a waypoint at runtime.
 *
 * @param transform  The TransformComponent whose position and rotation will
 *                   be written into the rigid body's world transform.
 *                   Scale has no effect on the physics body.
 */
void PhysicsComponent::syncFromTransform(const TransformComponent& transform) {
    if (!rigidBody) return;

    // Get current transform from physics
    btTransform trans;
    rigidBody->getMotionState()->getWorldTransform(trans);

    // Update position
    glm::vec3 pos = transform.getPosition();
    trans.setOrigin(btVector3(pos.x, pos.y, pos.z));

    // Update rotation
    glm::quat rot = transform.getRotation();
    trans.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));

    // Apply to rigid body
    rigidBody->getMotionState()->setWorldTransform(trans);
    rigidBody->setWorldTransform(trans);

    // Wake up the body so changes take effect immediately
    rigidBody->activate(true);
}