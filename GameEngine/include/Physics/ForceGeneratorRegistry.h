
/**
 * @file ForceGeneratorRegistry.h
 * @brief Declares the ForceGeneratorRegistry singleton — the central manager
 *        for all area-based force generators in the scene.
 *
 * The registry owns every ForceGenerator via unique_ptr and drives the
 * simulation each physics tick by iterating all dynamic rigid bodies in the
 * Bullet world and calling apply() on each enabled generator.
 *
 * Key responsibilities:
 *   - addGenerator() / factory methods  Register new generators.
 *   - update()                           Apply forces, then remove expired generators.
 *   - removeGenerator() / clearAll()     Explicit lifetime control.
 *   - findByName() / getGeneratorsByType() Query the generator list.
 *
 * Lifetime: global singleton. Call initialize() once after the Bullet world
 * is created (done by Physics::initialize()). Call clearAll() before
 * destroying the world (done by Scene::clear() and Physics::cleanup()).
 */
#ifndef FORCE_GENERATOR_REGISTRY_H
#define FORCE_GENERATOR_REGISTRY_H

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

class ForceGenerator;
enum class ForceGeneratorType;
//singleton registry that manages all force generators in the scene
// its responsibilities include: registering new generators, updating all generators each frame, removing expired generators, and providing query methods to find generators by name, type, etc.

/**
 * @brief Singleton registry that owns and updates all ForceGenerator instances.
 *
 * Non-copyable. Obtain the instance via ForceGeneratorRegistry::getInstance().
 */
class ForceGeneratorRegistry {
private:
    static ForceGeneratorRegistry* instance; ///< Singleton pointer

    btDiscreteDynamicsWorld* dynamicsWorld = nullptr; ///< Non-owning pointer set by initialize().
    std::vector<std::unique_ptr<ForceGenerator>> generators; ///< Owning storage for all generators.

    /// Private default constructor — use getInstance().
    ForceGeneratorRegistry() = default;

public:
    /**
    * @brief Destructor — calls clearAll() to free all owned generators.
    */
    ~ForceGeneratorRegistry();

    /**
     * @brief Returns the global ForceGeneratorRegistry instance, creating it
     *        on first call.
     *
     * The instance lives for the application lifetime. Call clearAll() before
     * shutting down the physics world.
     *
     * @return Reference to the singleton instance.
     */
    static ForceGeneratorRegistry& getInstance();

    /**
      * @brief Binds the registry to a Bullet dynamics world.
      *
      * Must be called once after Physics::initialize() and before any generators
      * are added or update() is called. The world pointer is used by update() to
      * iterate all collision objects each tick.
      *
      * @param world  The active Bullet dynamics world. Must not be null.
      */
    void initialize(btDiscreteDynamicsWorld* world);
    
	// Lifetime management methods to add/remove generators, clear all generators, etc.

    /**
      * @brief Takes ownership of a generator and registers it with the registry.
      *
      * The returned raw pointer is valid until the generator expires, is
      * explicitly removed, or clearAll() is called.
      *
      * @param generator  Heap-allocated generator to take ownership of. No-op if null.
      * @return           Non-owning raw pointer to the stored generator, or nullptr
      *                   if the input was null.
      */
    ForceGenerator* addGenerator(std::unique_ptr<ForceGenerator> generator);

    /**
      * @brief Removes and destroys a generator by raw pointer.
      *
      * Linear scan through the generator list. No-op if null or not found.
      *
      * @param generator  Pointer previously returned by addGenerator() or a
      *                   factory method. Must not be used after this call.
      */
    void removeGenerator(ForceGenerator* generator);

    /**
     * @brief Removes and destroys a generator by name.
     *
     * Delegates to the pointer overload after a findByName() lookup.
     *
     * @param name  Name assigned to the generator at creation time.
     * @return      True if found and removed, false otherwise.
     */
    bool removeGenerator(const std::string& name);

    /**
      * @brief Destroys all registered generators and clears the internal list.
      *
      * Called by Scene::clear() before a scene reload. Safe to call when empty.
      */
    void clearAll();


	// factory methods for creating common generator types (also add to registry)

     /**
     * @brief Creates a WindGenerator and registers it with the registry.
     *
     * Wind applies a constant directional force to all dynamic bodies within
     * the radius each physics tick. No distance-based falloff.
     *
     * @param name       Unique label for lookup and debug output.
     * @param position   Centre of the wind area in world space.
     * @param radius     Affected radius (0 = infinite).
     * @param direction  Wind direction (normalised internally).
     * @param strength   Force magnitude in Newtons.
     * @return           Raw pointer to the registered generator.
     */
    ForceGenerator* createWind(const std::string& name,
        const glm::vec3& position,
        float radius,
        const glm::vec3& direction,
        float strength);

