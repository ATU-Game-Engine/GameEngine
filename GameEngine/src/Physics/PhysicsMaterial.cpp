/**
 * @file PhysicsMaterial.cpp
 * @brief Implementation of PhysicsMaterial presets and the MaterialRegistry singleton.
 *
 * PhysicsMaterial is a lightweight value type that pairs friction and restitution
 * values with a name string. Eight built-in presets cover the most common surface
 * types (Default, Wood, Metal, Rubber, Ice, Concrete, Plastic, Glass).
 *
 * MaterialRegistry is a Meyer's singleton that stores all materials in an
 * unordered_map keyed by name. It is initialised on first access and pre-populated
 * with the eight built-in presets. Custom materials can be added at runtime via
 * registerMaterial().
 *
 * Physics::createRigidBody() calls getMaterial() to apply friction and restitution
 * to each newly created rigid body. If a requested material name is not found,
 * getMaterial() falls back to "Default" and logs a warning.
 */
#include "../include/Physics/PhysicsMaterial.h"
#include <iostream>

 /**
  * @brief Constructs a named material with explicit friction and restitution.
  *
  * @param name         Unique identifier used for registry lookups.
  * @param friction     Surface friction coefficient. 0 = frictionless (ice),
  *                     1 = high grip, >1 = very high grip (rubber on concrete).
  *                     Bullet multiplies the friction of both contacting surfaces.
  * @param restitution  Bounciness coefficient. 0 = no bounce (absorbs all energy),
  *                     1 = perfectly elastic (retains all energy). Values near 1
  *                     can cause energy gain — keep below 1 for stable simulation.
  */
PhysicsMaterial::PhysicsMaterial(const std::string& name,
    float friction,
    float restitution)
    : name(name), friction(friction), restitution(restitution)
{
}

// Default material constructor
/**
 * @brief Default constructor — creates the "Default" material preset.
 *
 * Equivalent to PhysicsMaterial("Default", 0.5f, 0.3f). Provided so the
 * type can be used as a map value without requiring an explicit initialiser.
 */
PhysicsMaterial::PhysicsMaterial()
    : PhysicsMaterial("Default", 0.5f, 0.3f) {
}


/**
 * @brief Returns the Default material — balanced friction and light bounce.
 *
 * Used as a fallback when no material name is specified or a lookup fails.
 * friction=0.5, restitution=0.3.
 */
PhysicsMaterial PhysicsMaterial::Default() {
    return PhysicsMaterial("Default", 0.5f, 0.3f);
}

/**
 * @brief Returns the Wood material — moderate friction, some bounce.
 *
 * Suitable for crates, floors, furniture.
 * friction=0.8, restitution=0.4.
 */
PhysicsMaterial PhysicsMaterial::Wood() {
    // Wood: moderate friction, some bounce
    return PhysicsMaterial("Wood", 0.8f, 0.4f);
}

/**
 * @brief Returns the Metal material — low friction, high bounce.
 *
 * Suitable for steel panels, machinery, railings.
 * friction=0.3, restitution=0.7.
 */
PhysicsMaterial PhysicsMaterial::Metal() {
    // Metal: low friction, high bounce
    return PhysicsMaterial("Metal", 0.3f, 0.7f);
}

/**
 * @brief Returns the Rubber material — maximum friction and near-full bounce.
 *
 * Objects should bounce almost as high as they fell and grip surfaces very
 * tightly. friction=1.0, restitution=0.95.
 */
PhysicsMaterial PhysicsMaterial::Rubber() {
    // Rubber: MAXIMUM BOUNCE - should bounce almost as high as it fell
    return PhysicsMaterial("Rubber", 1.0f, 0.95f);
}


/**
 * @brief Returns the Ice material — zero friction, minimal bounce.
 *
 * Objects slide with almost no resistance and barely bounce on contact.
 * friction=0.0, restitution=0.05.
 */
PhysicsMaterial PhysicsMaterial::Ice() {
    // Ice: ZERO friction, almost no bounce - should slide forever
    return PhysicsMaterial("Ice", 0.0f, 0.05f);
}


/**
 * @brief Returns the Concrete material — very high friction, no bounce.
 *
 * Objects stop dead on contact. Suitable for ground planes, walls, floors.
 * friction=1.5, restitution=0.0.
 */
PhysicsMaterial PhysicsMaterial::Concrete() {
    // Concrete: high friction, ZERO bounce - should stop dead
    return PhysicsMaterial("Concrete", 1.5f, 0.0f);
}


/**
 * @brief Returns the Plastic material — medium friction and bounce.
 *
 * Generic mid-range material for everyday props.
 * friction=0.5, restitution=0.5.
 */
PhysicsMaterial PhysicsMaterial::Plastic() {
    // Plastic: medium everything
    return PhysicsMaterial("Plastic", 0.5f, 0.5f);
}


/**
 * @brief Returns the Glass material — low friction, low bounce.
 *
 * Suitable for smooth, hard surfaces that don't grip or spring back much.
 * friction=0.2, restitution=0.2.
 */
