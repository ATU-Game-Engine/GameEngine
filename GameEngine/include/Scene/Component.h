#ifndef COMPONENT_H
#define COMPONENT_H

class GameObject;

/**
 * @brief Base class for all components.
 *
 * Components are modular pieces of functionality attached to GameObjects.
 * Each component owns a single responsibility (transform, physics, rendering,
 * etc.) and communicates with others through its shared owner.
 *
 * Derived classes should override update() if they require per-frame logic,
 * and may access the owning GameObject via the protected @p owner pointer,
 * which is guaranteed to be set before any engine call is made.
 *
 * Components are created and managed by their owning GameObject — do not
 * instantiate or delete them directly.
 */
class Component {
protected:
    /// @brief Non-owning pointer to the GameObject this component belongs to.
   ///        Set by setOwner() before any engine calls are made.
    GameObject* owner;  // Non-owning pointer to parent GameObject

public:
    /**
     * @brief Constructs a Component with no owner assigned.
     *
     * The owner is set by the GameObject immediately after construction
     * via setOwner(). Do not access @p owner inside a constructor of a
     * derived class — use a dedicated initialise method or override
     * the first update() call instead.
     */
    Component() : owner(nullptr) {}

    /**
     * @brief Virtual destructor to ensure correct cleanup of derived classes.
     *
     * Without a virtual destructor, deleting a Component through a base-class
     * pointer would only call the base destructor, leaking any resources
     * owned by the derived type.
     */
    virtual ~Component() = default;

    /**
     * @brief Sets the GameObject this component belongs to.
     *
     * Called by the owning GameObject immediately after the component is
     * created. Components should not call this directly.
     *
     * @param obj  Non-owning pointer to the parent GameObject.
     */
    void setOwner(GameObject* obj) { owner = obj; }

    /**
     * @brief Returns the GameObject this component is attached to.
     *
     * @return Non-owning pointer to the parent GameObject. Will be nullptr
     *         only before the component has been attached to an owner.
     */
    GameObject* getOwner() const { return owner; }

    /**
    * @brief Per-frame update hook. Override to add frame-by-frame logic.
    *
    * Called once per frame by the owning GameObject while the scene is
    * running. The base implementation is a no-op, so derived classes only
    * need to override this if they require per-frame processing.
    *
    * @param deltaTime  Elapsed time since the last frame, in seconds.
    */
    virtual void update(float deltaTime) {}
};

#endif // COMPONENT_H