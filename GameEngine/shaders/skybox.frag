#version 330 core

/**
 * @file skybox.frag
 * @brief Skybox fragment shader. Samples the cubemap texture using the
 *        interpolated direction vector from the vertex shader.
 *
 * The direction vector (TexCoords) is used directly as a cubemap lookup key.
 * OpenGL selects the correct cubemap face and texel automatically based on
 * the dominant axis of the direction vector.
 *
 * No lighting is applied — the skybox represents the infinitely distant
 * environment and is drawn as-is at maximum depth (GL_LEQUAL depth test
 * is set in Skybox::draw() to allow this).
 */

out vec4 FragColor;

// World Space direction vector from the vertex shader
in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}
