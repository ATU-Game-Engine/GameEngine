/**
 * @file Physics.h
 * @brief Declares the Physics class — the engine's wrapper around the Bullet
 *        Physics library.
 *
 * Physics owns all Bullet infrastructure (dynamics world, dispatcher,
 * broadphase, solver, collision shapes, rigid bodies) and is responsible for
 * their correct initialisation and teardown order. It also coordinates the
 * per-tick updates for ConstraintRegistry and TriggerRegistry.
 *
 * Typical usage:
 *   Physics physics;
 *   physics.initialize();                    // Create Bullet world
 *   btRigidBody* body = physics.createRigidBody(...);
 *   // per-frame:
 *   physics.update(1.0f / 60.0f);            // Step simulation
 *   physics.cleanup();                       // Shutdown
 *
 * Scene calls resizeRigidBody() when an object's collision shape changes
 * (e.g. via the inspector scale tool), which handles the body swap and
 * constraint rebuild automatically.
 */
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

/**
 * @brief Manages the Bullet Physics simulation world and all rigid bodies.
 *
 * Owns the full Bullet pipeline (collisionConfiguration → dispatcher →
 * broadphase → solver → dynamicsWorld) and tracks every rigid body and
 * collision shape for clean teardown. Scripts and gameplay code access
 * raycasting via getQuerySystem().
 */
class Physics {
private:
    // Core Bullet components
    btDefaultCollisionConfiguration* collisionConfiguration; ///< Memory and collision setup.
    btCollisionDispatcher* dispatcher; ///< Narrows collision pairs.
    btBroadphaseInterface* broadphase;  ///< Coarse pair culling (DBVT).
    btSequentialImpulseConstraintSolver* solver; ///< Single-threaded impulse solver.
    btDiscreteDynamicsWorld* dynamicsWorld;  ///< The active simulation world.

    std::unique_ptr<PhysicsQuery> querySystem;  ///< Raycast and overlap query wrapper.
    // store created rigid bodies and shapes in collections for cleanup
    std::vector<btRigidBody*> rigidBodies; ///< All created bodies — freed in cleanup().
    std::vector<btCollisionShape*> collisionShapes; ///< All created shapes — freed in cleanup()

    /**
     * @brief Applies friction and restitution from a PhysicsMaterial to a body.
     *
     * Called internally by createRigidBody() immediately after construction.
     *
     * @param body      The rigid body to configure.
     * @param material  Material whose properties should be applied.
     */
    void applyMaterial(btRigidBody* body, const PhysicsMaterial& material);

public:
    /**
     * @brief Constructs the Physics system with all Bullet pointers null.
     *
     * Call initialize() before any other methods.
     */
    Physics();

    /**
    * @brief Destructor — calls cleanup() to release all Bullet resources.
    */
    ~Physics();

    // Initialize the physics world and ground plane

     /**
     * @brief Creates and configures the Bullet dynamics world.
     *
     * Initialises the full Bullet pipeline, sets gravity to -9.8 m/s² on Y,
     * constructs the PhysicsQuery system, and initialises ConstraintRegistry
     * and TriggerRegistry with the new world pointer. Must be called once
     * before any rigid bodies are created or update() is called.
     */
    void initialize();

    // Clean up physics resources

    /**
     * @brief Destroys all physics resources in the correct teardown order.
     *
     * Clears constraints and triggers via their registries, removes and deletes
     * all rigid bodies and collision shapes, then deletes the Bullet pipeline
     * components. Safe to call multiple times. Called automatically by the
     * destructor.
     */
    void cleanup();
    
	// create a rigid body with a material
    /**
     * @brief Creates a rigid body with a named physics material.
     *
     * Allocates the appropriate Bullet collision shape for `type`, computes
     * local inertia for dynamic bodies (mass > 0), constructs the btRigidBody,
     * applies material friction/restitution, and adds it to the dynamics world.
     *
     * Shape size interpretation:
     *   - CUBE:    size = full extents (Bullet receives half-extents internally).
     *   - SPHERE:  size.x = radius; size.y and size.z are ignored.
     *   - CAPSULE: size.x = radius; size.y = total height (cylinder height =
     *              size.y - 2 * size.x, clamped to 0.1 minimum).
     *
     * @param type          Collision shape type (CUBE, SPHERE, CAPSULE).
     * @param position      Initial world-space position.
     * @param size          Shape dimensions (see above).
     * @param mass          Mass in kg. 0 = static (immovable) body.
     * @param materialName  Name registered with MaterialRegistry. Falls back to
     *                      "Default" if not found.
     * @return              Pointer to the new rigid body, owned by this Physics instance.
     */
    btRigidBody* createRigidBody(ShapeType type,
        const glm::vec3& position,
        const glm::vec3& size,
        float mass,
        const std::string& materialName = "Default");


