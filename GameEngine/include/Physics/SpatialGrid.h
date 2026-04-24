/**
 * @file SpatialGrid.h
 * @brief Declares the SpatialGrid class — a uniform spatial partitioning
 *        structure for fast proximity queries in the scene.
 *
 * The world is divided into a 3D grid of equal-sized cubic cells. Each
 * GameObject is registered in every cell its axis-aligned bounding box
 * overlaps, enabling radius and nearest-object queries to examine only nearby
 * cells rather than every object in the scene.
 *
 * Complexity summary:
 *   insertObject / removeObject : O(k)       k = cells the AABB overlaps (usually 1–8).
 *   updateObject                : O(k)       with early-out when the object has not
 *                                            moved more than 10% of cellSize.
 *   queryRadius                 : O(c + n)   c = cells in the query AABB,
 *                                            n = candidate objects in those cells.
 *   queryNearest                : O(queryRadius) then a linear min-distance pass.
 *
 * Two parallel data structures are maintained:
 *   cells         — cell coordinate → set of GameObjects in that cell.
 *   objectToCells — GameObject* → set of cells it currently occupies.
 * The reverse map makes removal and update O(k) without scanning the grid.
 *
 * Rule of thumb for cellSize: match it to your average query radius.
 * Too small → many empty cells, high memory overhead.
 * Too large → more candidates per query, slower exact-distance filtering.
 */
#ifndef SPATIALGRID_H
#define SPATIALGRID_H

#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

class GameObject;

/**
 * @brief FNV-1a hash function for glm::ivec3 grid cell coordinates.
 *
 * Enables glm::ivec3 to be used as an unordered_map / unordered_set key.
 * FNV-1a provides good spatial distribution — nearby integer coordinates
 * hash to well-separated buckets, avoiding clustering in the hash table.
 */
struct GridCellHash {

    /**
   * @brief Computes an FNV-1a hash of the three integer cell components.
   *
   * @param cell  The grid cell coordinate to hash.
   * @return      Hash value suitable for use as an unordered container key.
   */
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
 * @brief Uniform 3D spatial grid for O(k) proximity queries.
 *
 * Divides world space into equal cubic cells. Objects are registered in every
 * cell their bounding box overlaps. Radius queries touch only the cells that
 * intersect the query sphere's AABB, making them much faster than a linear
 * scan of the entire scene for large object counts.
 *
 * Typical use in Scene:
 *   spatialGrid->insertObject(obj);       // on spawn
 *   spatialGrid->updateObject(obj);       // every frame after physics update
 *   spatialGrid->removeObject(obj);       // before destruction
 *   spatialGrid->queryRadius(pos, r, f);  // AI / explosion detection
 */
class SpatialGrid {
public:
    /**
    * @brief Constructs a SpatialGrid with the given cell size.
    *
    * @param cellSize  Side length of each cubic grid cell in world units.
    *                  Must be positive. Values ≤ 0 are clamped to 10.0 with
    *                  a warning. Set to your average query radius for best
    *                  performance — 10.0 works well for most game scenes.
    */
    explicit SpatialGrid(float cellSize = 10.0f);

    // Core Operations 
    /**
     * @brief Inserts a GameObject into all grid cells its bounding box overlaps.
     *
     * No-op if the object is already present. Records the object's position
     * so updateObject() can detect meaningful movement using an early-out check.
     *
     * Call this once immediately after spawning an object.
     *
     * @param obj  The GameObject to insert. No-op if null.
     */
    void insertObject(GameObject* obj);   // Call when spawning

    /**
    * @brief Removes a GameObject from all cells it currently occupies.
    *
    * Uses the objectToCells reverse map for O(k) removal without scanning
    * the grid. Empty cells are freed to prevent unbounded map growth.
    *
    * Call this before destroying a GameObject.
    *
    * @param obj  The GameObject to remove. No-op if null or not in the grid.
    */
    void removeObject(GameObject* obj);   // Call when destroying

    /**
     * @brief Updates a moving GameObject's grid registration.
     *
     * Movement smaller than 10% of cellSize is ignored to avoid redundant
     * cell recalculations for objects that have barely moved. When a
     * significant move is detected, cells that are no longer overlapped are
     * released and new overlapping cells are registered.
     *
     * Call this every frame after the physics simulation has stepped.
     *
     * @param obj  The GameObject to update. Calls insertObject() if not yet
     *             in the grid. No-op if null.
     */
    void updateObject(GameObject* obj);   // Call every frame for moving objects

    /**
     * @brief Removes all objects from the grid and resets all internal state.
     *
     * Called by Scene::clear() before a scene reload. More efficient than
     * calling removeObject() individually for every object.
     */
    void clear();                         // Call when resetting scene

