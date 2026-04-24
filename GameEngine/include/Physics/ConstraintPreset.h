/**
 * @file ConstraintPreset.h
 * @brief Declares the ConstraintPreset factory class for creating common
 *        Bullet Physics constraint types wrapped in the engine's Constraint class.
 *
 * All methods are static — ConstraintPreset has no state and is never
 * instantiated. Each method handles the boilerplate of computing local-space
 * frames, constructing the correct Bullet subtype, applying parameters, and
 * caching those parameters on the returned Constraint for later rebuild().
 *
 * The returned unique_ptr is not automatically registered with the simulation.
 * Pass it to ConstraintRegistry::addConstraint() to make it active.
 *
 * All factory methods validate their inputs and return nullptr with a logged
 * error if a required GameObject is null or lacks a physics component. The
 * same-body guard (rbA == rbB) is also checked to prevent the Bullet assert
 * that fires when both sides of a constraint reference the same rigid body.
 */
#ifndef CONSTRAINTPRESET_H
#define CONSTRAINTPRESET_H

#include "Constraint.h"
#include "ConstraintParams.h"
#include <memory>
#include <glm/glm.hpp>

class GameObject;
/**
 * @brief Static factory class for creating typed Bullet constraints.
 *
 * Each method constructs a Constraint of the appropriate type, applies the
 * provided parameters to both the Bullet joint and the Constraint's cached
 * parameter structs (so rebuild() works after a resize), and returns
 * ownership to the caller.
 */
class ConstraintPreset {
public:
    // FIXED

    /**
     * @brief Creates a fixed constraint that locks all 6 DOFs between two bodies.
     *
     * The pivot is placed at body B's current world position expressed in
     * body A's local space, locking the two objects in their current relative
     * pose at the moment this is called.
     *
     * @param objA  Primary body. Must have a physics component.
     * @param objB  Secondary body. Must have a physics component.
     * @return      Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createFixed(GameObject* objA, GameObject* objB);

    // HINGE

     /**
     * @brief Creates a hinge constraint from a fully specified HingeParams struct.
     *
     * Allows rotation around a single axis. If objB is null the hinge is
     * anchored to the world. Pivot for body B is derived from pivotA converted
     * to world space then into B's local space for consistent placement.
     *
     * @param objA    Primary body. Must have a physics component.
     * @param objB    Secondary body, or nullptr to anchor to the world.
     * @param params  Pivot points, axis vectors, limit angles, and motor config.
     * @return        Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createHinge(GameObject* objA, GameObject* objB, const HingeParams& params);
    /**
     * @brief Creates a hinge constraint from a world-space pivot point and axis.
     *
     * Convenience overload that converts world-space inputs into body-local
     * HingeParams and delegates to the full createHinge overload. Useful when
     * the desired hinge position and axis are known in world space.
     *
     * @param objA        Primary body. Must have a physics component.
     * @param objB        Secondary body, or nullptr to anchor to the world.
     * @param worldPivot  Hinge pivot position in world space.
     * @param worldAxis   Hinge rotation axis in world space (normalised internally).
     * @return            Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createHinge(GameObject* objA, GameObject* objB,
        const glm::vec3& worldPivot, const glm::vec3& worldAxis);

    // SLIDER
     /**
     * @brief Creates a slider constraint that allows linear motion along one axis.
     *
     * Both bodies must have physics. The slider axis and pivot locations are
     * defined by the frame transforms in SliderParams. Optional limits cap
     * travel distance; an optional motor drives body B at a target velocity.
     *
     * @param objA    Primary body. Must have a physics component.
     * @param objB    Secondary body. Must have a physics component.
     * @param params  Frame transforms, limit values, and motor configuration.
     * @return        Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createSlider(GameObject* objA, GameObject* objB, const SliderParams& params);

    // SPRING
     /**
     * @brief Creates a 6DOF spring constraint with per-axis stiffness and damping.
     *
     * Built on btGeneric6DofSpringConstraint. Each of the 6 axes (0–2 linear,
     * 3–5 angular) can independently have a spring enabled. The equilibrium
     * point is set to the bodies' current relative pose at creation.
     *
     * @param objA    Primary body. Must have a physics component.
     * @param objB    Secondary body. Must have a physics component.
     * @param params  Per-axis enable flags, stiffness, damping, and pivot frames.
     * @return        Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createSpring(GameObject* objA, GameObject* objB, const SpringParams& params);

    /**
     * @brief Creates a spring constraint with only the vertical (Y) axis active.
     *
     * Convenience overload for the common suspension/bounce use case. Both
     * pivot points default to object centres. Delegates to the full createSpring
     * overload with a SpringParams configured for axis 1 (linear Y) only.
     *
     * @param objA      Primary body. Must have a physics component.
     * @param objB      Secondary body. Must have a physics component.
     * @param stiffness Spring stiffness on the Y axis (N/m).
     * @param damping   Damping ratio on the Y axis (1.0 = critically damped).
     * @return          Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createSpring(GameObject* objA, GameObject* objB, float stiffness, float damping);

    // GENERIC 6DOF
     /**
     * @brief Creates a generic 6DOF constraint with per-axis linear and angular limits.
     *
     * The most flexible preset — each of the 3 linear and 3 angular axes can
     * independently be free, locked, or limited to a range. Use this when none
     * of the specialised presets (Hinge, Slider, Spring) fit the required joint.
     *
     * Axes where useLinearLimits[i] or useAngularLimits[i] is false default to
     * locked (lower == upper == 0). Set lower > upper to leave an axis completely
     * free (Bullet convention).
     *
     * @param objA    Primary body. Must have a physics component.
     * @param objB    Secondary body. Must have a physics component.
     * @param params  Per-axis limit flags, limit values, and pivot frames.
     * @return        Owning pointer to the new Constraint, or nullptr on failure.
     */
    static std::unique_ptr<Constraint> createGeneric6Dof(GameObject* objA, GameObject* objB, const Generic6DofParams& params);
};

#endif // CONSTRAINTPRESET_H