/**
 * @file PhysicsQuery.cpp
 * @brief Implementation of PhysicsQuery — a wrapper around Bullet's raycast
 *        API that returns results as engine-friendly RaycastHit structs with
 *        direct GameObject references.
 *
 * PhysicsQuery is constructed by Physics::initialize() and stored as a
 * unique_ptr on the Physics instance. Scripts access it via
 * Physics::getQuerySystem().
 *
 * Provided query types:
 *   raycast()        — closest single hit along a ray.
 *   raycastAll()     — all hits along a ray, sorted nearest-first.
 *   isGrounded()     — downward raycast to test whether an object is on a surface.
 *   hasLineOfSight() — checks whether a ray reaches a world point unobstructed.
 *   canSeeObject()   — checks whether a ray reaches a specific GameObject.
 *
 * All methods return false / empty results rather than crashing when the
 * dynamics world pointer is null.
 */
#include "../include/Physics/PhysicsQuery.h"
#include "../include/Scene/GameObject.h"
#include <algorithm>
#include <iostream>

 /**
  * @brief Constructs a PhysicsQuery bound to the given Bullet dynamics world.
  *
  * @param world  The active Bullet dynamics world. A warning is logged if null,
  *               but construction still succeeds — all query methods will return
  *               empty/false results until a valid world is available.
  */
PhysicsQuery::PhysicsQuery(btDiscreteDynamicsWorld* world)
    : dynamicsWorld(world)
{
    if (!dynamicsWorld) {
        std::cerr << "Warning: PhysicsQuery created with null dynamics world" << std::endl;
    }
}
// Helper methods
// get gameObject from Bullet collision object user pointer (returns nullptr if no pointer or invalid)
/**
 * @brief Retrieves the GameObject stored in a Bullet collision object's user pointer.
 *
 * Every rigid body created by Physics::createRigidBody() has its owning
 * GameObject set as the user pointer via body->setUserPointer(). This method
 * safely casts that pointer back.
 *
 * @param obj  Bullet collision object to query. Returns nullptr if null.
 * @return     Pointer to the owning GameObject, or nullptr if the user pointer
 *             has not been set (e.g. trigger ghost objects).
 */
GameObject* PhysicsQuery::getGameObject(const btCollisionObject* obj) const {
    if (!obj) return nullptr;
    return static_cast<GameObject*>(obj->getUserPointer());
}

// convert Bullet raycast result to RaycastHit struct with GameObject reference and material properties 
/**
 * @brief Populates a RaycastHit from the result of a Bullet closest-ray test.
 *
 * Extracts world-space hit point, surface normal, distance from the ray
 * origin, and material properties (friction / restitution) from the hit
 * rigid body. If the result has no hit, the output struct is reset to its
 * default (zero-initialised) state.
 *
 * @param result    A completed ClosestRayResultCallback from Bullet's rayTest().
 *                  Must be cast-compatible with ClosestRayResultCallback.
 * @param rayStart  World-space origin of the ray (used to compute distance).
 * @param hitInfo   Output struct to fill with hit data.
 */
void PhysicsQuery::fillHitInfo(
    const btCollisionWorld::RayResultCallback& result,
    const glm::vec3& rayStart,
    RaycastHit& hitInfo) const
{
    if (!result.hasHit()) {
        hitInfo = RaycastHit();
        return;
    }

    // Cast to ClosestRayResultCallback to access hit data
    const btCollisionWorld::ClosestRayResultCallback& closestResult =
        static_cast<const btCollisionWorld::ClosestRayResultCallback&>(result);

    // Get GameObject from user pointer
    hitInfo.object = getGameObject(closestResult.m_collisionObject);

    // Hit point in world space
    hitInfo.point = glm::vec3(
        closestResult.m_hitPointWorld.x(),
        closestResult.m_hitPointWorld.y(),
        closestResult.m_hitPointWorld.z()
    );

    // Surface normal at hit point
    hitInfo.normal = glm::vec3(
        closestResult.m_hitNormalWorld.x(),
        closestResult.m_hitNormalWorld.y(),
        closestResult.m_hitNormalWorld.z()
    );

    // Distance from ray origin to hit point
    glm::vec3 diff = hitInfo.point - rayStart;
    hitInfo.distance = glm::length(diff);

    // Get material properties from hit object
    if (const btRigidBody* body = btRigidBody::upcast(closestResult.m_collisionObject)) {
        hitInfo.friction = body->getFriction();
        hitInfo.restitution = body->getRestitution();
    }
    else {
        hitInfo.friction = 0.5f;
        hitInfo.restitution = 0.0f;
    }
}


