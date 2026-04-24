/**
 * @file TriggerRegistry.cpp
 * @brief Implementation of the TriggerRegistry singleton — owns all Trigger
 *        instances and keeps their Bullet ghost objects synchronised with the
 *        dynamics world.
 *
 * Responsibilities:
 *   - Creating triggers and registering their btPairCachingGhostObjects as
 *     SensorTrigger collision objects in the Bullet world.
 *   - Ticking every enabled trigger each physics step so enter/stay/exit
 *     callbacks fire at the correct time.
 *   - Automatically removing triggers that have been marked for destruction
 *     (e.g. single-use triggers that have exhausted their maxUses count).
 *   - Providing lookup queries by name, type, position, and contained object.
 *
 * Lifetime: global singleton. Call initialize() once with the Bullet dynamics
 * world after Physics::initialize(). Call clearAll() (done by Scene::clear())
 * before destroying the world.
 */
#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/Trigger.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <algorithm>


// Initialize static instance
TriggerRegistry* TriggerRegistry::instance = nullptr;

/**
 * @brief Constructs the registry with a null world pointer.
 *
 * Call initialize() before any triggers are created.
 */
TriggerRegistry::TriggerRegistry() : dynamicsWorld(nullptr) {
}


/**
 * @brief Destructor — removes all triggers from the physics world and frees
 *        all owned Trigger objects.
 */
TriggerRegistry::~TriggerRegistry() {
    clearAll();
}


/**
 * @brief Returns the global TriggerRegistry instance, creating it on first call.
 *
 * Uses lazy initialisation. The instance lives for the application lifetime
 * and is never explicitly destroyed — call clearAll() before shutting down
 * the physics world.
 *
 * @return Reference to the singleton instance.
 */
TriggerRegistry& TriggerRegistry::getInstance() {
    if (!instance) {
        instance = new TriggerRegistry();
    }
    return *instance;
}


/**
 * @brief Binds the registry to a Bullet dynamics world.
 *
 * Must be called once after Physics::initialize(). All subsequent
 * addTrigger / removeTrigger calls forward ghost objects to this world.
 *
 * @param world  The active Bullet dynamics world. Must not be null.
 */
void TriggerRegistry::initialize(btDiscreteDynamicsWorld* world) {
    dynamicsWorld = world;
    std::cout << "TriggerRegistry initialized" << std::endl;
}

// Trigger Creation 

/**
 * @brief Constructs a new Trigger and registers it with the physics world.
 *
 * Convenience factory that constructs a Trigger in-place and delegates to
 * addTrigger(). Use this in preference to constructing a Trigger manually.
 *
 * @param name      Human-readable name for lookup and debug output.
 * @param type      Built-in behaviour type (TELEPORT, SPEED_ZONE, or EVENT).
 * @param position  World-space centre of the trigger volume.
 * @param size      Full extents of the box volume.
 * @return          Raw pointer to the new Trigger, or nullptr on failure.
 */
Trigger* TriggerRegistry::createTrigger(const std::string& name,
    TriggerType type,
    const glm::vec3& position,
    const glm::vec3& size)
{
    auto trigger = std::make_unique<Trigger>(name, type, position, size);
    return addTrigger(std::move(trigger));
}

/**
 * @brief Takes ownership of a Trigger and adds its ghost object to the world.
 *
 * The ghost object is registered as a SensorTrigger collision object so
 * Bullet tracks overlaps without generating contact responses. The returned
 * raw pointer is valid until the trigger is removed or clearAll() is called.
 *
 * @param trigger  Heap-allocated Trigger to take ownership of. Must not be null.
 * @return         Raw pointer to the stored Trigger, or nullptr if the input
 *                 is null or the registry is uninitialised.
 */
