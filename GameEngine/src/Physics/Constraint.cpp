/**
 * @file Constraint.cpp
 * @brief Implementation of the Constraint class, which wraps Bullet Physics
 *        constraint types (Fixed, Hinge, Slider, Spring, Generic6DOF) and
 *        provides a unified interface for creation, configuration, breaking
 *        thresholds, and runtime rebuild after rigid body replacement.
 *
 * Constraints link two GameObjects (or one GameObject to the world anchor)
 * and restrict their relative motion. All type-specific parameters are stored
 * on the Constraint so the Bullet object can be fully reconstructed via
 * rebuild() without losing settings — this is necessary because Bullet
 * constraints hold raw rigid body pointers that become invalid after a
 * physics body resize.
 */
#include "../include/Physics/Constraint.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <cmath>

 /**
  * @brief Constructs a Constraint wrapping an existing Bullet constraint.
  *
  * Takes ownership of the Bullet constraint pointer (deleted in destructor).
  * Stores back-references to both GameObjects for later rebuild operations.
  * Registers `this` as the Bullet constraint's user pointer so it can be
  * retrieved from physics callbacks.
  *
  * @param bulletConstraint  Heap-allocated Bullet constraint. Must not be null.
  * @param type              Enum identifying which Bullet subtype was passed.
  * @param objA              First GameObject. Must not be null.
  * @param objB              Second GameObject, or nullptr to anchor to the world.
  */
Constraint::Constraint(btTypedConstraint* bulletConstraint,
    ConstraintType type,
    GameObject* objA,
    GameObject* objB)
    : constraint(bulletConstraint),
    type(type),
    bodyA(objA),
    bodyB(objB),
    breakable(false),
    breakForce(INFINITY),
    breakTorque(INFINITY)
{
    if (constraint) {
        constraint->setUserConstraintPtr(this);
    }
}
/**
 * @brief Destroys the Constraint and frees the underlying Bullet object.
 *
 * The caller (ConstraintRegistry) is responsible for removing the constraint
 * from the Bullet dynamics world before this destructor runs, otherwise
 * Bullet will hold a dangling pointer.
 */
Constraint::~Constraint() {
    if (constraint) {
        delete constraint;
        constraint = nullptr;
    }
}
/**
 * @brief Stores the local-space frames used to construct this constraint.
 *
 * These are cached so that rebuild() can reconstruct the Bullet constraint
 * with identical pivot and axis data after a rigid body replacement.
 *
 * @param fA    Transform of the constraint pivot in body A's local space.
 * @param fB    Transform of the constraint pivot in body B's local space.
 * @param useA  Whether to use body A as the linear reference frame (relevant
 *              for Slider and Spring constraints).
 */
void Constraint::setFrames(const btTransform& fA, const btTransform& fB, bool useA) {
    frameInA = fA;
    frameInB = fB;
    useLinearReferenceFrameA = useA;
}

/**
 * @brief Reconstructs the Bullet constraint after its rigid bodies have been replaced.
 *
 * When a GameObject's physics body is resized (Physics::resizeRigidBody), the old
 * btRigidBody pointer is deleted and a new one is created. Any Bullet constraint
 * that referenced the old pointer becomes invalid. This method:
 *   1. Deletes the old (now-invalid) Bullet constraint.
 *   2. Fetches the current rigid body pointers from the owning GameObjects.
 *   3. Re-creates the correct Bullet constraint subtype using stored frames
 *      and type-specific parameter structs (hingeParams, sliderParams, etc.).
 *   4. Re-applies breaking thresholds and re-registers the user pointer.
 *
 * Must be called by ConstraintRegistry after any resize that affects a
 * constrained body.
 */
