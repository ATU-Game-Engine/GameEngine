/**
 * @file ConstraintRegistry.h
 * @brief Declares the ConstraintRegistry singleton — the central authority
 *        for all active Bullet Physics constraints in the scene.
 *
 * The registry owns every Constraint via unique_ptr and keeps them
 * synchronised with the Bullet dynamics world. Two acceleration indices
 * (name → Constraint* and GameObject* → [Constraint*]) enable O(1) lookup
 * without linear scans of the main vector.
 *
 * Key responsibilities:
 *   - addConstraint()               Registers a new constraint with Bullet and both indices.
 *   - removeConstraint()            Unregisters from Bullet, cleans up indices, frees memory.
 *   - removeConstraintsForObject()  Bulk-removes all joints touching a specific body.
 *   - rebuildConstraintsForObject() Reconstructs joints after a rigid body replacement.
 *   - detachConstraintsFromWorld()  Temporarily removes joints from Bullet without losing data.
 *   - update()                      Detects and removes broken constraints each tick.
 *
 * Lifetime: global singleton. Call initialize() once after the Bullet world
 * is created (done by Physics::initialize()). Call clearAll() before
 * destroying the world (done by Scene::clear() and Physics::cleanup()).
 */
#ifndef CONSTRAINTREGISTRY_H
#define CONSTRAINTREGISTRY_H

#include "Constraint.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

class GameObject;
class btDiscreteDynamicsWorld;

/**
 * @brief Singleton registry that owns all Constraint objects and manages
 *        their lifecycle in the Bullet dynamics world.
 *
 * Non-copyable. Obtain the instance via ConstraintRegistry::getInstance().
 */
class ConstraintRegistry {
private:
    static ConstraintRegistry* instance; ///< Singleton pointer, initialised on first getInstance() call.

    std::vector<std::unique_ptr<Constraint>> constraints; ///< Owning storage for all constraints.
    /// O(1) lookup by constraint name. Only populated for named constraints.
    std::unordered_map<std::string, Constraint*> nameIndex;
    /// O(1) lookup by GameObject — maps each body to the list of constraints it participates in.
    std::unordered_map<GameObject*, std::vector<Constraint*>> objectIndex;
    btDiscreteDynamicsWorld* dynamicsWorld; ///< Non-owning pointer set by initialize().
 
    /// Private constructor — use getInstance().
    ConstraintRegistry();

    /**
    * @brief Inserts a newly registered constraint into both acceleration indices.
    *
    * Adds to nameIndex only if the constraint has a non-empty name.
    * Adds to objectIndex for both bodyA and bodyB.
    *
    * @param constraint  The constraint to index. Must not be null.
    */
    void addToIndices(Constraint* constraint);

    /**
     * @brief Removes a constraint from both acceleration indices before erasure.
     *
     * Cleans up empty objectIndex entries to prevent unbounded map growth.
     *
     * @param constraint  The constraint about to be destroyed.
     */
    void removeFromIndices(Constraint* constraint);

public:
    /**
    * @brief Destructor — removes all constraints from Bullet and frees memory.
    */
    ~ConstraintRegistry();

    /// Non-copyable.
    ConstraintRegistry(const ConstraintRegistry&) = delete;
    ConstraintRegistry& operator=(const ConstraintRegistry&) = delete;

    /**
     * @brief Returns the global ConstraintRegistry instance, creating it on first call.
     *
     * @return Reference to the singleton.
     */
    static ConstraintRegistry& getInstance();


    /**
    * @brief Binds the registry to a Bullet dynamics world.
    *
    * Must be called once after the physics world is created before any
    * constraints are added.
    *
    * @param world  The active Bullet dynamics world. Must not be null.
    */
    void initialize(btDiscreteDynamicsWorld* world);

    // Add/Remove
    /**
     * @brief Takes ownership of a Constraint, adds it to Bullet, and indexes it.
     *
     * The Bullet constraint is added with disableCollisionsBetweenLinkedBodies
     * = true so the two bodies do not generate contact forces against each other.
     *
     * @param constraint  Heap-allocated Constraint to take ownership of.
     * @return            Raw pointer to the stored Constraint (valid until
     *                    removed or clearAll() is called), or nullptr on failure.
     */
    Constraint* addConstraint(std::unique_ptr<Constraint> constraint);