Trigger* TriggerRegistry::addTrigger(std::unique_ptr<Trigger> trigger) {
    if (!trigger) {
        std::cerr << "Error: Cannot add null trigger" << std::endl;
        return nullptr;
    }

    if (!dynamicsWorld) {
        std::cerr << "Error: TriggerRegistry not initialized with physics world" << std::endl;
        return nullptr;
    }

    // Add to physics world
    addToPhysicsWorld(trigger.get());

    // Store and return raw pointer
    Trigger* rawPtr = trigger.get();
    triggers.push_back(std::move(trigger));

    std::cout << "Added trigger '" << rawPtr->getName() << "' (total: "
        << triggers.size() << ")" << std::endl;

    return rawPtr;
}

//  Trigger Removal 

/**
 * @brief Removes a trigger by raw pointer, unregistering its ghost object
 *        from Bullet and freeing the owned Trigger.
 *
 * @param trigger  Pointer previously returned by createTrigger or addTrigger.
 *                 No-op if null or not found in the registry.
 */
void TriggerRegistry::removeTrigger(Trigger* trigger) {
    if (!trigger) return;

    // Find and remove
    auto it = std::find_if(triggers.begin(), triggers.end(),
        [trigger](const std::unique_ptr<Trigger>& t) {
            return t.get() == trigger;
        });

    if (it != triggers.end()) {
        // Remove from physics world
        removeFromPhysicsWorld(trigger);

        // Remove from vector (destroys the trigger)
        triggers.erase(it);

        std::cout << "Removed trigger (remaining: " << triggers.size() << ")" << std::endl;
    }
}

/**
 * @brief Removes a trigger by name.
 *
 * Looks up via findTriggerByName() then delegates to the pointer overload.
 *
 * @param name  Name assigned at creation time.
 * @return      True if found and removed.
 */
bool TriggerRegistry::removeTrigger(const std::string& name) {
    Trigger* trigger = findTriggerByName(name);
    if (trigger) {
        removeTrigger(trigger);
        return true;
    }
    return false;
}

/**
 * @brief Removes all triggers from the physics world and clears the registry.
 *
 * Called by Scene::clear() before a scene reload. Safe to call when empty.
 * No-op if the dynamics world has not been initialised.
 */
void TriggerRegistry::clearAll() {
    if (!dynamicsWorld) return;

    std::cout << "Removing all " << triggers.size() << " triggers..." << std::endl;

    // Remove all from physics world
    for (auto& trigger : triggers) {
        if (trigger) {
            removeFromPhysicsWorld(trigger.get());
        }
    }

    // Clear storage
    triggers.clear();

    std::cout << "All triggers cleared" << std::endl;
}

// Update

/**
 * @brief Ticks all enabled triggers and removes any that are pending destruction.
 *
 * Called once per physics tick by Physics::update(). Each enabled trigger's
 * update() method is called to fire enter/stay/exit callbacks. After all
 * triggers have been ticked, those marked isPendingDestroy() are removed from
 * the Bullet world and erased from the vector in a single pass.
 *
 * @param deltaTime  Physics timestep in seconds, forwarded to Trigger::update().
 */
void TriggerRegistry::update(float deltaTime) {
    // Update all active triggers
    for (auto& trigger : triggers) {
        if (trigger && trigger->isEnabled()) {
            trigger->update(dynamicsWorld, deltaTime);
        }
    }

    triggers.erase(
        std::remove_if(triggers.begin(), triggers.end(),
            [&](const std::unique_ptr<Trigger>& t)
            {
                if (t->isPendingDestroy())
                {
                    removeFromPhysicsWorld(t.get());
                    return true; // remove from vector
                }
                return false;
            }),
        triggers.end()
    );
}

//  Queries

/**
 * @brief Finds a trigger by its name string. Linear scan.
 *
 * @param name  Name to search for (case-sensitive).
 * @return      Raw pointer to the matching trigger, or nullptr if not found.
 */
Trigger* TriggerRegistry::findTriggerByName(const std::string& name) const {
    for (const auto& trigger : triggers) {
        if (trigger->getName() == name) {
            return trigger.get();
        }
    }
    return nullptr;
}

/**
 * @brief Returns all triggers of a specific type.
 *
 * @param type  TriggerType to filter by.
 * @return      Vector of matching raw trigger pointers.
 */
