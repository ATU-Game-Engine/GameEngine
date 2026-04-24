/**
 * @file Constraint.h
 * @brief Declares the Constraint class — a type-safe wrapper around a Bullet
 *        btTypedConstraint that stores all parameters needed to rebuild the
 *        joint after a rigid body replacement.
 *
 * A Constraint links two GameObjects (or one GameObject to the world anchor)
 * and restricts their relative motion. Supported Bullet joint types are
 * enumerated in ConstraintType (see ConstraintParams.h):
 *   FIXED, HINGE, SLIDER, SPRING, GENERIC_6DOF.
 *
 * Design decisions:
 *   - Non-copyable — ownership is transferred via unique_ptr and managed by
 *     ConstraintRegistry.
 *   - Parameter structs (HingeParams, SliderParams, SpringParams) are cached
 *     so rebuild() can fully reconstruct the Bullet joint without any input
 *     from the caller after a resize operation.
 *   - Breaking thresholds are optional — by default constraints never break.
 *   - Helper conversion functions (toBullet / fromBullet) are provided as
 *     inline free functions at the bottom of this header for use across the
 *     constraint subsystem.
 */
#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include "ConstraintParams.h"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>


 /**
  * @brief Wraps a Bullet btTypedConstraint and stores all data needed to
  *        recreate it if either linked rigid body is replaced.
  *
  * Lifetime is managed by ConstraintRegistry via unique_ptr. The raw pointer
  * to the underlying btTypedConstraint is owned by this class and deleted in
  * the destructor. The Bullet dynamics world must remove the constraint before
  * this object is destroyed to prevent dangling pointers in the broadphase.
  */
class GameObject;

class Constraint {
private:
    btTypedConstraint* constraint; ///< Owned Bullet constraint — deleted in destructor.
    ConstraintType type; ///< Identifies which Bullet subtype is stored.
    GameObject* bodyA; ///< Primary body (non-owning).
    GameObject* bodyB;  ///< Secondary body, or nullptr for world anchor (non-owning).
    std::string name;  ///< Optional human-readable label for lookup and serialisation.

    // Rebuild cache 
    // These fields are set by setFrames() and read by rebuild() to reconstruct
    // the Bullet constraint after Physics::resizeRigidBody() invalidates the
    // raw rigid body pointers.
    btTransform frameInA; ///< Constraint pivot frame in body A's local space.
    btTransform frameInB; ///< Constraint pivot frame in body B's local space.
    bool useLinearReferenceFrameA; ///< Reference frame flag for Slider/Spring constraints.

	// Breaking thresholds
    bool breakable;  ///< True if the constraint should be removed when an impulse threshold is exceeded.
    float breakForce; ///< Maximum impulse (N·s) before the constraint breaks.
    float breakTorque; ///< Maximum torque impulse (N·m·s) — stored for serialisation.

    // Type Specific Parameter Structs 
    HingeParams hingeParams; ///< Limits and motor settings for HINGE constraints.
    SliderParams sliderParams; ///< Limits and motor settings for SLIDER constraints.
    SpringParams springParams; ///< Per-axis stiffness/damping for SPRING constraints.

public:
    /**
     * @brief Constructs a Constraint wrapping an existing Bullet joint.
     *
     * Takes ownership of bulletConstraint (deleted in destructor). Registers
     * `this` as the Bullet constraint's user pointer for retrieval from physics
     * callbacks. bodyB may be nullptr to anchor the constraint to the world.
     *
     * @param bulletConstraint  Heap-allocated Bullet constraint. Must not be null.
     * @param type              Identifies the concrete Bullet subtype.
     * @param objA              Primary GameObject. Must not be null.
     * @param objB              Secondary GameObject, or nullptr for world anchor.
     */
    Constraint(btTypedConstraint* bulletConstraint,
        ConstraintType type,
        GameObject* objA,
        GameObject* objB = nullptr);
    /**
     * @brief Destroys the Constraint and deletes the owned Bullet joint.
     *
     * The caller (ConstraintRegistry) must remove the Bullet constraint from
     * the dynamics world before this destructor runs.
     */
    ~Constraint();

