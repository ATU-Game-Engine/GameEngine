#include <GL/glew.h> 
#include "../include/Scene/Scene.h"
#include "../include/Core/GameTime.h"
#include "../include/Core/Engine.h"
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Rendering/MeshFactory.h"
#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/Trigger.h" 
#include "../include/Physics/ForceGeneratorRegistry.h"
#include "../include/Physics/ForceGenerator.h"
#include "../include/Rendering/PointLightRegistry.h"
#include <unordered_map> 
#include <iostream>
#include <algorithm>  // std::min, std::max
#include <cfloat>  // for FLT_MAX
#include "../External/json/json.hpp"
using json = nlohmann::json;
#include <fstream>
#include <filesystem>


/**
 * @brief Constructs a new Scene with a reference to the physics world.
 *
 * The Scene does not own the Physics system - it just needs a reference to
 * create rigid bodies when spawning objects.
 *
 * @param physics Reference to the game's Physics system
 */
Scene::Scene(Physics& physics, Renderer& renderer) : physicsWorld(physics), renderer(renderer)
, spatialGrid(std::make_unique<SpatialGrid>(10.0f))//enable by defualt
{
    std::cout << "Scene created" << std::endl;
}


/**
 * @brief Destroys the scene and cleans up all game objects.
 *
 * Calls clear() to remove all objects. The Physics system handles cleanup
 * of rigid bodies separately.
 */
Scene::~Scene() {
    clear();
}


// Installs a lambda on the object that fires whenever addTag() is called.
// The lambda looks up the tag in tagScriptRegistry and calls the attacher if found.
// Called at the end of every spawn method so ALL objects get the callback,
// whether spawned at runtime or loaded from file.
void Scene::wireTagCallback(GameObject* obj)
{
    obj->setOnTagAddedCallback([this](GameObject* obj, const std::string& tag)
        {
            auto it = tagScriptRegistry.find(tag);
            if (it != tagScriptRegistry.end() && it->second.attach)
            {
                std::cout << "[TagScript] Attaching script for tag '" << tag
                    << "' to object '" << obj->getName() << "'" << std::endl;
                it->second.attach(obj);
            }
        });

    obj->setOnTagRemovedCallback([this](GameObject* obj, const std::string& tag)
        {
            auto it = tagScriptRegistry.find(tag);
            if (it != tagScriptRegistry.end() && it->second.remove)
            {
                std::cout << "[TagScript] Removing script for tag '" << tag
                    << "' from object '" << obj->getName() << "'" << std::endl;
                it->second.remove(obj);
            }
        });
}
// Stores a tag->script binding. Must be called in SetupScripts()
// before applyTagScriptsToExistingObjects() so loaded objects get scripts too.
void Scene::registerTagScript(const std::string& tag,
    std::function<void(GameObject*)> scriptAttacher,
    std::function<void(GameObject*)> scriptRemover)
{
    tagScriptRegistry[tag] = TagScriptBinding(scriptAttacher, scriptRemover);
    std::cout << "[TagScript] Registered script for tag '" << tag << "'" << std::endl;
}

// One - time pass after registerTagScript() calls to catch objects that
// already have tags from loadFromFile() before the registry was populated.
// Call at the END of SetupScripts() after all registerTagScript() calls
void Scene::applyTagScriptsToExistingObjects()
{
    for (auto& obj : gameObjects)
    {
        for (const auto& tag : obj->getTags())
        {
            auto it = tagScriptRegistry.find(tag);
            if (it != tagScriptRegistry.end() && it->second.attach)
            {
                std::cout << "[TagScript] Applying script for tag '" << tag
                    << "' to existing object '" << obj->getName() << "'" << std::endl;
                it->second.attach(obj.get());
            }
        }
    }
}
void Scene::registerTriggerScript(const std::string& behaviourTag,std::function<void(Trigger*)> onRegister,std::function<void(Trigger*)> onUnregister)
{
    triggerScriptRegistry[behaviourTag] = TriggerScriptBinding(onRegister, onUnregister);
    std::cout << "[TriggerScript] Registered behaviour tag '" << behaviourTag << "'" << std::endl;
}

