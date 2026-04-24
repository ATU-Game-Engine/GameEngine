#include "../include/UI/Raycast.h"

#include <algorithm> // for std::swap
#include <cfloat>    // for FLT_MAX
#include <cmath>     // for std::abs

// Tests whether a ray intersects an Axis-Aligned Bounding Box (AABB)
//
// rayOrigin   : world-space start of the ray (camera position)
// rayDir      : normalized world-space ray direction
// aabbMin     : minimum corner of the AABB (world-space)
// aabbMax     : maximum corner of the AABB (world-space)
// outDistance : distance from ray origin to first intersection point
//
// Returns true if the ray hits the box, false otherwise.
//
// This uses the slab method, which checks intersection
// against X, Y, and Z axis-aligned planes.

/**
 * @brief Tests whether a ray intersects an Axis-Aligned Bounding Box (AABB).
 *
 * Uses the slab method — intersects the ray against the X, Y, and Z axis-aligned
 * plane pairs and checks whether a valid overlap exists across all three.
 *
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
)
{
    // tMin / tMax represent the range along the ray
    // where the intersection is valid
    float tMin = 0.0f;
    float tMax = FLT_MAX;

    // Loop over X, Y, Z axes
    for (int axis = 0; axis < 3; axis++)
    {
        // If the ray is almost parallel to this axis
        if (std::abs(rayDir[axis]) < 0.0001f)
        {
            // If the ray origin is outside the slab,
            // it can never intersect
            if (rayOrigin[axis] < aabbMin[axis] ||
                rayOrigin[axis] > aabbMax[axis])
            {
                return false;
            }
        }
        else
        {
            // Compute intersection distances with the two planes
            float invDir = 1.0f / rayDir[axis];

            float t0 = (aabbMin[axis] - rayOrigin[axis]) * invDir;
            float t1 = (aabbMax[axis] - rayOrigin[axis]) * invDir;

            // Ensure t0 is the near plane and t1 is the far plane
            if (t0 > t1)
                std::swap(t0, t1);

            // Narrow the valid intersection range
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;

            // If the range becomes invalid, no intersection
            if (tMax < tMin)
                return false;
        }
    }

    // The ray hits the box at distance tMin
    outDistance = tMin;
    return true;
}
