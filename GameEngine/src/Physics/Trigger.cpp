/**
 * @file Trigger.cpp
 * @brief Implementation of the Trigger class — a non-physical volume that
 *        detects when dynamic rigid bodies enter, stay inside, or leave it.
 *
 * Triggers are implemented using Bullet's btPairCachingGhostObject with the
 * CF_NO_CONTACT_RESPONSE flag set, meaning they detect overlaps but exert no
 * physical forces. TriggerRegistry::update() calls Trigger::update() each
 * physics tick to fire the appropriate callbacks.
 *
 * Each trigger supports three event callbacks:
 *   onEnterCallback — fired once when a qualifying object first enters.
 *   onStayCallback  — fired every tick while a qualifying object remains inside.
 *   onExitCallback  — fired once when a qualifying object leaves.
 *
 * If no enter callback is set, executeDefaultBehavior() is called instead,
 * which handles the built-in TELEPORT and SPEED_ZONE types automatically.
 *
 * Triggers can optionally:
 *   - Filter by tags (requireTag) so only tagged objects activate them.
 *   - Limit the number of activations (maxUses) and self-destroy afterwards.
 *   - Carry a behaviourTag string used by Scene's triggerScriptRegistry to
 *     wire custom script callbacks at load time.
 */
#include "../include/Physics/Trigger.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/TriggerRegistry.h"
#include <iostream>
#include <algorithm>

uint64_t Trigger::nextID = 1;


/**
 * @brief Constructs a trigger volume at a given world position and size.
 *
 * Creates a btBoxShape sized to `size` and a btPairCachingGhostObject at
 * `pos` with CF_NO_CONTACT_RESPONSE so the volume detects overlaps without
 * generating contact forces. The ghost object must be added to the Bullet
 * world by TriggerRegistry after construction.
 *
 * Default state: enabled, debugVisualize on, force direction +Y,
 * forceMagnitude 10, unlimited uses.
 *
 * @param triggerName  Human-readable name used in the editor and debug output.
 * @param triggerType  Determines the default behaviour (TELEPORT, SPEED_ZONE,
 *                     or EVENT for callback-only triggers).
 * @param pos          World-space centre of the trigger volume.
 * @param size         Full extents of the box volume (not half-extents).
 */
Trigger::Trigger(const std::string& triggerName,
    TriggerType triggerType,
    const glm::vec3& pos,
    const glm::vec3& size)
    : name(triggerName),
    type(triggerType),
    position(pos),
    size(size),
    enabled(true),
    debugVisualize(true),
    teleportDestination(0.0f),
    forceDirection(0.0f, 1.0f, 0.0f),
    forceMagnitude(10.0f),
    id(nextID++)
{
    // Create collision shape (box by default)
    shape = new btBoxShape(btVector3(size.x, size.y, size.z));

    // Create ghost object
    ghostObject = new btPairCachingGhostObject();

    // Set transform
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(pos.x, pos.y, pos.z));
    ghostObject->setWorldTransform(transform);

    // Set collision shape
    ghostObject->setCollisionShape(shape);

    // Set collision flags to be a trigger (no physical response)
    ghostObject->setCollisionFlags(
        ghostObject->getCollisionFlags() |
        btCollisionObject::CF_NO_CONTACT_RESPONSE
    );

    std::cout << "Created trigger '" << name << "' at ("
        << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
}
/**
 * @brief Destroys the trigger and frees the Bullet ghost object and shape.
 *
 * TriggerRegistry is responsible for removing the ghost object from the
 * Bullet world before this destructor runs to prevent dangling pointers
 * inside the broadphase.
 */
Trigger::~Trigger() {
    delete ghostObject;
    delete shape;
}
// Tag filtering
/**
 * @brief Adds a required tag to this trigger's filter set.
 *
 * Once at least one required tag is set, only objects that have ALL required
 * tags will activate the trigger. An empty required-tag set means the trigger
 * affects every dynamic body regardless of tags.
 *
 * @param tag  The tag string that overlapping objects must have.
 */