    /**
      * @brief Creates a GravityWellGenerator and registers it with the registry.
      *
      * Force follows an inverse-square law. Positive strength attracts bodies
      * toward the well; negative strength repels them.
      *
      * @param name      Unique label for lookup and debug output.
      * @param position  World-space centre of the well.
      * @param radius    Affected radius (0 = infinite).
      * @param strength  Force scale — positive attracts, negative repels.
      * @return          Raw pointer to the registered generator.
      */
    ForceGenerator* createGravityWell(const std::string& name,
        const glm::vec3& position,
        float radius,
        float strength);

    /**
    * @brief Creates a VortexGenerator and registers it with the registry.
    *
    * A vortex applies a tangential spin force and an inward pull, causing
    * bodies to spiral toward the vortex centre.
    *
    * @param name             Unique label for lookup and debug output.
    * @param position         World-space centre of the vortex.
    * @param radius           Affected radius (0 = infinite).
    * @param axis             Rotation axis (normalised internally).
    * @param rotationStrength Tangential force magnitude (controls spin speed).
    * @param pullStrength     Inward radial force magnitude (controls spiral rate).
    * @return                 Raw pointer to the registered generator.
    */
    ForceGenerator* createVortex(const std::string& name,
        const glm::vec3& position,
        float radius,
        const glm::vec3& axis,
        float rotationStrength,
        float pullStrength);

    /**
     * @brief Creates an ExplosionGenerator and registers it with the registry.
     *
     * The explosion fires a single outward impulse to all bodies within the
     * blast radius on the first update tick, then is automatically removed.
     * Impulse falls off linearly from `strength` at the epicentre to zero at
     * the blast edge.
     *
     * @param name      Unique label for lookup and debug output.
     * @param position  World-space epicentre of the explosion.
     * @param radius    Blast radius. Bodies beyond this are unaffected.
     * @param strength  Peak impulse magnitude at the epicentre (N·s).
     * @return          Raw pointer to the registered generator (becomes invalid
     *                  after the next update tick when it is removed).
     */
    ForceGenerator* createExplosion(const std::string& name,
        const glm::vec3& position,
        float radius,
        float strength);

	// updates all generators (apply forces to bodies, remove expired generators, etc.)
    /**
     * @brief Applies all enabled generators to every dynamic rigid body, then
     *        removes any generators that have expired.
     *
     * Called once per physics tick by Physics::update(). For each enabled
     * generator, iterates all collision objects in the Bullet world, skips
     * static bodies, and calls apply(). After all bodies are processed,
     * ExplosionGenerators are marked as fired and expired generators are erased.
     *
     * @param deltaTime  Physics timestep in seconds, forwarded to apply().
     */
    void update(float deltaTime);

   
	// Queries
     /**
     * @brief Finds a generator by its name string. Linear scan.
     *
     * @param name  Name to search for (case-sensitive).
     * @return      Raw pointer to the generator, or nullptr if not found.
     */
    ForceGenerator* findByName(const std::string& name) const;
    /**
     * @brief Returns raw pointers to all registered generators.
     *
     * Intended for the serialiser (Scene::saveToFile) and editor panels.
     * Pointers remain valid until the next add, remove, or update call.
     *
     * @return  Vector of all generator pointers in registration order.
     */
    std::vector<ForceGenerator*> getAllGenerators() const;
    /**
     * @brief Returns all generators of a specific subtype.
     *
     * @param type  The ForceGeneratorType to filter by.
     * @return      Vector of matching raw generator pointers.
     */
    std::vector<ForceGenerator*> getGeneratorsByType(ForceGeneratorType type) const;
    /// Returns the total number of registered generators.
    size_t                       getGeneratorCount() const { return generators.size(); }
    /**
   * @brief Returns true if a generator with the given name is registered.
   *
   * @param name  Name to check.
   */
    bool hasGenerator(const std::string& name) const;
    /**
    * @brief Prints a summary of all registered generators to stdout.
    *
    * Lists name, type, position, radius, strength, and enabled state for
    * each generator. Useful for debugging force field setups.
    */
    void printStats() const;
};

#endif // FORCE_GENERATOR_REGISTRY_H