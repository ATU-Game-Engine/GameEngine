/**
 * @file Physics.cpp
 * @brief Implementation of the Physics system, which wraps the Bullet Physics
 *        library and provides rigid body creation, resizing, removal, and
 *        per-tick simulation stepping.
 *
 * The Physics class owns all Bullet infrastructure (world, dispatcher,
 * broadphase, solver, collision shapes, rigid bodies) and is responsible for
 * their correct initialisation and teardown order. It also coordinates with
 * ConstraintRegistry and TriggerRegistry so those systems are updated each
 * physics tick and cleaned up on shutdown.
 *
 * Typical per-frame flow (managed by Engine):
 *   1. Engine accumulates time and calls Physics::update() at a fixed 1/60 s rate.
 *   2. Physics::update() steps Bullet, then ticks ConstraintRegistry and
 *      TriggerRegistry.
 *   3. Scene::update() syncs GameObject transforms from the updated rigid bodies.
 */
#include "../include/Physics/Physics.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/PhysicsMaterial.h"
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Physics/PhysicsQuery.h"
#include "../include/Physics/TriggerRegistry.h" 
#include <iostream>

 /**
  * @brief Constructs the Physics system with all Bullet pointers null.
  *
  * Call initialize() before using any other methods.
  */
Physics::Physics(): 
    collisionConfiguration(nullptr), 
    dispatcher(nullptr), 
    broadphase(nullptr),
    solver(nullptr),
    dynamicsWorld(nullptr)
{
}

/**
 * @brief Destructor — calls cleanup() to release all Bullet resources.
 */
Physics::~Physics() {
    cleanup();
}

/**
 * @brief Creates and configures the Bullet physics world.
 *
 * Initialisation order follows Bullet's requirements:
 *   1. MaterialRegistry defaults are seeded so createRigidBody() can look
 *      up materials immediately.
 *   2. Collision pipeline components are created (configuration, dispatcher,
 *      broadphase, solver).
 *   3. The btDiscreteDynamicsWorld is constructed from those components.
 *   4. Gravity is set to -9.8 m/s² on the Y axis.
 *   5. PhysicsQuery, ConstraintRegistry, and TriggerRegistry are initialised
 *      with the new world pointer.
 *
 * Must be called once before any calls to createRigidBody() or update().
 */
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
/**
 * @brief Creates a rigid body using the "Default" physics material.
 *
 * Convenience overload that forwards to the full signature with materialName
 * set to "Default". Use when material properties do not matter or are handled
 * separately after creation.
 *
 * @param type      Collision shape type (CUBE, SPHERE, CAPSULE).
 * @param position  Initial world-space position.
 * @param size      Shape dimensions (half-extents for CUBE, radius for SPHERE,
 *                  radius + total height for CAPSULE).
 * @param mass      Mass in kg. 0 creates a static (immovable) body.
 * @return          Pointer to the new rigid body, owned by this Physics system.
 */
btRigidBody* Physics::createRigidBody(ShapeType type,
    const glm::vec3& position,
    const glm::vec3& size,
    float mass) {
    return createRigidBody(type, position, size, mass, "Default");
}

// create a rigid body with a material
/**
 * @brief Creates a rigid body with a named physics material.
 *
 * Allocates a Bullet collision shape appropriate to `type`, computes local
 * inertia for dynamic bodies, constructs the btRigidBody, applies material
 * properties (friction, restitution), and registers it with the dynamics world.
 *
 * Shape size interpretation:
 *   - CUBE:    size is full extents; Bullet receives half-extents internally.
 *   - SPHERE:  size.x is the radius; size.y and size.z are ignored.
 *   - CAPSULE: size.x is the radius; size.y is the total height (cylinder
 *              height = size.y - 2 * size.x, clamped to 0.1 minimum).
 *
 * Both the collision shape and the rigid body pointer are stored internally
 * for cleanup in removeRigidBody() and cleanup().
 *
 * @param type          Collision shape type.
 * @param position      Initial world-space position.
 * @param size          Shape dimensions (see above).
 * @param mass          Mass in kg. 0 = static body (infinite mass).
 * @param materialName  Name of a material registered with MaterialRegistry.
 *                      Falls back to "Default" if the name is not found.
 * @return              Pointer to the new rigid body, owned by this Physics system.
 */
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