    // Queries  for gameplay 

    /**
     * @brief Returns all GameObjects within a spherical radius of a point.
     *
     * First collects candidates from all cells in the sphere's AABB, then
     * performs an exact squared-distance test to exclude objects in the AABB
     * corners that fall outside the sphere. An optional filter lambda can
     * further restrict results (e.g. tag-based filtering).
     *
     * @param center  World-space centre of the query sphere.
     * @param radius  Radius of the sphere in world units.
     * @param filter  Optional predicate. Objects for which filter returns false
     *                are excluded. Pass nullptr to accept all objects.
     * @return        All GameObjects within the sphere that pass the filter.
     *
     * @example
     * // Find all enemies within 15 units
     * auto enemies = grid.queryRadius(pos, 15.0f,
     *     [](GameObject* o) { return o->hasTag("enemy"); });
     */
    std::vector<GameObject*> queryRadius(
        const glm::vec3& center,
        float radius,
        std::function<bool(GameObject*)> filter = nullptr
    ) const;

    /**
    * @brief Returns the nearest GameObject within a maximum radius.
    *
    * Delegates to queryRadius() for candidate collection, then performs a
    * linear min-distance pass. Uses squared distances to avoid sqrt calls.
    *
    * @param position   World-space point to measure from.
    * @param maxRadius  Maximum search radius. Objects beyond this are ignored.
    * @param filter     Optional predicate passed through to queryRadius().
    * @return           Pointer to the nearest qualifying object, or nullptr
    *                   if nothing was found within maxRadius.
    *
    * @example
    * // Find the nearest player within 20 units
    * GameObject* player = grid.queryNearest(enemyPos, 20.0f,
    *     [](GameObject* o) { return o->hasTag("player"); });
    */
    GameObject* queryNearest(
        const glm::vec3& position,
        float maxRadius,
        std::function<bool(GameObject*)> filter = nullptr
    ) const;

    // Debug Info 
      /// Returns the total number of objects currently tracked by the grid.
    int getObjectCount() const { return objectToCells.size(); }
    /// Returns the number of non-empty cells currently in the grid.
    int getActiveCellCount() const { return cells.size(); }

    /**
    * @brief Prints grid statistics to stdout.
    *
    * Reports cell size, tracked object count, active cell count, average
    * objects per cell, and maximum objects in any single cell. Use this to
    * diagnose whether cellSize is well-tuned for the current scene.
    */
    void printStats() const;

private:
    float cellSize; ///< Side length of each cubic grid cell in world units.

    /// Primary grid storage: cell coordinate → set of objects occupying that cell.
    std::unordered_map<glm::ivec3, std::unordered_set<GameObject*>, GridCellHash> cells;

    /// Reverse lookup: object → set of cells it currently occupies.
    /// Enables O(k) removal and update without scanning the full cell map.
    std::unordered_map<GameObject*, std::unordered_set<glm::ivec3, GridCellHash>> objectToCells;

    /// Caches the last recorded world position of each object.
    /// Used by updateObject() to skip grid recalculation for small movements.
    std::unordered_map<GameObject*, glm::vec3> lastKnownPositions;
    // Helper methods
    /**
     * @brief Converts a world-space position to an integer grid cell coordinate.
     *
     * Uses floor division so negative coordinates map to the correct cell
     * (e.g. position -0.1 with cellSize 10 maps to cell -1, not 0).
     *
     * @param worldPos  World-space position to convert.
     * @return          Integer grid cell coordinate.
     */
    glm::ivec3 worldToCell(const glm::vec3& worldPos) const;

    /**
    * @brief Returns all grid cells overlapped by an object's AABB.
    *
    * Expands the position by half the size in each direction to form the
    * bounding box, then delegates to getCellsInAABB().
    *
    * @param position  World-space centre of the object.
    * @param size      Full extents of the object's bounding box.
    * @return          All cell coordinates the AABB covers.
    */
    std::vector<glm::ivec3> getObjectCells(const glm::vec3& position, const glm::vec3& size) const;

    /**
     * @brief Returns all grid cells within an axis-aligned bounding box.
     *
     * Iterates the integer cell range from worldToCell(min) to worldToCell(max)
     * on each axis and returns every cell coordinate in that volume.
     *
     * @param min  World-space minimum corner of the AABB.
     * @param max  World-space maximum corner of the AABB.
     * @return     All cell coordinates whose cells intersect the AABB.
     */
    std::vector<glm::ivec3> getCellsInAABB(const glm::vec3& min, const glm::vec3& max) const;
};

#endif // SPATIALGRID_H