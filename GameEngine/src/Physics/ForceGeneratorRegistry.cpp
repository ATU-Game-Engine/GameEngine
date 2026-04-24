/**
 * @file ForceGeneratorRegistry.cpp
 * @brief Singleton registry that owns all active ForceGenerators and applies
 *        them to every dynamic rigid body in the physics world each tick.
 *
 * Responsibilities:
 *   - Storing and managing the lifetime of all ForceGenerator instances.
 *   - Providing factory helpers (createWind, createGravityWell, etc.) so
 *     callers don't need to construct generators manually.
 *   - Iterating every dynamic rigid body in the Bullet world each physics
 *     tick and calling each enabled generator's apply() method.
 *   - Automatically removing expired generators (e.g. fired explosions)
 *     after the frame in which they fire.
 *
 * Lifetime: global singleton. Call initialize() once after the Bullet world
 * is created. Call clearAll() (done by Scene::clear()) before the world is
 * destroyed.
 */
#include "../include/Physics/ForceGeneratorRegistry.h"
#include "../include/Physics/ForceGenerator.h"
#include <iostream>
#include <algorithm>

ForceGeneratorRegistry* ForceGeneratorRegistry::instance = nullptr;
/**
 * @brief Destructor — clears all owned generators.
 */
ForceGeneratorRegistry::~ForceGeneratorRegistry()
{
    clearAll();
}
/**
 * @brief Returns the global ForceGeneratorRegistry instance, creating it on
 *        first call.
 *
 * The instance is never explicitly destroyed — it lives for the application
 * lifetime. Call clearAll() before shutting down the physics world to release
 * generator resources cleanly.
 *
 * @return Reference to the singleton instance.
 */
ForceGeneratorRegistry& ForceGeneratorRegistry::getInstance()
{
    if (!instance)
        instance = new ForceGeneratorRegistry();
    return *instance;
}
/**
 * @brief Binds the registry to a Bullet dynamics world.
 *
 * Must be called once after the physics world is created. The world pointer
 * is used by update() to iterate all collision objects and apply forces.
 *
 * @param world  The active Bullet dynamics world. Must not be null.
 */
void ForceGeneratorRegistry::initialize(btDiscreteDynamicsWorld* world)
{
    dynamicsWorld = world;
    std::cout << "[ForceGeneratorRegistry] Initialized" << std::endl;
}

//Lifetime management methods to add/remove generators, clear all generators, etc.

// add a generator to the registry, returning a raw pointer (registry retains ownership)
/**
 * @brief Takes ownership of a generator and registers it with the registry.
 *
 * The registry holds the unique_ptr so the generator lives until it expires
 * or is explicitly removed. The returned raw pointer is valid until the
 * generator is removed or clearAll() is called.
 *
 * @param generator  Heap-allocated generator to take ownership of. No-op if null.
 * @return           Non-owning raw pointer to the stored generator, or nullptr
 *                   if the input was null.
 */
ForceGenerator* ForceGeneratorRegistry::addGenerator(std::unique_ptr<ForceGenerator> generator)
{
    if (!generator)
    {
        std::cerr << "[ForceGeneratorRegistry] Error: Cannot add null generator" << std::endl;
        return nullptr;
    }

    ForceGenerator* ptr = generator.get();
    generators.push_back(std::move(generator));

    std::cout << "[ForceGeneratorRegistry] Added '" << ptr->getName()
        << "' (total: " << generators.size() << ")" << std::endl;

    return ptr;
}

/**
 * @brief Removes and destroys a generator by raw pointer.
 *
 * Linear scan through the generator list. No-op if the pointer is null or
 * not found in the registry.
 *
 * @param generator  Pointer previously returned by addGenerator or a factory
 *                   method. Must not be used after this call.
 */
void ForceGeneratorRegistry::removeGenerator(ForceGenerator* generator)
{
    if (!generator) return;

    auto it = std::find_if(generators.begin(), generators.end(),
        [generator](const std::unique_ptr<ForceGenerator>& g) {
            return g.get() == generator;
        });

    if (it != generators.end())
    {
        std::cout << "[ForceGeneratorRegistry] Removed '" << (*it)->getName() << "'" << std::endl;
        generators.erase(it);
    }
}
/**
 * @brief Removes and destroys a generator by name.
 *
 * Looks up the generator via findByName(), then delegates to the pointer
 * overload.
 *
 * @param name  Name assigned to the generator at creation time.
 * @return      True if a generator with that name was found and removed.
 */
