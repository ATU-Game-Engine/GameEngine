#include "../../include/Rendering/MeshFactory.h"
#include <cmath>
#include "../../external/tinyobjloader/tiny_obj_loader.h"
#include <iostream>
#include <cfloat>
#include <algorithm>
#include <glm/glm.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Calculates the tangent and bitangent vectors for a single triangle.
 *
 * Used during mesh construction to populate the TBN matrix data required
 * by the normal mapping pipeline in the fragment shader.
 *
 * The tangent aligns with the texture U axis and the bitangent with the V axis.
 * Both are derived from the relationship between the triangle's edge vectors
 * and the corresponding UV deltas.
 *
 * @param pos1      World-space position of vertex 0.
 * @param pos2      World-space position of vertex 1.
 * @param pos3      World-space position of vertex 2.
 * @param uv1       Texture coordinate of vertex 0.
 * @param uv2       Texture coordinate of vertex 1.
 * @param uv3       Texture coordinate of vertex 2.
 * @param tangent   Output normalised tangent vector (U axis direction).
 * @param bitangent Output normalised bitangent vector (V axis direction).
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
 * @brief Creates a unit cube mesh centred at the origin.
 *
 * Each face has its own set of four vertices so per-face normals, tangents
 * and bitangents are correct. The cube spans [-0.5, 0.5] on all axes.
 *
 * Vertex format (14 floats per vertex):
 *   Position(3) | Normal(3) | TexCoord(2) | Tangent(3) | Bitangent(3)
 *
 * Tangent and bitangent are analytically correct for each face orientation,
 * matching the UV layout so normal mapping works without artefacts.
 *
 * @return Mesh A fully initialised cube mesh with GPU buffers ready to draw.
 */
Mesh MeshFactory::createCube() {
    // Format: pos(3) + normal(3) + uv(2) + tangent(3) + bitangent(3) = 14 floats
    std::vector<float> vertices = {
        // Front face (normal: 0, 0, 1, tangent: 1, 0, 0, bitangent: 0, 1, 0)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,

        // Back face (normal: 0, 0, -1, tangent: -1, 0, 0, bitangent: 0, 1, 0)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,

         // Left face (normal: -1, 0, 0, tangent: 0, 0, 1, bitangent: 0, 1, 0)
         -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f,

         // Right face (normal: 1, 0, 0, tangent: 0, 0, -1, bitangent: 0, 1, 0)
          0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
          0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
          0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f, 0.0f,

          // Top face (normal: 0, 1, 0, tangent: 1, 0, 0, bitangent: 0, 0, -1)
          -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
          -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
           0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
           0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,

           // Bottom face (normal: 0, -1, 0, tangent: 1, 0, 0, bitangent: 0, 0, 1)
           -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
           -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f
    };

    std::vector<unsigned int> indices = {
        0, 1, 2,  2, 3, 0,    // Front
        4, 5, 6,  6, 7, 4,    // Back
        8, 9, 10,  10, 11, 8,  // Left
        12, 13, 14,  14, 15, 12, // Right
        16, 17, 18,  18, 19, 16, // Top
        20, 21, 22,  22, 23, 20  // Bottom
    };

    Mesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}

/**
 * @brief Creates a UV sphere mesh centred at the origin.
 *
 * Vertices are generated using spherical coordinates, iterating over
 * latitude stacks and longitude sectors. Normals are the normalised
 * position vectors. Tangents are approximated as the horizontal
 * perpendicular to the sector angle, which is sufficient for most
 * sphere shading use cases.
 *
 * Degenerate triangles at the poles are skipped — the top stack
 * omits upper triangles and the bottom stack omits lower triangles.
 *
 * @param radius  Sphere radius. Default 0.5 gives a unit-diameter sphere.
 * @param sectors Number of longitude divisions. Higher = smoother.
 * @param stacks  Number of latitude divisions. Higher = smoother.
 * @return Mesh A fully initialised sphere mesh with GPU buffers ready to draw.
 */
Mesh MeshFactory::createSphere(float radius, int sectors, int stacks) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Generate vertices
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * M_PI / stacks;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * 2 * M_PI / sectors;

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal (normalized position)
            glm::vec3 normal = glm::normalize(glm::vec3(x / radius, y / radius, z / radius));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            // Texture coordinates
            float s = (float)j / sectors;
            float t = (float)i / stacks;
            vertices.push_back(s);
            vertices.push_back(t);

            // Tangent
            glm::vec3 tangent = glm::normalize(glm::vec3(-sinf(sectorAngle), cosf(sectorAngle), 0.0f));
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);

            // Bitangent (cross product of normal and tangent)
            glm::vec3 bitangent = glm::cross(normal, tangent);
            vertices.push_back(bitangent.x);
            vertices.push_back(bitangent.y);
            vertices.push_back(bitangent.z);
        }
    }

    // Generate indices
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    Mesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}

