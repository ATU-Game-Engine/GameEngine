/**
 * @file ConstraintPreset.cpp
 * @brief Factory functions for creating common Bullet Physics constraint types
 *        wrapped in the engine's Constraint class.
 *
 * Each preset handles the boilerplate of computing local-space frames,
 * constructing the correct Bullet subtype, applying parameter structs,
 * and caching those parameters on the returned Constraint so that
 * Constraint::rebuild() can fully reconstruct the joint after a rigid
 * body replacement (e.g. after Physics::resizeRigidBody).
 *
 * Supported presets:
 *   - Fixed     — locks all 6 DOFs between two bodies
 *   - Hinge     — single rotational axis, optional limits and motor
 *   - Slider    — single linear axis, optional limits and motor
 *   - Spring    — 6DOF spring with per-axis stiffness and damping
 *   - Generic6DOF — full 6DOF with per-axis linear and angular limits
 *
 * All functions return nullptr and log to stderr if required GameObjects
 * are missing or lack a physics body.
 */
#include "../include/Scene/GameObject.h"
#include "../include/Physics/ConstraintPreset.h"
#include <iostream>
#include <glm/gtc/constants.hpp>

 /**
  * @brief Converts a Bullet btVector3 to a GLM vec3.
  * @param v  Source Bullet vector.
  * @return   Equivalent GLM vector.
  */
