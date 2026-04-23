#ifndef PHYSICS_H
#define PHYSICS_H

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>  
#include <string>
#include "../include/Physics/PhysicsQuery.h"

enum class ShapeType;

class PhysicsMaterial;


class Physics {
private:
    // Core Bullet components
    btDefaultCollisionConfiguration* collisionConfiguration;
    btCollisionDispatcher* dispatcher;
    btBroadphaseInterface* broadphase;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* dynamicsWorld;

    std::unique_ptr<PhysicsQuery> querySystem;
    // store created rigid bodies and shapes in collections for cleanup
    std::vector<btRigidBody*> rigidBodies;
    std::vector<btCollisionShape*> collisionShapes;

    void applyMaterial(btRigidBody* body, const PhysicsMaterial& material);

public:
    Physics();
    ~Physics();

    // Initialize the physics world and ground plane
    void initialize();

    // Clean up physics resources
    void cleanup();
    
	// create a rigid body with a material
    btRigidBody* createRigidBody(ShapeType type,
        const glm::vec3& position,
        const glm::vec3& size,
        float mass,
        const std::string& materialName = "Default");

	// create a rigid body without a material
    btRigidBody* createRigidBody(
        ShapeType type,
        const glm::vec3& position,
        const glm::vec3& size,
        float mass
    );

    // Rigid body management
    btRigidBody* resizeRigidBody(
        GameObject* owner,
        ShapeType type,
        const glm::vec3& newScale,
        float mass,
        const std::string& materialName
    );

    //delete old rigid bodies
    void removeRigidBody(btRigidBody* body);

    //querys
    PhysicsQuery& getQuerySystem() { return *querySystem; }
    const PhysicsQuery& getQuerySystem() const { return *querySystem; }

    // Getters for physics world (useful when adding objects later)
    btDiscreteDynamicsWorld* getWorld() { return dynamicsWorld; }

    // Step the physics simulation by fixed deltaTime (always 1/60s)
    // This should be called from Engine's fixed timestep loop
    void update(float fixedDeltaTime);

    // Get number of active rigid bodies
    int getRigidBodyCount() const;

	

};


#endif // PHYSICS_H