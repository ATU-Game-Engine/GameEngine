/**
 * @file ConstraintParams.h
 * @brief Parameter structs and the ConstraintType enum used across the
 *        constraint subsystem.
 *
 * Each struct bundles the inputs required to create and later rebuild one
 * specific Bullet constraint subtype. They are stored on Constraint so that
 * Constraint::rebuild() can fully reconstruct the Bullet joint after a rigid
 * body replacement without requiring any additional input from the caller.
 *
 * They are also used as the data model for ConstraintTemplate, allowing
 * constraint configurations to be saved to disk and reapplied to arbitrary
 * object pairs at runtime.
 *
 * Axis conventions for spring/6DOF parameters:
 *   Indices 0–2 → linear X, Y, Z
 *   Indices 3–5 → angular X, Y, Z
 */
#ifndef CONSTRAINTPARAMS_H
#define CONSTRAINTPARAMS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

 /**
  * @brief Identifies which Bullet constraint subtype a Constraint wraps.
  *
  * Used to select the correct Bullet constructor in ConstraintPreset and
  * Constraint::rebuild(), and to drive the correct editor panel in the UI.
  */
enum class ConstraintType {
    FIXED,///< All 6 DOFs locked — objects move as one rigid unit.
    HINGE,///< Single rotational axis, like a door hinge or knee joint.
    SLIDER,///< Single linear axis, like a piston or drawer rail.
    SPRING, ///< 6DOF spring with per-axis stiffness and damping.
    GENERIC_6DOF ///< Full 6DOF with independent per-axis linear/angular limits.
};

/**
 * @brief Parameters for creating a hinge (revolute) constraint.
 *
 * A hinge allows rotation around a single axis defined by axisA/axisB in each
 * body's local space. pivotA/pivotB define the attachment points on each body
 * where the hinge pin is located.
 *
 * When objB is null (world-anchored hinge), only pivotA and axisA are used.
 *
 * Angle limits are in radians. A motor drives the hinge toward
 * motorTargetVelocity each tick, capped at motorMaxImpulse per step.
 */
struct HingeParams {
    glm::vec3 pivotA{ 0, 0, 0 }; ///< Hinge attachment point in body A's local space.
    glm::vec3 pivotB{ 0, 0, 0 }; ///< Hinge attachment point in body B's local space.
    glm::vec3 axisA{ 0, 1, 0 };///< Hinge attachment point in body B's local space .
    glm::vec3 axisB{ 0, 1, 0 }; ///< Rotation axis in body B's local space (default: +Y).

    bool useLimits = false; ///< True to clamp rotation to [lowerLimit, upperLimit].
    float lowerLimit = 0.0f;   ///< Minimum rotation angle in radians
    float upperLimit = 0.0f;  ///< Maximum rotation angle in radians.

    bool useMotor = false; ///< True to drive the hinge with a motor.
    float motorTargetVelocity = 0.0f; ///< Desired angular velocity in radians/second.
    float motorMaxImpulse = 0.0f;  ///< Maximum impulse the motor can apply per tick.
};

/**
 * @brief Parameters for creating a slider (prismatic) constraint.
 *
 * A slider allows linear translation along a single axis. The axis direction
 * and attachment points are encoded in local-space frame transforms
 * (position + rotation quaternion) for each body.
 *
 * frameAPos/frameARot define the slider frame on body A.
 * frameBPos/frameBRot define the slider frame on body B.
 * The X axis of these frames becomes the slide direction.
 *
 * A motor drives body B along the axis at motorTargetVelocity, capped at
 * motorMaxForce.
 */
struct SliderParams { 
    glm::vec3 frameAPos{ 0, 0, 0 };///< Slider frame origin in body A's local space.
    glm::quat frameARot{ 1, 0, 0, 0 }; ///< Slider frame orientation in body A's local space.
    glm::vec3 frameBPos{ 0, 0, 0 }; ///< Slider frame origin in body B's local space.
    glm::quat frameBRot{ 1, 0, 0, 0 }; ///< Slider frame orientation in body B's local space.

    bool useLimits = false; ///< True to clamp travel to [lowerLimit, upperLimit].
    float lowerLimit = 0.0f;     ///< Minimum linear position along the slide axis.
    float upperLimit = 0.0f; ///< Maximum linear position along the slide axis.

