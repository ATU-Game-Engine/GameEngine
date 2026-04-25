#version 330 core

/**
 * @file basic.vert
 * @brief Main scene vertex shader. Transforms geometry into world, light and
 *        clip space and constructs the TBN matrix for normal mapping.
 *
 * Vertex format (14 floats, matches Mesh::setData layout):
 *   Location 0 — Position   (vec3)
 *   Location 1 — Normal     (vec3)
 *   Location 2 — TexCoord   (vec2)
 *   Location 3 — Tangent    (vec3)
 *   Location 4 — Bitangent  (vec3)
 *
 * Outputs passed to basic.frag:
 *   FragPos           — World-space position for lighting calculations.
 *   Normal            — World-space normal (corrected for non-uniform scale).
 *   TexCoord          — UV coordinates scaled by uvTiling for texture repetition.
 *   FragPosLightSpace — Position in light clip space for shadow map lookup.
 *   TBN               — Tangent-space to world-space rotation matrix for
 *                       transforming normal map samples.
 */

layout (location = 0) in vec3 aPos;      // Position
layout (location = 1) in vec3 aNormal;   // Normal
layout (location = 2) in vec2 aTexCoord; // Texture Coordinate
layout (location = 3) in vec3 aTangent;  // Tangent
layout (location = 4) in vec3 aBitangent; // Bitangent


// Uniform matrices for transformations
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix; // Matrix to transform to light space for shadow mapping
uniform vec3 uvTiling; // UV tiling factor for texture coordinates

out vec3 FragPos;   // Position in world space
out vec3 Normal;    // Normal in world space
out vec2 TexCoord;   // Texture coordinates
out vec4 FragPosLightSpace; // Position in light space for shadow mapping
out mat3 TBN;  // Tangent-Bitangent-Normal matrix for normal mapping

void main()
{
     // Transform position to world space
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Transform normal to world space (use normal matrix to preserve perpendicularity)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord * uvTiling.xy;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0); // Calculate position in light space for shadow mapping

    // Calculate TBN matrix in world space
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    vec3 N = normalize(mat3(model) * aNormal);
    TBN = mat3(T, B, N);

    // Final position in clip space
    gl_Position = projection * view * vec4(FragPos, 1.0);
}