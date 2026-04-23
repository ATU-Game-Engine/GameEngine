#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include "ConstraintParams.h"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class GameObject;

class Constraint {
private:
    btTypedConstraint* constraint; 
    ConstraintType type;
    GameObject* bodyA;
    GameObject* bodyB;
    std::string name;

    // Cache for reconstruction
    btTransform frameInA;
    btTransform frameInB;
    bool useLinearReferenceFrameA;

    bool breakable;
    float breakForce;
    float breakTorque;

    // Parameter Structs 
    HingeParams hingeParams;
    SliderParams sliderParams;
    SpringParams springParams;

public:
    Constraint(btTypedConstraint* bulletConstraint,
        ConstraintType type,
        GameObject* objA,
        GameObject* objB = nullptr);

    ~Constraint();

    Constraint(const Constraint&) = delete;
    Constraint& operator=(const Constraint&) = delete;

	//rebuilding constraint when a body is resized or replaced - updates the cached frames and rebuilds the bullet constraint
    void setFrames(const btTransform& fA, const btTransform& fB, bool useA = true);
    void rebuild();

    // Getters
    btTypedConstraint* getBulletConstraint() const { return constraint; }
    ConstraintType getType() const { return type; }
    GameObject* getBodyA() const { return bodyA; }
    GameObject* getBodyB() const { return bodyB; }
    const std::string& getName() const { return name; }
    bool isBroken() const;
    bool isBreakable() const { return breakable; }
    float getBreakForce() const { return breakForce; }
    float getBreakTorque() const { return breakTorque; }

    const btTransform& getFrameInA() const { return frameInA; }
    const btTransform& getFrameInB() const { return frameInB; }
    bool getUseLinearReferenceFrameA() const { return useLinearReferenceFrameA; }
    const HingeParams& getHingeParams()  const { return hingeParams; }
    const SliderParams& getSliderParams() const { return sliderParams; }
    const SpringParams& getSpringParams() const { return springParams; }

    // Setters
    void setName(const std::string& newName) { name = newName; }
    void setEnabled(bool enabled);
    void setBreakingThreshold(float force, float torque);

    // Hinge controls
    void setAngleLimits(float lower, float upper);
    void enableMotor(float targetVelocity, float maxImpulse);
    void disableMotor();
    float getHingeAngle() const;

    // Slider controls
    void setLinearLimits(float lower, float upper);
    void enableLinearMotor(float targetVelocity, float maxForce);
    float getSliderPosition() const;

    // Spring controls
    void setSpringStiffness(int axis, float stiffness);
    void setSpringDamping(int axis, float damping);

    // Debug
    void printInfo() const;

};

// Helper conversion functions
inline btVector3 toBullet(const glm::vec3& v) {
    return btVector3(v.x, v.y, v.z);
}

inline btQuaternion toBullet(const glm::quat& q) {
    return btQuaternion(q.x, q.y, q.z, q.w);
}

inline glm::vec3 fromBullet(const btVector3& v) {
    return glm::vec3(v.x(), v.y(), v.z());
}

inline glm::quat fromBullet(const btQuaternion& q) {
    return glm::quat(q.w(), q.x(), q.y(), q.z());
}

#endif // CONSTRAINT_H