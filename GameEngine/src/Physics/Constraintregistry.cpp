/**
 * @file ConstraintRegistry.cpp
 * @brief Singleton registry that owns all active Bullet Physics constraints
 *        and keeps them synchronised with the dynamics world.
 *
 * Responsibilities:
 *   - Adding and removing constraints from both the registry and the Bullet
 *     dynamics world in a single call.
 *   - Maintaining two acceleration indices (name -> Constraint* and
 *     GameObject* -> [Constraint*]) for fast lookup without linear scans.
 *   - Rebuilding constraints after a rigid body replacement
 *     (Physics::resizeRigidBody invalidates raw btRigidBody pointers).
 *   - Detecting and cleaning up broken constraints each update tick.
 *
 * Lifetime: the registry is a global singleton. Call initialize() once with
 * the dynamics world pointer before any constraints are added. Call clearAll()
 * (done automatically by Scene::clear()) before destroying the world.
 */
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <algorithm>

// Initialize static instance
ConstraintRegistry* ConstraintRegistry::instance = nullptr;

ConstraintRegistry::ConstraintRegistry() : dynamicsWorld(nullptr) {
}

/**
 * @brief Destructor — removes all constraints from the dynamics world and
 *        frees all owned Constraint objects.
 */
ConstraintRegistry::~ConstraintRegistry() {
    clearAll();
}

/**
 * @brief Returns the global ConstraintRegistry instance, creating it on first call.
 *
 * The instance is intentionally never destroyed via a deleter — it lives for
 * the duration of the application. Call clearAll() explicitly before
 * shutting down the physics world.
 *
 * @return Reference to the singleton instance.
 */
ConstraintRegistry& ConstraintRegistry::getInstance() {
    if (!instance) {
        instance = new ConstraintRegistry();
    }
    return *instance;
}

/**
 * @brief Binds the registry to a Bullet dynamics world.
 *
 * Must be called once after the physics world is created and before any
 * constraints are added. All subsequent addConstraint / removeConstraint
 * calls forward to this world pointer.
 *
 * @param world  The active Bullet dynamics world. Must not be null.
 */
void ConstraintRegistry::initialize(btDiscreteDynamicsWorld* world) {
    dynamicsWorld = world;
    std::cout << "ConstraintRegistry initialized" << std::endl;
}

// Adding Constraints
/**
 * @brief Takes ownership of a Constraint, registers it with Bullet, and
 *        indexes it for fast lookup.
 *
 * The constraint is added to the Bullet dynamics world with
 * disableCollisionsBetweenLinkedBodies = true, which prevents the two
 * connected bodies from generating contact forces against each other.
 *
 * @param constraint  Heap-allocated Constraint to take ownership of.
 *                    Must not be null.
 * @return            Raw pointer to the stored Constraint (valid until
 *                    removeConstraint or clearAll is called), or nullptr
 *                    if the registry is uninitialised or the input is null.
 */
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
/**
 * @brief Removes a constraint by raw pointer, unregistering it from Bullet
 *        and freeing the owned Constraint object.
 *
 * Removes from both acceleration indices before erasing from the vector so
 * the indices are never left pointing at freed memory.
 *
 * @param constraint  Pointer previously returned by addConstraint. No-op if null.
 */
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


/**
 * @brief Removes a constraint by name.
 *
 * Looks up the constraint in the name index, then delegates to the
 * pointer overload. O(1) name lookup.
 *
 * @param name  Name assigned to the constraint at creation time.
 * @return      True if a constraint with that name was found and removed.
 */
bool ConstraintRegistry::removeConstraint(const std::string& name) {
    Constraint* constraint = findConstraintByName(name);
    if (constraint) {
        removeConstraint(constraint);
        return true;
    }
    return false;
}

/**
 * @brief Removes all constraints from both Bullet and the registry.
 *
 * Called by Scene::clear() before destroying GameObjects to ensure no
 * Bullet constraint holds a dangling rigid body pointer. Safe to call
 * when the registry is empty.
 */
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

/**
 * @brief Removes all constraints that involve a specific GameObject.
 *
 * Used by Scene before destroying an object to prevent Bullet holding
 * references to its rigid body. Uses the object index for O(k) lookup
 * where k is the number of constraints on that object.
 *
 * @param obj  The GameObject being removed. No-op if null or not indexed.
 */
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
/**
 * @brief Finds a constraint by its name string. O(1) via name index.
 *
 * @param name  The constraint's name as set at creation or via setName().
 * @return      Pointer to the Constraint, or nullptr if not found.
 */
Constraint* ConstraintRegistry::findConstraintByName(const std::string& name) const {
    auto it = nameIndex.find(name);
    if (it != nameIndex.end()) {
        return it->second;
    }
    return nullptr;
}

/**
 * @brief Returns all constraints that involve a specific GameObject.
 *
 * A constraint appears in this list if the object is either bodyA or bodyB.
 * O(1) via object index.
 *
 * @param obj  The GameObject to query.
 * @return     Vector of raw constraint pointers. Empty if none found.
 */