/**
 * @brief Creates a cylinder mesh centred at the origin with closed top and bottom caps.
 *
 * The mesh is built in three parts:
 *   1. Side walls — two rings of vertices (bottom and top) connected by quads.
 *   2. Bottom cap — a fan of triangles from a centre vertex to the bottom rim.
 *   3. Top cap    — a fan of triangles from a centre vertex to the top rim.
 *
 * Side normals point radially outward. Cap normals point along the negative
 * and positive Y axes respectively. Tangents on the sides are tangential to
 * the cylinder circumference; bitangents point upward along the Y axis.
 *
 * @param radius  Cylinder radius. Default 0.5.
 * @param height  Total cylinder height. Default 1.0.
 * @param sectors Number of circumferential divisions. Higher = smoother.
 * @return Mesh A fully initialised cylinder mesh with GPU buffers ready to draw.
 */
Mesh MeshFactory::createCylinder(float radius, float height, int sectors) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    float halfHeight = height / 2.0f;

    // Side walls
    for (int i = 0; i <= 1; ++i) {
        float y = (i == 0) ? -halfHeight : halfHeight;

        for (int j = 0; j <= sectors; ++j) {
            float angle = j * 2 * M_PI / sectors;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal (horizontal)
            glm::vec3 normal = glm::normalize(glm::vec3(x / radius, 0.0f, z / radius));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            // Texture coordinates
            vertices.push_back((float)j / sectors);
            vertices.push_back((float)i);

            // Tangent (perpendicular to normal around cylinder)
            glm::vec3 tangent = glm::normalize(glm::vec3(-sinf(angle), 0.0f, cosf(angle)));
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);

            // Bitangent (up direction for cylinder sides)
            glm::vec3 bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
            vertices.push_back(bitangent.x);
            vertices.push_back(bitangent.y);
            vertices.push_back(bitangent.z);
        }
    }

    // Side indices
    for (int i = 0; i < sectors; ++i) {
        int current = i;
        int next = i + 1;
        int currentTop = current + sectors + 1;
        int nextTop = next + sectors + 1;

        indices.push_back(current);
        indices.push_back(next);
        indices.push_back(nextTop);

        indices.push_back(current);
        indices.push_back(nextTop);
        indices.push_back(currentTop);
    }

    int baseCount = (sectors + 1) * 2;

    // Bottom cap center
    vertices.push_back(0.0f);
    vertices.push_back(-halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);  // tangent
    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);  // bitangent

    int bottomCenter = baseCount;

    // Bottom rim
    for (int j = 0; j <= sectors; ++j) {
        float angle = j * 2 * M_PI / sectors;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        vertices.push_back(x);
        vertices.push_back(-halfHeight);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(0.5f + 0.5f * cosf(angle));
        vertices.push_back(0.5f + 0.5f * sinf(angle));
        vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
    }

    // Bottom cap indices
    for (int j = 0; j < sectors; ++j) {
        indices.push_back(bottomCenter);
        indices.push_back(bottomCenter + j + 2);
        indices.push_back(bottomCenter + j + 1);
    }

    int topCenter = bottomCenter + sectors + 2;

    // Top cap center
    vertices.push_back(0.0f);
    vertices.push_back(halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);
    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(-1.0f);

    // Top rim
    for (int j = 0; j <= sectors; ++j) {
        float angle = j * 2 * M_PI / sectors;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        vertices.push_back(x);
        vertices.push_back(halfHeight);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(0.5f + 0.5f * cosf(angle));
        vertices.push_back(0.5f + 0.5f * sinf(angle));
        vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(-1.0f);
    }

    // Top cap indices
    for (int j = 0; j < sectors; ++j) {
        indices.push_back(topCenter);
        indices.push_back(topCenter + j + 1);
        indices.push_back(topCenter + j + 2);
    }

    Mesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}

