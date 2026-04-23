#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <algorithm>

// Initialize static instance
ConstraintRegistry* ConstraintRegistry::instance = nullptr;

ConstraintRegistry::ConstraintRegistry() : dynamicsWorld(nullptr) {
}

ConstraintRegistry::~ConstraintRegistry() {
    clearAll();
}

ConstraintRegistry& ConstraintRegistry::getInstance() {
    if (!instance) {
        instance = new ConstraintRegistry();
    }
    return *instance;
}

void ConstraintRegistry::initialize(btDiscreteDynamicsWorld* world) {
    dynamicsWorld = world;
    std::cout << "ConstraintRegistry initialized" << std::endl;
}

// Adding Constraints

Constraint* ConstraintRegistry::addConstraint(std::unique_ptr<Constraint> constraint) {
    if (!constraint) {
        std::cerr << "Error: Cannot add null constraint" << std::endl;
        return nullptr;
    }

    if (!dynamicsWorld) {
        std::cerr << "Error: ConstraintRegistry not initialized with physics world" << std::endl;
        return nullptr;
    }

    // Add to Bullet's dynamics world
    btTypedConstraint* bulletConstraint = constraint->getBulletConstraint();
    if (bulletConstraint) {
        // true = disable collision between linked bodies
        dynamicsWorld->addConstraint(bulletConstraint, true);
    }

    // Store the constraint and get raw pointer
    Constraint* rawPtr = constraint.get();
    constraints.push_back(std::move(constraint));

    // Update indices
    addToIndices(rawPtr);

    std::cout << "Added constraint (total: " << constraints.size() << ")" << std::endl;

    return rawPtr;
}
// Removing
void ConstraintRegistry::removeConstraint(Constraint* constraint) {
    if (!constraint) return;

    // Find and remove from vector
    auto it = std::find_if(constraints.begin(), constraints.end(),
        [constraint](const std::unique_ptr<Constraint>& c) {
            return c.get() == constraint;
        });

    if (it != constraints.end()) {
        // Remove from indices first
        removeFromIndices(constraint);

        // Remove from Bullet world
        if (dynamicsWorld && (*it)->getBulletConstraint()) {
            dynamicsWorld->removeConstraint((*it)->getBulletConstraint());
        }

        // Remove from vector (destroys the constraint)
        constraints.erase(it);

        std::cout << "Removed constraint (remaining: " << constraints.size() << ")" << std::endl;
    }
}

bool ConstraintRegistry::removeConstraint(const std::string& name) {
    Constraint* constraint = findConstraintByName(name);
    if (constraint) {
        removeConstraint(constraint);
        return true;
    }
    return false;
}

void ConstraintRegistry::clearAll() {
    if (!dynamicsWorld) return;

    std::cout << "Removing all " << constraints.size() << " constraints..." << std::endl;

    // Remove all from Bullet world
    for (auto& constraint : constraints) {
        if (constraint && constraint->getBulletConstraint()) {
            dynamicsWorld->removeConstraint(constraint->getBulletConstraint());
        }
    }

    // Clear all storage
    constraints.clear();
    nameIndex.clear();
    objectIndex.clear();

    std::cout << "All constraints cleared" << std::endl;
}

void ConstraintRegistry::removeConstraintsForObject(GameObject* obj) {
    if (!obj) return;

    auto it = objectIndex.find(obj);
    if (it == objectIndex.end()) return;

    // Get copy of constraint pointers
    std::vector<Constraint*> toRemove = it->second;

    // Remove each constraint
    for (Constraint* constraint : toRemove) {
        removeConstraint(constraint);
    }

    std::cout << "Removed " << toRemove.size() << " constraints for object" << std::endl;
}

//  Queries

