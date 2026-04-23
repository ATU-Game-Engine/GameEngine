#ifndef CONSTRAINTREGISTRY_H
#define CONSTRAINTREGISTRY_H

#include "Constraint.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

class GameObject;
class btDiscreteDynamicsWorld;

class ConstraintRegistry {
private:
    static ConstraintRegistry* instance;

    std::vector<std::unique_ptr<Constraint>> constraints;
    std::unordered_map<std::string, Constraint*> nameIndex;
    std::unordered_map<GameObject*, std::vector<Constraint*>> objectIndex;
    btDiscreteDynamicsWorld* dynamicsWorld;

    ConstraintRegistry();

    void addToIndices(Constraint* constraint);
    void removeFromIndices(Constraint* constraint);

public:
    ~ConstraintRegistry();

    ConstraintRegistry(const ConstraintRegistry&) = delete;
    ConstraintRegistry& operator=(const ConstraintRegistry&) = delete;

    static ConstraintRegistry& getInstance();
    void initialize(btDiscreteDynamicsWorld* world);

    // Add/Remove
    Constraint* addConstraint(std::unique_ptr<Constraint> constraint);
    void removeConstraint(Constraint* constraint);
    bool removeConstraint(const std::string& name);
    void clearAll();
    void removeConstraintsForObject(GameObject* obj);

	// rebuild all constraints involving this object (e.g. after resizing or replacing a rigid body)
    void rebuildConstraintsForObject(GameObject* obj);
    // Queries
    int getConstraintCount() const { return constraints.size(); }
    Constraint* findConstraintByName(const std::string& name) const;
    std::vector<Constraint*> findConstraintsByObject(GameObject* obj) const;
    std::vector<Constraint*> findConstraintsByType(ConstraintType type) const;
    std::vector<Constraint*> findBreakableConstraints() const;
    std::vector<Constraint*> getAllConstraints() const;
    bool hasConstraint(const std::string& name) const;

    void detachConstraintsFromWorld(GameObject* obj);
    // Update
    void update();

    // Debug
    void printStats() const;
    void printAllConstraints() const;
};

#endif // CONSTRAINTREGISTRY_H