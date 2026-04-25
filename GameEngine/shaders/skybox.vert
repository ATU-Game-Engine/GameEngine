#version 330 core

/**
 * @file skybox.vert
 * @brief Skybox vertex shader. Transforms the skybox cube vertices and
 *        produces a direction vector for cubemap sampling.
 *
 * The skybox cube spans [-1, 1] on all axes and is centred at the origin.
 * Translation is stripped from the view matrix in Skybox::draw() before
 * being passed here, so the cube appears to surround the camera at infinite
 * distance regardless of camera position.
 *
 * The vertex position is used directly as the cubemap direction vector
 * (TexCoords) because the cube vertices already represent directions from
 * the origin to the cube surface — no separate UV calculation is needed.
 *
 * Depth trick: by setting gl_Position = pos.xyww instead of pos.xyzw,
 * the z component in clip space is forced to equal w. After the perspective
 * divide (z/w) this always produces a depth of 1.0 — the maximum depth value.
 * Combined with the GL_LEQUAL depth test in Skybox::draw(), this ensures the
 * skybox is drawn behind all other geometry without needing a separate draw
 * order or disabling the depth test entirely.
 */

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

// Camera perspective
uniform mat4 projection;
// Camera view
uniform mat4 view;

void main()
{
    // Use vertex position as cubemap direction — each vertex points outward
    // from the cube centre, matching the cubemap face sampling convention
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    
    // Force depth to 1.0 (maximum) by setting z = w before perspective divide
    // This places the skybox at the far plane without disabling depth testing
    gl_Position = pos.xyww;
}