void Scene::applyTriggerScriptsToExistingTriggers()
{
    for (Trigger* trigger : TriggerRegistry::getInstance().getAllTriggers())
    {
        if (!trigger) continue;

        const std::string& tag = trigger->getBehaviourTag();
        if (tag.empty()) continue; // TELEPORT / SPEED_ZONE have no behaviour tag

        auto it = triggerScriptRegistry.find(tag);
        if (it != triggerScriptRegistry.end() && it->second.onRegister)
        {
            std::cout << "[TriggerScript] Wiring behaviour '" << tag
                << "' onto trigger '" << trigger->getName() << "'" << std::endl;
            it->second.onRegister(trigger);
        }
        else
        {
            std::cout << "[TriggerScript] Warning: no binding found for behaviour tag '"
                << tag << "' on trigger '" << trigger->getName() << "'" << std::endl;
        }
    }
}
/**
 * @brief Spawns a new game object in the scene with physics.
 *
 * This is the primary method for creating objects at runtime. It handles:
 * - Creating the physics rigid body with the specified material properties
 * - Creating the GameObject wrapper with rendering data
 * - Adding the object to the scene's management
 *
 * @param type The shape type (CUBE, SPHERE, or CAPSULE)
 * @param position World position to spawn at
 * @param size Shape dimensions (half-extents for cube, radius for sphere, radius+height for capsule)
 * @param mass Object mass in kg (0.0 = static/immovable object)
 * @param materialName Physics material name from MaterialRegistry (default: "Default")
 * @param texturePath Optional texture file path (default: "" = use default orange material)
 * @return GameObject* Pointer to the created object (owned by Scene, don't delete)
 *
 * @note The returned pointer remains valid until the object is removed or the scene is cleared.
 *
 * @example
 * // Create a dynamic wooden crate
 * auto* crate = scene.spawnObject(ShapeType::CUBE, glm::vec3(0, 5, 0),
 *                                  glm::vec3(1.0f), 10.0f, "Wood", "textures/crate.jpg");
 *
 * // Create a static ground plane
 * scene.spawnObject(ShapeType::CUBE, glm::vec3(0, 0, 0),
 *                   glm::vec3(100, 0.5f, 100), 0.0f, "Concrete");
 */
GameObject* Scene::spawnObject(ShapeType type,
    const glm::vec3& position,
    const glm::vec3& size,
    float mass,
    const std::string& materialName,
    const std::string& texturePath,
    const std::string& specularPath)
{
    // Create physics body
    btRigidBody* body = physicsWorld.createRigidBody(
        type,
        position,
        size,
        mass,
        materialName
    );

    // Create game object 
    auto obj = std::make_unique<GameObject>(type, body, size, materialName, texturePath);
    
    // Set specular texture if provided
    if (!specularPath.empty()) {
        obj->getRender().setSpecularTexturePath(specularPath);
    }
    
    // Store GameObject pointer in rigid body's user pointer
    // This allows raycasts to return the GameObject instead of just the raw physics body
    body->setUserPointer(obj.get());
    obj->updateFromPhysics();

    // Set render mesh based on shape type
    switch (type) {
    case ShapeType::CUBE:
        obj->getRender().setRenderMesh(renderer.getCubeMesh());
        break;
    case ShapeType::SPHERE:
        obj->getRender().setRenderMesh(renderer.getSphereMesh());
        break;
    case ShapeType::CAPSULE:
        obj->getRender().setRenderMesh(renderer.getCylinderMesh());
        break;
    }

    GameObject* ptr = obj.get();
    gameObjects.push_back(std::move(obj));
    // Add to spatial grid
    if (spatialGrid) {
        spatialGrid->insertObject(ptr);
    }
    wireTagCallback(ptr);
    // Log creation
    const char* shapeName = "Unknown";
    switch (type) {
    case ShapeType::CUBE: shapeName = "Cube"; break;
    case ShapeType::SPHERE: shapeName = "Sphere"; break;
    case ShapeType::CAPSULE: shapeName = "Capsule"; break;
    }

    std::cout << "Spawned " << shapeName << " at ("
        << position.x << ", " << position.y << ", " << position.z
        << ") with material: " << materialName << std::endl;

    return ptr;
}


// add spawnObject method without physics - for render only objects
GameObject* Scene::spawnRenderObject(
    ShapeType type,
    const glm::vec3& position,
    const glm::vec3& size,
    const std::string& texturePath,
    const std::string& specularPath)
{
    // Render-only constructor (no rigid body)
    auto obj = std::make_unique<GameObject>(type, position, size, texturePath);

    // Set specular texture if provided
    if (!specularPath.empty()) {
        obj->getRender().setSpecularTexturePath(specularPath);
    }

    switch (type) {
    case ShapeType::CUBE:
        obj->getRender().setRenderMesh(renderer.getCubeMesh());
        break;
    case ShapeType::SPHERE:
        obj->getRender().setRenderMesh(renderer.getSphereMesh());
        break;
    case ShapeType::CAPSULE:
        obj->getRender().setRenderMesh(renderer.getCylinderMesh());
        break;
    }


    GameObject* ptr = obj.get();
    gameObjects.push_back(std::move(obj));
    wireTagCallback(ptr);

    std::cout << "Spawned Render-Only Object at ("
        << position.x << ", " << position.y << ", " << position.z << ")\n";

    return ptr;
}


