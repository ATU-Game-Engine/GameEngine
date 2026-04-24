/**
 * @file ForceGenerator.cpp
 * @brief Implementation of all ForceGenerator subtypes.
 *
 * Each subtype applies a distinct force model to rigid bodies within its
 * radius every physics tick via ForceGeneratorRegistry::update(). All types
 * inherit the common base (name, position, radius, strength, enabled flag)
 * and override apply() with their own force calculation.
 *
 * Subtypes:
 *   WindGenerator        — constant directional force within a radius.
 *   GravityWellGenerator — inverse-square attraction or repulsion toward a point.
 *   VortexGenerator      — tangential spin combined with an inward pull.
 *   ExplosionGenerator   — single-frame radial impulse with linear distance falloff.
 */
#include "../include/Physics/ForceGenerator.h"
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>

uint64_t ForceGenerator::nextID = 1;


// Base
/**
 * @brief Constructs a ForceGenerator with common fields shared by all subtypes.
 *
 * Generators are enabled by default. The ID is assigned from a monotonically
 * increasing counter so each generator has a unique identity for the lifetime
 * of the application.
 *
 * @param name      Human-readable label used in the editor and debug output.
 * @param type      Enum tag identifying the concrete subtype.
 * @param position  World-space centre of the generator's area of effect.
 * @param radius    Radius of the area of effect. 0 or negative = infinite range.
 * @param strength  Primary force magnitude; interpretation varies by subtype.
 */
ForceGenerator::ForceGenerator(const std::string& name,
    ForceGeneratorType type,
    const glm::vec3& position,
    float radius,
    float strength)
    : name(name),
    type(type),
    position(position),
    radius(radius),
    enabled(true),
    strength(strength),
    id(nextID++)
{
}
/**
 * @brief Returns true if bodyPos falls within the generator's area of effect.
 *
 * A radius of zero or less is treated as infinite range, so every body
 * in the scene is considered in-range regardless of distance.
 *
 * @param bodyPos  World-space position of the rigid body being tested.
 * @return         True if the body is within range.
 */
bool ForceGenerator::isInRange(const glm::vec3& bodyPos) const
{
    // radius == 0 means infinite range
    if (radius <= 0.0f) return true;
    return glm::distance(bodyPos, position) <= radius;
}

// WIND
/**
 * @brief Constructs a wind generator with a normalised direction vector.
 *
 * @param name       Label for the editor and debug output.
 * @param position   Centre of the wind area.
 * @param radius     Radius within which bodies are affected (0 = infinite).
 * @param direction  Desired wind direction. Will be normalised internally.
 *                   Defaults to +X if a zero vector is passed.
 * @param strength   Force magnitude applied each physics tick (Newtons).
 */
WindGenerator::WindGenerator(const std::string& name,
    const glm::vec3& position,
    float radius,
    const glm::vec3& direction,
    float strength)
    : ForceGenerator(name, ForceGeneratorType::WIND, position, radius, strength)
{
    setDirection(direction);
}
/**
 * @brief Sets the wind direction, normalising the input vector.
 *
 * Falls back to +X if a zero-length vector is provided to prevent NaN
 * in the force calculation.
 *
 * @param dir  Desired wind direction (need not be pre-normalised).
 */
void WindGenerator::setDirection(const glm::vec3& dir)
{
    float len = glm::length(dir);
    direction = (len > 0.0001f) ? dir / len : glm::vec3(1, 0, 0);
}
/**
 * @brief Applies a constant directional force to a body within range.
 *
 * Force magnitude is uniform across the entire radius — there is no
 * distance-based falloff. The body is woken from sleep before the force
 * is applied so stationary objects respond immediately.
 *
 * @param body       The Bullet rigid body to push.
 * @param bodyPos    World-space position of the body (pre-fetched by registry).
 * @param deltaTime  Physics timestep (unused — Bullet integrates
 *                   applyCentralForce over dt internally).
 */
void WindGenerator::apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime)
{
    if (!body || !isInRange(bodyPos)) return;

    // Constant force regardless of distance
    btVector3 force(
        direction.x * strength,
        direction.y * strength,
        direction.z * strength
    );

    body->activate(true);
    body->applyCentralForce(force);
}

// GRAVITY WELL
/**
 * @brief Constructs a gravity well that attracts or repels bodies.
 *
 * @param name         Label for the editor and debug output.
 * @param position     World-space location of the well's centre.
 * @param radius       Radius within which bodies are affected (0 = infinite).
 * @param strength     Force scale. Positive = attract, negative = repel.
 * @param minDistance  Minimum clamped distance used in the force calculation.
 *                     Prevents the force approaching infinity when a body is
 *                     very close to the centre. Default: 1.0.
 */
GravityWellGenerator::GravityWellGenerator(const std::string& name,
    const glm::vec3& position,
    float radius,
    float strength,
    float minDistance)
    : ForceGenerator(name, ForceGeneratorType::GRAVITY_WELL, position, radius, strength),
    minDistance(minDistance)
{
}
/**
 * @brief Applies an inverse-square force directed toward or away from the well.
 *
 * Force magnitude follows F = strength / dist², clamped at minDistance to
 * prevent instability at very short ranges. Positive strength attracts,
 * negative strength repels.
 *
 * @param body       The Bullet rigid body to affect.
 * @param bodyPos    World-space position of the body.
 * @param deltaTime  Physics timestep (unused).
 */
