#include "../include/Physics/Physics.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/PhysicsMaterial.h"
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Physics/PhysicsQuery.h"
#include "../include/Physics/TriggerRegistry.h" 
#include <iostream>


Physics::Physics(): 
    collisionConfiguration(nullptr), 
    dispatcher(nullptr), 
    broadphase(nullptr),
    solver(nullptr),
    dynamicsWorld(nullptr)
{
}

Physics::~Physics() {
    cleanup();
}

void Physics::initialize() {
    std::cout << "Initializing Bullet Physics..." << std::endl;

    // Initialize material registry
    MaterialRegistry::getInstance().initializeDefaults();

	//create collision configuration, default memory and collision setup
    collisionConfiguration = new btDefaultCollisionConfiguration();


    //2 phase pipeline, broadphase and narrowphase
    //create collision dispatcher
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    //create general purpose broadphase 
    broadphase = new btDbvtBroadphase();


    //create constraint solver for contact resolution(doesnt use pararrel proccesing)
    solver = new btSequentialImpulseConstraintSolver();

    //create the dynamics world 
    dynamicsWorld = new btDiscreteDynamicsWorld(
        dispatcher,
        broadphase,
        solver,
        collisionConfiguration
    );

    //Set gravity (9.8 m/s² downward)
    dynamicsWorld->setGravity(btVector3(0, -9.8, 0));

    std::cout << "Physics world created with gravity: (0, -1.8, 0)" << std::endl;

    querySystem = std::make_unique<PhysicsQuery>(dynamicsWorld);  
    // Initialize constraint registry with our dynamics world
    ConstraintRegistry::getInstance().initialize(dynamicsWorld);
	// Initialize trigger registry with our dynamics world
    TriggerRegistry::getInstance().initialize(dynamicsWorld);
    std::cout << "Physics initialized successfully" << std::endl;
}
// create a rigid body without a material
btRigidBody* Physics::createRigidBody(ShapeType type,
    const glm::vec3& position,
    const glm::vec3& size,
    float mass) {
    return createRigidBody(type, position, size, mass, "Default");
}
// create a rigid body with a material
btRigidBody* Physics::createRigidBody(
    ShapeType type,
    const glm::vec3& position,
    const glm::vec3& size,
    float mass,
    const std::string& materialName)
{
    btCollisionShape* shape = nullptr;

    // Create collision shape
    switch (type) {
    case ShapeType::CUBE:
        // Bullet uses halfs for box shapes size
        shape = new btBoxShape(btVector3(size.x / 2.0f, size.y / 2.0f, size.z / 2.0f));
        std::cout << "Created box collider: " << size.x << "x" << size.y << "x" << size.z << std::endl;
        break;
    case ShapeType::SPHERE:
        // size.x = radius
        shape = new btSphereShape(size.x);
        std::cout << "Created sphere collider: radius=" << size.x << std::endl;
        break;
    case ShapeType::CAPSULE:{
        // Total = cylinderHeight + 2*radius, so: cylinderHeight = total - 2*radius
        float totalHeight = size.y;
        float cylinderHeight = totalHeight - 2.0f * size.x;

        // Clamp to prevent negative/zero cylinder height
        if (cylinderHeight < 0.1f) cylinderHeight = 0.1f;

        shape = new btCapsuleShape(size.x, cylinderHeight);
        std::cout << "Created capsule collider: radius=" << size.x << std::endl;
        break;
    }
    default:
        std:: cerr << "Unknown ShapeType for rigid body creation! defaulting to cube" << std::endl;
        shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        break;

    }

    collisionShapes.push_back(shape);

    // Set initial transform
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(position.x, position.y, position.z));

    // Calculate inertia for dynamic objects
    btVector3 localInertia(0, 0, 0);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, localInertia);
    }

    // Create motion state
    btDefaultMotionState* motionState = new btDefaultMotionState(transform);

    // Create rigid body
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
    btRigidBody* body = new btRigidBody(rbInfo);

	// Apply material properties - create material var and get instance of material from registry
    const PhysicsMaterial& material = MaterialRegistry::getInstance().getMaterial(materialName);
    applyMaterial(body, material);

    dynamicsWorld->addRigidBody(body);
    rigidBodies.push_back(body);

    return body;
}