void Scene::setObjectScale(GameObject* obj, const glm::vec3& newScale) {
    if (!obj) {
        std::cerr << "Error: Cannot set scale on null object" << std::endl;
        return;
    }

    json test;
    test["hello"] = "world";

    // Update visual scale
    obj->getTransform().setScale(newScale);

	// If no physics, exit early - just update render scale
    if (!obj->hasPhysics()) {
        std::cout << "Updated scale for render-only object" << std::endl;
        return;
    }

    // Handle physics resize
    std::cout << "Resizing physics body for object..." << std::endl;

    btRigidBody* oldBody = obj->getRigidBody();
    if (!oldBody) {
        std::cerr << "Error: Object has physics component but no rigid body!" << std::endl;
        return;
    }

    // Get current mass
    float mass = 1.0f / oldBody->getInvMass();
    if (oldBody->getInvMass() == 0.0f) {
        mass = 0.0f;  // Static object
    }

    // Warn about constraint breakage
    auto attachedConstraints = ConstraintRegistry::getInstance().findConstraintsByObject(obj);
    if (!attachedConstraints.empty()) {
        std::cout << "WARNING: Object has " << attachedConstraints.size()
            << " attached constraint(s) which will break during resize!" << std::endl;
    }

    glm::vec3 physicsScale = obj->getPhysicsScale();
    glm::vec3 collisionSize = newScale * physicsScale;
    // Resize the rigid body
    btRigidBody* newBody = physicsWorld.resizeRigidBody(
        oldBody,
        obj->getShapeType(),
        collisionSize,
        mass,
        obj->getMaterialName()
    );

    // Update GameObject's physics component
    if (newBody) {
        newBody->setUserPointer(obj);
        obj->getPhysics()->setRigidBody(newBody);

        if (spatialGrid) {
            spatialGrid->updateObject(obj);
        }

        std::cout << "Object scale updated successfully" << std::endl;
    }
    else {
        std::cerr << "Error: Failed to resize rigid body!" << std::endl;
    }
}

/**
 * @brief Updates all game objects in the scene.
 *
 * Synchronizes each GameObject's position and rotation with its physics
 * rigid body state. This should be called every frame after physics simulation
 * has stepped forward.
 *
 * The update flow is:
 * 1. Physics system simulates forces and collisions
 * 2. Scene::update() syncs GameObject transforms from physics
 * 3. Renderer draws objects at their updated positions
 */
void Scene::update(EngineMode mode) {
    // Only sync transforms from physics in GAME mode
    if (mode == EngineMode::Game || mode == EngineMode::Test)
    {
        // --- 1. Update gameplay scripts ---
        float dt = Time::GetDeltaTime();

        for (auto& obj : gameObjects)
        {
            obj->updateScripts(dt);
        }
		// --- 2. Fixed update for physics-related scripts ---
        const float fixedDt = 1.0f / 60.0f;
        for (auto& obj : gameObjects) {
            obj->fixedUpdateScripts(fixedDt);
        }
        // --- 3. Sync physics -> transform ---
        for (auto& obj : gameObjects)
        {
            if (obj->hasPhysics())
                obj->updateFromPhysics();
        }
    }
    else
    {
        // Editor mode: do NOT constantly overwrite gizmo transforms
        // But we still want one initial sync so objects appear
        
        if (!editorSyncedOnce) {
            for (auto& obj : gameObjects) {
                if (obj->hasPhysics())
                    obj->updateFromPhysics();
            }
            editorSyncedOnce = true;
        }
    }
    // Update spatial grid positions
    if (spatialGrid) {
        for (auto& obj : gameObjects) {
            spatialGrid->updateObject(obj.get());
        }
    }

    // --- Process deferred destruction ---
    if (!pendingDestroy.empty())
    {
        for (GameObject* obj : pendingDestroy)
        {
			// Call onDestroy() on all scripts before removing the object
            obj->notifyDestroy();
            // 1. Remove constraints
            ConstraintRegistry::getInstance().removeConstraintsForObject(obj);

            // 2. Remove from spatial grid
            if (spatialGrid)
                spatialGrid->removeObject(obj);

            // 3. Remove physics body
            if (obj->hasPhysics())
                physicsWorld.removeRigidBody(obj->getRigidBody());

            // 4. Remove from scene container
            gameObjects.erase(
                std::remove_if(gameObjects.begin(), gameObjects.end(),
                    [obj](const std::unique_ptr<GameObject>& ptr)
                    {
                        return ptr.get() == obj;
                    }),
                gameObjects.end()
            );
        }

        pendingDestroy.clear();
    }

}

// Spatial Queries

std::vector<GameObject*> Scene::findObjectsInRadius(
    const glm::vec3& center,
    float radius,
    std::function<bool(GameObject*)> filter) const
{
    // Use spatial grid if enabled - queries only nearby cells (O(k) where k = objects in range)
    if (spatialGrid) {
        return spatialGrid->queryRadius(center, radius, filter);
    }

    // Fallback: Check every object in scene (O(n) - slower but works for small scenes)
    std::vector<GameObject*> results;
    float radiusSquared = radius * radius;  // Compare squared distances 

    for (const auto& obj : gameObjects) {
        // Apply filter first if provided (e.g., only enemies, only pickups)
        if (filter && !filter(obj.get())) continue;

        // Calculate distance from center
        glm::vec3 diff = obj->getPosition() - center;
        float distSquared = glm::dot(diff, diff);  

        // Within radius?
        if (distSquared <= radiusSquared) {
            results.push_back(obj.get());
        }
    }

    return results;
}

