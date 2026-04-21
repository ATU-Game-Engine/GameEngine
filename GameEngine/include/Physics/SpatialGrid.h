#ifndef SPATIALGRID_H
#define SPATIALGRID_H

#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

class GameObject;

/**
 * @brief Hash function for grid cell coordinates.
 * Enables use of glm::ivec3 as unordered_map keys.
 */
struct GridCellHash {
    std::size_t operator()(const glm::ivec3& cell) const {
        // FNV-1a hash for good spatial distribution
        std::size_t h = 2166136261u;
        h = (h ^ static_cast<std::size_t>(cell.x)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(cell.y)) * 16777619u;
        h = (h ^ static_cast<std::size_t>(cell.z)) * 16777619u;
        return h;
    }
};

/**
 * @brief Spatial partitioning grid for fast proximity queries.
 *
 * Divides world into uniform cells. Objects are inserted into cells based on position.
 * This lets us query "what's near me?" without checking every object in the scene.
 *
 * Performance: O(k) queries where k = objects in nearby cells (vs O(n) for all objects)
 *
 * Use cases:
 * - Find objects in radius (explosions, AI detection)
 * - Find nearest object (pathfinding, targeting)
 * - Custom collision filtering
 */
class SpatialGrid {
public:
    /**
     * @param cellSize Size of each grid cell in world units
     *                 Rule of thumb: Set to your average query radius
     *                 Example: 10.0f works well for most games
     */
    explicit SpatialGrid(float cellSize = 10.0f);

    // Core Operations 

    void insertObject(GameObject* obj);   // Call when spawning
    void removeObject(GameObject* obj);   // Call when destroying
    void updateObject(GameObject* obj);   // Call every frame for moving objects
    void clear();                         // Call when resetting scene

    // Queries  for gameplay 

    /**
     * Find all objects within radius of a point.
     * @param filter Optional lambda to filter results, e.g., [](GameObject* o) { return o->hasTag("Enemy"); }
     */
    std::vector<GameObject*> queryRadius(
        const glm::vec3& center,
        float radius,
        std::function<bool(GameObject*)> filter = nullptr
    ) const;

    /**
     * Find the single nearest object to a point.
     */
    GameObject* queryNearest(
        const glm::vec3& position,
        float maxRadius,
        std::function<bool(GameObject*)> filter = nullptr
    ) const;

    // === Debug Info ===

    int getObjectCount() const { return objectToCells.size(); }
    int getActiveCellCount() const { return cells.size(); }
    void printStats() const;

private:
    float cellSize;

    // Grid storage: cell coordinate -> objects in that cell
    std::unordered_map<glm::ivec3, std::unordered_set<GameObject*>, GridCellHash> cells;

    // Reverse lookup: object -> cells it occupies (for fast updates/removal)
    std::unordered_map<GameObject*, std::unordered_set<glm::ivec3, GridCellHash>> objectToCells;

    std::unordered_map<GameObject*, glm::vec3> lastKnownPositions;
    // Helper methods
    glm::ivec3 worldToCell(const glm::vec3& worldPos) const;
    std::vector<glm::ivec3> getObjectCells(const glm::vec3& position, const glm::vec3& size) const;
    std::vector<glm::ivec3> getCellsInAABB(const glm::vec3& min, const glm::vec3& max) const;
};

#endif // SPATIALGRID_H