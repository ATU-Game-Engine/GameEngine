/**
 * @file SpatialGrid.cpp
 * @brief Implementation of the uniform spatial partitioning grid used for
 *        fast proximity queries in the scene.
 *
 * The world is divided into a 3D grid of equal-sized cubic cells. Each
 * GameObject is inserted into every cell its axis-aligned bounding box
 * overlaps, allowing radius queries to examine only nearby cells rather
 * than every object in the scene.
 *
 * Complexity:
 *   Insert / Remove : O(k)  where k = cells the object overlaps (usually 1–8).
 *   Update          : O(k)  with an early-out if the object hasn't moved far
 *                           enough to change its cell occupancy.
 *   queryRadius     : O(c + n)  where c = cells in the query AABB and
 *                               n = candidate objects in those cells.
 *   queryNearest    : O(queryRadius) — delegates then picks the minimum.
 *
 * Two acceleration structures are maintained in parallel:
 *   cells          — cell coordinate → set of objects in that cell.
 *   objectToCells  — object pointer → set of cells it currently occupies.
 * The reverse map makes removal and update O(k) without scanning all cells.
 *
 * Rule of thumb for cellSize: set it to your average query radius. Too small
 * means many empty cells and high memory overhead; too large means more
 * candidates per query and slower exact-distance filtering.
 */
#include "../include/Physics/SpatialGrid.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

 /**
  * @brief Constructs a SpatialGrid with the given cell size.
  *
  * @param cellSize  Side length of each cubic grid cell in world units.
  *                  Must be positive. If zero or negative, a warning is logged
  *                  and the size defaults to 10.0.
  */
SpatialGrid::SpatialGrid(float cellSize) : cellSize(cellSize) {
    if (cellSize <= 0.0f) {
        std::cerr << "Warning: Invalid cell size, using default 10.0f" << std::endl;
        this->cellSize = 10.0f;
    }
}


// Convert world position to grid cell coordinate
/**
 * @brief Converts a world-space position to a grid cell coordinate.
 *
 * Uses floor division so negative coordinates map to the correct cell
 * (e.g. position -0.1 with cellSize 10 maps to cell -1, not 0).
 *
 * @param worldPos  World-space position to convert.
 * @return          Integer grid cell coordinate.
 */
glm::ivec3 SpatialGrid::worldToCell(const glm::vec3& worldPos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(worldPos.x / cellSize)),
        static_cast<int>(std::floor(worldPos.y / cellSize)),
        static_cast<int>(std::floor(worldPos.z / cellSize))
    );
}

// Get all cells an object overlaps (large objects span multiple cells)
/**
 * @brief Returns all grid cells overlapped by an object's axis-aligned
 *        bounding box.
 *
 * The bounding box is derived from the object's position (centre) and
 * size (full extents), giving a half-size expansion in each direction.
 * Large objects will occupy multiple cells; small objects typically occupy
 * only one.
 *
 * @param position  World-space centre of the object.
 * @param size      Full extents of the object's bounding box.
 * @return          List of cell coordinates the AABB covers.
 */
std::vector<glm::ivec3> SpatialGrid::getObjectCells(
    const glm::vec3& position,
    const glm::vec3& size) const
{
    glm::vec3 halfSize = size * 0.5f;
    glm::vec3 min = position - halfSize;
    glm::vec3 max = position + halfSize;
    return getCellsInAABB(min, max);
}

// Get all cells in a bounding box
/**
 * @brief Returns all grid cells that fall within an axis-aligned bounding box.
 *
 * Iterates the integer cell range from the cell containing `min` to the cell
 * containing `max` on each axis and collects every cell coordinate in that
 * volume.
 *
 * @param min  World-space minimum corner of the AABB.
 * @param max  World-space maximum corner of the AABB.
 * @return     All cell coordinates whose cell intersects the AABB.
 */
std::vector<glm::ivec3> SpatialGrid::getCellsInAABB(
    const glm::vec3& min,
    const glm::vec3& max) const
{
    glm::ivec3 minCell = worldToCell(min);
    glm::ivec3 maxCell = worldToCell(max);

    std::vector<glm::ivec3> cellList;
    for (int x = minCell.x; x <= maxCell.x; ++x) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int z = minCell.z; z <= maxCell.z; ++z) {
                cellList.push_back(glm::ivec3(x, y, z));
            }
        }
    }
    return cellList;
}

