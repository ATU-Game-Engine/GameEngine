#include "../include/Physics/Trigger.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/TriggerRegistry.h"
#include <iostream>
#include <algorithm>

uint64_t Trigger::nextID = 1;

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

Trigger::~Trigger() {
    delete ghostObject;
    delete shape;
}
void Trigger::requireTag(const std::string& tag)
{
    std::cout << "[Trigger] requireTag called: '" << tag << "' on '" << name << "'" << std::endl;
    requiredTags.insert(tag);
}
// Returns true if obj has ALL of the trigger's required tags,
// or if no required tags have been set (empty = affect everything).
bool Trigger::passesTagFilter(GameObject* obj) const
{
    if (requiredTags.empty()) return true;   // no filter — affect everything
    for (const auto& tag : requiredTags)
        if (!obj->hasTag(tag)) return false; // missing at least one required tag
    return true;
}
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

void Trigger::setPosition(const glm::vec3& pos) {
    position = pos;

    if (ghostObject) {
        btTransform transform = ghostObject->getWorldTransform();
        transform.setOrigin(btVector3(pos.x, pos.y, pos.z));
        ghostObject->setWorldTransform(transform);
    }
}

void Trigger::setSize(const glm::vec3& newSize) {
    size = newSize;

    // Recreate collision shape with new size
    delete shape;
    shape = new btBoxShape(btVector3(newSize.x, newSize.y, newSize.z));

    if (ghostObject) {
        ghostObject->setCollisionShape(shape);
    }
}
void Trigger::setForce(const glm::vec3& direction, float magnitude)
{
    // Safely normalize — avoid NaN if zero vector passed
    float len = glm::length(direction);
    forceDirection = (len > 0.0001f) ? direction / len : glm::vec3(0, 1, 0);
    forceMagnitude = magnitude;
}