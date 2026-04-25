#include "../../include/Rendering/ShaderManager.h"
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>

/**
 * @brief Default constructor. No GPU resources are allocated at construction time.
 */
ShaderManager::ShaderManager() {
}

/**
 * @brief Destructor. Deletes all compiled shader programs from the GPU.
 */
ShaderManager::~ShaderManager() {
    cleanup();
}

/**
 * @brief Reads a GLSL shader source file from disk into a string.
 *
 * @param filepath Path to the shader source file (e.g. "shaders/basic.vert").
 * @return std::string The full shader source as a string, or empty string on failure.
 */
std::string ShaderManager::loadShaderSource(const std::string& filepath) {
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_FOUND: " << filepath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::cout << "Loaded shader: " << filepath << std::endl;
    return buffer.str();
}

/**
 * @brief Compiles a single GLSL shader stage from source.
 *
 * Creates an OpenGL shader object of the given type, uploads the source and
 * compiles it. Compilation errors are printed to stdout with the driver's
 * info log but do not throw — the caller receives the (possibly invalid)
 * shader ID and should check for link errors at the program stage.
 *
 * @param type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * @param source Null-terminated GLSL source string.
 * @return unsigned int OpenGL shader object ID.
 */
unsigned int ShaderManager::compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

/**
 * @brief Loads, compiles and links a vertex + fragment shader pair into a program.
 *
 * The compiled shader objects are deleted immediately after linking since they
 * are no longer needed once the program is on the GPU. The resulting program
 * ID is stored in the internal map under the given name and can be retrieved
 * with getProgram().
 *
 * @param vertPath Path to the vertex shader source file.
 * @param fragPath Path to the fragment shader source file.
 * @param name     Key used to retrieve the program later via getProgram().
 * @return unsigned int The linked OpenGL program ID, or 0 on failure.
 */
unsigned int ShaderManager::createProgram(const std::string& vertPath, const std::string& fragPath, const std::string& name) {
    // Load shader sources
    std::string vertexSource = loadShaderSource(vertPath);
    std::string fragmentSource = loadShaderSource(fragPath);

    if (vertexSource.empty() || fragmentSource.empty()) {
        std::cerr << "ERROR::SHADER::FAILED_TO_LOAD: " << name << std::endl;
        return 0;
    }

    const char* vertShader = vertexSource.c_str();
    const char* fragShader = fragmentSource.c_str();

    // Compile shaders
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertShader);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragShader);

    // Create program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED (" << name << ")\n" << infoLog << std::endl;
    }

    // Delete shaders (no longer needed after linking)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Store program
    shaderPrograms[name] = program;
    std::cout << "Shader program created: " << name << std::endl;

    return program;
}

/**
 * @brief Retrieves a previously created shader program by name.
 *
 * @param name The name the program was registered under in createProgram().
 * @return unsigned int The OpenGL program ID, or 0 if the name is not found.
 */
unsigned int ShaderManager::getProgram(const std::string& name) {
    auto it = shaderPrograms.find(name);
    if (it != shaderPrograms.end()) {
        return it->second;
    }
    std::cerr << "ERROR::SHADER::PROGRAM_NOT_FOUND: " << name << std::endl;
    return 0;
}

/**
 * @brief Deletes all stored shader programs from the GPU and clears the map.
 *
 * Called automatically by the destructor. Safe to call manually if programs
 * need to be released before the ShaderManager is destroyed.
 */
void ShaderManager::cleanup() {
    for (auto& pair : shaderPrograms) {
        glDeleteProgram(pair.second);
    }
    shaderPrograms.clear();
    std::cout << "ShaderManager cleaned up" << std::endl;
}