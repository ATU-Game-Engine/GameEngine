/**
 * @file Trigger.h
 * @brief Declares the Trigger class and TriggerType enum.
 *
 * A Trigger is a non-physical volume that detects when dynamic rigid bodies
 * enter, stay inside, or leave it. It is implemented using Bullet's
 * btPairCachingGhostObject with CF_NO_CONTACT_RESPONSE so overlaps are
 * reported without generating contact forces.
 *
 * Each trigger supports three event callbacks:
 *   onEnterCallback — fired once when a qualifying object first enters.
 *   onStayCallback  — fired every tick while a qualifying object is inside.
 *   onExitCallback  — fired once when a qualifying object leaves.
 *
 * If no enter callback is set, executeDefaultBehavior() handles the built-in
 * TELEPORT and SPEED_ZONE types automatically. EVENT triggers are fully
 * callback-driven via Scene's triggerScriptRegistry.
 *
 * Optional features:
 *   - Tag filtering (requireTag): only objects with ALL required tags activate the trigger.
 *   - Use limiting (setMaxUses): trigger disables and self-destructs after N activations.
 *   - Behaviour tag (setBehaviourTag): used by Scene::registerTriggerScript() to wire
 *     custom script callbacks at scene load time.
 *
 * Triggers are owned and updated by TriggerRegistry.
 */
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
 * @brief Identifies the built-in behaviour mode of a trigger.
 *
 * TELEPORT   — On enter, teleports the object to teleportDestination.
 * SPEED_ZONE — On enter, applies a one-shot central impulse to the object.
 * EVENT      — No built-in behaviour; fully driven by onEnterCallback,
 *              onStayCallback, and onExitCallback set via registerTriggerScript().
 */
enum class TriggerType {
    TELEPORT,      ///< Instantly moves the entering object to a destination position.
    SPEED_ZONE,    ///< Applies a directional impulse to the entering object.
    EVENT          ///< Callback-only trigger — no default behaviour.
};

/**
 * @brief A non-physical detection volume that fires callbacks on object overlap.
 *
 * Uses a btPairCachingGhostObject (CF_NO_CONTACT_RESPONSE) to track which
 * dynamic rigid bodies are currently inside the volume. TriggerRegistry::update()
 * calls Trigger::update() once per physics tick, which diffs the current overlap
 * list against the previous frame's list to detect enter and exit events.
 *
 * All user-facing configuration (callbacks, tag filters, use limits) is applied
 * through setters and is safe to call at any time, including from inside a callback.
 *
 * Triggers are created via TriggerRegistry::createTrigger() and must not be
 * directly instantiated or deleted by gameplay code.
 */ 
class Trigger {
private:
    btPairCachingGhostObject* ghostObject;  ///< Bullet ghost object — detects overlaps, no contact forces.
    btCollisionShape* shape;                 ///< Box collision shape owned by this trigger.

    std::string name;  ///< Human-readable label used in the editor and debug output.
    TriggerType type;  ///< Built-in behaviour mode.

    glm::vec3 position;  ///< World-space centre of the trigger volume.
    glm::vec3 size;  ///< Full extents of the box volume.

    bool enabled; ///< When false, update() returns immediately without firing callbacks.
    bool debugVisualize;  ///< Whether the editor should draw a debug outline for this trigger.

    // Objects currently inside the trigger
    std::vector<GameObject*> objectsInside; ///< Objects confirmed inside the volume last tick.

    // Callback functions (optional - can use default behavior based on type)
    std::function<void(GameObject*)> onEnterCallback; ///< Fired once on first entry.
    std::function<void(GameObject*)> onExitCallback; ///< Fired once on exit.
    std::function<void(GameObject*, float)> onStayCallback;  ///< Fired every tick while inside.

    // Teleport-specific data
    glm::vec3 teleportDestination; ///< Destination used by the TELEPORT built-in behaviour.
    // Speed zone data
    glm::vec3 forceDirection; ///< Normalised direction used by the SPEED_ZONE behaviour.
    float forceMagnitude;  ///< Impulse magnitude used by the SPEED_ZONE behaviour.


    /// Objects must carry ALL tags in this set to activate the trigger.
    /// Empty set = no filter (every dynamic body activates it).
    std::unordered_set<std::string> requiredTags;

    /// Identifies the script behaviour wired by Scene::registerTriggerScript().
    /// Empty for TELEPORT and SPEED_ZONE triggers that have no associated script.
    std::string behaviourTag;

