#version 330 core

/**
 * @file shadow_depth.vert
 * @brief Shadow pass vertex shader. Transforms geometry into light clip space
 *        to generate the depth values written to the shadow map.
 *
 * Only vertex positions are needed for depth rendering — normals, UVs and
 * tangents are ignored. The fragment shader (shadow_depth.frag) writes no
 * colour output; only the hardware depth buffer is populated.
 *
 * The resulting depth texture is sampled in basic.frag during the main pass
 * to determine whether each fragment is in shadow via PCF comparison.
 */

layout (location = 0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    // Transform directly from object space to light clip space
    // No normal or UV processing needed for a depth-only pass
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}