std::vector<Trigger*> TriggerRegistry::findTriggersByType(TriggerType type) const {
    std::vector<Trigger*> result;

    for (const auto& trigger : triggers) {
        if (trigger->getType() == type) {
            result.push_back(trigger.get());
        }
    }

    return result;
}

/**
 * @brief Returns raw pointers to all registered triggers.
 *
 * Intended for the serialiser (Scene::saveToFile) and editor panels.
 * Pointers remain valid until the next add, remove, or update call.
 *
 * @return  Vector of all trigger pointers in registration order.
 */

std::vector<Trigger*> TriggerRegistry::getAllTriggers() const {
    std::vector<Trigger*> result;
    result.reserve(triggers.size());

    for (const auto& trigger : triggers) {
        result.push_back(trigger.get());
    }

    return result;
}

/**
 * @brief Returns true if a trigger with the given name is registered.
 *
 * @param name  Name to check.
 */
bool TriggerRegistry::hasTrigger(const std::string& name) const {
    return findTriggerByName(name) != nullptr;
}

/**
 * @brief Returns all triggers whose centre positions are within a radius.
 *
 * Distance is measured centre-to-centre using squared distance to avoid
 * unnecessary square root calls. Does not account for trigger volume size —
 * use findTriggerContainingPoint() if you need exact containment.
 *
 * @param position  World-space query point.
 * @param radius    Maximum distance from position to trigger centre.
 * @return          Vector of triggers within the radius.
 */
std::vector<Trigger*> TriggerRegistry::findTriggersInRadius(
    const glm::vec3& position,
    float radius) const
{
    std::vector<Trigger*> result;
    float radiusSquared = radius * radius;

    for (const auto& trigger : triggers) {
        glm::vec3 triggerPos = trigger->getPosition();
        glm::vec3 diff = triggerPos - position;
        float distSquared = glm::dot(diff, diff);

        if (distSquared <= radiusSquared) {
            result.push_back(trigger.get());
        }
    }

    return result;
}


/**
 * @brief Returns the first trigger whose axis-aligned bounding box contains
 *        the given world-space point.
 *
 * The AABB is computed as [position - size, position + size], making size
 * equivalent to half-extents for this check. Returns the first match found
 * if multiple triggers overlap the point.
 *
 * @param point  World-space point to test.
 * @return       First trigger containing the point, or nullptr if none.
 */
Trigger* TriggerRegistry::findTriggerContainingPoint(const glm::vec3& point) const {
    for (const auto& trigger : triggers) {
        glm::vec3 triggerPos = trigger->getPosition();
        glm::vec3 triggerSize = trigger->getSize();

        // Check if point is inside axis-aligned bounding box
        glm::vec3 min = triggerPos - triggerSize;
        glm::vec3 max = triggerPos + triggerSize;

        if (point.x >= min.x && point.x <= max.x &&
            point.y >= min.y && point.y <= max.y &&
            point.z >= min.z && point.z <= max.z) {
            return trigger.get();
        }
    }

    return nullptr;
}


/**
 * @brief Returns all triggers that currently have a specific GameObject inside them.
 *
 * Checks each trigger's objectsInside list (maintained by Trigger::update())
 * for the given object pointer. Useful for querying which zones a player
 * is currently occupying.
 *
 * @param obj  The GameObject to search for. Returns empty vector if null.
 * @return     All triggers that contain obj in their current objectsInside list.
 */
std::vector<Trigger*> TriggerRegistry::findTriggersContainingObject(GameObject* obj) const {
    std::vector<Trigger*> result;

    if (!obj) return result;

    for (const auto& trigger : triggers) {
        const auto& objectsInside = trigger->getObjectsInside();

        if (std::find(objectsInside.begin(), objectsInside.end(), obj) != objectsInside.end()) {
            result.push_back(trigger.get());
        }
    }

    return result;
}

//  Debug 

/**
 * @brief Prints a summary of registered triggers broken down by type and
 *        enabled state.
 */
