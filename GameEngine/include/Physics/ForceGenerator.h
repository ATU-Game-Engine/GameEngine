/**
 * @file ForceGenerator.h
 * @brief Declares the ForceGenerator base class and all concrete subtypes
 *        used by ForceGeneratorRegistry to apply area-based forces each tick.
 *
 * Subtypes:
 *   WindGenerator        — constant directional force within a radius.
 *   GravityWellGenerator — inverse-square attraction or repulsion toward a point.
 *   VortexGenerator      — tangential spin combined with an inward pull.
 *   ExplosionGenerator   — single-frame radial impulse that auto-expires.
 *
 * ForceGeneratorRegistry calls apply() on every enabled generator for every
 * dynamic rigid body in the scene each physics tick. Each generator is
 * responsible for its own range check (isInRange) and force calculation.
 *
 * Generators are owned by ForceGeneratorRegistry via unique_ptr and identified
 * at runtime by the ForceGeneratorType enum.
 */
#ifndef FORCE_GENERATOR_H
#define FORCE_GENERATOR_H

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

class GameObject;

/**
 * @brief Identifies the concrete subtype of a ForceGenerator.
 *
 * Used by ForceGeneratorRegistry to handle type-specific logic (e.g. marking
 * explosions as fired after one tick) and by the editor to display the correct
 * parameter panel.
 */
enum class ForceGeneratorType {
    WIND, ///< Constant directional force over a region. No distance falloff.
    GRAVITY_WELL, ///< Inverse-square attraction (positive strength) or repulsion (negative).
    VORTEX, ///< Tangential spin + inward pull, creating a spiral/orbital effect.
    EXPLOSION ///< Single-frame radial impulse. Auto-removed after firing.
}; 


// Base class
/**
 * @brief Abstract base for all area-based force generators.
 *
 * Stores the common state shared by every subtype: world-space position,
 * radius of effect, strength, enabled flag, name, type tag, and a unique ID.
 * Subclasses override apply() to implement their specific force model.
 *
 * A radius of zero or less means infinite range — the generator affects every
 * dynamic body in the scene regardless of distance.
 */
class ForceGenerator {
protected:
    std::string          name; ///< Human-readable label for the editor and debug output.
    ForceGeneratorType   type;  ///< Concrete subtype identifier.
    glm::vec3            position;   ///< World-space centre of the generator's area of effect.
    float                radius;    ///< Radius of effect in world units. 0 or negative = infinite
    bool                 enabled; ///< When false, apply() is never called by the registry.
    float                strength; ///< Primary force magnitude; exact meaning varies by subtype.

    uint64_t id;  ///< Unique ID assigned at construction from nextID.
    static uint64_t nextID;   ///< Monotonically increasing ID counter.
 

public:

    /**
    * @brief Constructs a ForceGenerator with common base-class fields.
    *
    * Generators are enabled by default. ID is assigned from nextID.
    *
    * @param name      Human-readable label.
    * @param type      Concrete subtype identifier.
    * @param position  World-space centre of the area of effect.
    * @param radius    Radius of effect (0 or negative = infinite).
    * @param strength  Primary force magnitude.
    */
    ForceGenerator(const std::string& name,
        ForceGeneratorType type,
        const glm::vec3& position,
        float radius,
        float strength);


    virtual ~ForceGenerator() = default;

	// apply force to single rigid body based on its position and the generator's parameters