btRigidBody* Physics::resizeRigidBody(
    GameObject* owner,
    ShapeType type,
    const glm::vec3& newScale,
    float mass,
    const std::string& materialName)
{
    if (!owner || !owner->getRigidBody()) return nullptr;

    btRigidBody* oldBody = owner->getRigidBody();
    // Save current state
    btTransform transform;
    oldBody->getMotionState()->getWorldTransform(transform);
    btVector3 linearVel = oldBody->getLinearVelocity();
    btVector3 angularVel = oldBody->getAngularVelocity();


    bool isActive = oldBody->isActive();
    float linearDamping = oldBody->getLinearDamping();
    float angularDamping = oldBody->getAngularDamping();

    std::cout << "Resizing rigid body at ("
        << transform.getOrigin().x() << ", "
        << transform.getOrigin().y() << ", "
        << transform.getOrigin().z() << ")" << std::endl;
	// Detach constraints before removing the body, so they can be properly rebuilt with the new body
    ConstraintRegistry::getInstance().detachConstraintsFromWorld(owner);
    //  Remove old body
    removeRigidBody(oldBody);
    

    //Create new body with new scale
    btRigidBody* newBody = createRigidBody(
        type,
        glm::vec3(transform.getOrigin().x(),
            transform.getOrigin().y(),
            transform.getOrigin().z()),
        newScale,  // NEW SCALE
        mass,
        materialName
    );

    if (!newBody) {
        std::cerr << "Error: Failed to create new rigid body during resize" << std::endl;
        return nullptr;
    }

    //Restore saved state
    newBody->setWorldTransform(transform);
    newBody->getMotionState()->setWorldTransform(transform);
    newBody->setLinearVelocity(linearVel);
    newBody->setAngularVelocity(angularVel);
    newBody->setDamping(linearDamping, angularDamping);

    if (isActive) {
        newBody->activate(true);
    }
    owner->setRigidBody(newBody);
    std::cout << "Rigid body resized successfully" << std::endl;
    ConstraintRegistry::getInstance().rebuildConstraintsForObject(owner);
    return newBody;
}

void Physics::removeRigidBody(btRigidBody* body) {
    if (!body || !dynamicsWorld) return;

    // Must remove from world BEFORE deleting anything
    dynamicsWorld->removeRigidBody(body);

    // Remove and delete the collision shape - this is the missing step.
    // Without this, orphaned shapes accumulate and Bullet's broadphase
    // cache can hold stale references to freed shape memory → crash.
    btCollisionShape* shape = body->getCollisionShape();
    if (shape) {
        auto shapeIt = std::find(collisionShapes.begin(), collisionShapes.end(), shape);
        if (shapeIt != collisionShapes.end()) {
            collisionShapes.erase(shapeIt);
        }
        delete shape;
    }

    // Remove body from tracking vector
    auto it = std::find(rigidBodies.begin(), rigidBodies.end(), body);
    if (it != rigidBodies.end()) {
        rigidBodies.erase(it);
    }

    delete body->getMotionState();
    delete body;
}

void Physics::applyMaterial(btRigidBody* body, const PhysicsMaterial& material) {
    if (!body) return;

    // Set friction (surface grip)
    body->setFriction(material.friction);

    // Set restitution (bounciness)
    body->setRestitution(material.restitution);

    // Density is used at creation time to calculate mass
    std::cout << " Applied '" << material.name << "': "
        << "friction=" << body->getFriction()
        << ", restitution=" << body->getRestitution() << std::endl;
    
}

void Physics::update(float fixedDeltaTime) {
    if (!dynamicsWorld) return;

    //Step the simulation by exactly fixedDeltaTime (should always be 1/60s)
    //maxSubSteps = 1 because Engine.cpp already handles the fixed timestep loop
    //This just advances physics by one fixed step
    dynamicsWorld->stepSimulation(fixedDeltaTime, 1, fixedDeltaTime);
    // Update constraints (check for broken constraints)
    ConstraintRegistry::getInstance().update();
	// Update triggers (check for enter/exit events)
    TriggerRegistry::getInstance().update(fixedDeltaTime);
}

int Physics::getRigidBodyCount() const {
    if (!dynamicsWorld) return 0;
    return dynamicsWorld->getNumCollisionObjects();
}

void Physics::cleanup() {
    if (!dynamicsWorld) return ;

    std::cout << "Cleaning up physics..." << std::endl;
    
    // Clear all constraints first (via registry)
    ConstraintRegistry::getInstance().clearAll();
    // Clear all triggers
    TriggerRegistry::getInstance().clearAll();

    // delete all rigid bodies (including ground)
    for (btRigidBody* body : rigidBodies) {
        dynamicsWorld->removeRigidBody(body);
        delete body->getMotionState();
        delete body;
    }
    rigidBodies.clear();

    // delete all collision shapes
    for (btCollisionShape* shape : collisionShapes) {
        delete shape;
    }
    collisionShapes.clear();

    //Delete dynamics world and components
    delete dynamicsWorld;
    delete solver;
    delete broadphase;
    delete dispatcher;
    delete collisionConfiguration;

    dynamicsWorld = nullptr;
    solver = nullptr;
    broadphase = nullptr;
    dispatcher = nullptr;
    collisionConfiguration = nullptr;

    std::cout << "Physics cleaned up" << std::endl;
}