inline glm::vec3 toGlm(const btVector3& v)
{
    return glm::vec3(v.x(), v.y(), v.z());
}
//  FIXED Constraints 
/**
 * @brief Creates a fixed constraint that locks all 6 DOFs between two bodies.
 *
 * The constraint pivot is placed at body B's current world position expressed
 * in body A's local space, so the two objects are locked in their current
 * relative pose at the moment this is called. Neither translation nor rotation
 * is permitted after the constraint is active.
 *
 * Implemented as a btGeneric6DofConstraint with all linear and angular limits
 * set to zero, which is more stable than btFixedConstraint in most Bullet versions.
 *
 * @param objA  First body. Must have a physics component.
 * @param objB  Second body. Must have a physics component.
 * @return      Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createFixed(
    GameObject* objA, GameObject* objB)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create fixed constraint - objA missing or no physics" << std::endl;
        return nullptr;
    }

    if (!objB || !objB->hasPhysics()) {
        std::cerr << "Error: Cannot create fixed constraint - objB missing or no physics" << std::endl;
        return nullptr;
    }

    btRigidBody* rbA = objA->getRigidBody();
    btRigidBody* rbB = objB->getRigidBody();

    // Calculate relative transform from A to B
    btTransform worldA = rbA->getWorldTransform();
    btTransform worldB = rbB->getWorldTransform();
    btTransform frameInA = worldA.inverse() * worldB;// B's position relative to A
    btTransform frameInB;
    frameInB.setIdentity();

    if (rbA == rbB) {
        std::cerr << "Error: Cannot create fixed constraint — both bodies are the same ("
            << objA->getName() << ")" << std::endl;
        return nullptr;
    }
    // Create bullet fixed constraint (generic 6DOF with all DOFs locked)
    btGeneric6DofConstraint* fixedConstraint = new btGeneric6DofConstraint(
        *rbA, *rbB, frameInA, frameInB, true
    );

    // Lock all axes
    fixedConstraint->setLinearLowerLimit(btVector3(0, 0, 0));
    fixedConstraint->setLinearUpperLimit(btVector3(0, 0, 0));
    fixedConstraint->setAngularLowerLimit(btVector3(0, 0, 0));
    fixedConstraint->setAngularUpperLimit(btVector3(0, 0, 0));

	//wrap bullet in constraint class
    auto constraint = std::make_unique<Constraint>(
        fixedConstraint, ConstraintType::FIXED, objA, objB
    );
    constraint->setFrames(frameInA, frameInB, true);
	//return ownship of pointer
    std::cout << "Created FIXED constraint" << std::endl;
    return constraint;
}

// HINGE Constraints 
/**
 * @brief Creates a hinge constraint from a fully specified HingeParams struct.
 *
 * The hinge allows rotation around a single axis. If objB is null the hinge
 * is anchored to the physics world (body A swings around a fixed world point).
 *
 * When two bodies are provided, the pivot for body B is derived by converting
 * pivotA from A's local space to world space, then into B's local space, so
 * the hinge is positioned consistently regardless of the objects' current
 * world transforms.
 *
 * Limits and motor settings from params are applied both to the Bullet
 * constraint and cached on the returned Constraint for rebuild() survival.
 *
 * @param objA    Primary body. Must have a physics component.
 * @param objB    Secondary body, or nullptr to anchor to the world.
 * @param params  Struct containing pivot points, axis vectors, limit angles,
 *                and motor configuration.
 * @return        Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createHinge(
    GameObject* objA, GameObject* objB, const HingeParams& params)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create hinge - objA missing or no physics" << std::endl;
        return nullptr;
    }
	// get physics bodies if they exist, hinge can be created with just one body (hinged to world)
    btRigidBody* rbA = objA->getRigidBody();
    btRigidBody* rbB = objB ? objB->getRigidBody() : nullptr;

	// store as bullet hinge constraint pointer
    btHingeConstraint* hinge;

	// if we have two bodies, create hinge between them, otherwise hinge to world
    if (rbB) {
        if (rbA == rbB) {
            std::cerr << "Error: Cannot create hinge — both bodies are the same ("
                << objA->getName() << ")" << std::endl;
            return nullptr;
        }
        // Convert pivotA from A's local space to world space, then into B's local space
        btVector3 worldPivot = rbA->getWorldTransform() * toBullet(params.pivotA);
        btVector3 localPivotB = rbB->getWorldTransform().inverse() * worldPivot;
        hinge = new btHingeConstraint(
            *rbA, *rbB,
            toBullet(params.pivotA), localPivotB,
            toBullet(params.axisA), toBullet(params.axisB)
        );
    }
    else {
        hinge = new btHingeConstraint(
            *rbA,
            toBullet(params.pivotA),
            toBullet(params.axisA)
        );
    }

    // Apply limits if enabled
    if (params.useLimits) {
        hinge->setLimit(params.lowerLimit, params.upperLimit);
    }

    // Apply motor if enabled
    if (params.useMotor) {
        hinge->enableMotor(true);
        hinge->setMotorTarget(params.motorTargetVelocity, 1.0f);
        hinge->setMaxMotorImpulse(params.motorMaxImpulse);
    }

    auto constraint = std::make_unique<Constraint>(
        hinge, ConstraintType::HINGE, objA, objB
    );
    constraint->setFrames(hinge->getAFrame(), hinge->getBFrame(), true);

    if (params.useLimits)
        constraint->setAngleLimits(params.lowerLimit, params.upperLimit);
    if (params.useMotor)
        constraint->enableMotor(params.motorTargetVelocity, params.motorMaxImpulse);

    std::cout << "Created HINGE constraint";
    if (params.useLimits) {
        std::cout << " with limits [" << params.lowerLimit << ", " << params.upperLimit << "]";
    }
    std::cout << std::endl;

    return constraint;
}
/**
 * @brief Creates a hinge constraint from a world-space pivot point and axis.
 *
 * Convenience overload that converts the world-space inputs into body-local
 * HingeParams and delegates to the full createHinge overload. Useful when
 * the caller knows the desired hinge position and axis in world space but
 * does not want to compute local-space transforms manually.
 *
 * @param objA        Primary body. Must have a physics component.
 * @param objB        Secondary body, or nullptr to anchor to the world.
 * @param worldPivot  Hinge pivot position in world space.
 * @param worldAxis   Hinge rotation axis in world space (need not be normalised).
 * @return            Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createHinge(
    GameObject* objA, GameObject* objB,
    const glm::vec3& worldPivot, const glm::vec3& worldAxis)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create hinge - objA missing or no physics" << std::endl;
        return nullptr;
    }
    btRigidBody* rbA = objA->getRigidBody();
    // Convert world-space pivot to local space
    HingeParams params;
    btTransform worldTransformA = rbA->getWorldTransform();
    btVector3 localPivotA = worldTransformA.inverse() * toBullet(worldPivot);
    params.pivotA = toGlm(localPivotA);

    glm::vec3 axis = worldAxis;
    if (glm::length(axis) > 0.0001f)
        axis = glm::normalize(axis);

    params.axisA = axis;


    if (objB && objB->hasPhysics()) {
        params.pivotB = worldPivot - objB->getPosition();
        params.axisB = glm::normalize(worldAxis);
    }
    else {
        params.pivotB = worldPivot;
        params.axisB = glm::normalize(worldAxis);
    }

    return createHinge(objA, objB, params);
}

//  SLIDER Constraints 
/**
 * @brief Creates a slider constraint that allows linear motion along one axis.
 *
 * Both bodies must have physics. The slider axis and attachment points are
 * defined by the frame transforms in SliderParams (frameAPos/Rot for body A,
 * frameBPos/Rot for body B). The local-space frames determine both the axis
 * direction and the pivot locations on each body.
 *
 * Optional limits cap how far body B can slide along the axis relative to A.
 * An optional motor drives B at a target velocity up to a maximum force.
 *
 * @param objA    Primary body. Must have a physics component.
 * @param objB    Secondary body. Must have a physics component.
 * @param params  Frame transforms, limit values, and motor configuration.
 * @return        Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createSlider(
    GameObject* objA, GameObject* objB, const SliderParams& params)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create slider - objA missing or no physics" << std::endl;
        return nullptr;
    }

    if (!objB || !objB->hasPhysics()) {
        std::cerr << "Error: Cannot create slider - objB missing or no physics" << std::endl;
        return nullptr;
    }

    btRigidBody* rbA = objA->getRigidBody();
    btRigidBody* rbB = objB->getRigidBody();

    if (rbA == rbB) {
        std::cerr << "Error: Cannot create slider — both bodies are the same ("
            << objA->getName() << ")" << std::endl;
        return nullptr;
    }
    // Create transforms for the slider frames
	// frameInA is the position and orientation of the slider in A's local space, if its attached to A at (0,0) at a 25% rotation around z ect
    btTransform frameInA, frameInB;
	frameInA.setOrigin(toBullet(params.frameAPos)); // set the position of the frame in A's local space i.e . where the slider is attached to A
    frameInA.setRotation(toBullet(params.frameARot));
    frameInB.setOrigin(toBullet(params.frameBPos));
    frameInB.setRotation(toBullet(params.frameBRot));

    btSliderConstraint* slider = new btSliderConstraint(
        *rbA, *rbB, frameInA, frameInB, true
    );

    // Apply limits if enabled
    if (params.useLimits) {
        slider->setLowerLinLimit(params.lowerLimit);
        slider->setUpperLinLimit(params.upperLimit);
    }

    // Apply motor if enabled
    if (params.useMotor) {
        slider->setPoweredLinMotor(true);
        slider->setTargetLinMotorVelocity(params.motorTargetVelocity);
        slider->setMaxLinMotorForce(params.motorMaxForce);
    }

    auto constraint = std::make_unique<Constraint>(
        slider, ConstraintType::SLIDER, objA, objB
    );
    constraint->setFrames(frameInA, frameInB, true);

    if (params.useLimits)
        constraint->setLinearLimits(params.lowerLimit, params.upperLimit);
    if (params.useMotor)
        constraint->enableLinearMotor(params.motorTargetVelocity, params.motorMaxForce);


    std::cout << "Created SLIDER constraint";
    if (params.useLimits) {
        std::cout << " with limits [" << params.lowerLimit << ", " << params.upperLimit << "]";
    }
    std::cout << std::endl;

    return constraint;
}

//  SPRING Constraints 
/**
 * @brief Creates a 6DOF spring constraint with per-axis stiffness and damping.
 *
 * Built on btGeneric6DofSpringConstraint. Each of the 6 axes (0-2 linear,
 * 3-5 angular) can independently have a spring enabled. The equilibrium
 * point is set to the bodies' current relative pose at creation time.
 *
 * Stiffness controls how strongly the spring resists displacement from
 * equilibrium (higher = stiffer). Damping controls how quickly oscillations
 * decay (1.0 = critically damped, no overshoot; <1.0 = bouncy).
 *
 * @param objA    Primary body. Must have a physics component.
 * @param objB    Secondary body. Must have a physics component.
 * @param params  Per-axis enable flags, stiffness, damping, and pivot frames.
 * @return        Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createSpring(
    GameObject* objA, GameObject* objB, const SpringParams& params)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create spring - objA missing or no physics" << std::endl;
        return nullptr;
    }

    if (!objB || !objB->hasPhysics()) {
        std::cerr << "Error: Cannot create spring - objB missing or no physics" << std::endl;
        return nullptr;
    }

    btRigidBody* rbA = objA->getRigidBody();
    btRigidBody* rbB = objB->getRigidBody();

    if (rbA == rbB) {
        std::cerr << "Error: Cannot create spring — both bodies are the same ("
            << objA->getName() << ")" << std::endl;
        return nullptr;
    }
    // Create transforms
    btTransform frameInA, frameInB;
    frameInA.setOrigin(toBullet(params.pivotA));
    frameInA.setRotation(toBullet(params.rotA));
    frameInB.setOrigin(toBullet(params.pivotB));
    frameInB.setRotation(toBullet(params.rotB));

	// create bullet spring constraint between the two rigid bodies connected at the specified frames with springs enabled
    btGeneric6DofSpringConstraint* spring = new btGeneric6DofSpringConstraint(
        *rbA, *rbB, frameInA, frameInB, true
    );

    // Configure springs
    for (int i = 0; i < 6; ++i) {
        if (params.enableSpring[i]) {
            spring->enableSpring(i, true);
			spring->setStiffness(i, params.stiffness[i]); //stuffness- how resiliant to compression the spring is
			spring->setDamping(i, params.damping[i]); //damping- how much energy is lost  when the spring compresses/extends
        }
    }

    // Set equilibrium point to current position
    spring->setEquilibriumPoint();

	//wrap bullet constraint in Constraint class
    auto constraint = std::make_unique<Constraint>(
        spring, ConstraintType::SPRING, objA, objB
    );
    constraint->setFrames(frameInA, frameInB, true);

	// Cache spring parameters in our Constraint class for easy access and potential rebuilding
    for (int i = 0; i < 6; ++i) {
        if (params.enableSpring[i]) {
            constraint->setSpringStiffness(i, params.stiffness[i]);
            constraint->setSpringDamping(i, params.damping[i]);
        }
    }

    int activeSpringCount = 0;
    for (int i = 0; i < 6; ++i) {
        if (params.enableSpring[i]) activeSpringCount++;
    }
    std::cout << "Created SPRING constraint with " << activeSpringCount << " active axes" << std::endl;

    return constraint;
}

/**
 * @brief Simplified spring creation that enables only the vertical (Y) axis.
 *
 * Convenience overload for the common case of a suspension or bounce spring
 * where only vertical displacement needs to be spring-constrained. Both pivot
 * points default to the object centres (0, 0, 0 in local space).
 *
 * Delegates to the full createSpring overload with a SpringParams struct
 * configured for axis 1 (linear Y) only.
 *
 * @param objA       Primary body. Must have a physics component.
 * @param objB       Secondary body. Must have a physics component.
 * @param stiffness  Spring stiffness on the Y axis (N/m).
 * @param damping    Damping ratio on the Y axis (1.0 = critically damped).
 * @return           Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createSpring(
    GameObject* objA, GameObject* objB, float stiffness, float damping)
{
    SpringParams params;

    // Enable spring on Y axis (vertical) - typical for suspension
    params.enableSpring[1] = true;
    params.stiffness[1] = stiffness;
    params.damping[1] = damping;

    // Set pivot points at object centers
    params.pivotA = glm::vec3(0.0f);
    params.pivotB = glm::vec3(0.0f);

    return createSpring(objA, objB, params);
}

//  GENERIC 6DOF Constraints
/**
 * @brief Simplified spring creation that enables only the vertical (Y) axis.
 *
 * Convenience overload for the common case of a suspension or bounce spring
 * where only vertical displacement needs to be spring-constrained. Both pivot
 * points default to the object centres (0, 0, 0 in local space).
 *
 * Delegates to the full createSpring overload with a SpringParams struct
 * configured for axis 1 (linear Y) only.
 *
 * @param objA       Primary body. Must have a physics component.
 * @param objB       Secondary body. Must have a physics component.
 * @param stiffness  Spring stiffness on the Y axis (N/m).
 * @param damping    Damping ratio on the Y axis (1.0 = critically damped).
 * @return           Owning pointer to the new Constraint, or nullptr on failure.
 */