void Trigger::requireTag(const std::string& tag)
{
    std::cout << "[Trigger] requireTag called: '" << tag << "' on '" << name << "'" << std::endl;
    requiredTags.insert(tag);
}
// Returns true if obj has ALL of the trigger's required tags,
// or if no required tags have been set (empty = affect everything).
/**
 * @brief Returns true if `obj` passes the trigger's tag filter.
 *
 * An empty requiredTags set always returns true (trigger affects everything).
 * Otherwise the object must have every tag in requiredTags.
 *
 * @param obj  The GameObject to test.
 * @return     True if the object has all required tags (or no filter is set).
 */
bool Trigger::passesTagFilter(GameObject* obj) const
{
    if (requiredTags.empty()) return true;   // no filter — affect everything
    for (const auto& tag : requiredTags)
        if (!obj->hasTag(tag)) return false; // missing at least one required tag
    return true;
}

/**
 * @brief Detects enter, stay, and exit events by diffing the current overlap
 *        list against the previous frame's list.
 *
 * Called once per physics tick by TriggerRegistry::update(). The update loop:
 *   1. Queries Bullet's ghost object for all currently overlapping bodies.
 *   2. Skips static bodies (invMass == 0) and objects that fail the tag filter.
 *   3. Compares the current list against objectsInside (last frame's list) to
 *      identify newly entered objects.
 *   4. For new entries: checks the usage limit, increments the use counter,
 *      fires onEnterCallback or executeDefaultBehavior(), and marks the trigger
 *      for destruction if maxUses is reached.
 *   5. For objects already inside: fires onStayCallback if set.
 *   6. For objects that were inside last frame but are no longer: fires
 *      onExitCallback if set.
 *   7. Replaces objectsInside with the current frame's list.
 *
 * @param world      The Bullet dynamics world (unused directly, reserved for
 *                   future overlap query variants).
 * @param deltaTime  Physics timestep passed to onStayCallback.
 */
void Trigger::update(btDiscreteDynamicsWorld* world, float deltaTime) {
    if (!enabled || !ghostObject) return;

    // Get all overlapping objects from Bullet
    int numOverlapping = ghostObject->getNumOverlappingObjects();

    // Build list of GameObjects currently overlapping
    std::vector<GameObject*> currentlyInside;

    for (int i = 0; i < numOverlapping; ++i) {
        btCollisionObject* colObj = ghostObject->getOverlappingObject(i);
		// Ignore static objects (mass = 0)
        btRigidBody* rb = btRigidBody::upcast(colObj);
        if (!rb || rb->getInvMass() == 0.0f) continue;
        // Get GameObject from user pointer
        GameObject* obj = static_cast<GameObject*>(colObj->getUserPointer());
        if (obj) {
            std::cout << "[Trigger debug] Overlapping: '" << obj->getName()
                << "' tags: ";
            for (const auto& t : obj->getTags()) std::cout << "'" << t << "' ";
            std::cout << "| Required: ";
            for (const auto& t : requiredTags) std::cout << "'" << t << "' ";
            std::cout << std::endl;
        }
        else {
            std::cout << "[Trigger debug] Overlapping object has null user pointer" << std::endl;
        }
        if (obj && passesTagFilter(obj) ) {
            currentlyInside.push_back(obj);
        }
    }

    // === Detect NEW entries (objects that just entered) ===
    for (GameObject* obj : currentlyInside) {
        // Was this object NOT in the trigger last frame?
        bool wasInside = std::find(objectsInside.begin(), objectsInside.end(), obj)
            != objectsInside.end();

        if (!wasInside) {
            // Object just entered!
            std::cout << "[Trigger '" << name << "'] Object entered" << std::endl;
			// Add to our list of what's inside
            objectsInside.push_back(obj);

            // Check usage limit BEFORE triggering
            if (!canActivate())
                continue;

            incrementUse();

            if (onEnterCallback) {
                onEnterCallback(obj);
            }
            else {
                executeDefaultBehavior(obj);
            }

            // Destroy trigger if max uses reached
            if (shouldDestroy())
            {
                setEnabled(false);
                markForDestroy();
            }
        }
        else {
            // Object was already inside, call stay callback
            if (onStayCallback) {
                onStayCallback(obj, deltaTime);
            }
        }
    }

    // === Detect exits (objects that just left) ===
    for (GameObject* obj : objectsInside) {
        // Is this object no longer in the trigger?
        bool stillInside = std::find(currentlyInside.begin(), currentlyInside.end(), obj)
            != currentlyInside.end();

        if (!stillInside) {
            // Object just exited!
            std::cout << "[Trigger '" << name << "'] Object exited" << std::endl;

            if (onExitCallback) {
                onExitCallback(obj);
            }
        }
    }

	// sync our main list with Bullet's current state for the next frame
    objectsInside = currentlyInside;
}