	// create a rigid body without a material
     /**
     * @brief Creates a rigid body using the "Default" physics material.
     *
     * Convenience overload that forwards to the full signature with
     * materialName = "Default".
     *
     * @param type      Collision shape type.
     * @param position  Initial world-space position.
     * @param size      Shape dimensions.
     * @param mass      Mass in kg. 0 = static body.
     * @return          Pointer to the new rigid body.
     */
    btRigidBody* createRigidBody(
        ShapeType type,
        const glm::vec3& position,
        const glm::vec3& size,
        float mass
    );

    // Rigid body management
     /**
     * @brief Replaces a GameObject's rigid body with one of a new size.
     *
     * Required when the collision shape dimensions change (e.g. after a scale
     * edit in the inspector). The operation:
     *   1. Saves world transform, velocities, and damping from the old body.
     *   2. Detaches constraints from Bullet (without losing registry data).
     *   3. Removes and destroys the old body.
     *   4. Creates a new body at the saved position with the new scale.
     *   5. Restores all saved state.
     *   6. Calls ConstraintRegistry::rebuildConstraintsForObject() to reconnect
     *      all joints to the new body pointer.
     *
     * @param owner        GameObject whose body is being replaced. Must have a
     *                     valid physics component and rigid body.
     * @param type         Collision shape type for the new body.
     * @param newScale     New collision shape dimensions.
     * @param mass         Mass for the new body (typically same as old body).
     * @param materialName Physics material name for the new body.
     * @return             Pointer to the new rigid body, or nullptr on failure.
     */
    btRigidBody* resizeRigidBody(
        GameObject* owner,
        ShapeType type,
        const glm::vec3& newScale,
        float mass,
        const std::string& materialName
    );

    //delete old rigid bodies
     /**
     * @brief Removes a rigid body from the simulation and frees all its memory.
     *
     * Removal order: world → collision shape → tracking vectors → motion state
     * → body. The collision shape is also deleted here to prevent orphaned
     * shapes accumulating in the broadphase cache and causing stale-pointer
     * crashes across scene reloads.
     *
     * @param body  The rigid body to remove. No-op if null or world is null.
     */
    void removeRigidBody(btRigidBody* body);

    //querys
     /**
     * @brief Returns the PhysicsQuery system for raycasting and overlap queries.
     *
     * Scripts access this via Physics::getQuerySystem() to perform raycasts,
     * ground checks, and line-of-sight tests without needing direct Bullet access.
     */
    PhysicsQuery& getQuerySystem() { return *querySystem; }
    const PhysicsQuery& getQuerySystem() const { return *querySystem; }

    // Getters for physics world (useful when adding objects later)
    /**
     * @brief Returns the underlying Bullet dynamics world.
     *
     * Exposed for registries (ConstraintRegistry, TriggerRegistry) that need
     * direct world access during initialisation. Prefer higher-level APIs for
     * gameplay code.
     */
    btDiscreteDynamicsWorld* getWorld() { return dynamicsWorld; }


    // Step the physics simulation by fixed deltaTime (always 1/60s)
    // This should be called from Engine's fixed timestep loop
     /**
     * @brief Advances the physics simulation by one fixed timestep.
     *
     * Called by Engine at a fixed rate of 1/60 s. maxSubSteps is 1 because
     * Engine's fixed-timestep loop already ensures the delta never exceeds
     * fixedDeltaTime. After stepping, ticks ConstraintRegistry (broken joint
     * detection) and TriggerRegistry (enter/exit/stay callbacks).
     *
     * @param fixedDeltaTime  Fixed simulation timestep in seconds (always 1/60).
     */
    void update(float fixedDeltaTime);


    // Get number of active rigid bodies
    /**
     * @brief Returns the number of collision objects currently in the world.
     *
     * Includes rigid bodies and ghost objects (triggers). Useful for
     * performance monitoring.
     *
     * @return  Object count, or 0 if the world has not been initialised.
     */
    int getRigidBodyCount() const;

	

};


#endif // PHYSICS_H