    /**
     * @brief Applies this generator's force to a single rigid body.
     *
     * Called by ForceGeneratorRegistry::update() once per physics tick for
     * every dynamic rigid body in the scene. Implementations should call
     * isInRange() first and return early if the body is out of range.
     *
     * @param body       The Bullet rigid body to affect.
     * @param bodyPos    World-space position of the body (pre-fetched by registry).
     * @param deltaTime  Physics timestep in seconds.
     */
    virtual void apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime) = 0;

	// removes one -shot generators after they have fired (e.g. Explosion)
     /**
     * @brief Returns true if this generator has expired and should be removed.
     *
     * Overridden by ExplosionGenerator to return true after it fires once.
     * The base implementation always returns false (persistent generators).
     *
     * @return  False for all persistent generator types.
     */
    virtual bool isExpired() const { return false; }

    // Getters
    uint64_t              getID()       const { return id; } ///< Unique generator ID.
    const std::string& getName()     const { return name; } ///< Human-readable name.
    ForceGeneratorType    getType()     const { return type; } ///< Concrete subtype tag.
    glm::vec3             getPosition() const { return position; } ///< World-space centre.
    float                 getRadius()   const { return radius; } ///< Radius of effect.
    float                 getStrength() const { return strength; }  ///< Force magnitude.
    bool                  isEnabled()   const { return enabled; } ///< Whether apply() is called.
     
    // Setters 
    void setName(const std::string& n) { name = n; }  ///< Updates the display name.
    void setPosition(const glm::vec3& p) { position = p; }  ///< Moves the generator in world space.
    void setRadius(float r) { radius = r; } ///< Changes the area of effect radius.
    void setStrength(float s) { strength = s; }  ///< Changes the force magnitude.
    void setEnabled(bool e) { enabled = e; }  ///< Enables or disables the generator

protected:
    /** Returns true if bodyPos is within this generator's radius of effect. */
     /**
     * @brief Returns true if bodyPos falls within the generator's area of effect.
     *
     * A radius of zero or less is treated as infinite range.
     *
     * @param bodyPos  World-space position to test.
     * @return         True if the position is within the configured radius.
     */
    bool isInRange(const glm::vec3& bodyPos) const;
};


// WIND
// applies constant directional force to all bodies in a box region. Force does not fall off with distance.
/**
 * @brief Applies a constant directional force to all bodies within a radius.
 *
 * Force magnitude is uniform throughout the radius — no distance-based
 * falloff. Useful for corridors, fans, and sustained weather effects.
 */
class WindGenerator : public ForceGenerator {
private:
    glm::vec3 direction;  ///< Normalised wind direction vector.

public:
    /**
    * @brief Constructs a wind generator.
    *
    * @param name       Human-readable label.
    * @param position   Centre of the wind area in world space.
    * @param radius     Affected radius (0 = infinite).
    * @param direction  Wind direction (normalised internally; defaults to +X if zero).
    * @param strength   Force magnitude in Newtons applied each physics tick.
    */
    WindGenerator(const std::string& name,
        const glm::vec3& position,
        float radius,
        const glm::vec3& direction,
        float strength);

    /**
     * @brief Applies a constant directional force to a body within range.
     *
     * @param body       Bullet rigid body to push.
     * @param bodyPos    World-space position of the body.
     * @param deltaTime  Physics timestep (unused — Bullet integrates force over dt).
     */
    void apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime) override;

    /**
    * @brief Sets the wind direction, normalising the input vector.
    *
    * Falls back to +X if a zero-length vector is passed.
    *
    * @param dir  Desired wind direction (need not be pre-normalised).
    */
    void      setDirection(const glm::vec3& dir);

    /// Returns the current normalised wind direction.
    glm::vec3 getDirection() const { return direction; }
};

// GRAVITY WELL
/**
 * @brief Applies an inverse-square force directed toward or away from a point.
 *
 * Positive strength attracts bodies toward the well's position; negative
 * strength repels them. minDistance clamps the denominator to prevent the
 * force from approaching infinity at very short ranges.
 */
class GravityWellGenerator : public ForceGenerator {
private:
    float minDistance;  ///< Minimum clamped distance used in the force calculation.

public:

    /**
     * @brief Constructs a gravity well generator.
     *
     * @param name         Human-readable label.
     * @param position     World-space centre of the well.
     * @param radius       Affected radius (0 = infinite).
     * @param strength     Force scale. Positive = attract, negative = repel.
     * @param minDistance  Distance floor to prevent infinite force at the centre.
     *                     Defaults to 1.0.
     */
    GravityWellGenerator(const std::string& name,
        const glm::vec3& position,
        float radius,
        float strength,
        float minDistance = 1.0f);