// Default behavior for built-in trigger types


/**
 * @brief Executes the built-in behaviour for TELEPORT and SPEED_ZONE triggers.
 *
 * Called by update() when no onEnterCallback has been registered. EVENT
 * triggers do nothing here — they are intended to be fully driven by callbacks
 * set via registerTriggerScript().
 *
 * TELEPORT: immediately sets the object's position to teleportDestination.
 * SPEED_ZONE: applies a one-shot central impulse in forceDirection scaled by
 *             forceMagnitude.
 *
 * @param obj  The GameObject that just entered the trigger.
 */
void Trigger::executeDefaultBehavior(GameObject* obj) {
    if (!obj) return;

    switch (type) {
    case TriggerType::TELEPORT:
        std::cout << "[TELEPORT] Teleporting to (" << teleportDestination.x
            << ", " << teleportDestination.y << ", " << teleportDestination.z << ")" << std::endl;
        obj->setPosition(teleportDestination);
        break;

    case TriggerType::SPEED_ZONE:
        if (obj->hasPhysics()) {
            btRigidBody* body = obj->getRigidBody();
            if (body) {
                btVector3 force(
                    forceDirection.x * forceMagnitude,
                    forceDirection.y * forceMagnitude,
                    forceDirection.z * forceMagnitude
                );
                body->applyCentralImpulse(force);
                std::cout << "[SPEED ZONE] Applied force" << std::endl;
            }
        }
        break;

    case TriggerType::EVENT:
        // Custom behavior handled by callbacks
        break;
    }
}

//Setters

/**
 * @brief Moves the trigger volume to a new world-space position.
 *
 * Updates both the cached position field and the Bullet ghost object's
 * world transform so overlap detection immediately reflects the new location.
 *
 * @param pos  New world-space centre position.
 */
void Trigger::setPosition(const glm::vec3& pos) {
    position = pos;

    if (ghostObject) {
        btTransform transform = ghostObject->getWorldTransform();
        transform.setOrigin(btVector3(pos.x, pos.y, pos.z));
        ghostObject->setWorldTransform(transform);
    }
}

/**
 * @brief Resizes the trigger volume by recreating the Bullet collision shape.
 *
 * Bullet collision shapes are immutable after creation, so resizing requires
 * deleting the old btBoxShape and creating a new one. The ghost object's shape
 * pointer is updated immediately so the change takes effect on the next
 * broadphase update.
 *
 * @param newSize  New full extents of the box volume (not half-extents).
 */
void Trigger::setSize(const glm::vec3& newSize) {
    size = newSize;

    // Recreate collision shape with new size
    delete shape;
    shape = new btBoxShape(btVector3(newSize.x, newSize.y, newSize.z));

    if (ghostObject) {
        ghostObject->setCollisionShape(shape);
    }
}

/**
 * @brief Sets the force direction and magnitude for SPEED_ZONE triggers.
 *
 * The direction is normalised internally. If a zero vector is passed the
 * direction defaults to +Y to prevent NaN in the force calculation.
 *
 * @param direction  Desired force direction in world space. Need not be
 *                   pre-normalised. Falls back to +Y if zero-length.
 * @param magnitude  Force magnitude in N·s (applied as an impulse on enter).
 */
void Trigger::setForce(const glm::vec3& direction, float magnitude)
{
    // Safely normalize — avoid NaN if zero vector passed
    float len = glm::length(direction);
    forceDirection = (len > 0.0001f) ? direction / len : glm::vec3(0, 1, 0);
    forceMagnitude = magnitude;
}