bool ForceGeneratorRegistry::removeGenerator(const std::string& name)
{
    ForceGenerator* gen = findByName(name);
    if (gen)
    {
        removeGenerator(gen);
        return true;
    }
    return false;
}
/**
 * @brief Destroys all registered generators and clears the internal list.
 *
 * Called by Scene::clear() before destroying GameObjects to ensure no
 * generator holds state referencing the old scene. Safe to call when empty.
 */
void ForceGeneratorRegistry::clearAll()
{
    generators.clear();
    std::cout << "[ForceGeneratorRegistry] All generators cleared" << std::endl;
}


// factory methods for creating common generator types
/**
 * @brief Creates and registers a WindGenerator.
 *
 * Wind applies a constant directional force to all dynamic bodies within
 * the specified radius each physics tick.
 *
 * @param name       Unique label for lookup and debug output.
 * @param position   Centre of the wind area in world space.
 * @param radius     Affected radius (0 = infinite).
 * @param direction  Wind direction (will be normalised internally).
 * @param strength   Force magnitude in Newtons.
 * @return           Raw pointer to the registered generator.
 */
ForceGenerator* ForceGeneratorRegistry::createWind(const std::string& name,
    const glm::vec3& position,
    float radius,
    const glm::vec3& direction,
    float strength)
{
    return addGenerator(std::make_unique<WindGenerator>(name, position, radius, direction, strength));
}

/**
 * @brief Creates and registers a GravityWellGenerator.
 *
 * A gravity well applies an inverse-square force toward (positive strength)
 * or away from (negative strength) the well's position.
 *
 * @param name      Unique label for lookup and debug output.
 * @param position  Centre of the gravity well in world space.
 * @param radius    Affected radius (0 = infinite).
 * @param strength  Force scale — positive attracts, negative repels.
 * @return          Raw pointer to the registered generator.
 */
ForceGenerator* ForceGeneratorRegistry::createGravityWell(const std::string& name,
    const glm::vec3& position,
    float radius,
    float strength)
{
    return addGenerator(std::make_unique<GravityWellGenerator>(name, position, radius, strength));
}

/**
 * @brief Creates and registers a VortexGenerator.
 *
 * A vortex applies a tangential spin force and an inward pull to bodies
 * within range, causing them to spiral toward the vortex centre.
 *
 * @param name             Unique label for lookup and debug output.
 * @param position         Centre of the vortex in world space.
 * @param radius           Affected radius (0 = infinite).
 * @param axis             Rotation axis (will be normalised internally).
 * @param rotationStrength Tangential force magnitude controlling spin speed.
 * @param pullStrength     Inward radial force magnitude controlling spiral rate.
 * @return                 Raw pointer to the registered generator.
 */
ForceGenerator* ForceGeneratorRegistry::createVortex(const std::string& name,
    const glm::vec3& position,
    float radius,
    const glm::vec3& axis,
    float rotationStrength,
    float pullStrength)
{
    return addGenerator(std::make_unique<VortexGenerator>(
        name, position, radius, axis, rotationStrength, pullStrength));
}

/**
 * @brief Creates and registers an ExplosionGenerator.
 *
 * An explosion applies a single outward impulse to all bodies within the
 * blast radius on the first update tick after creation, then removes itself
 * automatically. Impulse falls off linearly from the epicentre to the edge.
 *
 * @param name      Unique label for lookup and debug output.
 * @param position  Epicentre of the explosion in world space.
 * @param radius    Blast radius — bodies beyond this are unaffected.
 * @param strength  Peak impulse at the epicentre in N·s.
 * @return          Raw pointer to the registered generator (will be invalid
 *                  after the next update tick when the explosion is removed).
 */
ForceGenerator* ForceGeneratorRegistry::createExplosion(const std::string& name,
    const glm::vec3& position,
    float radius,
    float strength)
{
    return addGenerator(std::make_unique<ExplosionGenerator>(name, position, radius, strength));
}


