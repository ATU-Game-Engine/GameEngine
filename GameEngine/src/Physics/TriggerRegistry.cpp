#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/Trigger.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <algorithm>


// Initialize static instance
TriggerRegistry* TriggerRegistry::instance = nullptr;

TriggerRegistry::TriggerRegistry() : dynamicsWorld(nullptr) {
}

TriggerRegistry::~TriggerRegistry() {
    clearAll();
}

TriggerRegistry& TriggerRegistry::getInstance() {
    if (!instance) {
        instance = new TriggerRegistry();
    }
    return *instance;
}

void TriggerRegistry::initialize(btDiscreteDynamicsWorld* world) {
    dynamicsWorld = world;
    std::cout << "TriggerRegistry initialized" << std::endl;
}

// === Trigger Creation ===

Trigger* TriggerRegistry::createTrigger(const std::string& name,
    TriggerType type,
    const glm::vec3& position,
    const glm::vec3& size)
{
    auto trigger = std::make_unique<Trigger>(name, type, position, size);
    return addTrigger(std::move(trigger));
}

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

// === Trigger Removal ===

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

bool TriggerRegistry::removeTrigger(const std::string& name) {
    Trigger* trigger = findTriggerByName(name);
    if (trigger) {
        removeTrigger(trigger);
        return true;
    }
    return false;
}

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

// === Update ===

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

// === Queries ===

Trigger* TriggerRegistry::findTriggerByName(const std::string& name) const {
    for (const auto& trigger : triggers) {
        if (trigger->getName() == name) {
            return trigger.get();
        }
    }
    return nullptr;
}

std::vector<Trigger*> TriggerRegistry::findTriggersByType(TriggerType type) const {
    std::vector<Trigger*> result;

    for (const auto& trigger : triggers) {
        if (trigger->getType() == type) {
            result.push_back(trigger.get());
        }
    }

    return result;
}

std::vector<Trigger*> TriggerRegistry::getAllTriggers() const {
    std::vector<Trigger*> result;
    result.reserve(triggers.size());

    for (const auto& trigger : triggers) {
        result.push_back(trigger.get());
    }

    return result;
}

bool TriggerRegistry::hasTrigger(const std::string& name) const {
    return findTriggerByName(name) != nullptr;
}

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

// === Debug ===

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

// === Private Helpers ===

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

void TriggerRegistry::removeFromPhysicsWorld(Trigger* trigger) {
    if (!dynamicsWorld || !trigger) return;

    btPairCachingGhostObject* ghostObject = trigger->getGhostObject();
    if (ghostObject) {
        dynamicsWorld->removeCollisionObject(ghostObject);
    }
}