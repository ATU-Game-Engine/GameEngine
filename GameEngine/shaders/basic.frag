#version 330 core

/**
 * @file basic.frag
 * @brief Main scene fragment shader implementing Phong lighting with shadows,
 *        normal mapping, specular mapping and triplanar texture projection.
 *
 * Lighting model:
 *   - One directional light with PCF shadow mapping (3x3 kernel)
 *   - Up to 16 point lights with smoothstep distance attenuation
 *   - Phong specular (shininess = 32) for both directional and point lights
 *
 * Texture sampling:
 *   - Triplanar world-space projection for diffuse and normal map sampling,
 *     eliminating UV stretching on non-uniformly scaled objects.
 *   - Blend weights derived from the surface normal raised to the power of 4
 *     to produce sharp, seam-free transitions between projection axes.
 *
 * Special cases:
 *   - objectColor = (0,0,0) triggers an early-out returning solid black,
 *     used by the wireframe outline pass.
 *   - uIsSelected blends the result toward uHighlightColor for editor selection.
 *   - objectAlpha drives the output alpha for camera occlusion fading.
 */

out vec4 FragColor;

in vec3 FragPos;    // From vertex shader
in vec3 Normal;     // From vertex shader
in vec2 TexCoord;   // Texture coordinates
in vec4 FragPosLightSpace; // For vertex shader
in mat3 TBN;

uniform vec3 objectColor;
uniform vec3 lightDir;      // Light direction
uniform vec3 viewPos;       // Camera position
uniform vec3 lightColor;    // Light color
uniform vec3 uvTiling;
uniform float objectAlpha; // Object alpha for transparency

// Point lights - max 16
#define MAX_POINT_LIGHTS 16
uniform int         numPointLights;
uniform vec3        pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3        pointLightColours[MAX_POINT_LIGHTS];
uniform float       pointLightIntensities[MAX_POINT_LIGHTS];
uniform float       pointLightRadii[MAX_POINT_LIGHTS];

uniform sampler2D textureSampler;  // Texture sampler
uniform sampler2D shadowMap; // shadow depth texture
uniform sampler2D specularMap;
uniform sampler2D normalMap;
uniform bool useTexture;           // Flag to enable/disable texture
uniform bool useSpecularMap;
uniform bool useNormalMap;

uniform bool uIsSelected;
uniform vec3 uHighlightColor;
uniform float uHighlightStrength;

/**
 * @brief Calculates the shadow factor for the current fragment using PCF.
 *
 * Transforms the fragment's light-space position to shadow map UV coordinates
 * via perspective divide and remapping from [-1,1] to [0,1]. Samples a 3x3
 * neighbourhood of depth texels and averages the results to produce soft
 * shadow edges (Percentage Closer Filtering).
 *
 * A small constant bias of 0.005 is subtracted from the current depth before
 * comparison to prevent shadow acne caused by self-shadowing precision errors.
 *
 * Fragments outside the shadow map boundary (projCoords.z > 1.0) are treated
 * as fully lit since they lie beyond the light frustum.
 *
 * @param fragPosLightSpace Fragment position transformed by the light space matrix.
 * @return float Shadow factor in [0, 1]. 0.0 = fully lit, 1.0 = fully shadowed.
 */
float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map = no shadow
    if(projCoords.z > 1.0)
        return 0.0;
    
    // Get closest depth from light's perspective
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    
    // Get current fragment depth
    float currentDepth = projCoords.z;
    
    // Bias to prevent shadow acne
    float bias = 0.005;
    
    // PCF (Percentage Closer Filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

void main()
{
    // Check if drawing edges (black)
    if (objectColor.r < 0.1 && objectColor.g < 0.1 && objectColor.b < 0.1) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
     // Calculate norm FIRST so triplanar can use it
    vec3 norm;
      if (useNormalMap) {
        vec3 blendWeights = abs(normalize(Normal));
        blendWeights = pow(blendWeights, vec3(4.0));
        blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z);

        float ts = uvTiling.x;
        vec3 nXY = texture(normalMap, FragPos.xy * ts).rgb * 2.0 - 1.0;
        vec3 nYZ = texture(normalMap, FragPos.zy * ts).rgb * 2.0 - 1.0;
        vec3 nXZ = texture(normalMap, FragPos.xz * ts).rgb * 2.0 - 1.0;

        vec3 sampledNormal = normalize(
            nXY * blendWeights.z +
            nYZ * blendWeights.x +
            nXZ * blendWeights.y
        );
        sampledNormal.xy *= 2.0;
        sampledNormal = normalize(sampledNormal);
        norm = normalize(TBN * sampledNormal);
    } else {
        norm = normalize(Normal);
    }

    // Now triplanar can use norm
    vec3 baseColor;
    if (useTexture) {
       vec3 blendWeights = abs(norm);
        blendWeights = pow(blendWeights, vec3(4.0));
        blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z);

        float scale = uvTiling.x;
        vec3 colorXY = texture(textureSampler, FragPos.xy * uvTiling.x).rgb;
        vec3 colorYZ = texture(textureSampler, FragPos.zy * uvTiling.x).rgb;
        vec3 colorXZ = texture(textureSampler, FragPos.xz * uvTiling.x).rgb;

        baseColor = colorXY * blendWeights.z + 
                    colorYZ * blendWeights.x + 
                    colorXZ * blendWeights.y;
    } else {
        baseColor = objectColor;
    }
    
    // DIrectional light calculations
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    vec3 lightDirection = normalize(-lightDir);
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    if (useSpecularMap)
        specularStrength = texture(specularMap, TexCoord).r;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    float shadow = ShadowCalculation(FragPosLightSpace);
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);

    // Point lights
    for (int i = 0; i < numPointLights; i++)
    {
        vec3  toLight    = pointLightPositions[i] - FragPos;
        float dist       = length(toLight);
        float radius     = pointLightRadii[i];

        if (dist >= radius) continue;

        float attenuation = 1.0 - smoothstep(0.0, radius, dist);
        attenuation *= pointLightIntensities[i];

        vec3 lightDirPt = normalize(toLight);

        float diffPt = max(dot(norm, lightDirPt), 0.0);

        vec3 reflectPt = reflect(-lightDirPt, norm);
        float specPt = pow(max(dot(viewDir, reflectPt), 0.0), 32);

        lighting += attenuation * (diffPt + specularStrength * specPt) * pointLightColours[i];
    }

    // Final Output
    vec3 result = lighting * baseColor;

    // Apply selection tint
    if (uIsSelected) {
    result = mix(result, uHighlightColor, uHighlightStrength);
    }
    FragColor = vec4(result, objectAlpha);



}