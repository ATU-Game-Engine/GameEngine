#include "../include/Rendering/Cubemap.h"
#include "../external/stb/stb_image.h"
#include <iostream>

/**
 * @brief Default constructor. Initialises all members to zero/empty state.
 *
 * No GPU resources are allocated until loadFromFiles() is called.
 */
Cubemap::Cubemap() : textureID(0), width(0), height(0) {
}

/**
 * @brief Destructor. Releases the OpenGL cubemap texture if one was created.
 */
Cubemap::~Cubemap() {
    cleanup();
}

/**
 * @brief Move constructor. Transfers ownership of the GPU texture from another Cubemap.
 *
 * The source object is left in a valid but empty state (textureID = 0) so its
 * destructor does not double-free the GPU resource.
 *
 * @param other The Cubemap to move from.
 */
Cubemap::Cubemap(Cubemap&& other) noexcept
    : textureID(other.textureID), width(other.width), height(other.height) {
    other.textureID = 0;
    other.width = 0;
    other.height = 0;
}

/**
 * @brief Move assignment operator. Transfers ownership of the GPU texture.
 *
 * Cleans up any existing GPU resource before taking ownership of the other
 * object's texture. The source is left in an empty state.
 *
 * @param other The Cubemap to move from.
 * @return Reference to this Cubemap.
 */
Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this != &other) {
        cleanup();
        textureID = other.textureID;
        width = other.width;
        height = other.height;
        other.textureID = 0;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

/**
 * @brief Loads six face images from disk and uploads them to a GPU cubemap texture.
 *
 * Faces must be provided in the standard OpenGL order:
 *   0 = +X (right), 1 = -X (left), 2 = +Y (top),
 *   3 = -Y (bottom), 4 = +Z (front), 5 = -Z (back)
 *
 * Vertical flipping is disabled for cubemap faces because the OpenGL cubemap
 * coordinate system expects the images to be unflipped.
 *
 * Texture filtering is set to GL_LINEAR on both minification and magnification.
 * All three wrap modes (S, T, R) are set to GL_CLAMP_TO_EDGE to prevent seams
 * at cubemap face boundaries.
 *
 * @param faces Vector of exactly 6 file paths, one per cubemap face.
 * @return true  if all 6 faces were loaded and uploaded successfully.
 * @return false if the vector does not contain exactly 6 paths, or if any
 *               image file fails to load (GPU resources are cleaned up on failure).
 */
bool Cubemap::loadFromFiles(const std::vector<std::string>& faces) {
    if (faces.size() != 6) {
        std::cerr << "ERROR::CUBEMAP: Must provide exactly 6 face textures" << std::endl;
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // Don't flip cubemap textures
    stbi_set_flip_vertically_on_load(false);

    // Load each face
    for (unsigned int i = 0; i < faces.size(); i++) {
        int w, h, channels;
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &channels, 0);

        if (data) {
            // Store first face dimensions
            if (i == 0) {
                width = w;
                height = h;
            }

            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

            // Upload to corresponding cubemap face
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,  // Face: +X, -X, +Y, -Y, +Z, -Z
                0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data
            );

            stbi_image_free(data);
            std::cout << "Loaded cubemap face " << i << ": " << faces[i] << std::endl;
        }
        else {
            std::cerr << "ERROR::CUBEMAP: Failed to load " << faces[i] << std::endl;
            std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
            cleanup();
            return false;
        }
    }

    // Set cubemap parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    std::cout << "Cubemap created successfully (ID: " << textureID << ")" << std::endl;
    return true;
}

/**
 * @brief Binds the cubemap texture to the specified texture unit.
 *
 * @param slot Texture unit index (0-based). The cubemap is bound to GL_TEXTURE0 + slot.
 */
void Cubemap::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
}

/**
 * @brief Unbinds any cubemap texture from GL_TEXTURE_CUBE_MAP target.
 */
void Cubemap::unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

/**
 * @brief Deletes the OpenGL cubemap texture and resets the ID to zero.
 *
 * Safe to call multiple times — subsequent calls are no-ops if the texture
 * has already been deleted.
 */
void Cubemap::cleanup() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
}