/**
 * @brief Replaces a GameObject's rigid body with a new one at a different scale.
 *
 * Used when the visual scale of an object changes and the collision shape must
 * match. The operation:
 *   1. Saves the current world transform, velocities, and damping from the old body.
 *   2. Detaches constraints from the Bullet world (without removing them from
 *      the registry) so they can be rebuilt with the new body pointer.
 *   3. Removes and destroys the old rigid body.
 *   4. Creates a new rigid body at the saved position with the new scale.
 *   5. Restores all saved state to the new body.
 *   6. Updates the GameObject's physics component to point at the new body.
 *   7. Triggers ConstraintRegistry::rebuildConstraintsForObject() to reconnect
 *      all joints to the new body pointer.
 *
 * @param owner        The GameObject whose body is being replaced. Must have a
 *                     valid physics component and rigid body.
 * @param type         Collision shape type for the new body.
 * @param newScale     New collision shape dimensions.
 * @param mass         Mass for the new body (typically same as old).
 * @param materialName Physics material name for the new body.
 * @return             Pointer to the new rigid body, or nullptr on failure.
 */
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

/**
 * @brief Removes a rigid body from the simulation and frees all associated memory.
 *
 * Removal order is important:
 *   1. Remove from the Bullet dynamics world first — accessing a body after
 *      removal from the world causes undefined behaviour.
 *   2. Delete and untrack the collision shape — orphaned shapes accumulate in
 *      the broadphase cache and can cause stale-pointer crashes on reload.
 *   3. Remove from the internal tracking vectors.
 *   4. Delete the motion state and the rigid body itself.
 *
 * @param body  The rigid body to remove. No-op if null or world is null.
 */
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

// MAaterial application helper
/**
 * @brief Applies friction and restitution from a PhysicsMaterial to a body.
 *
 * Called internally by createRigidBody() after the body is constructed.
 * Density from the material is used at mass-calculation time, not here.
 *
 * @param body      The rigid body to configure. No-op if null.
 * @param material  The material whose properties should be applied.
 */
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

/**
 * @brief Advances the physics simulation by one fixed timestep.
 *
 * Called by Engine at a fixed rate of 1/60 s. maxSubSteps is set to 1
 * because the Engine's fixed-timestep loop already ensures the delta never
 * exceeds fixedDeltaTime — Bullet does not need to subdivide further.
 *
 * After stepping, ConstraintRegistry::update() checks for broken joints and
 * TriggerRegistry::update() fires enter/exit/stay callbacks for all triggers.
 *
 * @param fixedDeltaTime  The fixed simulation timestep in seconds (always 1/60).
 */
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

/**
 * @brief Returns the number of collision objects currently in the dynamics world.
 *
 * Includes both rigid bodies and ghost objects (triggers). Useful for
 * performance monitoring and debug overlays.
 *
 * @return  Object count, or 0 if the world has not been initialised.
 */
int Physics::getRigidBodyCount() const {
    if (!dynamicsWorld) return 0;
    return dynamicsWorld->getNumCollisionObjects();
}

/**
 * @brief Destroys all physics resources in the correct teardown order.
 *
 * Constraints and triggers are cleared via their registries first, then all
 * rigid bodies are removed from the world and deleted, then collision shapes
 * are deleted, and finally the Bullet world and pipeline components are
 * destroyed and their pointers nulled.
 *
 * Safe to call multiple times — the early-out guard on dynamicsWorld prevents
 * double-free. Called automatically by the destructor.
 */
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