void GravityWellGenerator::apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime)
{
    if (!body || !isInRange(bodyPos)) return;

    glm::vec3 delta = position - bodyPos;
    float dist = glm::length(delta);

    // Clamp to avoid infinite force at the center
    dist = std::max(dist, minDistance);

    // Direction toward center (positive strength = attract, negative = repel)
    glm::vec3 dir = delta / dist;

    // Inverse-square falloff: force = strength / dist^2
    float forceMag = strength / (dist * dist);

    btVector3 force(
        dir.x * forceMag,
        dir.y * forceMag,
        dir.z * forceMag
    );

    body->activate(true);
    body->applyCentralForce(force);
}

// VORTEX
/**
 * @brief Constructs a vortex generator that spins bodies around an axis.
 *
 * @param name             Label for the editor and debug output.
 * @param position         World-space centre of the vortex.
 * @param radius           Radius within which bodies are affected (0 = infinite).
 * @param axis             Rotation axis (will be normalised). Defaults to +Y
 *                         if a zero vector is passed.
 * @param rotationStrength Tangential force magnitude — controls spin speed.
 *                         Stored as the base class `strength` field.
 * @param pullStrength     Inward radial force magnitude — controls how tightly
 *                         bodies spiral toward the vortex centre.
 */
VortexGenerator::VortexGenerator(const std::string& name,
    const glm::vec3& position,
    float radius,
    const glm::vec3& axis,
    float rotationStrength,
    float pullStrength)
    : ForceGenerator(name, ForceGeneratorType::VORTEX, position, radius, rotationStrength),
    pullStrength(pullStrength)
{
    float len = glm::length(axis);
    this->axis = (len > 0.0001f) ? axis / len : glm::vec3(0, 1, 0);
}
/**
 * @brief Applies a combined tangential spin and inward pull to a body.
 *
 * The tangential force is computed as cross(axis, toCenter), producing a
 * vector perpendicular to both — this creates the orbital/spinning motion.
 * A separate inward radial force (pullStrength) causes bodies to spiral
 * inward rather than orbit at a fixed distance.
 *
 * Bodies exactly at the vortex centre (dist < 0.001) are skipped to avoid
 * a zero-length cross product producing NaN.
 *
 * @param body       The Bullet rigid body to affect.
 * @param bodyPos    World-space position of the body.
 * @param deltaTime  Physics timestep (unused).
 */
void VortexGenerator::apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime)
{
    if (!body || !isInRange(bodyPos)) return;

    glm::vec3 toCenter = position - bodyPos;
    float dist = glm::length(toCenter);

    if (dist < 0.001f) return;  // at the exact center, skip

    // Tangential force: perpendicular to both the axis and the direction to center
    // This creates the spinning/orbital motion
    glm::vec3 tangent = glm::normalize(glm::cross(axis, toCenter));
    glm::vec3 rotForce = tangent * strength;

    // Inward pull toward the center axis
    glm::vec3 inward = glm::normalize(toCenter);
    glm::vec3 pullForce = inward * pullStrength;

    btVector3 totalForce(
        rotForce.x + pullForce.x,
        rotForce.y + pullForce.y,
        rotForce.z + pullForce.z
    );

    body->activate(true);
    body->applyCentralForce(totalForce);
}

// EXPLOSION
/**
 * @brief Constructs a single-use explosion generator.
 *
 * Explosions fire exactly once on the first update tick after creation and
 * are then marked for removal by the registry via the `fired` flag.
 *
 * @param name      Label for the editor and debug output.
 * @param position  World-space epicentre of the explosion.
 * @param radius    Blast radius. Bodies beyond this distance are unaffected.
 * @param strength  Peak impulse magnitude at the epicentre (N·s).
 */
ExplosionGenerator::ExplosionGenerator(const std::string& name,
    const glm::vec3& position,
    float radius,
    float strength)
    : ForceGenerator(name, ForceGeneratorType::EXPLOSION, position, radius, strength),
    fired(false)
{
}
/**
 * @brief Applies a single outward impulse to a body within the blast radius.
 *
 * Impulse magnitude falls off linearly from `strength` at the epicentre to
 * zero at the radius edge. Bodies at the exact epicentre receive a straight
 * upward impulse rather than an undefined radial direction.
 *
 * Uses applyCentralImpulse rather than applyCentralForce because an explosion
 * is an instantaneous event — the impulse is applied once and does not
 * accumulate over multiple ticks. The `fired` flag is set to true by
 * ForceGeneratorRegistry after all bodies have been processed, at which point
 * the registry removes this generator from the scene.
 *
 * @param body       The Bullet rigid body to blast.
 * @param bodyPos    World-space position of the body.
 * @param deltaTime  Physics timestep (unused — impulse is instantaneous).
 */
void ExplosionGenerator::apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime)
{
    // Explosion only fires once across all bodies in the same frame
    // fired is set to true by the registry after all bodies have been processed
    if (!body || !isInRange(bodyPos)) return;

    glm::vec3 dir = bodyPos - position;
    float dist = glm::length(dir);

    if (dist < 0.001f)
    {
        // Body is at the epicenter — apply upward force
        dir = glm::vec3(0, 1, 0);
        dist = 1.0f;
    }

    // Linear falloff: full force at center, zero at edge of radius
    float falloff = 1.0f - (dist / radius);
    falloff = std::max(falloff, 0.0f);

    glm::vec3 normalizedDir = dir / dist;
    float impulseMag = strength * falloff;

    btVector3 impulse(
        normalizedDir.x * impulseMag,
        normalizedDir.y * impulseMag,
        normalizedDir.z * impulseMag
    );

    body->activate(true);
    body->applyCentralImpulse(impulse);
}