    /**
    * @brief Applies an inverse-square force toward or away from the well centre.
    *
    * @param body       Bullet rigid body to affect.
    * @param bodyPos    World-space position of the body.
    * @param deltaTime  Physics timestep (unused).
    */
    void apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime) override;
    /// Sets the minimum clamped distance used to prevent infinite force.
    void  setMinDistance(float d) { minDistance = d; }
    /// Returns the current minimum distance clamp value.
    float getMinDistance() const { return minDistance; }
};

// VORTEX
/**
 * @brief Applies a tangential spin and inward pull to create a spiral/orbital effect.
 *
 * The tangential force (cross product of axis and direction-to-centre) creates
 * the spinning motion. The inward pull (pullStrength) causes bodies to spiral
 * toward the centre rather than orbiting at a fixed distance.
 */
class VortexGenerator : public ForceGenerator {
private:
    glm::vec3 axis;           ///< Normalised rotation axis (typically +Y for a vertical vortex).
    float     pullStrength;   ///< Inward radial force magnitude — controls spiral rate

public:
    /**
     * @brief Constructs a vortex generator.
     *
     * @param name             Human-readable label.
     * @param position         World-space centre of the vortex.
     * @param radius           Affected radius (0 = infinite).
     * @param axis             Rotation axis (normalised internally; defaults to +Y if zero).
     * @param rotationStrength Tangential force magnitude stored as base class `strength`.
     * @param pullStrength     Inward radial force magnitude.
     */
    VortexGenerator(const std::string& name,
        const glm::vec3& position,
        float radius,
        const glm::vec3& axis,
        float rotationStrength,
        float pullStrength);

    /**
     * @brief Applies combined tangential spin and inward pull to a body.
     *
     * @param body       Bullet rigid body to affect.
     * @param bodyPos    World-space position of the body.
     * @param deltaTime  Physics timestep (unused).
     */
    void apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime) override;
    /// Sets the rotation axis (normalised internally).
    void      setAxis(const glm::vec3& a) { axis = glm::normalize(a); }
    /// Sets the inward radial pull strength.
    void      setPullStrength(float s) { pullStrength = s; }
    /// Returns the current normalised rotation axis.
    glm::vec3 getAxis()        const { return axis; }
    /// Returns the current inward pull strength.
    float     getPullStrength()const { return pullStrength; }
};

// EXPLOSION
// One-shot radial burst outward from a point. Force is applied only once, then the generator expires and is removed from the registry. Strength falls off with distance.

/**
 * @brief Applies a single outward radial impulse then auto-expires.
 *
 * On the first update tick after creation, apply() fires an outward impulse
 * to every body within the blast radius (linear falloff: full strength at the
 * epicentre, zero at the edge). ForceGeneratorRegistry calls markFired() after
 * all bodies have been processed and then removes the generator via isExpired().
 */
class ExplosionGenerator : public ForceGenerator {
private:
    bool fired;  ///< Set to true by markFired() after the explosion has processed all bodies.

public:
    /**
    * @brief Constructs a single-use explosion generator.
    *
    * @param name      Human-readable label.
    * @param position  World-space epicentre.
    * @param radius    Blast radius. Bodies beyond this distance are unaffected.
    * @param strength  Peak impulse magnitude at the epicentre (N·s).
    */
    ExplosionGenerator(const std::string& name,
        const glm::vec3& position,
        float radius,
        float strength);
    /**
    * @brief Applies a single outward impulse to a body within the blast radius.
    *
    * Uses applyCentralImpulse (instantaneous) rather than applyCentralForce.
    * Bodies at the exact epicentre receive a straight upward impulse.
    *
    * @param body       Bullet rigid body to blast.
    * @param bodyPos    World-space position of the body.
    * @param deltaTime  Physics timestep (unused — impulse is instantaneous).
    */
    void apply(btRigidBody* body, const glm::vec3& bodyPos, float deltaTime) override;
    /**
   * @brief Returns true once markFired() has been called, signalling the
   *        registry to remove this generator.
   */
    bool isExpired() const override { return fired; }

    /**
      * @brief Marks the explosion as having fired.
      *
      * Called by ForceGeneratorRegistry after all bodies have received their
      * impulse in the current tick to ensure no body is skipped before removal.
      */
    void markFired() { fired = true; }
};

#endif // FORCE_GENERATOR_H