// Raycasting

/**
 * @brief Casts a ray and returns the closest hit against all collision layers.
 *
 * Convenience overload that uses btBroadphaseProxy::AllFilter as the collision
 * mask, so every physics object in the world is a valid hit target.
 *
 * @param from     World-space ray origin.
 * @param to       World-space ray end point.
 * @param hitInfo  Output struct filled with hit data on success.
 * @return         True if the ray hit something, false if the path was clear.
 */
bool PhysicsQuery::raycast(
    const glm::vec3& from,
    const glm::vec3& to,
    RaycastHit& hitInfo) const
{
    // Default: raycast against everything
    return raycast(from, to, hitInfo, btBroadphaseProxy::AllFilter);
}

/**
 * @brief Casts a ray against a filtered set of collision layers and returns
 *        the closest hit.
 *
 * The collisionMask parameter allows the caller to restrict which Bullet
 * collision groups are tested (e.g. exclude triggers or debris layers).
 * Uses Bullet's ClosestRayResultCallback internally so only one heap
 * allocation is made per call.
 *
 * @param from           World-space ray origin.
 * @param to             World-space ray end point.
 * @param hitInfo        Output struct filled with hit data on success.
 * @param collisionMask  Bullet broadphase filter mask. Use AllFilter to test
 *                       all objects, or a bitmask for selective testing.
 * @return               True if the ray hit something within the mask.
 */
bool PhysicsQuery::raycast(
    const glm::vec3& from,
    const glm::vec3& to,
    RaycastHit& hitInfo,
    short collisionMask) const
{
    if (!dynamicsWorld) {
        std::cerr << "Error: PhysicsQuery has no dynamics world" << std::endl;
        return false;
    }

    // Convert GLM to Bullet
    btVector3 btFrom(from.x, from.y, from.z);
    btVector3 btTo(to.x, to.y, to.z);

    // Perform raycast
    btCollisionWorld::ClosestRayResultCallback rayCallback(btFrom, btTo);
    rayCallback.m_collisionFilterMask = collisionMask;

    dynamicsWorld->rayTest(btFrom, btTo, rayCallback);

    // Fill hit info
    fillHitInfo(rayCallback, from, hitInfo);

    return rayCallback.hasHit();
}

/**
 * @brief Casts a ray and returns all hits along its length, sorted nearest-first.
 *
 * Uses Bullet's AllHitsRayResultCallback to collect every intersection rather
 * than stopping at the closest. The returned vector is sorted by ascending
 * distance so index 0 is always the nearest hit.
 *
 * Useful for piercing projectiles, portal detection, or occlusion queries
 * that need to know about every surface along a path.
 *
 * @param from  World-space ray origin.
 * @param to    World-space ray end point.
 * @return      Vector of RaycastHit structs, sorted nearest-first. Empty if
 *              no objects were hit or the dynamics world is null.
 */
std::vector<RaycastHit> PhysicsQuery::raycastAll(
    const glm::vec3& from,
    const glm::vec3& to) const
{
    std::vector<RaycastHit> hits;

    if (!dynamicsWorld) {
        std::cerr << "Error: PhysicsQuery has no dynamics world" << std::endl;
        return hits;
    }

    // Convert GLM to Bullet
    btVector3 btFrom(from.x, from.y, from.z);
    btVector3 btTo(to.x, to.y, to.z);

    // Use AllHitsRayResultCallback to get all intersections
    btCollisionWorld::AllHitsRayResultCallback rayCallback(btFrom, btTo);
    dynamicsWorld->rayTest(btFrom, btTo, rayCallback);

    if (!rayCallback.hasHit()) {
        return hits;
    }

    // Convert all hits to RaycastHit structs
    int numHits = rayCallback.m_collisionObjects.size();
    hits.reserve(numHits);

    for (int i = 0; i < numHits; ++i) {
        RaycastHit hit;

        // Get GameObject
        hit.object = getGameObject(rayCallback.m_collisionObjects[i]);

        // Hit point
        const btVector3& hitPoint = rayCallback.m_hitPointWorld[i];
        hit.point = glm::vec3(hitPoint.x(), hitPoint.y(), hitPoint.z());

        // Normal
        const btVector3& hitNormal = rayCallback.m_hitNormalWorld[i];
        hit.normal = glm::vec3(hitNormal.x(), hitNormal.y(), hitNormal.z());

        // Distance
        glm::vec3 diff = hit.point - from;
        hit.distance = glm::length(diff);

        // Material properties
        if (const btRigidBody* body = btRigidBody::upcast(rayCallback.m_collisionObjects[i])) {
            hit.friction = body->getFriction();
            hit.restitution = body->getRestitution();
        }
        else {
            hit.friction = 0.5f;
            hit.restitution = 0.0f;
        }

        hits.push_back(hit);
    }

    // Sort by distance (closest first)
    std::sort(hits.begin(), hits.end(),
        [](const RaycastHit& a, const RaycastHit& b) {
            return a.distance < b.distance;
        });

    return hits;
}


