#include <GL/glew.h>
#include "../include/Rendering/Mesh.h"
#include <glm/glm.hpp>
#include <cmath>
#include "../../external/tinyobjloader/tiny_obj_loader.h"
#include <iostream>
#include <cfloat>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Calculates the tangent and bitangent vectors for a single triangle.
 *
 * Tangent and bitangent are required by the TBN matrix in the vertex shader
 * to transform normal map samples from tangent space into world space.
 *
 * The calculation uses the UV delta between triangle edges to determine the
 * directions within the triangle that correspond to the texture U and V axes.
 *
 * @param pos1      World-space position of vertex 0.
 * @param pos2      World-space position of vertex 1.
 * @param pos3      World-space position of vertex 2.
 * @param uv1       Texture coordinate of vertex 0.
 * @param uv2       Texture coordinate of vertex 1.
 * @param uv3       Texture coordinate of vertex 2.
 * @param tangent   Output tangent vector (normalised), aligned with the U axis.
 * @param bitangent Output bitangent vector (normalised), aligned with the V axis.
 */
static void calculateTangentBitangent(
    const glm::vec3& pos1, const glm::vec3& pos2, const glm::vec3& pos3,
    const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3,
    glm::vec3& tangent, glm::vec3& bitangent)
{
    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;

    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    tangent = glm::normalize(tangent);

    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
    bitangent = glm::normalize(bitangent);
}

/**
 * @brief Default constructor. Initialises all GPU handles to zero.
 *
 * No GPU resources are allocated until setData() is called.
 */
Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0) {
}

/**
 * @brief Destructor. Releases all GPU resources held by this mesh.
 */
Mesh::~Mesh() {
    cleanup();
}

/**
 * @brief Move constructor. Transfers GPU resource ownership from another Mesh.
 *
 * The source mesh is left with zero handles so its destructor performs no
 * GPU work, preventing double-free of the transferred buffers.
 *
 * @param other The Mesh to move from.
 */
Mesh::Mesh(Mesh&& other) noexcept
    : VAO(other.VAO),
    VBO(other.VBO),
    EBO(other.EBO),
    vertices(std::move(other.vertices)),
    indices(std::move(other.indices)),
    indexCount(other.indexCount) {

    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.indexCount = 0;
}

/**
 * @brief Move assignment operator. Releases existing GPU resources then
 *        transfers ownership from another Mesh.
 *
 * @param other The Mesh to move from.
 * @return Reference to this Mesh.
 */
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        cleanup();

        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        indexCount = other.indexCount;

        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.indexCount = 0;
    }
    return *this;
}

/**
 * @brief Uploads interleaved vertex data and index data to the GPU.
 *
 * Expects vertices in the following interleaved format (14 floats per vertex):
 *
 *   Location 0 — Position   (3 floats, offset  0)
 *   Location 1 — Normal     (3 floats, offset  3)
 *   Location 2 — TexCoord   (2 floats, offset  6)
 *   Location 3 — Tangent    (3 floats, offset  8)
 *   Location 4 — Bitangent  (3 floats, offset 11)
 *
 * This layout matches the attribute declarations in basic.vert and must be
 * kept in sync with any changes to the vertex shader.
 *
 * @param interleavedVertices Flat array of vertex data in the format above.
 * @param inds                Index array defining the triangles to draw.
 */
void Mesh::setData(const std::vector<float>& interleavedVertices,
    const std::vector<unsigned int>& inds) {
    vertices = interleavedVertices;
    indices = inds;
    indexCount = inds.size();

    // Generate buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Upload interleaved vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW);

    // Position attribute (location 0) - 3 floats, stride 14
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1) - 3 floats, offset 3
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute (location 2) - 2 floats, offset 6
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Tangent attribute (location 3) - 3 floats, offset 8
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Bitangent attribute (location 4) - 3 floats, offset 11
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(4);

    // Upload indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    glBindVertexArray(0);
}

/**
 * @brief Issues a draw call for this mesh using indexed triangle rendering.
 *
 * Binds the VAO, calls glDrawElements, then unbinds. Must only be called
 * after setData() has been called and the mesh has valid GPU resources.
 */
void Mesh::draw() const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

/**
 * @brief Deletes the VAO, VBO and EBO and resets all handles to zero.
 *
 * Safe to call multiple times — the check on VAO != 0 prevents double-deletion.
 * Called automatically by the destructor and move assignment operator.
 */
void Mesh::cleanup() {
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
    }
}