/**
 * @brief Inserts a GameObject into all grid cells its bounding box overlaps.
 *
 * No-op if the object is already present in the grid (guarded by checking
 * the objectToCells map). Records the object's position in lastKnownPositions
 * so updateObject() can detect meaningful movement later.
 *
 * Call this once when a GameObject is spawned.
 *
 * @param obj  The GameObject to insert. No-op if null.
 */
void SpatialGrid::insertObject(GameObject* obj) {
    if (!obj) return;
    if (objectToCells.find(obj) != objectToCells.end()) return; // Already inserted

    std::vector<glm::ivec3> overlappingCells = getObjectCells(obj->getPosition(), obj->getScale());

    for (const auto& cell : overlappingCells) {
        cells[cell].insert(obj);
        objectToCells[obj].insert(cell);
    }
    lastKnownPositions[obj] = obj->getPosition();
}


/**
 * @brief Removes a GameObject from all grid cells it currently occupies.
 *
 * Iterates the objectToCells reverse map to find the cells to clean up in
 * O(k) without scanning the whole grid. Empty cells are erased to prevent
 * unbounded map growth over time.
 *
 * Call this before destroying a GameObject.
 *
 * @param obj  The GameObject to remove. No-op if null or not in the grid.
 */
void SpatialGrid::removeObject(GameObject* obj) {
    if (!obj) return;

    auto it = objectToCells.find(obj);
    if (it == objectToCells.end()) return; // Not in grid

    // Remove from all cells
    for (const auto& cell : it->second) {
        auto cellIt = cells.find(cell);
        if (cellIt != cells.end()) {
            cellIt->second.erase(obj);
            if (cellIt->second.empty()) {
                cells.erase(cellIt); // Free empty cells
            }
        }
    }

    objectToCells.erase(it);
    lastKnownPositions.erase(obj);
}


/**
 * @brief Updates a GameObject's grid registration after it has moved.
 *
 * Called every frame for all moving objects. To avoid redundant work,
 * position changes smaller than 10% of cellSize are ignored — the object
 * has not moved far enough to change which cells it occupies.
 *
 * When a move is detected:
 *   1. Cells the object no longer overlaps are cleaned up.
 *   2. New cells are registered.
 *   3. The objectToCells entry is replaced with the new cell set.
 *
 * If the object is not yet in the grid, insertObject() is called instead.
 *
 * @param obj  The GameObject to update. No-op if null.
 */
void SpatialGrid::updateObject(GameObject* obj) {
    if (!obj) return;

    auto it = objectToCells.find(obj);
    if (it == objectToCells.end()) {
        insertObject(obj); // Not in grid yet
        return;
    }

	// Check if object moved significantly (e.g., more than 10% of cell size) to avoid unnecessary updates
    glm::vec3 currentPos = obj->getPosition();
    auto posIt = lastKnownPositions.find(obj);
    if (posIt != lastKnownPositions.end())
    {
        glm::vec3 delta = currentPos - posIt->second;
        float thresholdSq = (cellSize * 0.1f) * (cellSize * 0.1f);
        if (glm::dot(delta, delta) < thresholdSq)
            return;
    }

    lastKnownPositions[obj] = currentPos;

    // Calculate new cells
    std::vector<glm::ivec3> newCells = getObjectCells(obj->getPosition(), obj->getScale());
    std::unordered_set<glm::ivec3, GridCellHash> newCellSet(newCells.begin(), newCells.end());

    // Early exit if no change
    if (it->second == newCellSet) return;

    // Remove from old cells
    for (const auto& cell : it->second) {
        if (newCellSet.find(cell) == newCellSet.end()) {
            auto cellIt = cells.find(cell);
            if (cellIt != cells.end()) {
                cellIt->second.erase(obj);
                if (cellIt->second.empty()) cells.erase(cellIt);
            }
        }
    }

    // Add to new cells
    for (const auto& cell : newCellSet) {
        if (it->second.find(cell) == it->second.end()) {
            cells[cell].insert(obj);
        }
    }

    it->second = newCellSet;
}

/**
 * @brief Removes all objects from the grid and resets all internal state.
 *
 * Called by Scene::clear() before a scene reload. Frees all cell and
 * object map entries without requiring individual removeObject() calls.
 */
