#pragma once

#include <glm/glm.hpp>

// Tests whether a ray intersects an Axis-Aligned Bounding Box (AABB)
//
// rayOrigin   : world-space start of the ray (camera position)
// rayDir      : normalized world-space ray direction
// aabbMin     : minimum corner of the AABB (world-space)
// aabbMax     : maximum corner of the AABB (world-space)
// outDistance : distance from ray origin to first intersection point
//
// Returns true if the ray hits the box, false otherwise.

/**
 * @brief Tests whether a ray intersects an AABB using the slab method.
 * @param rayOrigin   World-space ray origin.
 * @param rayDir      Normalised world-space ray direction.
 * @param aabbMin     Minimum corner of the AABB in world space.
 * @param aabbMax     Maximum corner of the AABB in world space.
 * @param outDistance Distance from the ray origin to the first intersection point.
 * @return true if the ray hits the box, false otherwise.
 */
bool RayIntersectsAABB(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    float& outDistance
);