void Constraint::rebuild() {
    if (!bodyA) return;

    // 1. Clean up the old, now-invalid Bullet pointer

    if (constraint) {

        delete constraint;
        constraint = nullptr;
    }

    // 2. Fetch fresh pointers from the GameObjects 
    btRigidBody* rbA = bodyA->getRigidBody();
    btRigidBody* rbB = bodyB ? bodyB->getRigidBody() : &btTypedConstraint::getFixedBody();

    // 3. Re-instantiate the Bullet constraint and re-apply "Blueprint" settings
    switch (type) {
    case ConstraintType::FIXED:
        constraint = new btFixedConstraint(*rbA, *rbB, frameInA, frameInB);
        break;

    case ConstraintType::HINGE: {
        auto hinge = new btHingeConstraint(*rbA, *rbB, frameInA, frameInB);

        // Restore persistent settings
        if (hingeParams.useLimits) {
            hinge->setLimit(hingeParams.lowerLimit, hingeParams.upperLimit);
        }
        if (hingeParams.useMotor) {
            hinge->enableMotor(true);
            hinge->setMotorTarget(hingeParams.motorTargetVelocity, 1.0f);
            hinge->setMaxMotorImpulse(hingeParams.motorMaxImpulse);
        }
        constraint = hinge;
        break;
    }

    case ConstraintType::SLIDER: {
        auto slider = new btSliderConstraint(*rbA, *rbB, frameInA, frameInB, useLinearReferenceFrameA);

        // Restore persistent settings
        if (sliderParams.useLimits) {
            slider->setLowerLinLimit(sliderParams.lowerLimit);
            slider->setUpperLinLimit(sliderParams.upperLimit);
        }
        if (sliderParams.useMotor) {
            slider->setTargetLinMotorVelocity(sliderParams.motorTargetVelocity);
            slider->setMaxLinMotorForce(sliderParams.motorMaxForce);
            slider->setPoweredLinMotor(true);
        }
        constraint = slider;
        break;
    }

    case ConstraintType::SPRING: {
        auto spring = new btGeneric6DofSpringConstraint(*rbA, *rbB, frameInA, frameInB, useLinearReferenceFrameA);

        // Restore spring axes settings
        for (int i = 0; i < 6; ++i) {
            if (springParams.enableSpring[i]) {
                spring->enableSpring(i, true);
                spring->setStiffness(i, springParams.stiffness[i]);
                spring->setDamping(i, springParams.damping[i]);
            }
        }
        constraint = spring;
        break;
    }

    case ConstraintType::GENERIC_6DOF:
        constraint = new btGeneric6DofConstraint(*rbA, *rbB, frameInA, frameInB, useLinearReferenceFrameA);
        break;
    }

    // 4. Link back and restore breaking thresholds
    if (constraint) {
        constraint->setUserConstraintPtr(this);
        if (breakable) {
            constraint->setBreakingImpulseThreshold(breakForce);
        }
    }
}

// State QUeries
// Getters
/**
 * @brief Returns true if the constraint has been broken or disabled.
 *
 * Bullet disables a constraint internally when the applied impulse exceeds
 * the breaking threshold set by setBreakingThreshold(). A null constraint
 * pointer is also treated as broken.
 */
bool Constraint::isBroken() const {
    return constraint ? !constraint->isEnabled() : true;
}

// General controls
/**
 * @brief Enables or disables the constraint without destroying it.
 *
 * A disabled constraint has no effect on the simulation but remains in the
 * dynamics world and can be re-enabled at any time.
 *
 * @param enabled  True to enable, false to disable.
 */
void Constraint::setEnabled(bool enabled) {
    if (constraint) {
        constraint->setEnabled(enabled);
    }
}
/**
 * @brief Configures the constraint to break when a force or torque threshold is exceeded.
 *
 * Once broken, Bullet disables the constraint automatically. Check isBroken()
 * each frame if you need to react to breakage (e.g. spawn debris, play a sound).
 * Only the force threshold is forwarded to Bullet; torque is stored for
 * serialisation purposes but Bullet's impulse threshold covers both.
 *
 * @param force   Maximum impulse (N·s) before the constraint breaks.
 * @param torque  Maximum torque impulse (N·m·s) — stored but not separately
 *                enforced by Bullet in most constraint types.
 */
void Constraint::setBreakingThreshold(float force, float torque) {
    breakable = true;
    breakForce = force;
    breakTorque = torque;

    if (constraint) {
        constraint->setBreakingImpulseThreshold(force);
    }

    std::cout << "Set breaking threshold: force=" << force
        << ", torque=" << torque << std::endl;
}


// Controls (Hinge) 
/**
 * @brief Sets the angular travel limits for a hinge constraint.
 *
 * Both angles are in radians. The hinge will resist motion outside
 * [lower, upper] with a hard stop. Stores values in hingeParams so
 * they survive a rebuild().
 *
 * @param lower  Minimum angle in radians (typically negative).
 * @param upper  Maximum angle in radians (typically positive).
 */
void Constraint::setAngleLimits(float lower, float upper) {
    hingeParams.useLimits = true;
    hingeParams.lowerLimit = lower;
    hingeParams.upperLimit = upper;

    if (type == ConstraintType::HINGE && constraint) {
        static_cast<btHingeConstraint*>(constraint)->setLimit(lower, upper);
    }
}
/**
 * @brief Enables and configures the hinge motor.
 *
 * The motor drives the hinge toward a target angular velocity each tick.
 * maxImpulse acts as the motor's force limit — lower values produce a
 * weaker motor that stalls under load.
 *
 * @param targetVelocity  Desired angular velocity in radians/second.
 * @param maxImpulse      Maximum impulse applied per physics tick.
 */
void Constraint::enableMotor(float targetVelocity, float maxImpulse) {
    hingeParams.useMotor = true;
    hingeParams.motorTargetVelocity = targetVelocity;
    hingeParams.motorMaxImpulse = maxImpulse;

    if (type == ConstraintType::HINGE && constraint) {
        auto h = static_cast<btHingeConstraint*>(constraint);
        h->enableMotor(true);
        h->setMotorTarget(targetVelocity, 1.0f);
        h->setMaxMotorImpulse(maxImpulse);
    }
}

/**
 * @brief Disables the hinge motor, leaving the hinge free to rotate passively.
 */