    uint64_t id; ///< Unique trigger ID assigned at construction.
    static uint64_t nextID; ///< Monotonically increasing ID counter.

    /**
     * @brief Returns true if obj has ALL tags in requiredTags, or if the
     *        set is empty (no filter configured).
     *
     * @param obj  The GameObject to test.
     */
    bool passesTagFilter(GameObject* obj) const;

    int maxUses = -1;  ///< Maximum number of activations. -1 = unlimited.
    int useCount = 0; ///< Number of times this trigger has fired.

    bool pendingDestroy = false; ///< True when shouldDestroy() has been triggered.

public:
    /**
      * @brief Constructs a trigger volume at the given world position and size.
      *
      * Creates a btBoxShape and a btPairCachingGhostObject with
      * CF_NO_CONTACT_RESPONSE. The ghost object must be added to the Bullet
      * world by TriggerRegistry after construction.
      *
      * @param triggerName  Human-readable label.
      * @param triggerType  Built-in behaviour mode (TELEPORT, SPEED_ZONE, EVENT).
      * @param pos          World-space centre of the trigger volume.
      * @param size         Full extents of the box volume (not half-extents).
      */
    Trigger(const std::string& triggerName,
        TriggerType triggerType,
        const glm::vec3& pos,
        const glm::vec3& size);

    /**
     * @brief Destructor — deletes the ghost object and collision shape.
     *
     * TriggerRegistry must remove the ghost object from Bullet before this
     * destructor runs to prevent dangling pointers in the broadphase.
     */
    ~Trigger();

    //  Core Functionality 


    /**
     * @brief Detects enter, stay, and exit events by diffing the Bullet overlap
     *        list against the previous frame's objectsInside list.
     *
     * Should be called once per physics tick by TriggerRegistry::update().
     * Skips static bodies and objects that fail the tag filter. Checks the
     * usage limit before firing callbacks and marks for destruction if exhausted.
     *
     * @param world      Bullet dynamics world (reserved for future query variants).
     * @param deltaTime  Physics timestep forwarded to onStayCallback.
     */
    void update(btDiscreteDynamicsWorld* world, float deltaTime);

    /**
   * @brief Executes the built-in behaviour for TELEPORT and SPEED_ZONE triggers.
   *
   * Called by update() when no onEnterCallback is set. EVENT triggers do
   * nothing here — they rely entirely on registered callbacks.
   *
   * @param obj  The GameObject that entered the trigger.
   */
    void executeDefaultBehavior(GameObject* obj);

    // === Getters ===

    uint64_t getID() const { return id; } ///< Unique trigger ID.
    const std::string& getName() const { return name; } ///< Display name.
    TriggerType getType() const { return type; }   ///< Built-in behaviour type.
    glm::vec3 getPosition() const { return position; }    ///< World-space centre.
    glm::vec3 getSize() const { return size; }   ///< Full box extents. 
    bool isEnabled() const { return enabled; }   ///< Whether the trigger fires.
    bool shouldDebugVisualize() const { return debugVisualize; } ///< Editor debug draw flag.

    /// Returns the Bullet ghost object (used by TriggerRegistry to add/remove from world).
    btPairCachingGhostObject* getGhostObject() const { return ghostObject; }

    /// Returns the list of objects currently confirmed inside the trigger.
    const std::vector<GameObject*>& getObjectsInside() const { return objectsInside; }

    /// Returns the behaviour tag used by Scene::registerTriggerScript() for script wiring.
    const std::string& getBehaviourTag()         const { return behaviourTag; }
 
    //  Setters 
    /// Sets the behaviour tag used to look up a registered trigger script.
    void setBehaviourTag(const std::string& tag) { behaviourTag = tag; }

    void setName(const std::string& newName) { name = newName; }  ///< Updates the display name.
    void setEnabled(bool enable) { enabled = enable; } ///< Enables or disables the trigger.
    void setDebugVisualize(bool visualize) { debugVisualize = visualize; }  ///< Controls editor outline.

    /**
    * @brief Moves the trigger to a new world-space position.
    *
    * Updates both the cached position and the ghost object's world transform.
    *
    * @param pos  New world-space centre position.
    */
    void setPosition(const glm::vec3& pos);

    /**
     * @brief Resizes the trigger volume by recreating the Bullet collision shape.
     *
     * Bullet shapes are immutable after creation, so a new btBoxShape is
     * allocated and the ghost object's shape pointer is updated.
     *
     * @param newSize  New full extents of the box (not half-extents).
     */
    void setSize(const glm::vec3& newSize);