    /**
    * @brief Removes a constraint by raw pointer, unregistering it from Bullet
    *        and freeing its memory.
    *
    * Removes from both indices before erasing from the vector. No-op if null.
    *
    * @param constraint  Pointer previously returned by addConstraint().
    */
    void removeConstraint(Constraint* constraint);

    /**
     * @brief Removes a constraint by name. O(1) via name index.
     *
     * @param name  The constraint's name as set at creation or via setName().
     * @return      True if found and removed.
     */
    bool removeConstraint(const std::string& name);

    /**
    * @brief Removes all constraints from Bullet and clears the registry.
    *
    * Called by Scene::clear() and Physics::cleanup() to prevent dangling
    * rigid body pointers in the dynamics world across scene reloads.
    */
    void clearAll();

    /**
    * @brief Removes all constraints that involve a specific GameObject.
    *
    * Uses the objectIndex for O(k) lookup where k is the number of constraints
    * on that object. Called by Scene before destroying a GameObject.
    *
    * @param obj  The GameObject being removed. No-op if null or not indexed.
    */
    void removeConstraintsForObject(GameObject* obj);

	// rebuild all constraints involving this object (e.g. after resizing or replacing a rigid body)
     /**
     * @brief Rebuilds all constraints involving a specific GameObject after
     *        its rigid body has been replaced.
     *
     * Calls Constraint::rebuild() on each affected constraint to swap in the
     * new btRigidBody pointers, then re-adds the new Bullet constraints to the
     * dynamics world. Called by Physics::resizeRigidBody() after the body swap.
     *
     * @param obj  The GameObject whose body was just replaced.
     */
    void rebuildConstraintsForObject(GameObject* obj);


    // Queries

    /// Returns the total number of registered constraints.
    int getConstraintCount() const { return constraints.size(); }


    /**
    * @brief Finds a constraint by name. O(1) via name index.
    *
    * @param name  Name to search for.
    * @return      Raw pointer to the constraint, or nullptr if not found.
    */
    Constraint* findConstraintByName(const std::string& name) const;


    /**
    * @brief Returns all constraints that involve a specific GameObject. O(1).
    *
    * A constraint appears in the result if the object is either bodyA or bodyB.
    *
    * @param obj  The GameObject to query.
    * @return     Vector of raw constraint pointers. Empty if none found.
    */
    std::vector<Constraint*> findConstraintsByObject(GameObject* obj) const;

    /**
     * @brief Returns all constraints of a specific type. O(n) linear scan.
     *
     * Intended for editor queries, not hot gameplay paths.
     *
     * @param type  ConstraintType to filter by.
     * @return      Vector of matching raw constraint pointers.
     */
    std::vector<Constraint*> findConstraintsByType(ConstraintType type) const;

    /**
     * @brief Returns all constraints that have a breaking threshold configured.
     *
     * @return  Vector of raw pointers to breakable constraints.
     */
    std::vector<Constraint*> findBreakableConstraints() const;


    /**
     * @brief Returns raw pointers to every registered constraint.
     *
     * Intended for the serialiser (Scene::saveToFile) and debug rendering.
     * Pointers remain valid until the next structural modification.
     *
     * @return  All constraint pointers in registration order.
     */
    std::vector<Constraint*> getAllConstraints() const;


    /**
    * @brief Returns true if a constraint with the given name is registered.
    *
    * @param name  Name to check. O(1) via name index.
    */
    bool hasConstraint(const std::string& name) const;


    /**
   * @brief Removes all of an object's constraints from the Bullet world
   *        without removing them from the registry.
   *
   * Used before Physics::resizeRigidBody() so constraints aren't left
   * referencing an invalid rigid body pointer during the swap. Call
   * rebuildConstraintsForObject() afterward to re-attach them.
   *
   * @param obj  The GameObject whose constraints should be temporarily detached.
   */
    void detachConstraintsFromWorld(GameObject* obj);
    // Update

    /**
     * @brief Scans all breakable constraints and removes any that Bullet has
     *        disabled due to an impulse threshold being exceeded.
     *
     * Called once per physics tick by Physics::update(). After a break event
     * Bullet sets the constraint's enabled flag to false but does not remove
     * it — this method detects and cleans up that state.
     */
    void update();

    // Debug

    /**
     * @brief Prints a summary of all constraints broken down by type,
     *        breakability, and broken state.
     */
    void printStats() const;

    /**
     * @brief Prints the full details of every registered constraint via
     *        Constraint::printInfo().
     */
    void printAllConstraints() const;
};

#endif // CONSTRAINTREGISTRY_H