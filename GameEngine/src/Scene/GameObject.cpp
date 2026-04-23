#include "../include/Scene/GameObject.h"
#include "../include/Scene/ScriptComponent.h"
#include <iostream>

/**
 * @brief Constructs a new GameObject with physics and rendering data.
 *
 * Creates a game object that wraps a Bullet Physics rigid body and stores
 * rendering information like shape, texture, and material. The GameObject
 * does not own the rigid body - it's managed by the Physics system.
 *
 * @param type The collision shape type (CUBE, SPHERE, or CAPSULE)
 * @param body Pointer to the Bullet rigid body (owned by Physics system, can be nullptr for render-only objects)
 * @param scale Object dimensions (half-extents for cubes, radius for spheres, radius+height for capsules)
 * @param materialName Physics material name for friction/restitution properties (default: "Default")
 * @param texturePath Path to texture file relative to executable (default: "" = use default orange color)
 *
 * @note Position and rotation are initialized to origin and identity - they get updated from
 *       physics on the first updateFromPhysics() call.
 */

uint64_t GameObject::nextID = 1;


GameObject::GameObject(ShapeType type, btRigidBody* body, const glm::vec3& scale,
                       const std::string& materialName, const std::string& texturePath)
     : transform(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale),
      render(type, texturePath),
      physicsScale(glm::vec3(1.0f, 1.0f, 1.0f))
     {

        id = nextID++;
    // Create physics component if body is provided
    if (body) {
        physics = std::make_unique<PhysicsComponent>(body, materialName);
        physics->setOwner(this);
        
    }

    transform.setOwner(this);
    render.setOwner(this);
}

/**
 * @brief Creates a GameObject WITHOUT physics (render-only).
 */
GameObject::GameObject(ShapeType type, const glm::vec3& position,
    const glm::vec3& scale, const std::string& texturePath)
    : transform(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale),
    render(type, texturePath),
    physics(nullptr),
    physicsScale(glm::vec3(1.0f, 1.0f, 1.0f))
{
    id = nextID++;

    transform.setOwner(this);
    render.setOwner(this);
}

/**
 * @brief Destroys the GameObject.
 *
 * Components are automatically cleaned up via unique_ptr.
 * 
 */
GameObject::~GameObject() {
    // Physics cleanup handled by Physics system
}


/**
 * @brief Synchronizes GameObject transform with its physics rigid body.
 *
 * Pulls the current position and rotation from the Bullet physics simulation
 * and updates the GameObject's cached transform. This should be called every
 * frame after physics simulation has stepped.
 *
 * The flow is:
 * 1. Physics system simulates forces, collisions, constraints
 * 2. updateFromPhysics() reads the new transform from Bullet
 * 3. Renderer uses the updated position/rotation to draw the object
 *
 * @note Does nothing if rigidBody is nullptr (for render-only objects without physics)
 * @note Uses the motion state to get interpolated transforms for smooth rendering
 */
void GameObject::updateFromPhysics() {
    if (physics) {
        physics->syncToTransform(transform);
    }
}

/**
 * @brief Sets the object's position in world space.
 *
 * Updates both the cached position AND the physics rigid body (if present).
 * This is essential because physics simulation will override any changes
 * that aren't also written to the rigid body.
 *
 * Use cases:
 * - Editor: Moving objects with the transform gizmo
 * - Gameplay: Teleporting player/objects to spawn points
 * - Respawn: Resetting objects to initial positions
 *
 * @param newPos The new world position
 *
 * @note If the object has physics, this wakes up the rigid body so the change
 *       takes effect immediately and the object participates in collision detection.
 * @note Preserves the current rotation - only position is changed.
 */
void GameObject::setPosition(const glm::vec3& newPos)
{
    transform.setPosition(newPos);

    // Sync to physics if present
    if (physics) {
        physics->syncFromTransform(transform);
    }
    
}
void GameObject::setPhysicsScale(const glm::vec3& scale) {
    physicsScale = scale;
    // Actual physics body resizing happens in Scene::setObjectPhysicsScale This just stores the value
  
}
//This only updates visual scale
// Physics collision shape is not resized.
// Use Scene::setObjectScale() instead to properly update physics.
void GameObject::setScale(const glm::vec3& newScale)
{
    transform.setScale(newScale);
}

void GameObject::setRotation(const glm::quat& newRot)
{
    transform.setRotation(newRot);
    // Sync to physics if present
    if (physics) {
        physics->syncFromTransform(transform);
    }
}


// scripts

void GameObject::updateScripts(float dt)
{
    for (auto& script : scripts)
        if (!script->pendingRemoval)
            script->onUpdate(dt);

    scripts.erase(
        std::remove_if(scripts.begin(), scripts.end(),
            [](const std::unique_ptr<ScriptComponent>& s) {
                if (s->pendingRemoval) { s->onDestroy(); return true; }
                return false;
            }),
        scripts.end()
    );
}

void GameObject::fixedUpdateScripts(float fixedDt)
{
    for (auto& script : scripts)
        if (!script->pendingRemoval)
            script->onFixedUpdate(fixedDt);
}

void GameObject::notifyDestroy()
{
    for (auto& script : scripts)
        script->onDestroy();
}
