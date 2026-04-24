/**
 * @file PhysicsMaterial.h
 * @brief Declares PhysicsMaterial (a friction/restitution value type) and
 *        MaterialRegistry (the singleton that stores and provides them).
 *
 * PhysicsMaterial is a lightweight value type pairing friction and restitution
 * with a name string. Eight built-in presets cover the most common surface
 * types and are registered automatically on first use.
 *
 * MaterialRegistry is a Meyer's singleton backed by an unordered_map. It is
 * populated on construction and can be extended at runtime via registerMaterial().
 * Physics::createRigidBody() calls getMaterial() to apply properties to each
 * newly created rigid body.
 *
 * @note Material changes after a rigid body has been created do not propagate
 *       automatically — only new rigid bodies pick up the updated values.
 */
#ifndef PHYSICS_MATERIAL_H
#define PHYSICS_MATERIAL_H

#include <string>
#include <unordered_map>
#include <memory>

/**
 * @brief Defines physical material properties for rigid bodies.
 *
 * PhysicsMaterial wraps Bullet Physics friction and restitution values,
 * providing a high-level interface for defining how objects interact physically.
 *
 * Key concepts:
 * - Friction: Controls surface grip (0=ice, 1+=rubber)
 * - Restitution: Controls bounciness (0=no bounce, 1=perfect bounce)
 *
 * Design pattern:
 * - Preset materials via static factory methods (Wood(), Metal(), etc.)
 * - Runtime customization via MaterialRegistry
 * - Applied to rigid bodies when objects are spawned
 *
 * @note These values are applied when creating rigid bodies and don't
 *       automatically update existing objects if the registry is modified.
 */
class PhysicsMaterial {
public:
    // Material properties
    float friction;      // Surface friction (0.0 = low grip, 1.0+ = high grip)
    float restitution;   // Bounciness (0.0 = no bounce, 1.0 = perfect bounce-no energy loss)
  
    std::string name;    // Material id

	// Materials properties constructor
     /**
     * @brief Constructs a named material with explicit friction and restitution.
     *
     * @param name         Unique identifier for registry lookup.
     * @param friction     Surface friction coefficient (see class description).
     * @param restitution  Bounciness coefficient (see class description).
     */
    PhysicsMaterial(const std::string& name,
        float friction,
        float restitution);

    // Default constructor (creates "Default" material)
     /**
     * @brief Default constructor — creates the "Default" material preset.
     *
     * Equivalent to PhysicsMaterial("Default", 0.5f, 0.3f). Provided so the
     * type can be used as an unordered_map value without an explicit initialiser.
     */
    PhysicsMaterial();

    // common material types
    static PhysicsMaterial Default();
    static PhysicsMaterial Wood();
    static PhysicsMaterial Metal();
    static PhysicsMaterial Rubber();
    static PhysicsMaterial Ice();
    static PhysicsMaterial Concrete();
    static PhysicsMaterial Plastic();
    static PhysicsMaterial Glass();
};

// Global material registry - manages and provides access to materials
class MaterialRegistry {
private:
    MaterialRegistry();  // Private constructor for singleton
    std::unordered_map<std::string, PhysicsMaterial> materials;

    // Prevent copying
    MaterialRegistry(const MaterialRegistry&) = delete;
    MaterialRegistry& operator=(const MaterialRegistry&) = delete;

public:
    // Get the singleton instance
    static MaterialRegistry& getInstance();

    /**
     * @brief Registers a new material or updates an existing one.
     *
     * If a material with the same name already exists, it will be replaced.
     * This allows runtime customization of material properties.
     *
     * @param material The material to register
     *
     * @note Logs registration to console for debugging
     * @note Does NOT update existing rigid bodies - only affects new objects
     *
     * @example
     * // Create custom material
     * PhysicsMaterial custom("SuperBouncy", 0.9f, 0.98f);
     * MaterialRegistry::getInstance().registerMaterial(custom);
     *
     * // Use in scene
     * scene.spawnObject(ShapeType::SPHERE, pos, size, 1.0f, "SuperBouncy");
     */
    void registerMaterial(const PhysicsMaterial& material);

    /**
      * @brief Retrieves a material by name.
      *
      * Looks up a material in the registry. If not found, returns "Default"
      * as a safe fallback and logs a warning.
      *
      * @param name Material name to look up (case-sensitive)
      * @return Const reference to the material (or Default if not found)
      *
      * @warning Returns reference to internal storage - valid until material
      *          is unregistered or registry is destroyed
      *
      * @example
      * const PhysicsMaterial& wood = registry.getMaterial("Wood");
      * body->setFriction(wood.friction);
      * body->setRestitution(wood.restitution);
      */
    const PhysicsMaterial& getMaterial(const std::string& name) const;

    /**
      * @brief Checks if a material exists in the registry.
      *
      * Useful for validating material names before use, especially when
      * loading from configuration files or user input.
      *
      * @param name Material name to check
      * @return true if material exists, false otherwise
      *
      * @example
      * if (!registry.hasMaterial(userMaterial)) {
      *     std::cerr << "Unknown material: " << userMaterial << std::endl;
      *     userMaterial = "Default";
      * }
      */
    bool hasMaterial(const std::string& name) const;


    /**
     * @brief Gets all registered material names.
     *
     * Returns a list of all materials currently in the registry.
     * Perfect for populating UI dropdowns or listing available options.
     *
     * @return Vector of material name strings
     *
     * @note Order is unspecified (unordered_map iteration)
     * @note Returns by value (copy) but vector is small
     *
     * @example
     * // ImGui dropdown
     * auto names = registry.getAllMaterialNames();
     * for (const auto& name : names) {
     *     if (ImGui::Selectable(name.c_str())) {
     *         selectedMaterial = name;
     *     }
     * }
     */
    std::vector<std::string> getAllMaterialNames() const;

    /**
    * @brief Initializes the registry with preset materials.
    *
    * Called automatically by the constructor. Registers these 8 presets:
    * Default, Wood, Metal, Rubber, Ice, Concrete, Plastic, Glass
    *
    * @note Idempotent - safe to call multiple times
    * @note Public for potential re-initialization scenarios
    */
    void initializeDefaults();
};


#endif // PHYSICS_MATERIAL_H