    /// Non-copyable — ownership is exclusively via unique_ptr.
    Constraint(const Constraint&) = delete;
    Constraint& operator=(const Constraint&) = delete;

	// Rebuild Interface
	//rebuilding constraint when a body is resized or replaced - updates the cached frames and rebuilds the bullet constraint
    /**
     * @brief Caches the local-space pivot frames used to construct this constraint.
     *
     * Must be called immediately after construction so rebuild() has the data
     * it needs to recreate the joint after a rigid body replacement.
     *
     * @param fA    Pivot frame in body A's local space.
     * @param fB    Pivot frame in body B's local space.
     * @param useA  Reference frame flag for Slider/Spring constraints.
     */
    void setFrames(const btTransform& fA, const btTransform& fB, bool useA = true);
    /**
     * @brief Recreates the Bullet constraint using fresh rigid body pointers.
     *
     * Called by ConstraintRegistry::rebuildConstraintsForObject() after
     * Physics::resizeRigidBody() replaces the btRigidBody that this constraint
     * referenced. Deletes the old (now-invalid) Bullet constraint, fetches
     * current rigid body pointers from bodyA/bodyB, and reconstructs the
     * correct Bullet subtype using cached frames and parameter structs.
     */
    void rebuild();

    // Getters
    /// Returns the underlying Bullet constraint pointer (non-owning access).
    btTypedConstraint* getBulletConstraint() const { return constraint; }
    /// Returns the constraint type enum identifying the Bullet subtype.
    ConstraintType getType() const { return type; }
    /// Returns the primary GameObject (never null for a valid constraint).
    GameObject* getBodyA() const { return bodyA; }
    /// Returns the secondary GameObject, or nullptr if anchored to the world.
    GameObject* getBodyB() const { return bodyB; }
    /// Returns the constraint's name string (empty if unnamed).
    const std::string& getName() const { return name; }
    /// Returns true if the constraint has been disabled by a breaking event.
    bool isBroken() const;
    /// Returns true if a breaking impulse threshold has been configured.
    bool isBreakable() const { return breakable; }
    /// Returns the configured breaking force threshold in N·s.
    float getBreakForce() const { return breakForce; }
    /// Returns the configured breaking torque threshold in N·m·s.
    float getBreakTorque() const { return breakTorque; }


    /// Returns the cached pivot frame in body A's local space.
    const btTransform& getFrameInA() const { return frameInA; }
    /// Returns the cached pivot frame in body B's local space.
    const btTransform& getFrameInB() const { return frameInB; }
    /// Returns the useLinearReferenceFrameA flag (Slider/Spring only).
    bool getUseLinearReferenceFrameA() const { return useLinearReferenceFrameA; }
    /// Returns the cached hinge parameters (meaningful for HINGE type only).
    const HingeParams& getHingeParams()  const { return hingeParams; }
    /// Returns the cached slider parameters (meaningful for SLIDER type only).
    const SliderParams& getSliderParams() const { return sliderParams; }
    /// Returns the cached spring parameters (meaningful for SPRING type only).
    const SpringParams& getSpringParams() const { return springParams; }

    // Setters
    /// Sets the human-readable name used for lookup and serialisation.
    void setName(const std::string& newName) { name = newName; }
    /**
    * @brief Enables or disables the constraint without removing it from the world.
    *
    * A disabled constraint has no effect on the simulation but remains in the
    * dynamics world and can be re-enabled at any time.
    *
    * @param enabled  True to enable, false to disable.
    */
    void setEnabled(bool enabled);
    /**
     * @brief Sets impulse thresholds at which Bullet will automatically break
     *        this constraint.
     *
     * Once broken, the constraint is disabled by Bullet. ConstraintRegistry
     * detects this each tick via isBroken() and removes the constraint.
     *
     * @param force   Maximum linear impulse (N·s) before breakage.
     * @param torque  Maximum angular impulse (N·m·s) — stored for serialisation;
     *                Bullet's single threshold covers both in practice.
     */
    void setBreakingThreshold(float force, float torque);