void SpatialGrid::clear() {
    cells.clear();
    objectToCells.clear();
    lastKnownPositions.clear();
}

/**
 * @brief Returns all GameObjects within a spherical radius of a point.
 *
 * First gathers candidate objects from all cells in the query AABB (the
 * bounding box of the sphere), then filters candidates by exact squared
 * distance to eliminate objects in the corners of the AABB that fall outside
 * the sphere. An optional filter lambda can further restrict results.
 *
 * Using a set for candidates automatically deduplicates objects that span
 * multiple cells and would otherwise appear more than once.
 *
 * @param center  World-space centre of the query sphere.
 * @param radius  Radius of the query sphere in world units.
 * @param filter  Optional predicate. Objects for which filter returns false
 *                are excluded. Pass nullptr to accept all objects.
 * @return        All GameObjects whose positions are within `radius` of
 *                `center` and pass the filter.
 */
std::vector<GameObject*> SpatialGrid::queryRadius(
    const glm::vec3& center,
    float radius,
    std::function<bool(GameObject*)> filter) const
{
    // Get bounding box for the sphere
    glm::vec3 radiusVec(radius, radius, radius);
    std::vector<glm::ivec3> queryCells = getCellsInAABB(center - radiusVec, center + radiusVec);

    // Collect candidates (use set to avoid duplicates)
    std::unordered_set<GameObject*> candidates;
    for (const auto& cell : queryCells) {
        auto it = cells.find(cell);
        if (it != cells.end()) {
            for (GameObject* obj : it->second) {
                candidates.insert(obj);
            }
        }
    }

    // Filter by actual distance
    std::vector<GameObject*> results;
    float radiusSquared = radius * radius;

    for (GameObject* obj : candidates) {
        if (filter && !filter(obj)) continue;

        glm::vec3 diff = obj->getPosition() - center;
        float distSquared = glm::dot(diff, diff);

        if (distSquared <= radiusSquared) {
            results.push_back(obj);
        }
    }

    return results;
}

/**
 * @brief Returns the nearest GameObject within a maximum radius.
 *
 * Delegates to queryRadius() to collect candidates, then performs a linear
 * scan to find the minimum squared distance. Uses squared distances throughout
 * to avoid unnecessary square root calls.
 *
 * @param position   World-space point to measure from.
 * @param maxRadius  Maximum search radius. Objects beyond this are ignored.
 * @param filter     Optional predicate passed through to queryRadius().
 * @return           Pointer to the nearest qualifying object, or nullptr if
 *                   nothing was found within maxRadius.
 */
GameObject* SpatialGrid::queryNearest(
    const glm::vec3& position,
    float maxRadius,
    std::function<bool(GameObject*)> filter) const
{
    std::vector<GameObject*> candidates = queryRadius(position, maxRadius, filter);
    if (candidates.empty()) return nullptr;

    GameObject* nearest = nullptr;
    float minDistSquared = std::numeric_limits<float>::max();

    for (GameObject* obj : candidates) {
        glm::vec3 diff = obj->getPosition() - position;
        float distSquared = glm::dot(diff, diff);

        if (distSquared < minDistSquared) {
            minDistSquared = distSquared;
            nearest = obj;
        }
    }

    return nearest;
}



/**
 * @brief Prints grid statistics to stdout.
 *
 * Reports cell size, total tracked objects, active cell count, average objects
 * per cell, and the maximum number of objects in any single cell. Useful for
 * tuning cellSize — a healthy grid has a low average and a low maximum.
 */
void SpatialGrid::printStats() const {
    std::cout << "=== Spatial Grid ===" << std::endl;
    std::cout << "Cell size: " << cellSize << " units" << std::endl;
    std::cout << "Objects: " << objectToCells.size() << std::endl;
    std::cout << "Active cells: " << cells.size() << std::endl;

    if (!cells.empty()) {
        size_t total = 0;
        size_t maxInCell = 0;
        for (const auto& pair : cells) {
            size_t count = pair.second.size();
            total += count;
            maxInCell = std::max(maxInCell, count);
        }
        std::cout << "Avg per cell: " << (float)total / cells.size() << std::endl;
        std::cout << "Max in one cell: " << maxInCell << std::endl;
    }
    std::cout << "====================" << std::endl;
}