GameObject* Scene::findNearestObject(
    const glm::vec3& position,
    float maxRadius,
    std::function<bool(GameObject*)> filter) const
{
    // Use spatial grid if enabled
    if (spatialGrid) {
        return spatialGrid->queryNearest(position, maxRadius, filter);
    }

    // Fallback: Check all objects and track closest
    GameObject* nearest = nullptr;
    float minDistSquared = maxRadius * maxRadius;  // Start with max search radius

    for (const auto& obj : gameObjects) {
        // Apply filter if provided
        if (filter && !filter(obj.get())) continue;

        // Calculate squared distance
        glm::vec3 diff = obj->getPosition() - position;
        float distSquared = glm::dot(diff, diff);

        // Is this closer than current best?
        if (distSquared < minDistSquared) {
            minDistSquared = distSquared;
            nearest = obj.get();
        }
    }

    return nearest;  // Returns nullptr if nothing found
}

std::vector<GameObject*> Scene::findObjectsByTag(const std::string& tag) const
{
    std::vector<GameObject*> results;
    for (const auto& obj : gameObjects) {
        if (obj->hasTag(tag))
            results.push_back(obj.get());
    }
    return results;
}

// controls

void Scene::setSpatialGridEnabled(bool enabled) {
    // Enabling grid when it doesn't exist
    if (enabled && !spatialGrid) {
        spatialGrid = std::make_unique<SpatialGrid>(10.0f);

        // Populate grid with all existing objects
        for (const auto& obj : gameObjects) {
            spatialGrid->insertObject(obj.get());
        }
        std::cout << "Spatial grid enabled" << std::endl;
    }
    // Disabling grid when it exists
    else if (!enabled && spatialGrid) {
        spatialGrid.reset();  // Destroys grid and frees memory
        std::cout << "Spatial grid disabled" << std::endl;
    }
    // else: Already in requested state, do nothing
}

void Scene::printSpatialStats() const {
    if (spatialGrid) {
        spatialGrid->printStats();  // Show cell count, objects per cell, memory usage
    }
    else {
        std::cout << "Spatial grid is disabled" << std::endl;
    }
}
/**
 * @brief Removes all objects from the scene.
 *
 * Destroys all GameObjects and their associated physics bodies.
 * The Physics system handles cleanup of rigid bodies when GameObject
 * destructors are called.
 *
 * @note This does NOT reset the Physics world itself - only removes objects.
 */
void Scene::clear() {
    std::cout << "Clearing scene (" << gameObjects.size() << " objects)" << std::endl;

    // Drain the deferred-destroy queue FIRST to avoid dangling raw pointers
    pendingDestroy.clear();

    // Constraints must be removed before the bodies they reference
    ConstraintRegistry::getInstance().clearAll();
    TriggerRegistry::getInstance().clearAll();
    ForceGeneratorRegistry::getInstance().clearAll();
    PointLightRegistry::getInstance().clearAll();

    // Explicitly unregister every rigid body from Bullet BEFORE destroying GameObjects.
    // Without this, bodies linger in the physics world across reloads and cause crashes.
    for (auto& obj : gameObjects) {
        if (obj->hasPhysics() && obj->getRigidBody()) {
            physicsWorld.removeRigidBody(obj->getRigidBody());
        }
    }

    if (spatialGrid) {
        spatialGrid->clear();
    }

    gameObjects.clear();
}

void Scene::requestDestroy(GameObject* obj)
{
    if (!obj) return;

    // Prevent double-queue
    if (std::find(pendingDestroy.begin(), pendingDestroy.end(), obj) != pendingDestroy.end())
        return;

    pendingDestroy.push_back(obj);
}