    // Hinge controls
    /**
     * @brief Sets the angular travel limits for a hinge constraint (radians).
     *
     * @param lower  Minimum angle (radians, typically negative).
     * @param upper  Maximum angle (radians, typically positive).
     */
    void setAngleLimits(float lower, float upper);
    /**
     * @brief Enables the hinge motor with a target angular velocity and force cap.
     *
     * @param targetVelocity  Desired angular velocity in radians/second.
     * @param maxImpulse      Maximum impulse applied per tick (motor strength).
     */
    void enableMotor(float targetVelocity, float maxImpulse);
    /// Disables the hinge motor, leaving the joint free to rotate passively.
    void disableMotor();
    /**
     * @brief Returns the current hinge angle in radians.
     *
     * @return Current angle, or 0.0f if this is not a hinge constraint.
     */
    float getHingeAngle() const;

    // Slider controls
    /**
     * @brief Sets the linear travel limits for a slider constraint (world units).
     *
     * @param lower  Minimum linear position along the slider axis.
     * @param upper  Maximum linear position along the slider axis.
     */
    void setLinearLimits(float lower, float upper);
    /**
     * @brief Enables the slider motor with a target velocity and force cap.
     *
     * @param targetVelocity  Desired linear velocity in units/second.
     * @param maxForce        Maximum force applied per tick.
     */
    void enableLinearMotor(float targetVelocity, float maxForce);
    /**
    * @brief Returns the current linear position of the slider in world units.
    *
    * @return Current position, or 0.0f if this is not a slider constraint.
    */
    float getSliderPosition() const;

    // Spring controls
    /**
     * @brief Sets the spring stiffness on a specific DOF axis.
     *
     * Axes 0–2 are linear (X, Y, Z); axes 3–5 are angular (X, Y, Z).
     * Enabling a spring axis makes the constraint resist displacement from
     * equilibrium proportionally to stiffness.
     *
     * @param axis       DOF index in [0, 5].
     * @param stiffness  Spring stiffness coefficient (N/m for linear axes).
     */
    void setSpringStiffness(int axis, float stiffness);
    /**
     * @brief Sets the damping coefficient on a specific DOF spring axis.
     *
     * 1.0 = critically damped (no overshoot). < 1.0 = underdamped (bouncy).
     * > 1.0 = overdamped (sluggish). Typical game values: 0.1–1.0.
     *
     * @param axis     DOF index in [0, 5].
     * @param damping  Damping ratio (dimensionless).
     */
    void setSpringDamping(int axis, float damping);

    // Debug
    /// Prints a one-line summary of this constraint (name, type, bodies, status).
    void printInfo() const;

};

// Helper conversion functions
/**
 * @brief Converts a GLM vec3 to a Bullet btVector3.
 * @param v  Source GLM vector.
 * @return   Equivalent Bullet vector.
 */
inline btVector3 toBullet(const glm::vec3& v) {
    return btVector3(v.x, v.y, v.z);
}

/**
 * @brief Converts a GLM quaternion to a Bullet btQuaternion.
 * @param q  Source GLM quaternion (w, x, y, z order).
 * @return   Equivalent Bullet quaternion.
 */
inline btQuaternion toBullet(const glm::quat& q) {
    return btQuaternion(q.x, q.y, q.z, q.w);
}


/**
 * @brief Converts a Bullet btVector3 to a GLM vec3.
 * @param v  Source Bullet vector.
 * @return   Equivalent GLM vector.
 */
inline glm::vec3 fromBullet(const btVector3& v) {
    return glm::vec3(v.x(), v.y(), v.z());
}

/**
 * @brief Converts a Bullet btQuaternion to a GLM quaternion.
 * @param q  Source Bullet quaternion.
 * @return   Equivalent GLM quaternion (w, x, y, z order).
 */
inline glm::quat fromBullet(const btQuaternion& q) {
    return glm::quat(q.w(), q.x(), q.y(), q.z());
}

#endif // CONSTRAINT_H