// update method to apply all active generators to all rigid bodies in the world, then remove expired generators
/**
 * @brief Applies all enabled generators to every dynamic rigid body, then
 *        removes any generators that have expired.
 *
 * Called once per physics tick by the engine. The update loop:
 *   1. Iterates all collision objects in the Bullet world.
 *   2. For each enabled generator, calls apply() on every dynamic rigid body
 *      (static objects are skipped since they cannot be moved by forces).
 *   3. After all bodies have been processed, marks any unfired ExplosionGenerators
 *      as fired — this ensures the explosion impulse reaches every body in the
 *      same frame before the generator is removed.
 *   4. Erases all expired generators from the list.
 *
 * @param deltaTime  Physics timestep in seconds (passed through to apply()).
 */
void ForceGeneratorRegistry::update(float deltaTime)
{
    if (!dynamicsWorld) return;

    // Iterate every collision object in the physics world
    int numObjects = dynamicsWorld->getNumCollisionObjects();
    const btCollisionObjectArray& objArray = dynamicsWorld->getCollisionObjectArray();

    for (auto& gen : generators)
    {
        if (!gen || !gen->isEnabled()) continue;

        for (int i = 0; i < numObjects; ++i)
        {
            btCollisionObject* colObj = objArray[i];
            btRigidBody* body = btRigidBody::upcast(colObj);

            // Skip non-rigid-bodies, sleeping static bodies
            if (!body) continue;
            if (body->isStaticObject()) continue;

            // Get body world position
            const btVector3& btPos = body->getCenterOfMassPosition();
            glm::vec3 bodyPos(btPos.x(), btPos.y(), btPos.z());

            gen->apply(body, bodyPos, deltaTime);
        }
    }

    // Mark explosions as fired after all bodies have been processed this frame,
    // then remove any expired generators.
    for (auto& gen : generators)
    {
        if (gen && gen->getType() == ForceGeneratorType::EXPLOSION && !gen->isExpired())
        {
            static_cast<ExplosionGenerator*>(gen.get())->markFired();
        }
    }

    // Remove expired generators
    generators.erase(
        std::remove_if(generators.begin(), generators.end(),
            [](const std::unique_ptr<ForceGenerator>& g) {
                return g && g->isExpired();
            }),
        generators.end()
    );
}



// querty methods to find generators by name, type, etc.
/**
 * @brief Finds a generator by its name string.
 *
 * Linear scan — generator lists are expected to be small. Returns the first
 * match if multiple generators share a name.
 *
 * @param name  Name to search for.
 * @return      Raw pointer to the generator, or nullptr if not found.
 */
ForceGenerator* ForceGeneratorRegistry::findByName(const std::string& name) const
{
    for (const auto& gen : generators)
    {
        if (gen->getName() == name)
            return gen.get();
    }
    return nullptr;
}

// get raw pointers to all generators (registry retains ownership)
/**
 * @brief Returns raw pointers to every registered generator.
 *
 * Intended for the serialiser (Scene::saveToFile) and editor panels.
 * Pointers remain valid until the next add, remove, or update call.
 *
 * @return  Vector of all generator pointers in registration order.
 */
std::vector<ForceGenerator*> ForceGeneratorRegistry::getAllGenerators() const
{
    std::vector<ForceGenerator*> result;
    result.reserve(generators.size());
    for (const auto& gen : generators)
        result.push_back(gen.get());
    return result;
}

/**
 * @brief Returns all generators of a specific subtype.
 *
 * Useful for editor panels that display generators grouped by type.
 *
 * @param type  The ForceGeneratorType to filter by.
 * @return      Vector of matching raw generator pointers.
 */
std::vector<ForceGenerator*> ForceGeneratorRegistry::getGeneratorsByType(ForceGeneratorType type) const
{
    std::vector<ForceGenerator*> result;
    for (const auto& gen : generators)
    {
        if (gen->getType() == type)
            result.push_back(gen.get());
    }
    return result;
}

/**
 * @brief Returns true if a generator with the given name is registered.
 *
 * @param name  Name to check.
 * @return      True if found.
 */
bool ForceGeneratorRegistry::hasGenerator(const std::string& name) const
{
    return findByName(name) != nullptr;
}