std::vector<Constraint*> ConstraintRegistry::findConstraintsByObject(GameObject* obj) const {
    auto it = objectIndex.find(obj);
    if (it != objectIndex.end()) {
        return it->second;
    }
    return std::vector<Constraint*>();
}


/**
 * @brief Returns all constraints of a specific type (e.g. all hinges).
 *
 * O(n) linear scan — intended for editor queries and debug tools, not
 * hot gameplay paths.
 *
 * @param type  The constraint type to filter by.
 * @return      Vector of matching raw constraint pointers.
 */
std::vector<Constraint*> ConstraintRegistry::findConstraintsByType(ConstraintType type) const {
    std::vector<Constraint*> result;

    for (const auto& constraint : constraints) {
        if (constraint->getType() == type) {
            result.push_back(constraint.get());
        }
    }

    return result;
}

/**
 * @brief Returns all constraints that have a breaking threshold set.
 *
 * Useful for systems that need to poll or visualise which joints can snap.
 *
 * @return  Vector of raw pointers to breakable constraints.
 */
std::vector<Constraint*> ConstraintRegistry::findBreakableConstraints() const {
    std::vector<Constraint*> result;

    for (const auto& constraint : constraints) {
        if (constraint->isBreakable()) {
            result.push_back(constraint.get());
        }
    }

    return result;
}


/**
 * @brief Returns raw pointers to every registered constraint.
 *
 * Intended for the serialiser (Scene::saveToFile) and debug rendering.
 * Pointers remain valid until the next add or remove operation.
 *
 * @return  Vector of all constraint pointers in registration order.
 */
std::vector<Constraint*> ConstraintRegistry::getAllConstraints() const {
    std::vector<Constraint*> result;
    result.reserve(constraints.size());

    for (const auto& constraint : constraints) {
        result.push_back(constraint.get());
    }

    return result;
}


/**
 * @brief Returns true if a constraint with the given name is registered.
 *
 * @param name  Name to check. O(1) via name index.
 * @return      True if found.
 */
bool ConstraintRegistry::hasConstraint(const std::string& name) const {
    return nameIndex.find(name) != nameIndex.end();
}


/**
 * @brief Rebuilds all constraints involving a specific GameObject after its
 *        rigid body has been replaced.
 *
 * Physics::resizeRigidBody destroys the old btRigidBody and creates a new one.
 * Any Bullet constraint referencing the old pointer is now invalid. This method:
 *   1. Calls Constraint::rebuild() on each affected constraint, which deletes
 *      the old Bullet object and creates a new one using the stored frames and
 *      parameter structs.
 *   2. Re-adds the new Bullet constraint to the dynamics world.
 *
 * The constraints remain in the registry — only their internal Bullet pointers
 * are replaced.
 *
 * @param obj  The GameObject whose rigid body was just replaced.
 */
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
/**
 * @brief Removes all of an object's constraints from the dynamics world without
 *        removing them from the registry.
 *
 * Used when temporarily detaching an object from simulation (e.g. during a
 * resize) while preserving the constraint data for potential re-attachment.
 * Call rebuildConstraintsForObject() to re-add them after the body is replaced.
 *
 * @param obj  The GameObject whose constraints should be detached from Bullet.
 */
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
/**
 * @brief Scans all breakable constraints and removes any that Bullet has
 *        disabled due to an impulse threshold being exceeded.
 *
 * Should be called once per frame after the physics step. When a constraint
 * breaks, Bullet sets its enabled flag to false but does not remove it from
 * the world or free it. This method detects that state and cleans up.
 *
 * After a removal the loop restarts from the beginning to avoid iterator
 * invalidation caused by the structural change to the constraints vector.
 */
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
/**
 * @brief Prints a summary of registered constraints broken down by type,
 *        breakability, and broken state.
 */
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
/**
 * @brief Prints the full details of every registered constraint via
 *        Constraint::printInfo().
 */
void ConstraintRegistry::printAllConstraints() const {
    std::cout << "\n=== All Constraints ===" << std::endl;

    int index = 0;
    for (const auto& constraint : constraints) {
        std::cout << "\n[" << index++ << "] ";
        constraint->printInfo();
    }

    std::cout << "=====================\n" << std::endl;
}




//  Private Helper Methods 

/**
 * @brief Inserts a constraint into the name and object acceleration indices.
 *
 * Only adds to the name index if the constraint has a non-empty name. Adds
 * to the object index for both bodyA and bodyB (if non-null), allowing
 * O(1) lookup from either end of the joint.
 *
 * @param constraint  The newly registered constraint to index.
 */
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

/**
 * @brief Removes a constraint from both acceleration indices.
 *
 * Cleans up the object index entry entirely if it becomes empty after removal,
 * preventing unbounded growth of the map with stale GameObject keys.
 *
 * @param constraint  The constraint about to be erased from the vector.
 */
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