void Constraint::disableMotor() {
    hingeParams.useMotor = false;
    if (type == ConstraintType::HINGE && constraint) {
        static_cast<btHingeConstraint*>(constraint)->enableMotor(false);
    }
}
/**
 * @brief Returns the current hinge angle in radians.
 *
 * @return Current angle, or 0.0f if this is not a hinge constraint.
 */
float Constraint::getHingeAngle() const {
    if (type == ConstraintType::HINGE && constraint) {
        return static_cast<btHingeConstraint*>(constraint)->getHingeAngle();
    }
    return 0.0f;
}

//  Controls (Slider)
/**
 * @brief Sets the linear travel limits for a slider constraint.
 *
 * Limits are in local units along the slider axis. The slider will hard-stop
 * outside [lower, upper]. Stored in sliderParams for rebuild() survival.
 *
 * @param lower  Minimum linear position (typically <= 0).
 * @param upper  Maximum linear position (typically >= 0).
 */
void Constraint::setLinearLimits(float lower, float upper) {
    sliderParams.useLimits = true;
    sliderParams.lowerLimit = lower;
    sliderParams.upperLimit = upper;

    if (type == ConstraintType::SLIDER && constraint) {
        auto s = static_cast<btSliderConstraint*>(constraint);
        s->setLowerLinLimit(lower);
        s->setUpperLinLimit(upper);
    }
}
/**
 * @brief Enables and configures the slider's linear motor.
 *
 * The motor drives body B along the slider axis at targetVelocity.
 * maxForce caps the applied force, acting like a motor torque limit.
 *
 * @param targetVelocity  Desired linear velocity in units/second.
 * @param maxForce        Maximum force applied per physics tick.
 */
void Constraint::enableLinearMotor(float targetVelocity, float maxForce) {
    sliderParams.useMotor = true;
    sliderParams.motorTargetVelocity = targetVelocity;
    sliderParams.motorMaxForce = maxForce;

    if (type == ConstraintType::SLIDER && constraint) {
        auto s = static_cast<btSliderConstraint*>(constraint);
        s->setTargetLinMotorVelocity(targetVelocity);
        s->setMaxLinMotorForce(maxForce);
        s->setPoweredLinMotor(true);
    }
}
/**
 * @brief Returns the current linear position of the slider in local units.
 *
 * @return Current position along the slider axis, or 0.0f if not a slider.
 */
float Constraint::getSliderPosition() const {
    if (type == ConstraintType::SLIDER && constraint) {
        return static_cast<btSliderConstraint*>(constraint)->getLinearPos();
    }
    return 0.0f;
}

//  Controls (Spring) 
/**
 * @brief Sets the spring stiffness for one of the six DOF axes.
 *
 * Axes 0-2 are linear (X, Y, Z), axes 3-5 are angular (X, Y, Z).
 * Enabling a spring on an axis makes the constraint resist displacement
 * from the equilibrium point proportionally to stiffness.
 * Higher stiffness = stiffer spring (more resistive force per unit offset).
 *
 * @param axis       DOF index in range [0, 5].
 * @param stiffness  Spring stiffness coefficient (N/m for linear axes).
 */
void Constraint::setSpringStiffness(int axis, float stiffness) {
    if (axis < 0 || axis >= 6) return;
    springParams.enableSpring[axis] = true;
    springParams.stiffness[axis] = stiffness;

    if (type == ConstraintType::SPRING && constraint) {
        auto s = static_cast<btGeneric6DofSpringConstraint*>(constraint);
        s->enableSpring(axis, true);
        s->setStiffness(axis, stiffness);
    }
}
/**
 * @brief Sets the damping coefficient for one of the six DOF spring axes.
 *
 * Damping reduces oscillation. A value of 1.0 is critical damping (no
 * overshoot). Values below 1.0 are underdamped (bouncy), above 1.0 are
 * overdamped (sluggish). Typical game values are in the range [0.1, 1.0].
 *
 * @param axis     DOF index in range [0, 5].
 * @param damping  Damping ratio (dimensionless).
 */
void Constraint::setSpringDamping(int axis, float damping) {
    if (axis < 0 || axis >= 6) return;
    springParams.damping[axis] = damping;

    if (type == ConstraintType::SPRING && constraint) {
        static_cast<btGeneric6DofSpringConstraint*>(constraint)->setDamping(axis, damping);
    }
}

// Debug
/**
 * @brief Prints a summary of this constraint to stdout.
 *
 * Useful for debugging constraint state during editor sessions or when
 * tracking down unexpected breakage.
 */
void Constraint::printInfo() const {
    std::cout << "Constraint: " << name << " [Type: " << (int)type << "]" << std::endl;
    std::cout << " - Body A: " << (bodyA ? bodyA->getName() : "None") << std::endl;
    std::cout << " - Body B: " << (bodyB ? bodyB->getName() : "World") << std::endl;
    std::cout << " - Status: " << (isBroken() ? "Broken/Disabled" : "Active") << std::endl;
}