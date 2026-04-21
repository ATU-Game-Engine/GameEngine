#include "../include/Physics/SpatialGrid.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

SpatialGrid::SpatialGrid(float cellSize) : cellSize(cellSize) {
    if (cellSize <= 0.0f) {
        std::cerr << "Warning: Invalid cell size, using default 10.0f" << std::endl;
        this->cellSize = 10.0f;
    }
}

// Convert world position to grid cell coordinate
glm::ivec3 SpatialGrid::worldToCell(const glm::vec3& worldPos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(worldPos.x / cellSize)),
        static_cast<int>(std::floor(worldPos.y / cellSize)),
        static_cast<int>(std::floor(worldPos.z / cellSize))
    );
}

// Get all cells an object overlaps (large objects span multiple cells)
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

void SpatialGrid::clear() {
    cells.clear();
    objectToCells.clear();
    lastKnownPositions.clear();
}

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