Constraint* ConstraintRegistry::findConstraintByName(const std::string& name) const {
    auto it = nameIndex.find(name);
    if (it != nameIndex.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<Constraint*> ConstraintRegistry::findConstraintsByObject(GameObject* obj) const {
    auto it = objectIndex.find(obj);
    if (it != objectIndex.end()) {
        return it->second;
    }
    return std::vector<Constraint*>();
}

std::vector<Constraint*> ConstraintRegistry::findConstraintsByType(ConstraintType type) const {
    std::vector<Constraint*> result;

    for (const auto& constraint : constraints) {
        if (constraint->getType() == type) {
            result.push_back(constraint.get());
        }
    }

    return result;
}

std::vector<Constraint*> ConstraintRegistry::findBreakableConstraints() const {
    std::vector<Constraint*> result;

    for (const auto& constraint : constraints) {
        if (constraint->isBreakable()) {
            result.push_back(constraint.get());
        }
    }

    return result;
}

std::vector<Constraint*> ConstraintRegistry::getAllConstraints() const {
    std::vector<Constraint*> result;
    result.reserve(constraints.size());

    for (const auto& constraint : constraints) {
        result.push_back(constraint.get());
    }

    return result;
}

bool ConstraintRegistry::hasConstraint(const std::string& name) const {
    return nameIndex.find(name) != nameIndex.end();
}

void ConstraintRegistry::rebuildConstraintsForObject(GameObject* obj) {
    if (!obj || !dynamicsWorld) return;

    auto it = objectIndex.find(obj);
    if (it == objectIndex.end()) return;

    // Use a copy of the pointers to avoid iterator invalidation issues
    std::vector<Constraint*> affected = it->second;

    for (Constraint* c : affected) {
        
        // Re-trigger the internal reconstruction
		// This will update the cached frames and create a new Bullet constraint
        c->rebuild();

        // Re-add to the world
        if (c->getBulletConstraint()) {
            dynamicsWorld->addConstraint(c->getBulletConstraint(), true);
        }
    }
}

// detach constraints from world without removing them from registry (e.g. when an object is being removed but we want to keep the constraint data for potential reuse)
void ConstraintRegistry::detachConstraintsFromWorld(GameObject* obj) {
    if (!obj || !dynamicsWorld) return;

    auto it = objectIndex.find(obj);
    if (it == objectIndex.end()) return;

    for (Constraint* c : it->second) {
        if (c->getBulletConstraint()) {
            dynamicsWorld->removeConstraint(c->getBulletConstraint());
        }
    }
}

// Update 

void ConstraintRegistry::update()
{
    for (auto it = constraints.begin(); it != constraints.end(); )
    {
        Constraint* c = it->get();

        if (c && c->isBreakable())
        {
            btTypedConstraint* bc = c->getBulletConstraint();

            if (bc && !bc->isEnabled())
            {
                std::cout << "Constraint broken: " << c->getName() << std::endl;
                removeConstraint(c);
                it = constraints.begin(); // safe restart after structural change
                continue;
            }
        }
        ++it;
    }
}

// ========== Debug ==========

void ConstraintRegistry::printStats() const {
    std::cout << "\n=== Constraint Registry Stats ===" << std::endl;
    std::cout << "Total constraints: " << constraints.size() << std::endl;

    // Count by type
    int fixed = 0, hinge = 0, slider = 0, spring = 0, coneTwist = 0, dof6 = 0;
    int breakable = 0, broken = 0;

    for (const auto& constraint : constraints) {
        switch (constraint->getType()) {
        case ConstraintType::FIXED: fixed++; break;
        case ConstraintType::HINGE: hinge++; break;
        case ConstraintType::SLIDER: slider++; break;
        case ConstraintType::SPRING: spring++; break;
        case ConstraintType::GENERIC_6DOF: dof6++; break;
        }

        if (constraint->isBreakable()) breakable++;
        if (constraint->isBroken()) broken++;
    }

    std::cout << "\nBy type:" << std::endl;
    std::cout << "  Fixed: " << fixed << std::endl;
    std::cout << "  Hinge: " << hinge << std::endl;
    std::cout << "  Slider: " << slider << std::endl;
    std::cout << "  Spring: " << spring << std::endl;
    std::cout << "  Cone-Twist: " << coneTwist << std::endl;
    std::cout << "  Generic 6DOF: " << dof6 << std::endl;

    std::cout << "\nBreakable: " << breakable << std::endl;
    std::cout << "Broken: " << broken << std::endl;
    std::cout << "Named constraints: " << nameIndex.size() << std::endl;
    std::cout << "Objects with constraints: " << objectIndex.size() << std::endl;
    std::cout << "=================================\n" << std::endl;
}

void ConstraintRegistry::printAllConstraints() const {
    std::cout << "\n=== All Constraints ===" << std::endl;

    int index = 0;
    for (const auto& constraint : constraints) {
        std::cout << "\n[" << index++ << "] ";
        constraint->printInfo();
    }

    std::cout << "=====================\n" << std::endl;
}




// ========== Private Helper Methods ==========

void ConstraintRegistry::addToIndices(Constraint* constraint) {
    if (!constraint) return;

    // Add to name index if named
    if (!constraint->getName().empty()) {
        nameIndex[constraint->getName()] = constraint;
    }

    // Add to object index
    GameObject* bodyA = constraint->getBodyA();
    GameObject* bodyB = constraint->getBodyB();

    if (bodyA) {
        objectIndex[bodyA].push_back(constraint);
    }

    if (bodyB) {
        objectIndex[bodyB].push_back(constraint);
    }
}

void ConstraintRegistry::removeFromIndices(Constraint* constraint) {
    if (!constraint) return;

    // Remove from name index
    if (!constraint->getName().empty()) {
        nameIndex.erase(constraint->getName());
    }

    // Remove from object index
    GameObject* bodyA = constraint->getBodyA();
    GameObject* bodyB = constraint->getBodyB();

    auto removeFromObjectIndex = [this, constraint](GameObject* obj) {
        if (!obj) return;

        auto it = objectIndex.find(obj);
        if (it != objectIndex.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), constraint), vec.end());

            // Remove object entry if no more constraints
            if (vec.empty()) {
                objectIndex.erase(it);
            }
        }
        };

    removeFromObjectIndex(bodyA);
    removeFromObjectIndex(bodyB);
}