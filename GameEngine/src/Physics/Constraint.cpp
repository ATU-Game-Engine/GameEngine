#include "../include/Physics/Constraint.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <cmath>

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

Constraint::~Constraint() {
    if (constraint) {
        delete constraint;
        constraint = nullptr;
    }
}
void Constraint::setFrames(const btTransform& fA, const btTransform& fB, bool useA) {
    frameInA = fA;
    frameInB = fB;
    useLinearReferenceFrameA = useA;
}

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

// --- Hinge Implementation ---
// Getters

bool Constraint::isBroken() const {
    return constraint ? !constraint->isEnabled() : true;
}

// Setters

void Constraint::setEnabled(bool enabled) {
    if (constraint) {
        constraint->setEnabled(enabled);
    }
}

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


// ========== Type-Specific Controls (Hinge) ==========

void Constraint::setAngleLimits(float lower, float upper) {
    hingeParams.useLimits = true;
    hingeParams.lowerLimit = lower;
    hingeParams.upperLimit = upper;

    if (type == ConstraintType::HINGE && constraint) {
        static_cast<btHingeConstraint*>(constraint)->setLimit(lower, upper);
    }
}

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

void Constraint::disableMotor() {
    hingeParams.useMotor = false;
    if (type == ConstraintType::HINGE && constraint) {
        static_cast<btHingeConstraint*>(constraint)->enableMotor(false);
    }
}

float Constraint::getHingeAngle() const {
    if (type == ConstraintType::HINGE && constraint) {
        return static_cast<btHingeConstraint*>(constraint)->getHingeAngle();
    }
    return 0.0f;
}

// ========== Type-Specific Controls (Slider) ==========

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

float Constraint::getSliderPosition() const {
    if (type == ConstraintType::SLIDER && constraint) {
        return static_cast<btSliderConstraint*>(constraint)->getLinearPos();
    }
    return 0.0f;
}

// ========== Type-Specific Controls (Spring) ==========

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

void Constraint::setSpringDamping(int axis, float damping) {
    if (axis < 0 || axis >= 6) return;
    springParams.damping[axis] = damping;

    if (type == ConstraintType::SPRING && constraint) {
        static_cast<btGeneric6DofSpringConstraint*>(constraint)->setDamping(axis, damping);
    }
}

// ========== Debug ==========

void Constraint::printInfo() const {
    std::cout << "Constraint: " << name << " [Type: " << (int)type << "]" << std::endl;
    std::cout << " - Body A: " << (bodyA ? bodyA->getName() : "None") << std::endl;
    std::cout << " - Body B: " << (bodyB ? bodyB->getName() : "World") << std::endl;
    std::cout << " - Status: " << (isBroken() ? "Broken/Disabled" : "Active") << std::endl;
}