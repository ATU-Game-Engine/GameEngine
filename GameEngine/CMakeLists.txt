cmake_minimum_required(VERSION 4.2.0)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)
# Add this line for Windows


find_package(Bullet REQUIRED)
message(STATUS "BULLET_LIBRARIES: ${BULLET_LIBRARIES}")
message(STATUS "BULLET_INCLUDE_DIRS: ${BULLET_INCLUDE_DIRS}")

find_package(glfw3 REQUIRED)
message(STATUS "GLFW3_FOUND: ${glfw3_FOUND}")

find_package(OpenGL REQUIRED)
message(STATUS "OPENGL_FOUND: ${OPENGL_FOUND}")
message(STATUS "OPENGL_LIBRARIES: ${OPENGL_LIBRARIES}")

find_package(glm REQUIRED)
message(STATUS "GLM_FOUND: ${glm_FOUND}") 

find_package(GLEW REQUIRED)
message(STATUS "GLEW_FOUND: ${GLEW_FOUND}")

add_executable(GameEngine)


target_sources(GameEngine PRIVATE
   # Core
    src/Core/Main.cpp
    src/Core/Engine.cpp
    src/Core/GameTime.cpp
    
    # Rendering
    src/Rendering/Renderer.cpp
    src/Rendering/Mesh.cpp
    src/Rendering/MeshFactory.cpp
    src/Rendering/Texture.cpp
    src/Rendering/Camera.cpp
    src/Rendering/DirectionalLight.cpp
    src/Rendering/Cubemap.cpp
    src/Rendering/Skybox.cpp
    src/Rendering/ShadowMap.cpp
    src/Rendering/TextureManager.cpp
    src/Rendering/ShaderManager.cpp
    src/Rendering/PointLightRegistry.cpp
    
    # Physics
    src/Physics/Physics.cpp
    src/Physics/PhysicsMaterial.cpp
    src/Physics/SpatialGrid.cpp
    src/Physics/Constraint.cpp 
    src/Physics/Constraintregistry.cpp 
    src/Physics/Constraintpreset.cpp
    src/Physics/ConstraintTemplate.cpp
    src/Physics/PhysicsQuery.cpp
    src/Physics/Trigger.cpp
    src/Physics/TriggerRegistry.cpp
    src/Physics/ForceGenerator.cpp
    src/Physics/ForceGeneratorRegistry.cpp

    # Scene
    src/Scene/Scene.cpp
    src/Scene/GameObject.cpp
    src/Scene/Transform.cpp
    src/Scene/PhysicsComponent.cpp
    
    # Input
    src/Input/Input.cpp
    src/Input/CameraController.cpp

    # UI
    src/UI/DebugUI.cpp 
    src/UI/Raycast.cpp 
    src/UI/TriggerEditorPanel.cpp 
    src/UI/ForceGeneratorPanel.cpp
    src/UI/PointLightPanel.cpp
    src/UI/ConstraintCreatorPanel.cpp
    src/UI/InspectorPanel.cpp
    src/UI/LightingPanel.cpp
    src/UI/ModelImporterPanel.cpp
    src/UI/SpawnPanel.cpp
    # Testing
    src/Testing/TestUI.cpp

    # External
    src/External/stb_image.cpp
    external/tinyobjloader/tiny_obj_loader.cc

    # Editor
    src/Editor/Gizmo.cpp

     # Saves
     src/Saves/SceneSavePanel.cpp

     # Gameplay
     src/Gameplay/PlayerController.cpp
     src/Gameplay/GameScene.cpp
     src/Gameplay/EnemyAi.cpp
    # --- ImGui Core ---
    ${CMAKE_SOURCE_DIR}/external/imgui/core/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/core/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/core/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/core/imgui_tables.cpp

    # --- ImGui Backends ---
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp


)

# Copy shaders to build directory
add_custom_command(TARGET GameEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:GameEngine>/shaders
    COMMENT "Copying shaders to build directory"
)

# Copy textures to build directory
add_custom_command(TARGET GameEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/textures
    $<TARGET_FILE_DIR:GameEngine>/textures
    COMMENT "Copying textures to build directory"
)

# Copy models to build directory
add_custom_command(TARGET GameEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models
    $<TARGET_FILE_DIR:GameEngine>/models
    COMMENT "Copying models to build directory"
)


# --- ImGui include dirs ---
target_include_directories(GameEngine PRIVATE
    include
    ${BULLET_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/tinyobjloader
    ${CMAKE_SOURCE_DIR}/external/imgui/core
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)


target_link_libraries(GameEngine ${BULLET_LIBRARIES}
	glfw
	OpenGL::GL
	glm::glm
	GLEW::GLEW
)