GameObject* Scene::loadAndSpawnModel(const std::string& filepath,
    const glm::vec3& position,
    const glm::vec3& meshScale, 
    bool enablePhysics,
    float mass,
    const glm::vec3& physicsBoxScale,
    const std::string& materialName)
{
    // Load mesh from file
    Mesh loadedMesh = MeshFactory::loadFromFile(filepath);

    // Check if load was successful (mesh has vertices)
    if (loadedMesh.getVertexCount() == 0) {
        std::cerr << "Failed to load model from: " << filepath << std::endl;
        return nullptr;
    }

    std::cout << "Successfully loaded model: " << filepath << std::endl;

    // Store mesh in static cache
    static std::unordered_map<std::string, Mesh> loadedMeshes;
    if (loadedMeshes.find(filepath) == loadedMeshes.end()) {
        loadedMeshes[filepath] = std::move(loadedMesh);
    }
    Mesh* meshPtr = &loadedMeshes[filepath];

    GameObject* obj = nullptr;
 
    if (enablePhysics) {

        const std::vector<float>& verts = meshPtr->getVertices();
        const size_t FLOATS_PER_VERTEX = 14; // Assuming 14 floats per vertex: position(3) + normal(3) + texcoord(2) + tangent(4) + bitangent(4)

        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);
        std::cout << "minBounds Y (raw before normalization): " << minBounds.y << std::endl;
        std::cout << "maxBounds Y (raw before normalization): " << maxBounds.y << std::endl;
        for (size_t i = 0; i < verts.size(); i += FLOATS_PER_VERTEX)
        {
            glm::vec3 p(verts[i], verts[i + 1], verts[i + 2]);
            minBounds = glm::min(minBounds, p);
            maxBounds = glm::max(maxBounds, p);
        }
        // Apply mesh scale
        glm::vec3 scaledMin = minBounds * meshScale;
        glm::vec3 scaledMax = maxBounds * meshScale;

        // Exact half extents — perfectly encapsulates mesh in all directions
        glm::vec3 halfExtents = (scaledMax - scaledMin) * 0.5f;
        halfExtents = glm::max(halfExtents, glm::vec3(0.01f));

        // Mesh center in world space — box is centered exactly on mesh
        glm::vec3 meshCenter = (scaledMin + scaledMax) * 0.5f;
        glm::vec3 spawnPosition = glm::vec3(
            position.x + meshCenter.x,
            position.y + halfExtents.y + meshCenter.y,
            position.z + meshCenter.z
        );
        glm::vec3 fullExtents = halfExtents * 2.0f;
        // DEBUG 
        std::cout << "minBounds: " << minBounds.y << std::endl;
        std::cout << "maxBounds: " << maxBounds.y << std::endl;
        std::cout << "meshCenter.y: " << meshCenter.y << std::endl;
        std::cout << "meshScale: " << meshScale.x << ", " << meshScale.y << ", " << meshScale.z << std::endl;
        std::cout << "scaledMin.y: " << scaledMin.y << std::endl;
        std::cout << "scaledMax.y: " << scaledMax.y << std::endl;
        std::cout << "halfExtents (final): " << halfExtents.x << ", " << halfExtents.y << ", " << halfExtents.z << std::endl;
        // Create rigid body at adjusted position
        btRigidBody* body = physicsWorld.createRigidBody(
            ShapeType::CUBE,
            spawnPosition,
            fullExtents,
            mass,
            materialName
        );

        // Create GameObject with physics
        auto objUnique = std::make_unique<GameObject>(
            ShapeType::CUBE, body, halfExtents, materialName, ""
        );
        body->setUserPointer(objUnique.get());

        // Set render mesh and transform
        objUnique->getRender().setRenderMesh(meshPtr);
        objUnique->getRender().setModelPath(filepath);

        // Apply visual mesh scale
        objUnique->getTransform().setScale(meshScale);

        // Store physics scale for future reference
        objUnique->setPhysicsScale(physicsBoxScale);

        // sync transform from physics body so position is correct immediately 
        objUnique->updateFromPhysics();

        obj = objUnique.get();
        gameObjects.push_back(std::move(objUnique));

        if (spatialGrid) {
            spatialGrid->insertObject(obj);
        }
    }
    else {
        // Create render-only object
        auto objUnique = std::make_unique<GameObject>(
            ShapeType::CUBE, position, meshScale, ""
        );

        objUnique->getRender().setRenderMesh(meshPtr);
        objUnique->getRender().setModelPath(filepath);

        obj = objUnique.get();
        gameObjects.push_back(std::move(objUnique));

        std::cout << "Spawned render-only model at (" << position.x << ", "
            << position.y << ", " << position.z << ")" << std::endl;
    }
    // wire up tag->script callback for model-spawned objects too
    wireTagCallback(obj);
    return obj;
}

void Scene::setObjectPhysicsScale(GameObject* obj, const glm::vec3& newPhysicsScale) {
    if (!obj || !obj->hasPhysics()) {
        std::cerr << "Error: Cannot set physics scale on object without physics" << std::endl;
        return;
    }

    std::cout << "Resizing physics collision shape..." << std::endl;

    btRigidBody* oldBody = obj->getRigidBody();
    if (!oldBody) {
        std::cerr << "Error: No rigid body found!" << std::endl;
        return;
    }

    // Get current properties
    float mass = 1.0f / oldBody->getInvMass();
    if (oldBody->getInvMass() == 0.0f) {
        mass = 0.0f;  // Static object
    }

    // Get base collision shape size (from original scale, not current physics scale)
    glm::vec3 baseScale = obj->getScale();

    // Calculate new collision shape size
    glm::vec3 newCollisionSize = baseScale * newPhysicsScale;

    // Resize the rigid body
    btRigidBody* newBody = physicsWorld.resizeRigidBody(
        oldBody,
        obj->getShapeType(),
        newCollisionSize,
        mass,
        obj->getMaterialName()
    );

    if (newBody) {
        newBody->setUserPointer(obj);
        obj->getPhysics()->setRigidBody(newBody);
        obj->setPhysicsScale(newPhysicsScale);

        if (spatialGrid) {
            spatialGrid->updateObject(obj);
        }

        std::cout << "Physics scale updated successfully" << std::endl;
    }
    else {
        std::cerr << "Error: Failed to resize rigid body!" << std::endl;
    }
}

