#include "../include/Rendering/DirectionalLight.h"
#include <glm/gtc/matrix_transform.hpp>


/**
 * @brief Constructs a DirectionalLight with the given direction, colour and intensity.
 *
 * The direction is normalised on construction so callers do not need to
 * pre-normalise the input vector.
 *
 * @param dir   World-space direction the light is shining toward.
 * @param col   RGB colour of the light (each component in [0, 1]).
 * @param inten Intensity multiplier applied to the colour when shading.
 */
DirectionalLight::DirectionalLight(const glm::vec3& dir, const glm::vec3& col, float inten)
    : direction(glm::normalize(dir)), color(col), intensity(inten) {
}

/**
 * @brief Sets the light direction, normalising the input vector.
 *
 * @param dir New light direction in world space.
 */
void DirectionalLight::setDirection(const glm::vec3& dir) {
    direction = glm::normalize(dir);
}

/**
 * @brief Sets the light colour.
 *
 * @param col New RGB colour. Components are typically in [0, 1].
 */
void DirectionalLight::setColor(const glm::vec3& col) {
    color = col;
}

/**
 * @brief Sets the light intensity, clamped to the range [0, 10].
 *
 * @param inten New intensity value. Values outside [0, 10] are clamped.
 */
void DirectionalLight::setIntensity(float inten) {
    intensity = glm::clamp(inten, 0.0f, 10.0f);  // Reasonable range
}

/**
 * @brief Computes the light-space matrix used for directional shadow mapping.
 *
 * Places a virtual camera far from the scene in the opposite direction of the
 * light, looking toward the scene centre. An orthographic projection is used
 * because directional lights have parallel rays and no perspective distortion.
 *
 * A degenerate up-vector case is handled: when the light direction is nearly
 * vertical (|y| > 0.99), the world X axis is used as the up vector instead of
 * the world Y axis to avoid a zero-length cross product inside glm::lookAt.
 *
 * @param sceneCenter World-space centre point of the scene, used as the
 *                    look-at target and the centre of the shadow frustum.
 * @param sceneRadius Approximate radius of the scene. Controls how far the
 *                    virtual light camera is placed and how large the
 *                    orthographic frustum is.
 * @return glm::mat4 The combined light projection * light view matrix,
 *                   ready to be passed to the shadow depth shader as
 *                   lightSpaceMatrix.
 */
glm::mat4 DirectionalLight::getLightSpaceMatrix(const glm::vec3& sceneCenter, float sceneRadius) const {
    // Position light far away in the direction it's pointing
    glm::vec3 lightPos = sceneCenter - direction * sceneRadius * 2.0f;

    // Choose up vector that's not parallel to light direction
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // If light is pointing mostly up/down, use a different up vector
    if (std::abs(direction.y) > 0.99f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);  // Use X axis instead
    }

    // Look at scene center
    glm::mat4 lightView = glm::lookAt(
        lightPos,
        sceneCenter,
        up
    );

    // Orthographic projection (directional light covers entire scene)
    float orthoSize = sceneRadius * 1.5f;
    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        0.1f, sceneRadius * 4.0f
    );

    return lightProjection * lightView;
}