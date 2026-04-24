/**
 * @file PhysicsQuery.h
 * @brief Declares RaycastHit and PhysicsQuery — the engine's interface for
 *        querying the Bullet Physics world without direct Bullet API access.
 *
 * PhysicsQuery wraps Bullet's rayTest() API and returns results as
 * engine-friendly RaycastHit structs that include a direct GameObject pointer,
 * world-space hit point and normal, distance, and material properties.
 *
 * Scripts access the query system via Physics::getQuerySystem(). Typical uses:
 *   - Raycasting for weapon hit detection, interaction systems, camera occlusion.
 *   - Ground checks in character controllers (isGrounded).
 *   - AI line-of-sight tests (hasLineOfSight, canSeeObject).
 *
 * PhysicsQuery is constructed by Physics::initialize() and stored as a
 * unique_ptr on the Physics instance. It holds a non-owning pointer to the
 * Bullet dynamics world and must not outlive it.
 */
#ifndef PHYSICSQUERY_H
#define PHYSICSQUERY_H

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <vector>

class GameObject;

/**
 * @brief Stores the result of a single raycast intersection.
 *
 * Populated by PhysicsQuery::raycast() and PhysicsQuery::raycastAll().
 * Use hasHit() to check whether the ray intersected anything before reading
 * the other fields.
 *
 * Material properties (friction, restitution) are read directly from the
 * hit Bullet rigid body at query time. For non-rigid-body colliders (e.g.
 * trigger ghost objects) these default to 0.5 and 0.0 respectively.
 */
struct RaycastHit {
    GameObject* object; ///< The GameObject that was hit, or nullptr if nothing was hit.
    glm::vec3 point;  ///< World-space position of the intersection point.
    glm::vec3 normal; ///< World-space surface normal at the intersection point.
    float distance; ///< Distance from the ray origin to the hit point in world units.
    float friction; ///< Surface friction of the hit body at the time of the query.
    float restitution; ///< Bounciness of the hit body at the time of the query.


    /**
    * @brief Returns true if the ray hit something.
    *
    * Equivalent to checking object != nullptr. Use this before accessing
    * point, normal, distance, or material properties.
    */
    bool hasHit() const { return object != nullptr; }

    /**
    * @brief Default constructor — initialises all fields to zero / nullptr.
    *
    * Produces a "no hit" result that passes the hasHit() == false check.
    */
    RaycastHit()
        : object(nullptr), point(0), normal(0), distance(0),
        friction(0), restitution(0) {
    }
};

/**
 * @brief Wraps Bullet's rayTest() API to provide engine-friendly raycast queries.
 *
 * Constructed once by Physics::initialize() and held as a unique_ptr.
 * Scripts obtain a reference via Physics::getQuerySystem().
 *
 * All methods return false / empty results (rather than crashing) when called
 * before the dynamics world has been initialised.
 */
class PhysicsQuery {
private:
    btDiscreteDynamicsWorld* dynamicsWorld; ///< Non-owning pointer to the Bullet world.

    /**
    * @brief Retrieves the GameObject stored in a Bullet collision object's user pointer.
    *
    * Every rigid body created by Physics::createRigidBody() has its owning
    * GameObject stored as the user pointer. Returns nullptr for ghost objects
    * or bodies that have not had setUserPointer() called.
    *
    * @param obj  Bullet collision object to query.
    * @return     Owning GameObject, or nullptr if not set.
    */
    GameObject* getGameObject(const btCollisionObject* obj) const;

    /**
    * @brief Populates a RaycastHit from a completed Bullet closest-ray result.
    *
    * Extracts hit point, surface normal, distance from ray origin, and
    * material properties. Resets hitInfo to defaults if the result has no hit.
    *
    * @param result    Completed ClosestRayResultCallback from Bullet's rayTest().
    * @param rayStart  World-space ray origin (used to compute distance).
    * @param hitInfo   Output struct to populate.
    */
    void fillHitInfo(
        const btCollisionWorld::RayResultCallback& result,
        const glm::vec3& rayStart,
        RaycastHit& hitInfo
    ) const;

public:

    /**
    * @brief Constructs a PhysicsQuery bound to the given dynamics world.
    *
    * @param world  Active Bullet dynamics world. A warning is logged if null,
    *               but construction succeeds — all queries return empty results.
    */
    explicit PhysicsQuery(btDiscreteDynamicsWorld* world);

    // Basic raycast

    /**
     * @brief Casts a ray and returns the closest hit against all collision layers.
     *
     * Convenience overload using btBroadphaseProxy::AllFilter as the mask.
     *
     * @param from     World-space ray origin.
     * @param to       World-space ray end point.
     * @param hitInfo  Output struct filled on success.
     * @return         True if the ray hit something.
     */
    bool raycast(const glm::vec3& from, const glm::vec3& to, RaycastHit& hitInfo) const;

    /**
    * @brief Casts a ray against a filtered set of collision layers and returns
    *        the closest hit.
    *
    * @param from           World-space ray origin.
    * @param to             World-space ray end point.
    * @param hitInfo        Output struct filled on success.
    * @param collisionMask  Bullet broadphase filter mask. Use AllFilter to test
    *                       all objects, or a bitmask for selective filtering.
    * @return               True if the ray hit something within the mask.
    */
    bool raycast(const glm::vec3& from, const glm::vec3& to, RaycastHit& hitInfo, short collisionMask) const;

    // Get all hits along ray
    /**
     * @brief Casts a ray and returns all hits along its length, sorted nearest-first.
     *
     * Uses Bullet's AllHitsRayResultCallback to collect every intersection.
     * Useful for piercing projectiles or occlusion queries that need every surface.
     *
     * @param from  World-space ray origin.
     * @param to    World-space ray end point.
     * @return      Vector of RaycastHit structs sorted by ascending distance.
     *              Empty if nothing was hit or the world is uninitialised.
     */
    std::vector<RaycastHit> raycastAll(const glm::vec3& from, const glm::vec3& to) const;

    // Convenience methods
     /**
     * @brief Returns true if a surface is detected within maxDistance below position.
     *
     * Fires a downward ray and uses a 90% threshold to provide a small margin
     * for floating-point error and slight surface penetration, preventing false
     * negatives when an object is just barely touching the ground.
     *
     * @param position     World-space origin of the ground check.
     * @param maxDistance  Maximum downward search distance. Defaults to 0.2.
     * @return             True if a surface was found within 90% of maxDistance.
     */
    bool isGrounded(const glm::vec3& position, float maxDistance = 0.2f) const;
    /**
     * @brief Returns true if a ray from `from` to `to` reaches the target point
     *        without being blocked.
     *
     * If the first hit point is within 1 cm of `to`, the path is clear. If
     * nothing is hit the path is also clear. Any other hit means obstruction.
     *
     * @param from  World-space observer position.
     * @param to    World-space target point.
     * @return      True if the path is unobstructed.
     */
    bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to) const;

    /**
     * @brief Returns true if the closest ray hit is exactly the specified GameObject.
     *
     * Fires a raycast toward target->getPosition() + targetOffset and checks
     * whether the first hit object matches `target`. Any other object occluding
     * the path returns false.
     *
     * @param from          World-space observer position (e.g. enemy eye position).
     * @param target        The GameObject that must be the first hit.
     * @param targetOffset  Offset applied to target's position (e.g. to aim at
     *                      the centre-of-mass rather than the origin). Defaults to (0,0,0).
     * @return              True if the closest hit along the ray is `target`.
     */
    bool canSeeObject(const glm::vec3& from, GameObject* target, const glm::vec3& targetOffset = glm::vec3(0)) const;
};

#endif // PHYSICSQUERY_H