bool Scene::saveToFile(const std::string& path) const
{
    json sceneJson;
    sceneJson["objects"] = json::array();

    for (const auto& objPtr : gameObjects)
    {
        const GameObject& obj = *objPtr;

        json o;

        // Basic info
        o["id"] = obj.getID();
        o["name"] = obj.getName();
        // Tags
        o["tags"] = json::array();
        for (const auto& tag : obj.getTags())
            o["tags"].push_back(tag);
        o["shape"] = (int)obj.getShapeType();

        // Transform
        o["transform"]["position"] = {
            obj.getPosition().x,
            obj.getPosition().y,
            obj.getPosition().z
        };

        o["transform"]["rotation"] = {
            obj.getRotation().x,
            obj.getRotation().y,
            obj.getRotation().z,
            obj.getRotation().w
        };

        o["transform"]["scale"] = {
            obj.getScale().x,
            obj.getScale().y,
            obj.getScale().z
        };

        // Render
        o["render"]["texture"] = obj.getTexturePath();
        o["render"]["specular"] = obj.getRender().getSpecularTexturePath();
        o["render"]["normal"] = obj.getRender().getNormalTexturePath();
        o["render"]["modelPath"] = obj.getRender().getModelPath();

        // Physics
        o["physics"]["enabled"] = obj.hasPhysics();
        o["physics"]["physicsScale"] = {
            obj.getPhysicsScale().x,
            obj.getPhysicsScale().y,
            obj.getPhysicsScale().z
        };

        if (obj.hasPhysics())
        {
            btRigidBody* rb = obj.getRigidBody();

            float mass = 0.0f;
            if (rb->getInvMass() != 0.0f)
                mass = 1.0f / rb->getInvMass();

            o["physics"]["mass"] = mass;
            o["physics"]["material"] = obj.getMaterialName();
        }

        sceneJson["objects"].push_back(o);
    }
    sceneJson["triggers"] = json::array();

    std::vector<Trigger*> allTriggers =
        TriggerRegistry::getInstance().getAllTriggers();

    for (Trigger* trigger : allTriggers)
    {
        if (!trigger) continue;

        json t;

        t["id"] = trigger->getID();
        t["name"] = trigger->getName();
        t["type"] = static_cast<int>(trigger->getType());
        t["enabled"] = trigger->isEnabled();
        t["debugVisualize"] = trigger->shouldDebugVisualize();

        glm::vec3 pos = trigger->getPosition();
        t["position"] = { pos.x, pos.y, pos.z };

        glm::vec3 size = trigger->getSize();
        t["size"] = { size.x, size.y, size.z };

        if (trigger->getType() == TriggerType::TELEPORT)
        {
            glm::vec3 dest = trigger->getTeleportDestination();
            t["teleportDestination"] = { dest.x, dest.y, dest.z };
        }
        else if (trigger->getType() == TriggerType::SPEED_ZONE)
        {
            glm::vec3 dir = trigger->getForceDirection();
            t["forceDirection"] = { dir.x, dir.y, dir.z };
            t["forceMagnitude"] = trigger->getForceMagnitude();
        }
		// Tags for triggers too,for trigger-specific scripts or filtering
        t["requiredTags"] = json::array();
        for (const auto& tag : trigger->getRequiredTags())
            t["requiredTags"].push_back(tag);

		// will be empty string for TELEPORT and SPEED_ZONE triggers which have no behaviour tag
        t["behaviourTag"] = trigger->getBehaviourTag();

        t["maxUses"] = trigger->getMaxUses();

        sceneJson["triggers"].push_back(t);
    }

    sceneJson["pointLights"] = json::array();
    for (PointLight* light : PointLightRegistry::getInstance().getAllLights())
    {
        if (!light) continue;

        json l;
        l["name"] = light->getName();
        l["enabled"] = light->isEnabled();
        l["intensity"] = light->getIntensity();
        l["radius"] = light->getRadius();

        glm::vec3 pos = light->getPosition();
        l["position"] = { pos.x, pos.y, pos.z };

        glm::vec3 col = light->getColour();
        l["colour"] = { col.r, col.g, col.b };

        sceneJson["pointLights"].push_back(l);
    }

    sceneJson["forceGenerators"] = json::array();
    for (ForceGenerator* gen : ForceGeneratorRegistry::getInstance().getAllGenerators())
    {
        if (!gen) continue;

        json g;
        g["name"] = gen->getName();
        g["type"] = static_cast<int>(gen->getType());
        g["enabled"] = gen->isEnabled();

        glm::vec3 pos = gen->getPosition();
        g["position"] = { pos.x, pos.y, pos.z };
        g["radius"] = gen->getRadius();
        g["strength"] = gen->getStrength();

        switch (gen->getType())
        {
        case ForceGeneratorType::WIND:
        {
            auto* w = static_cast<WindGenerator*>(gen);
            glm::vec3 dir = w->getDirection();
            g["direction"] = { dir.x, dir.y, dir.z };
            break;
        }
        case ForceGeneratorType::GRAVITY_WELL:
        {
            auto* gw = static_cast<GravityWellGenerator*>(gen);
            g["minDistance"] = gw->getMinDistance();
            break;
        }
        case ForceGeneratorType::VORTEX:
        {
            auto* v = static_cast<VortexGenerator*>(gen);
            glm::vec3 axis = v->getAxis();
            g["axis"] = { axis.x, axis.y, axis.z };
            g["pullStrength"] = v->getPullStrength();
            break;
        }
        case ForceGeneratorType::EXPLOSION:
            // No extra fields needed — fires and expires on first update after load.
            break;
        default: break;
        }
        sceneJson["forceGenerators"].push_back(g);
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream file(path);
    if (!file.is_open())
    {
        std::cout << "Failed to save scene: " << path << std::endl;
        return false;
    }

    // Save directional light settings separately since they aren't GameObjects
    json& dl = sceneJson["directionalLight"];
    dl["direction"] = { savedLightDir.x, savedLightDir.y, savedLightDir.z };
    dl["colour"] = { savedLightCol.r, savedLightCol.g, savedLightCol.b };
    dl["intensity"] = savedLightIntensity;

    file << sceneJson.dump(4);

    std::cout << "Scene saved to " << path << std::endl;
    return true;
}

bool Scene::loadFromFile(const std::string& path)
{
    editorSyncedOnce = false;
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "Failed to load scene: " << path << std::endl;
        return false;
    }

    json sceneJson;
    file >> sceneJson;

    // Remove existing objects
    clear();

    for (const auto& o : sceneJson["objects"])
    {
        ShapeType shape = (ShapeType)o["shape"].get<int>();

        glm::vec3 position(
            o["transform"]["position"][0],
            o["transform"]["position"][1],
            o["transform"]["position"][2]
        );

        glm::quat rotation(
            o["transform"]["rotation"][3],
            o["transform"]["rotation"][0],
            o["transform"]["rotation"][1],
            o["transform"]["rotation"][2]
        );

        glm::vec3 scale(
            o["transform"]["scale"][0],
            o["transform"]["scale"][1],
            o["transform"]["scale"][2]
        );

        std::string texture = o["render"]["texture"];
        std::string specular = o["render"].contains("specular") ? o["render"]["specular"].get<std::string>() : "";
        std::string normal = o["render"].contains("normal") ? o["render"]["normal"].get<std::string>() : "";
        std::string modelPath = o["render"].contains("modelPath") ? o["render"]["modelPath"].get<std::string>() : "";

        bool physicsEnabled = o["physics"]["enabled"];

        GameObject* obj = nullptr;

        if (physicsEnabled)
        {
            float mass = o["physics"]["mass"];
            std::string material = o["physics"]["material"];

            glm::vec3 physScale(
                o["physics"]["physicsScale"][0],
                o["physics"]["physicsScale"][1],
                o["physics"]["physicsScale"][2]
            );

            glm::vec3 collisionSize = scale * physScale;

            if (!modelPath.empty())
            {
                obj = loadAndSpawnModel(modelPath, position, scale, true, mass, physScale, material);
            }
            else
            {
                obj = spawnObject(shape, position, collisionSize, mass, material, texture, specular);
                if (!normal.empty())
                    obj->getRender().setNormalTexturePath(normal);
                obj->setScale(scale);
                obj->setPhysicsScale(physScale);
                obj->getPhysics()->syncFromTransform(obj->getTransform());
                obj->updateFromPhysics();
            }
        }
        else
        {
            if (!modelPath.empty())
            {
                obj = loadAndSpawnModel(modelPath, position, scale, false, 0.0f, glm::vec3(1.0f), "Default");
            }
            else
            {
                obj = spawnRenderObject(shape, position, scale, texture, specular);
                if (!normal.empty())
                    obj->getRender().setNormalTexturePath(normal);
            }
        }

       

        obj->setRotation(rotation);

        if (o.contains("name"))
            obj->setName(o["name"]);

        if (o.contains("tags"))
            for (const auto& tag : o["tags"])
                obj->addTag(tag.get<std::string>());
    }

    std::cout << "Scene loaded from " << path << std::endl;
    if (sceneJson.contains("triggers"))
    {
        std::cout << "Loading triggers..." << std::endl;

        for (const auto& t : sceneJson["triggers"])
        {
            std::string name =
                t.contains("name")
                ? t["name"].get<std::string>()
                : "Unnamed_Trigger";

            TriggerType type =
                static_cast<TriggerType>(t["type"].get<int>());

            glm::vec3 position(
                t["position"][0],
                t["position"][1],
                t["position"][2]);

            glm::vec3 size(
                t["size"][0],
                t["size"][1],
                t["size"][2]);

            Trigger* trigger =
                TriggerRegistry::getInstance().createTrigger(
                    name, type, position, size);

            if (!trigger) continue;

            if (t.contains("maxUses"))
                trigger->setMaxUses(t["maxUses"]);

            if (t.contains("enabled"))
                trigger->setEnabled(t["enabled"]);

            if (t.contains("debugVisualize"))
                trigger->setDebugVisualize(t["debugVisualize"]);

            if (type == TriggerType::TELEPORT &&
                t.contains("teleportDestination"))
            {
                glm::vec3 dest(
                    t["teleportDestination"][0],
                    t["teleportDestination"][1],
                    t["teleportDestination"][2]);

                trigger->setTeleportDestination(dest);
            }
            else if (type == TriggerType::SPEED_ZONE &&
                t.contains("forceDirection") &&
                t.contains("forceMagnitude"))
            {
                glm::vec3 dir(
                    t["forceDirection"][0],
                    t["forceDirection"][1],
                    t["forceDirection"][2]);

                float magnitude = t["forceMagnitude"];
                trigger->setForce(dir, magnitude);
            }
			// restore trigger tags for filtering or trigger-specific scripts
            if (t.contains("requiredTags"))
                for (const auto& tag : t["requiredTags"])
                    trigger->requireTag(tag.get<std::string>());

            if (t.contains("behaviourTag"))
                trigger->setBehaviourTag(t["behaviourTag"].get<std::string>());
            std::cout << "Loaded trigger: " << name << std::endl;
        }

        std::cout << "Triggers loaded successfully" << std::endl;

        applyTriggerScriptsToExistingTriggers();
    }

    if (sceneJson.contains("pointLights"))
    {
        for (const auto& l : sceneJson["pointLights"])
        {
            std::string name = l.value("name", "PointLight");
            glm::vec3 position(l["position"][0], l["position"][1], l["position"][2]);
            glm::vec3 colour(l["colour"][0], l["colour"][1], l["colour"][2]);
            float intensity = l.value("intensity", 1.0f);
            float radius = l.value("radius", 10.0f);
            bool enabled = l.value("enabled", true);

            PointLight* light = PointLightRegistry::getInstance().addLight(
                name, position, colour, intensity, radius);

            if (light)
                light->setEnabled(enabled);
        }
    }

    if (sceneJson.contains("directionalLight"))
    {
        const auto& dl = sceneJson["directionalLight"];
        savedLightDir = glm::vec3(dl["direction"][0], dl["direction"][1], dl["direction"][2]);
        savedLightCol = glm::vec3(dl["colour"][0], dl["colour"][1], dl["colour"][2]);
        savedLightIntensity = dl.value("intensity", 0.8f);
    }

    if (sceneJson.contains("forceGenerators"))
    {
        std::cout << "Loading force generators..." << std::endl;
        for (const auto& g : sceneJson["forceGenerators"])
        {
            std::string name = g.value("name", "Unnamed_Generator");
            ForceGeneratorType type = static_cast<ForceGeneratorType>(g["type"].get<int>());
            glm::vec3 position(g["position"][0], g["position"][1], g["position"][2]);
            float radius = g.value("radius", 0.0f);
            float strength = g.value("strength", 0.0f);
            bool  enabled = g.value("enabled", true);

            ForceGenerator* gen = nullptr;

            switch (type)
            {
            case ForceGeneratorType::WIND:
            {
                glm::vec3 dir(g["direction"][0], g["direction"][1], g["direction"][2]);
                gen = ForceGeneratorRegistry::getInstance()
                    .createWind(name, position, radius, dir, strength);
                break;
            }
            case ForceGeneratorType::GRAVITY_WELL:
            {
                float minDist = g.value("minDistance", 1.0f);
                gen = ForceGeneratorRegistry::getInstance()
                    .addGenerator(std::make_unique<GravityWellGenerator>(
                        name, position, radius, strength, minDist));
                break;
            }
            case ForceGeneratorType::VORTEX:
            {
                glm::vec3 axis(g["axis"][0], g["axis"][1], g["axis"][2]);
                float pull = g.value("pullStrength", 0.0f);
                gen = ForceGeneratorRegistry::getInstance()
                    .createVortex(name, position, radius, axis, strength, pull);
                break;
            }
            case ForceGeneratorType::EXPLOSION:
            {
                gen = ForceGeneratorRegistry::getInstance()
                    .createExplosion(name, position, radius, strength);
                break;
            }
            default: break;
            }

            if (gen)
            {
                gen->setEnabled(enabled);
                std::cout << "Loaded force generator: " << name << std::endl;
            }
        }
        std::cout << "Force generators loaded successfully" << std::endl;
    }

    std::cout << "Applying transforms..." << std::endl;
    // --- Apply exact transforms to physics bodies (NO simulation) ---
    for (auto& obj : gameObjects)
    {
        if (obj->hasPhysics())
        {
            // Copy transform -> rigid body
            obj->getPhysics()->syncFromTransform(obj->getTransform());
        }
    }
    std::cout << "Freezing bodies..." << std::endl;
    // --- Freeze everything so nothing moves ---
    for (auto& obj : gameObjects)
    {
        if (obj->hasPhysics())
        {
            btRigidBody* body = obj->getRigidBody();
            if (body)
            {
                body->setLinearVelocity(btVector3(0, 0, 0));
                body->setAngularVelocity(btVector3(0, 0, 0));
                body->clearForces();
                body->setActivationState(ISLAND_SLEEPING);
            }
        }
    }
    std::cout << "loadFromFile complete." << std::endl;
    return true;
}