    bool useMotor = false; ///< True to drive the slider with a motor.
    float motorTargetVelocity = 0.0f; ///< Desired linear velocity in units/second.
    float motorMaxForce = 0.0f;  ///< Maximum force the motor can apply per tick.
};


/**
 * @brief Parameters for creating a 6DOF spring constraint.
 *
 * Built on Bullet's btGeneric6DofSpringConstraint. Each of the 6 axes can
 * independently have a spring enabled with its own stiffness and damping:
 *   Indices 0–2 → linear spring along X, Y, Z.
 *   Indices 3–5 → angular spring around X, Y, Z.
 *
 * pivotA/pivotB define the attachment points on each body.
 * rotA/rotB define the local-space orientation of the constraint frame.
 *
 * Stiffness controls resistance to displacement from equilibrium (higher = stiffer).
 * Damping controls oscillation decay (1.0 = critically damped, 0.0 = no damping).
 *
 * The equilibrium point is set to the bodies' current relative pose at creation.
 */
struct SpringParams {
    glm::vec3 pivotA{ 0, 0, 0 };  ///< Spring attachment point in body A's local space.
    glm::vec3 pivotB{ 0, 0, 0 };  ///< Spring attachment point in body B's local space.
    glm::quat rotA{ 1, 0, 0, 0 };  ///< Constraint frame orientation in body A's local space.
    glm::quat rotB{ 1, 0, 0, 0 };  ///< Constraint frame orientation in body B's local space.

    // Spring axes: 0-2 = linear X,Y,Z; 3-5 = angular X,Y,Z
    /// Per-axis spring enable flags. Indices 0–2 = linear X/Y/Z, 3–5 = angular X/Y/Z.
    bool enableSpring[6] = { false, false, false, false, false, false };
    /// Per-axis stiffness coefficients (N/m for linear, N·m/rad for angular).
    float stiffness[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    /// Per-axis damping ratios. 1.0 = critically damped, <1.0 = bouncy, >1.0 = overdamped.
    float damping[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
};

/**
 * @brief Parameters for creating a generic 6DOF constraint.
 *
 * Built on Bullet's btGeneric6DofConstraint. Each of the 3 linear and 3
 * angular axes can be independently free, locked, or clamped to a range:
 *   - useLinearLimits[i]  = true: axis i is limited to [lowerLinearLimit[i],
 *                                  upperLinearLimit[i]].
 *   - useAngularLimits[i] = true: axis i is limited to [lowerAngularLimit[i],
 *                                  upperAngularLimit[i]] (radians).
 *   - When both lower and upper are 0 (the default), Bullet locks that axis.
 *   - Setting lower > upper signals Bullet to leave the axis completely free.
 *
 * pivotA/pivotB define attachment points; rotA/rotB define the local-space
 * orientation of the constraint frame on each body.
 */
struct Generic6DofParams {
    glm::vec3 pivotA{ 0, 0, 0 }; ///< Constraint frame origin in body A's local space.
    glm::vec3 pivotB{ 0, 0, 0 };  ///< Constraint frame origin in body B's local space.
    glm::quat rotA{ 1, 0, 0, 0 }; ///< Constraint frame orientation in body A's local space.
    glm::quat rotB{ 1, 0, 0, 0 };  ///< Constraint frame orientation in body B's local space.

    /// Per-axis linear limit enable flags (indices 0–2 → X, Y, Z).
    bool useLinearLimits[3] = { false, false, false };
    /// Per-axis lower linear limit in world units (active when useLinearLimits[i] = true)
    float lowerLinearLimit[3] = { 0.0f, 0.0f, 0.0f };
    /// Per-axis upper linear limit in world units (active when useLinearLimits[i] = true).
    float upperLinearLimit[3] = { 0.0f, 0.0f, 0.0f };

    /// Per-axis angular limit enable flags (indices 0–2 → X, Y, Z).
    bool useAngularLimits[3] = { false, false, false };
    /// Per-axis lower angular limit in radians (active when useAngularLimits[i] = true).
    float lowerAngularLimit[3] = { 0.0f, 0.0f, 0.0f };
    /// Per-axis upper angular limit in radians (active when useAngularLimits[i] = true).
    float upperAngularLimit[3] = { 0.0f, 0.0f, 0.0f };
};

#endif // CONSTRAINTPARAMS_H