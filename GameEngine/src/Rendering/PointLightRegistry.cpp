#include "../include/Rendering/PointLightRegistry.h"
#include <algorithm>
#include <iostream>

uint64_t PointLight::nextID = 1;
PointLightRegistry* PointLightRegistry::instance = nullptr;

/**
 * @brief Returns the singleton instance of the PointLightRegistry.
 *
 * Creates the instance on first call. Subsequent calls return the same object.
 * The registry is never destroyed during the application lifetime.
 *
 * @return Reference to the global PointLightRegistry instance.
 */
PointLightRegistry& PointLightRegistry::getInstance() {
    if (!instance)
        instance = new PointLightRegistry();
    return *instance;
}

/**
 * @brief Creates a new point light and adds it to the registry.
 *
 * The registry takes ownership of the light via a unique_ptr. The returned
 * raw pointer remains valid until the light is removed or clearAll() is called.
 *
 * @param name      Display name for the light, used for lookup and editor display.
 * @param position  Initial world-space position of the light.
 * @param colour    RGB colour of the light (each component typically in [0, 1]).
 * @param intensity Brightness multiplier applied during shading.
 * @param radius    Maximum world-space distance at which the light has any effect.
 * @return PointLight* Raw pointer to the newly created light. Owned by the registry.
 */
PointLight* PointLightRegistry::addLight(const std::string& name,
    const glm::vec3& position,
    const glm::vec3& colour,
    float intensity,
    float radius)
{
    auto light = std::make_unique<PointLight>(name, position, colour, intensity, radius);
    PointLight* raw = light.get();
    lights.push_back(std::move(light));
    std::cout << "[PointLightRegistry] Added '" << name
        << "' (total: " << lights.size() << ")" << std::endl;
    return raw;
}

/**
 * @brief Removes a point light from the registry by pointer.
 *
 * The light is destroyed immediately. Any raw pointers to it held elsewhere
 * become dangling after this call and must not be used.
 *
 * @param light Pointer to the light to remove. No-op if nullptr or not found.
 */
void PointLightRegistry::removeLight(PointLight* light) {
    if (!light) return;
    auto it = std::find_if(lights.begin(), lights.end(),
        [light](const std::unique_ptr<PointLight>& l) { return l.get() == light; });
    if (it != lights.end()) {
        lights.erase(it);
        std::cout << "[PointLightRegistry] Removed light" << std::endl;
    }
}

/**
 * @brief Removes a point light from the registry by name.
 *
 * Looks up the first light with the given name and removes it.
 *
 * @param name Name of the light to remove.
 * @return true  if a light with that name was found and removed.
 * @return false if no light with that name exists.
 */
bool PointLightRegistry::removeLight(const std::string& name) {
    PointLight* l = findByName(name);
    if (l) { removeLight(l); return true; }
    return false;
}

/**
 * @brief Destroys all point lights and empties the registry.
 *
 * All raw pointers previously returned by addLight() become dangling after
 * this call. Called automatically by Scene::clear() on scene reload.
 */
void PointLightRegistry::clearAll() {
    lights.clear();
    std::cout << "[PointLightRegistry] All lights cleared" << std::endl;
}

/**
 * @brief Finds a point light by name.
 *
 * Returns the first light whose name matches exactly. Returns nullptr if
 * no match is found.
 *
 * @param name Name to search for.
 * @return PointLight* Pointer to the matching light, or nullptr.
 */
PointLight* PointLightRegistry::findByName(const std::string& name) const {
    for (const auto& l : lights)
        if (l->getName() == name) return l.get();
    return nullptr;
}

/**
 * @brief Returns a vector of raw pointers to all registered point lights.
 *
 * The returned pointers are valid as long as the lights remain in the registry.
 * Used by the renderer each frame to upload light data to the shader and to
 * draw debug wireframe spheres.
 *
 * @return std::vector<PointLight*> All currently registered point lights.
 */
std::vector<PointLight*> PointLightRegistry::getAllLights() const {
    std::vector<PointLight*> result;
    result.reserve(lights.size());
    for (const auto& l : lights)
        result.push_back(l.get());
    return result;
}