/**
 * @brief Loads a triangle mesh from a Wavefront .obj file using tinyobjloader.
 *
 * Only triangulated faces are processed — any face with a vertex count other
 * than 3 is skipped. If the file contains no normals for a vertex, a default
 * upward normal (0, 1, 0) is used. Missing UV coordinates default to (0, 0).
 *
 * Tangent and bitangent vectors are calculated per triangle using
 * calculateTangentBitangent() and assigned to all three vertices of each
 * face. The final vertex data is written in the standard 14-float interleaved
 * format expected by Mesh::setData().
 *
 * After loading, the mesh is normalised to unit scale: the largest dimension
 * is scaled to 2 units and the mesh is centred at the origin. This ensures
 * consistent behaviour regardless of the original model's coordinate scale.
 *
 * @param filepath Path to the .obj file. The associated .mtl file (if any)
 *                 must be in the same directory.
 * @return Mesh A fully initialised mesh. Returns an empty Mesh if loading fails.
 */
Mesh MeshFactory::loadFromFile(const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str());

    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "ERROR: " << err << std::endl;
    }
    if (!success) {
        std::cerr << "Failed to load model: " << filepath << std::endl;
        return Mesh();
    }

    std::cout << "Loaded model: " << filepath << std::endl;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Temporary storage for calculating tangents
    struct TempVertex {
        glm::vec3 pos, normal;
        glm::vec2 uv;
        glm::vec3 tangent, bitangent;
    };
    std::vector<TempVertex> tempVertices;

    // Process shapes
    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = shape.mesh.num_face_vertices[f];

            if (fv != 3) {
                index_offset += fv;
                continue;
            }

            // Get triangle vertices
            TempVertex v[3];
            for (size_t v_idx = 0; v_idx < 3; v_idx++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v_idx];

                // Position
                v[v_idx].pos = glm::vec3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                );

                // Normal
                if (idx.normal_index >= 0) {
                    v[v_idx].normal = glm::vec3(
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    );
                }
                else {
                    v[v_idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // UV
                if (idx.texcoord_index >= 0) {
                    v[v_idx].uv = glm::vec2(
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]
                    );
                }
                else {
                    v[v_idx].uv = glm::vec2(0.0f, 0.0f);
                }
            }

            // Calculate tangent and bitangent for this triangle
            glm::vec3 tangent, bitangent;
            calculateTangentBitangent(
                v[0].pos, v[1].pos, v[2].pos,
                v[0].uv, v[1].uv, v[2].uv,
                tangent, bitangent
            );

            // Assign to all 3 vertices
            for (int i = 0; i < 3; i++) {
                v[i].tangent = tangent;
                v[i].bitangent = bitangent;
                tempVertices.push_back(v[i]);
                indices.push_back(static_cast<unsigned int>(indices.size()));
            }

            index_offset += fv;
        }
    }

    // Convert to interleaved format (14 floats per vertex)
    for (const auto& v : tempVertices) {
        vertices.push_back(v.pos.x);
        vertices.push_back(v.pos.y);
        vertices.push_back(v.pos.z);
        vertices.push_back(v.normal.x);
        vertices.push_back(v.normal.y);
        vertices.push_back(v.normal.z);
        vertices.push_back(v.uv.x);
        vertices.push_back(v.uv.y);
        vertices.push_back(v.tangent.x);
        vertices.push_back(v.tangent.y);
        vertices.push_back(v.tangent.z);
        vertices.push_back(v.bitangent.x);
        vertices.push_back(v.bitangent.y);
        vertices.push_back(v.bitangent.z);
    }

    // Normalize to unit scale
    if (vertices.size() > 0) {
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (size_t i = 0; i < vertices.size(); i += 14) {  // 14 floats per vertex
            minBounds.x = std::min(minBounds.x, vertices[i + 0]);
            minBounds.y = std::min(minBounds.y, vertices[i + 1]);
            minBounds.z = std::min(minBounds.z, vertices[i + 2]);
            maxBounds.x = std::max(maxBounds.x, vertices[i + 0]);
            maxBounds.y = std::max(maxBounds.y, vertices[i + 1]);
            maxBounds.z = std::max(maxBounds.z, vertices[i + 2]);
        }

        glm::vec3 center = (minBounds + maxBounds) * 0.5f;  
        glm::vec3 size = maxBounds - minBounds;
        float maxDim = std::max(size.x, std::max(size.y, size.z));
        float normalizeScale = 2.0f / maxDim;

        for (size_t i = 0; i < vertices.size(); i += 14) {  // 14 floats per vertex
            // Center first, then scale — so mesh center lands at origin
            vertices[i + 0] = (vertices[i + 0] - center.x) * normalizeScale;
            vertices[i + 1] = (vertices[i + 1] - center.y) * normalizeScale;
            vertices[i + 2] = (vertices[i + 2] - center.z) * normalizeScale;
        }

        std::cout << "  Normalized to unit scale" << std::endl;
    }

    Mesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}