// quick Queries

/**
 * @brief Tests whether a position is resting on or very close to a surface.
 *
 * Fires a downward ray of length maxDistance from position. The 90% threshold
 * on hit distance provides a small margin for floating-point error and slight
 * surface penetration, preventing false negatives when an object is just barely
 * touching the ground.
 *
 * Typical usage: character controller ground check before applying jump impulse.
 *
 * @param position     World-space origin of the ground check.
 * @param maxDistance  Maximum downward distance to check (default: 1.1f in header).
 * @return             True if a surface was detected within 90% of maxDistance.
 */
bool PhysicsQuery::isGrounded(
    const glm::vec3& position,
    float maxDistance) const
{
    glm::vec3 rayStart = position;
    glm::vec3 rayEnd = position - glm::vec3(0, maxDistance, 0);

    RaycastHit hit;
    if (raycast(rayStart, rayEnd, hit)) {
        // Consider grounded if hit is within 90% of max distance
        // (leaves small margin for floating point error)
        return hit.distance < (maxDistance * 0.9f);
    }

    return false;
}


/**
 * @brief Returns true if a ray from `from` to `to` reaches the target point
 *        without hitting anything else first.
 *
 * Fires a raycast and checks whether the first hit point is within 1 cm of
 * the target world position. If the ray hits an object before reaching `to`,
 * the line of sight is considered blocked.
 *
 * If no object is hit at all, the path is considered clear (returns true).
 *
 * @param from  World-space observer position.
 * @param to    World-space target point to check visibility toward.
 * @return      True if the path from `from` to `to` is unobstructed.
 */
bool PhysicsQuery::hasLineOfSight(
    const glm::vec3& from,
    const glm::vec3& to) const
{
    RaycastHit hit;
    if (raycast(from, to, hit)) {
        // If raycast hit something, check if it's exactly at the target point
        // (within small epsilon for floating point comparison)
        glm::vec3 diff = hit.point - to;
        float distToTarget = glm::length(diff);

        // If hit point is very close to target, line of sight is clear
        return distToTarget < 0.01f;
    }

    // No hit means clear line of sight
    return true;
}



/**
 * @brief Returns true if a ray from `from` reaches a specific GameObject
 *        without being blocked by another object.
 *
 * Fires a raycast toward targetOffset applied to target's world position.
 * Succeeds only if the first hit object is exactly `target` — any other
 * object occluding the path returns false.
 *
 * Useful for AI visibility checks where the AI must have an unobstructed
 * direct hit on the player, not just a nearby surface.
 *
 * @param from          World-space observer position (e.g. enemy eye position).
 * @param target        The GameObject that must be hit for the check to pass.
 * @param targetOffset  Local offset applied to target->getPosition() to aim
 *                      at a specific point on the target (e.g. centre-of-mass).
 * @return              True if the closest hit along the ray is `target`.
 */
bool PhysicsQuery::canSeeObject(
    const glm::vec3& from,
    GameObject* target,
    const glm::vec3& targetOffset) const
{
    if (!target) return false;

    glm::vec3 targetPoint = target->getPosition() + targetOffset;

    RaycastHit hit;
    if (raycast(from, targetPoint, hit)) {
        // Did we hit the target object specifically?
        return hit.object == target;
    }

    // No hit means nothing in that direction (shouldn't happen if target exists)
    return false;
}