    // Callbacks 

    /**
     * @brief Sets a custom enter callback, overriding the default built-in behaviour.
     *
     * The callback is called once per object the first time it enters the volume.
     *
     * @param callback  Lambda or function receiving the entering GameObject.
     */
    void setOnEnterCallback(std::function<void(GameObject*)> callback) {
        onEnterCallback = callback;
    }

    /**
     * @brief Sets a callback fired once when an object leaves the volume.
     *
     * @param callback  Lambda or function receiving the exiting GameObject.
     */
    void setOnExitCallback(std::function<void(GameObject*)> callback) {
        onExitCallback = callback;
    }

    /**
     * @brief Sets a callback fired every tick while an object remains inside.
     *
     * @param callback  Lambda or function receiving the object and deltaTime.
     */
    void setOnStayCallback(std::function<void(GameObject*, float)> callback) {
        onStayCallback = callback;
    }
    void clearOnEnterCallback() { onEnterCallback = nullptr; }
    void clearOnExitCallback() { onExitCallback = nullptr; }
    void clearOnStayCallback() { onStayCallback = nullptr; }

    // Type-Specific Configuration 

    /**
     * @brief Sets the teleport destination for TELEPORT triggers.
     *
     * The entering object will be moved to this world-space position on activation.
     *
     * @param dest  World-space destination position.
     */
    void setTeleportDestination(const glm::vec3& dest) {
        teleportDestination = dest;
    }
    /// Returns the configured teleport destination.
    glm::vec3 getTeleportDestination() const { return teleportDestination; }

    /**
     * @brief Sets the force direction and magnitude for SPEED_ZONE triggers.
     *
     * The direction is normalised internally. Falls back to +Y for zero vectors.
     *
     * @param direction  Desired impulse direction (normalised internally).
     * @param magnitude  Impulse magnitude in N·s.
     */
    void setForce(const glm::vec3& direction, float magnitude);
    /// Returns the normalised force direction used by SPEED_ZONE triggers.
    glm::vec3 getForceDirection() const { return forceDirection; }
    /// Returns the impulse magnitude used by SPEED_ZONE triggers.
    float getForceMagnitude() const { return forceMagnitude; }

    /**
     * @brief Adds a required tag to the trigger's filter set.
     *
     * Once any tag is added, only objects carrying ALL required tags will
     * activate this trigger. An empty set means no filter (default).
     *
     * @param tag  Tag string the entering object must have.
     */
    void requireTag(const std::string& tag);
    /// Removes a single tag from the required set.
    void removeRequiredTag(const std::string& tag) { requiredTags.erase(tag); }
    /// Clears all required tags — trigger reverts to affecting every object.
    void clearRequiredTags() { requiredTags.clear(); }
    /// Returns true if at least one required tag has been set.
    bool hasTagFilter() const { return !requiredTags.empty(); }
    /// Returns the full set of required tags.
    const std::unordered_set<std::string>& getRequiredTags() const { return requiredTags; }

    /**
     * @brief Sets the maximum number of times this trigger can activate.
     *
     * After maxUses activations, canActivate() returns false and the trigger
     * disables and queues itself for destruction. Pass -1 for unlimited uses
     * (the default).
     *
     * @param uses  Maximum activation count, or -1 for unlimited.
     */
    void setMaxUses(int uses) { maxUses = uses; }
    /// Returns the configured maximum uses (-1 = unlimited).
    int getMaxUses() const { return maxUses; }
    /// Returns the number of times this trigger has fired so far.
    int getUseCount() const { return useCount; }

    /**
   * @brief Returns true if the trigger can still activate.
   *
   * Always true for unlimited triggers (maxUses < 0).
   */
    bool canActivate() const {
        return maxUses < 0 || useCount < maxUses;
    }

    /// Increments the activation counter. Called by update() after each successful activation.
    void incrementUse() { useCount++; }

    /**
     * @brief Returns true if the maximum use count has been reached and the
     *        trigger should be destroyed.
     */
    bool shouldDestroy() const {
        return maxUses >= 0 && useCount >= maxUses;
    }

    /// Marks this trigger for removal by TriggerRegistry on the next update tick.
    void markForDestroy() { pendingDestroy = true; }

    /// Returns true if this trigger has been marked for destruction.
    bool isPendingDestroy() const { return pendingDestroy; }
};

#endif // TRIGGER_H