#ifndef TRIGGER_H
#define TRIGGER_H
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include <vector>
#include <unordered_set>

class GameObject;

/**
 * @brief Defines the type/behavior of a trigger zone
 */
enum class TriggerType {
    TELEPORT,       // Moves object to another location
    SPEED_ZONE,     // Changes object velocity
    EVENT          // User-defined behavior via callbacks
};

/**
 * @brief A trigger volume that detects object entry/exit without physical collision response.
 *
 * Uses Bullet's ghost objects to detect overlaps without affecting physics simulation.
 * Triggers fire callbacks when objects enter, stay in, or exit the trigger volume.
 *
 * Examples:
 * - Goal zones that end the level
 * - Death zones that respawn the player
 * - Checkpoints that save progress
 * - Speed boosts or jump pads
 */
class Trigger {
private:
    btPairCachingGhostObject* ghostObject;  // Bullet's ghost object for collision detection
    btCollisionShape* shape;                 // Trigger volume shape (box, sphere, etc.)

    std::string name;
    TriggerType type;

    glm::vec3 position;
    glm::vec3 size;

    bool enabled;
    bool debugVisualize;  // Whether to draw in editor

    // Objects currently inside the trigger
    std::vector<GameObject*> objectsInside;

    // Callback functions (optional - can use default behavior based on type)
    std::function<void(GameObject*)> onEnterCallback;
    std::function<void(GameObject*)> onExitCallback;
    std::function<void(GameObject*, float)> onStayCallback;  // Called every frame object is inside

    // Teleport-specific data
    glm::vec3 teleportDestination;
    // Speed zone data
    glm::vec3 forceDirection;
    float forceMagnitude;


    // Tag filter — if non - empty, only objects carrying ALL required tags are affected.
    // An empty set means "affect everything" 
    std::unordered_set<std::string> requiredTags;
    // identifies what the trigger does seperate from required tags  that filters what acitvates the trigger
    // used only for event triggers 
    std::string behaviourTag;

    uint64_t id;
    static uint64_t nextID;

    // Returns true if obj has ALL required tags (or if no filter is set).
    bool passesTagFilter(GameObject* obj) const;

    int maxUses = -1;   // -1 = infinite uses
    int useCount = 0;

    bool pendingDestroy = false;

public:
    /**
     * @brief Create a trigger volume
     * @param triggerName Display name
     * @param triggerType Type of trigger behavior
     * @param pos World position (center)
     * @param size Dimensions (half-extents for box, radius for sphere)
     */
    Trigger(const std::string& triggerName,
        TriggerType triggerType,
        const glm::vec3& pos,
        const glm::vec3& size);

    ~Trigger();

    // === Core Functionality ===

    /**
     * @brief Check for objects entering/exiting the trigger
     * Should be called every physics step
     */
    void update(btDiscreteDynamicsWorld* world, float deltaTime);

    /**
     * @brief Execute trigger's default behavior based on type
     */
    void executeDefaultBehavior(GameObject* obj);

    // === Getters ===

    uint64_t getID() const { return id; }
    const std::string& getName() const { return name; }
    TriggerType getType() const { return type; }
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getSize() const { return size; }
    bool isEnabled() const { return enabled; }
    bool shouldDebugVisualize() const { return debugVisualize; }

    btPairCachingGhostObject* getGhostObject() const { return ghostObject; }

    const std::vector<GameObject*>& getObjectsInside() const { return objectsInside; }

    const std::string& getBehaviourTag()         const { return behaviourTag; }
 
    // === Setters ===
    void setBehaviourTag(const std::string& tag) { behaviourTag = tag; }

    void setName(const std::string& newName) { name = newName; }
    void setEnabled(bool enable) { enabled = enable; }
    void setDebugVisualize(bool visualize) { debugVisualize = visualize; }

    void setPosition(const glm::vec3& pos);
    void setSize(const glm::vec3& newSize);

    // === Callbacks ===

    /**
     * @brief Set custom callback for when object enters trigger
     * Overrides default behavior
     */
    void setOnEnterCallback(std::function<void(GameObject*)> callback) {
        onEnterCallback = callback;
    }

    void setOnExitCallback(std::function<void(GameObject*)> callback) {
        onExitCallback = callback;
    }

    void setOnStayCallback(std::function<void(GameObject*, float)> callback) {
        onStayCallback = callback;
    }
    void clearOnEnterCallback() { onEnterCallback = nullptr; }
    void clearOnExitCallback() { onExitCallback = nullptr; }
    void clearOnStayCallback() { onStayCallback = nullptr; }

    // === Type-Specific Configuration ===

    /**
     * @brief Set teleport destination (for TELEPORT type)
     */
    void setTeleportDestination(const glm::vec3& dest) {
        teleportDestination = dest;
    }

    glm::vec3 getTeleportDestination() const { return teleportDestination; }

    /**
     * @brief Set force to apply (for SPEED_ZONE type)
     */
    void setForce(const glm::vec3& direction, float magnitude);
    glm::vec3 getForceDirection() const { return forceDirection; }
    float getForceMagnitude() const { return forceMagnitude; }

    // When one or more required tags are set, the trigger will ONLY fire for
    // objects that carry ALL of those tags. Empty = affect every object (default).
    void requireTag(const std::string& tag);
    void removeRequiredTag(const std::string& tag) { requiredTags.erase(tag); }
    void clearRequiredTags() { requiredTags.clear(); }
    bool hasTagFilter() const { return !requiredTags.empty(); }
    const std::unordered_set<std::string>& getRequiredTags() const { return requiredTags; }

    void setMaxUses(int uses) { maxUses = uses; }
    int getMaxUses() const { return maxUses; }

    int getUseCount() const { return useCount; }

    bool canActivate() const {
        return maxUses < 0 || useCount < maxUses;
    }

    void incrementUse() { useCount++; }

    bool shouldDestroy() const {
        return maxUses >= 0 && useCount >= maxUses;
    }

    void markForDestroy() { pendingDestroy = true; }
    bool isPendingDestroy() const { return pendingDestroy; }
};

#endif // TRIGGER_H