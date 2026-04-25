#include "../../include/Rendering/ShadowMap.h"
#include <iostream>

/**
 * @brief Constructs a ShadowMap with the given resolution.
 *
 * No GPU resources are allocated until initialize() is called.
 *
 * @param width  Width of the shadow depth texture in texels.
 * @param height Height of the shadow depth texture in texels.
 */
ShadowMap::ShadowMap(unsigned int width, unsigned int height)
    : depthMapFBO(0), depthMapTexture(0), shadowWidth(width), shadowHeight(height) {
}

/**
 * @brief Destructor. Releases the depth texture and framebuffer from the GPU.
 */
ShadowMap::~ShadowMap() {
    cleanup();
}

/**
 * @brief Allocates the depth texture and framebuffer on the GPU.
 *
 * Creates a depth-only framebuffer object (FBO) with a 2D depth texture
 * attached. The texture stores depth values written during the shadow pass
 * and is later sampled in the main pass to determine whether fragments
 * are in shadow.
 *
 * Texture configuration:
 * - Internal format: GL_DEPTH_COMPONENT (32-bit float depth values)
 * - Filtering: GL_NEAREST to avoid interpolation artefacts in depth comparisons
 * - Wrap mode: GL_CLAMP_TO_BORDER with a white border (1,1,1,1) so fragments
 *   outside the shadow map are treated as fully lit rather than fully shadowed
 *
 * The FBO has no colour attachment — glDrawBuffer and glReadBuffer are both
 * set to GL_NONE because only depth data is needed for shadow mapping.
 *
 * Prints an error to stderr if the framebuffer is not complete after setup.
 */
void ShadowMap::initialize() {
    // Create framebuffer for shadow pass
    glGenFramebuffers(1, &depthMapFBO);

    // Create depth texture
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // Border color (areas outside shadow map are lit)
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);

    // We don't need color buffer for shadow pass
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::SHADOWMAP::Framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "Shadow map initialized (" << shadowWidth << "x" << shadowHeight << ")" << std::endl;
}

/**
 * @brief Binds the shadow FBO and prepares it for the depth-only shadow pass.
 *
 * Sets the viewport to match the shadow texture resolution and clears the
 * depth buffer so the previous frame's depth data does not persist.
 * Call this before rendering the scene with the shadow depth shader.
 */
void ShadowMap::bindForWriting() {
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, shadowWidth, shadowHeight);
    glClear(GL_DEPTH_BUFFER_BIT);
}

/**
 * @brief Binds the depth texture to a texture unit for sampling in the main pass.
 *
 * Called during the main render pass after the shadow pass is complete.
 * The texture unit index must match the shadowMap sampler uniform in the
 * fragment shader.
 *
 * @param textureUnit Zero-based texture unit index (bound to GL_TEXTURE0 + textureUnit).
 */
void ShadowMap::bindForReading(unsigned int textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
}

/**
 * @brief Unbinds the shadow FBO, restoring the default framebuffer.
 *
 * Called after the shadow pass is complete so subsequent rendering goes
 * to the screen rather than the depth texture.
 */
void ShadowMap::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Deletes the depth texture and FBO from the GPU.
 *
 * Safe to call multiple times — zero handles are checked before deletion.
 * Called automatically by the destructor.
 */
void ShadowMap::cleanup() {
    if (depthMapTexture) {
        glDeleteTextures(1, &depthMapTexture);
        depthMapTexture = 0;
    }
    if (depthMapFBO) {
        glDeleteFramebuffers(1, &depthMapFBO);
        depthMapFBO = 0;
    }
    std::cout << "Shadow map cleaned up" << std::endl;
}