#include "../../include/Rendering/TextureManager.h"
#include <iostream>

/**
 * @brief Default constructor. The texture cache is empty until loadTexture() is called.
 */
TextureManager::TextureManager() {
}

/**
 * @brief Destructor. Releases all cached GPU textures.
 */
TextureManager::~TextureManager() {
    cleanup();
}

/**
 * @brief Loads a texture from disk and caches it, or returns the cached copy.
 *
 * On the first call with a given filepath the image is loaded from disk via
 * Texture::loadFromFile() and stored in the internal cache keyed by filepath.
 * Subsequent calls with the same path return a pointer to the already-uploaded
 * GPU texture without reloading or re-uploading it.
 *
 * The returned pointer is valid as long as the TextureManager is alive and
 * cleanup() has not been called. It must not be deleted by the caller.
 *
 * @param filepath Path to the image file to load.
 * @return Texture* Pointer to the cached texture, or nullptr if loading failed.
 */
Texture* TextureManager::loadTexture(const std::string& filepath) {
    // Check if texture is already cached
    auto it = textureCache.find(filepath);
    if (it != textureCache.end()) {
        // Already loaded - return cached texture
        return &it->second;
    }

    // Not cached - load new texture
    Texture texture;
    if (!texture.loadFromFile(filepath)) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return nullptr;
    }

    // Store in cache and return pointer
    textureCache[filepath] = std::move(texture);
    std::cout << "Cached texture: " << filepath << std::endl;
    return &textureCache[filepath];
}

/**
 * @brief Releases all cached GPU textures and clears the cache.
 *
 * After this call all pointers previously returned by loadTexture() are
 * invalid. Called automatically by the destructor.
 */
void TextureManager::cleanup() {
    for (auto& pair : textureCache) {
        pair.second.cleanup();
    }
    textureCache.clear();
    std::cout << "TextureManager cleaned up (" << textureCache.size() << " textures)" << std::endl;
}