void TriggerRegistry::printStats() const {
    std::cout << "\n=== Trigger Registry Stats ===" << std::endl;
    std::cout << "Total triggers: " << triggers.size() << std::endl;

    // Count by type
    int goalZones = 0, deathZones = 0, checkpoints = 0;
    int teleports = 0, speedZones = 0, custom = 0;
    int enabled = 0, disabled = 0;

    for (const auto& trigger : triggers) {
        switch (trigger->getType()) {
        case TriggerType::TELEPORT: teleports++; break;
        case TriggerType::SPEED_ZONE: speedZones++; break;
        case TriggerType::EVENT: custom++; break;
        }

        if (trigger->isEnabled()) {
            enabled++;
        }
        else {
            disabled++;
        }
    }

    std::cout << "\nBy type:" << std::endl;
    std::cout << "  Goal Zones: " << goalZones << std::endl;
    std::cout << "  Death Zones: " << deathZones << std::endl;
    std::cout << "  Checkpoints: " << checkpoints << std::endl;
    std::cout << "  Teleports: " << teleports << std::endl;
    std::cout << "  Speed Zones: " << speedZones << std::endl;
    std::cout << "  Custom: " << custom << std::endl;

    std::cout << "\nStatus:" << std::endl;
    std::cout << "  Enabled: " << enabled << std::endl;
    std::cout << "  Disabled: " << disabled << std::endl;
    std::cout << "================================\n" << std::endl;
}

/**
 * @brief Prints the full details of every registered trigger to stdout.
 *
 * Lists name, ID, type, position, size, enabled state, and current occupancy
 * count for each trigger.
 */
void TriggerRegistry::printAllTriggers() const {
    std::cout << "\n=== All Triggers ===" << std::endl;

    int index = 0;
    for (const auto& trigger : triggers) {
        std::cout << "\n[" << index++ << "] " << trigger->getName() << std::endl;
        std::cout << "  ID: " << trigger->getID() << std::endl;

        std::cout << "  Type: ";
        switch (trigger->getType()) {
        case TriggerType::TELEPORT: std::cout << "Teleport"; break;
        case TriggerType::SPEED_ZONE: std::cout << "Speed Zone"; break;
        case TriggerType::EVENT: std::cout << "Custom"; break;
        }
        std::cout << std::endl;

        glm::vec3 pos = trigger->getPosition();
        glm::vec3 size = trigger->getSize();
        std::cout << "  Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "  Size: (" << size.x << ", " << size.y << ", " << size.z << ")" << std::endl;
        std::cout << "  Enabled: " << (trigger->isEnabled() ? "Yes" : "No") << std::endl;
        std::cout << "  Objects inside: " << trigger->getObjectsInside().size() << std::endl;
    }

    std::cout << "====================\n" << std::endl;
}

// Private Helpers 

/**
 * @brief Registers a trigger's ghost object with the Bullet dynamics world.
 *
 * The ghost object is added with the SensorTrigger collision group and
 * AllFilter mask so it receives overlap notifications from all other objects
 * without generating contact forces.
 *
 * @param trigger  The trigger whose ghost object should be added. No-op if
 *                 null or if the ghost object pointer is null.
 */
void TriggerRegistry::addToPhysicsWorld(Trigger* trigger) {
    if (!dynamicsWorld || !trigger) return;

    btPairCachingGhostObject* ghostObject = trigger->getGhostObject();
    if (ghostObject) {
        dynamicsWorld->addCollisionObject(
            ghostObject,
            btBroadphaseProxy::SensorTrigger,
            btBroadphaseProxy::AllFilter
        );
    }
}


/**
 * @brief Removes a trigger's ghost object from the Bullet dynamics world.
 *
 * Must be called before the Trigger destructor runs to prevent the broadphase
 * from holding a dangling pointer to the deleted ghost object.
 *
 * @param trigger  The trigger whose ghost object should be removed.
 */
void TriggerRegistry::removeFromPhysicsWorld(Trigger* trigger) {
    if (!dynamicsWorld || !trigger) return;

    btPairCachingGhostObject* ghostObject = trigger->getGhostObject();
    if (ghostObject) {
        dynamicsWorld->removeCollisionObject(ghostObject);
    }
}