PhysicsMaterial PhysicsMaterial::Glass() {
    // Glass: low friction, low bounce
    return PhysicsMaterial("Glass", 0.2f, 0.2f);
}

/**
 * @brief Gets the singleton MaterialRegistry instance.
 *
 * Implements Meyer's Singleton pattern - thread-safe in C++11 and later.
 * The instance is created on first access and persists for the program lifetime.
 *
 * @return Reference to the global MaterialRegistry
 */
MaterialRegistry& MaterialRegistry::getInstance() {
	
    static MaterialRegistry instance;
    return instance;
}

/**
 * @brief Private constructor - initializes registry with default materials.
 *
 * Called automatically on first getInstance() access.
 * Registers all preset materials (Default, Wood, Metal, etc.).
 */
MaterialRegistry::MaterialRegistry() {
    initializeDefaults();
}

/**
 * @brief Registers or updates a material in the registry.
 *
 * If a material with the same name exists, it will be replaced.
 * This allows runtime customization of material properties.
 *
 * Use cases:
 * - Adding custom materials from game configuration
 * - Editor tools for tweaking physics properties
 * - Mod support for user-defined materials
 *
 * @param material The material to register (name must be unique)
 *
 * @note Logs registration to console for debugging
 *
 * @example
 * PhysicsMaterial custom("Bouncy", 0.8f, 0.9f);
 * MaterialRegistry::getInstance().registerMaterial(custom);
 */
void MaterialRegistry::registerMaterial(const PhysicsMaterial& material) {
    materials[material.name] = material;
    std::cout << "Registered material: " << material.name
        << " (friction=" << material.friction
        << ", restitution=" << material.restitution
         << ")" << std::endl;
}


/**
 * @brief Retrieves a material by name.
 *
 * Looks up a material in the registry. If not found, returns the "Default"
 * material as a safe fallback and logs a warning.
 *
 * @param name The material name to look up (case-sensitive)
 * @return Const reference to the material (or Default if not found)
 *
 * @warning If "Default" material is missing (should never happen), this will throw.
 * @note Logs a warning to stderr if material not found
 *
 * @example
 * const PhysicsMaterial& wood = MaterialRegistry::getInstance().getMaterial("Wood");
 * rigidBody->setFriction(wood.friction);
 */
const PhysicsMaterial& MaterialRegistry::getMaterial(const std::string& name) const {
    auto it = materials.find(name);
    if (it != materials.end()) {
        return it->second;
    }

    // Return default material if not found
    std::cerr << "Warning: Material '" << name << "' not found, using Default" << std::endl;
    return materials.at("Default");
}


/**
 * @brief Checks if a material exists in the registry.
 *
 * Use this to validate material names before attempting to use them,
 * especially when loading from user data or configuration files.
 *
 * @param name Material name to check
 * @return true if material exists, false otherwise
 *
 * @example
 * if (registry.hasMaterial("CustomWood")) {
 *     // Safe to use
 * }
 */
bool MaterialRegistry::hasMaterial(const std::string& name) const {
    return materials.find(name) != materials.end();
}

/**
 * @brief Gets all registered material names.
 *
 * Returns a list of all material names currently in the registry.
 * Useful for populating UI dropdowns or displaying available options.
 *
 * @return Vector of material name strings
 *
 * @note Order is unspecified (comes from unordered_map iteration)
 * @note Vector is returned by value (copy) but is small enough to be efficient
 *
 * @example
 * // Populate ImGui combo box
 * std::vector<std::string> names = registry.getAllMaterialNames();
 * for (const auto& name : names) {
 *     ImGui::Selectable(name.c_str());
 * }
 */
std::vector<std::string> MaterialRegistry::getAllMaterialNames() const {
    std::vector<std::string> names;
    names.reserve(materials.size());

    for (const auto& pair : materials) {
        names.push_back(pair.first);
    }

    return names;
}


/**
 * @brief Initializes the registry with all preset materials.
 *
 * Called automatically by the constructor. Registers the 8 built-in
 * material presets: Default, Wood, Metal, Rubber, Ice, Concrete, Plastic, Glass.
 *
 * @note This method is idempotent - safe to call multiple times if needed
 * @note Logs initialization progress to console
 */
void MaterialRegistry::initializeDefaults() {
    std::cout << "Initializing default physics materials..." << std::endl;

    registerMaterial(PhysicsMaterial::Default());
    registerMaterial(PhysicsMaterial::Wood());
    registerMaterial(PhysicsMaterial::Metal());
    registerMaterial(PhysicsMaterial::Rubber());
    registerMaterial(PhysicsMaterial::Ice());
    registerMaterial(PhysicsMaterial::Concrete());
    registerMaterial(PhysicsMaterial::Plastic());
    registerMaterial(PhysicsMaterial::Glass());

    std::cout << "Physics materials initialized (" << materials.size() << " materials)" << std::endl;
}