std::unique_ptr<Constraint> ConstraintPreset::createGeneric6Dof(
    GameObject* objA, GameObject* objB, const Generic6DofParams& params)
{
    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot create 6DOF - objA missing or no physics" << std::endl;
        return nullptr;
    }

    if (!objB || !objB->hasPhysics()) {
        std::cerr << "Error: Cannot create 6DOF - objB missing or no physics" << std::endl;
        return nullptr;
    }

    btRigidBody* rbA = objA->getRigidBody();
    btRigidBody* rbB = objB->getRigidBody();

    if (rbA == rbB) {
        std::cerr << "Error: Cannot create 6DOF — both bodies are the same ("
            << objA->getName() << ")" << std::endl;
        return nullptr;
    }
    // Create transforms
    btTransform frameInA, frameInB;
    frameInA.setOrigin(toBullet(params.pivotA));
    frameInA.setRotation(toBullet(params.rotA));
    frameInB.setOrigin(toBullet(params.pivotB));
    frameInB.setRotation(toBullet(params.rotB));

    btGeneric6DofConstraint* dof6 = new btGeneric6DofConstraint(
        *rbA, *rbB, frameInA, frameInB, true
    );

    // Apply linear limits
     btVector3 linearLower(0, 0, 0);
    btVector3 linearUpper(0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        if (params.useLinearLimits[i]) {
            linearLower[i] = params.lowerLinearLimit[i];
            linearUpper[i] = params.upperLinearLimit[i];
        }
    }
    dof6->setLinearLowerLimit(linearLower);
    dof6->setLinearUpperLimit(linearUpper);

    // Apply angular limits
    btVector3 angularLower(0, 0, 0);
    btVector3 angularUpper(0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        if (params.useAngularLimits[i]) {
            angularLower[i] = params.lowerAngularLimit[i];
            angularUpper[i] = params.upperAngularLimit[i];
        }
    }
    dof6->setAngularLowerLimit(angularLower);
    dof6->setAngularUpperLimit(angularUpper);

    auto constraint = std::make_unique<Constraint>(
        dof6, ConstraintType::GENERIC_6DOF, objA, objB
    );
    constraint->setFrames(frameInA, frameInB, true);
    std::cout << "Created GENERIC_6DOF constraint" << std::endl;

    return constraint;
}