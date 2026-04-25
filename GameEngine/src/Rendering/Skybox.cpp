#include "../include/Rendering/Skybox.h"
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

/**
 * @brief Default constructor. Initialises all handles to zero.
 *
 * No GPU resources are allocated until loadCubemap() is called.
 */
Skybox::Skybox() : VAO(0), VBO(0), shaderProgram(0) {
}

/**
 * @brief Destructor. Releases all GPU resources held by the skybox.
 */
Skybox::~Skybox() {
    cleanup();
}

/**
 * @brief Creates the VAO and VBO for the skybox cube geometry.
 *
 * The skybox is rendered as a unit cube using position-only vertices (3 floats
 * each, no normals or UVs). The cube has 36 vertices (6 faces × 2 triangles
 * × 3 vertices) laid out as non-indexed triangle lists.
 *
 * The cube is centred at the origin and spans [-1, 1] on all axes. Because
 * translation is stripped from the view matrix before rendering, the cube
 * appears to surround the camera at infinite distance regardless of position.
 */
void Skybox::setupMesh() {
    // Skybox cube vertices (positions only, no normals/UVs needed)
    float skyboxVertices[] = {
        // Positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

/**
 * @brief Reads a GLSL shader source file from disk into a string.
 *
 * @param filepath Path to the shader file.
 * @return std::string The shader source, or empty string on failure.
 */
std::string Skybox::loadShaderSource(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR::SKYBOX::SHADER::FILE_NOT_FOUND: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief Compiles a single GLSL shader stage from source.
 *
 * Compilation errors are printed to stdout but do not throw. The caller
 * should check for link errors at the program stage.
 *
 * @param type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * @param source Null-terminated GLSL source string.
 * @return unsigned int OpenGL shader object ID.
 */
unsigned int Skybox::compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SKYBOX::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

/**
 * @brief Loads and links the skybox vertex and fragment shaders.
 *
 * Reads shaders/skybox.vert and shaders/skybox.frag, compiles them and
 * links them into a program stored in shaderProgram. The individual shader
 * objects are deleted after linking. Returns early if either source file
 * fails to load.
 */
void Skybox::setupShaders() {
    std::string vertexSource = loadShaderSource("shaders/skybox.vert");
    std::string fragmentSource = loadShaderSource("shaders/skybox.frag");

    if (vertexSource.empty() || fragmentSource.empty()) {
        std::cerr << "ERROR::SKYBOX::FAILED_TO_LOAD_SHADERS" << std::endl;
        return;
    }

    const char* vShaderCode = vertexSource.c_str();
    const char* fShaderCode = fragmentSource.c_str();

    unsigned int vertex = compileShader(GL_VERTEX_SHADER, vShaderCode);
    unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fShaderCode);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertex);
    glAttachShader(shaderProgram, fragment);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SKYBOX::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

/**
 * @brief Loads the six cubemap face images and prepares the skybox for rendering.
 *
 * Delegates image loading to Cubemap::loadFromFiles(), then calls setupMesh()
 * and setupShaders() to create the GPU geometry and shader program.
 *
 * @param faces Six image file paths in the order: +X, -X, +Y, -Y, +Z, -Z.
 * @return true  if the cubemap loaded successfully.
 * @return false if any face image failed to load.
 */
bool Skybox::loadCubemap(const std::vector<std::string>& faces) {
    if (!cubemap.loadFromFiles(faces)) {
        return false;
    }

    setupMesh();
    setupShaders();

    return true;
}

/**
 * @brief Renders the skybox around the scene.
 *
 * The depth function is temporarily changed to GL_LEQUAL so the skybox
 * passes depth testing at the maximum depth value (1.0) without being
 * clipped by objects already in the depth buffer.
 *
 * The translation component is stripped from the view matrix by converting
 * it to a 3×3 rotation matrix and back to a 4×4. This keeps the skybox
 * centred on the camera so it appears infinitely far away regardless of
 * camera position.
 *
 * @param view       The camera view matrix (translation will be stripped internally).
 * @param projection The camera projection matrix.
 */
void Skybox::draw(const glm::mat4& view, const glm::mat4& projection) {
    // Change depth function so skybox is drawn at max depth
    glDepthFunc(GL_LEQUAL);

    glUseProgram(shaderProgram);

    // Remove translation from view matrix (only rotation)
    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Bind cubemap
    cubemap.bind(0);
    glUniform1i(glGetUniformLocation(shaderProgram, "skybox"), 0);

    // Draw cube
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Restore default depth function
    glDepthFunc(GL_LESS);
}

/**
 * @brief Deletes the VAO, VBO, shader program and cubemap texture from the GPU.
 *
 * Safe to call multiple times — zero handles are checked before deletion.
 * Called automatically by the destructor.
 */
void Skybox::cleanup() {
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        VAO = VBO = 0;
    }
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    cubemap.cleanup();
}