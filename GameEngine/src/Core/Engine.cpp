#include "../include/Misc/FileUtils.h"
#include <iostream>
#include "../include/Core/Engine.h"
#include <btBulletDynamicsCommon.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "../include/Rendering/Renderer.h"
#include "../include/Input/Input.h"
#include "../include/Rendering/Camera.h"
#include "../include/Input/CameraController.h"
#include "../include/Core/GameTime.h"
#include "../include/Physics/Physics.h"
#include "../include/Scene/Scene.h"
#include "../include/Debug/DebugUI.h"
#include "../include/Debug/DebugUIContext.h"
#include "../External/imgui/core/imgui.h"
#include "../External/imgui/backends/imgui_impl_glfw.h"
#include "../External/imgui/backends/imgui_impl_opengl3.h"
#include "../include/UI/Raycast.h"
#include "../include/Physics/PhysicsMaterial.h"
#include "../include/Editor/Gizmo.h"
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Physics/ConstraintPreset.h"
#include "../include/Physics/ConstraintTemplate.h"
#include "../include/Saves/SceneSavePanel.h"
#include "../include/Physics/TriggerRegistry.h" 
#include "../include/Physics/Trigger.h"
#include "../include/Physics/ForceGeneratorRegistry.h"
#include "../include/Physics/ForceGenerator.h"
#include "../include/Gameplay/GameScene.h"
#include "../include/Rendering/PointLightRegistry.h"
#include "../include/Testing/TestUI.h"
#include "../include/Physics/PhysicsQuery.h"
#include <filesystem>

/**
 * @brief Placeholder simulation tick. Currently unused.
 * @param dt Elapsed time in seconds to advance the simulation by
 */
void simulate(double dt)
{
    static double totalTime = 0.0;
    totalTime += dt;

    // Print message every simulated second
    if (static_cast<int>(totalTime) % 1 == 0)
    {
        // Placeholder for future physics updates
        // std::cout << "Simulating at t = " << totalTime << "s" << std::endl;
    }
}

/**
 * @brief Initialises all engine systems and runs the main loop.
 *
 * Sets up GLFW, OpenGL, ImGui, input, physics, scene, camera, and all
 * editor systems, then loops until the window is closed. Each frame handles
 * input, fixed-timestep physics, gizmo interaction, editor ray picking,
 * UI drawing, rendering, and frame limiting. Returns 0 on clean exit, -1
 * if GLFW or GLEW initialisation fails
 */
int Start(void)
{
    GLFWwindow* window;

    // Tracks whether the engine is currently running in editor mode
    // Editor mode (UI + selection) or game mode (gameplay + camera control).
    EngineMode engineMode = EngineMode::Editor;

    // Display mode on screen
    float modeDisplayTimer = 0.0f;
    const float modeDisplayDuration = 2.0f;
    std::string modeDisplayText = "EDITOR MODE";

    // Currently selected object in editor mode.
    // This is editor-only state and does NOT belong in Renderer or Scene.
    std::vector<GameObject*> selectedObjects;

    Trigger* selectedTrigger = nullptr;

    // Currently selected force generator (editor-onlythe same as triggers)
    ForceGenerator* selectedForceGenerator = nullptr;

    PointLight* selectedPointLight = nullptr;

    if (!glfwInit())
    {
        std::cout << "Failed to init GLFW" << std::endl;
        return -1;
    }

    std::cout << "GLFW initialized" << std::endl;

    // Request OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(960, 720, "Game Engine", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "Window created" << std::endl;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    std::cout << "Initializing GLEW..." << std::endl;
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cout << "GLEW Error: " << glewGetErrorString(err) << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "GLEW initialized successfully" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // ImGui initialization (Engine-owned)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize Input System
    Input::Initialize(window);

    // Initialize Time System
    Time::Initialize();
    Time::SetTargetFPS(60.0f);
  

    // Set background color
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    // Create and initialize renderer
    Renderer renderer;
    renderer.initialize();

    // Load skybox
    std::vector<std::string> skyboxFaces = {
        "textures/skybox/right.jpg",
        "textures/skybox/left.jpg",
        "textures/skybox/top.jpg",
        "textures/skybox/bottom.jpg",
        "textures/skybox/front.jpg",
        "textures/skybox/back.jpg"
    };

    if (renderer.loadSkybox(skyboxFaces)) {
        std::cout << "Skybox loaded successfully!" << std::endl;
    }
    else {
        std::cout << "Failed to load skybox" << std::endl;
    }

	// Create and initialize physics system
    Physics physics;
    physics.initialize();
    ConstraintRegistry::getInstance().initialize(physics.getWorld());
    TriggerRegistry::getInstance().initialize(physics.getWorld()); 
    ForceGeneratorRegistry::getInstance().initialize(physics.getWorld());

	// register ghost pair callback for trigger detection (bullet uses ghost objects to detect overlaps without physical response)
    physics.getWorld()->getBroadphase()->getOverlappingPairCache()
        ->setInternalGhostPairCallback(new btGhostPairCallback()); 

    std::cout << "Physics world has " << physics.getRigidBodyCount()<< " rigid bodies" << std::endl;
    // Initialize constraint templates
    ConstraintTemplateRegistry::getInstance().load();
    ConstraintTemplateRegistry::getInstance().initializeDefaults();
    std::cout << "Loaded " << ConstraintTemplateRegistry::getInstance().getTemplateCount()
        << " constraint templates" << std::endl;

    // Create scene manager
    Scene scene(physics, renderer);
    
    // TODO: Replace hardcoded scene with file loading
    // scene.loadFromFile("scenes/test_level.json");


    //create camera
    Camera camera(
        glm::vec3(0.0f, 0.0f, 5.0f),  //Position
        glm::vec3(0.0f, 1.0f, 0.0f),  //World up
		45.0f,                         //FOV
        -90.0f,                        //Yaw
        15.0f                           //Pitch
    );

    CameraController cameraController(camera, 5.0f, 0.1f);

    SetupScripts(scene, camera, physics);
    SetupGameScene(scene, camera, physics);
   
    cameraController.setMode(CameraController::Mode::FREE);  // Start in orbit mode
    cameraController.setOrbitalCenter(glm::vec3(0.0f));
    cameraController.setOrbitalRadius(25.0f);

    // Tell Input which camera to rotate
    Input::SetCameraController(&cameraController);

    // Create Debug UI
    DebugUI debugUI;

    // Create TestUI
    TestUI testUI;

    // Create gizmo
    EditorGizmo gizmo;

    // --- Window tracking ---
    bool isFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    bool isMinimized = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);

    // --- Fixed timestep setup ---
    const double fixedDt = 1.0 / 60.0; // Fixed timestep 60fps
    double accumulator = 0.0; // Collects the elapsed time to decide when to run the next physics update
    int physicsSteps = 0;
    double physicsTimer = 0.0;

    // Counters for testing
    double physicsTime = 0.0;

    std::cout << "Renderer initialized, entering main loop" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "F1 - Toggle camera mode (Orbit/Free)" << std::endl;
    std::cout << "WASD - Move (Free mode)" << std::endl;
    std::cout << "Space/Ctrl - Up/Down (Free mode)" << std::endl;
    std::cout << "Mouse - (Not yet implemented)" << std::endl;

    // ===================================
    // Main Loop
    // ===================================
    while (!glfwWindowShouldClose(window))
    {

        // UI Toggle flag
        static bool showUI = true;

        //Update time (calculates deltaTime automatically) 
        Time::Update();
        float deltaTime = Time::GetDeltaTime();
        
        // Reset per-frame input states
        Input::BeginFrame();

        //poll for input events 
        glfwPollEvents();

        // Toggle between Editor and Game modes.
        // Editor mode:
        //  - Mouse cursor is visible
        //  - ImGui and editor tools are active
        //  - Object selection is allowed
        // Game mode:
        //  - Mouse cursor is captured
        //  - Camera controls are active
        //  - Editor interaction is disabled
        if (!ImGui::GetIO().WantCaptureKeyboard &&
            Input::GetKeyPressed(GLFW_KEY_E))
        {
            if (engineMode == EngineMode::Editor)
            {
                engineMode = EngineMode::Game;

                selectedObjects.clear();
                selectedTrigger = nullptr; // Clear force trigger selection
                selectedForceGenerator = nullptr; // Clear force generator selection too
                selectedPointLight = nullptr; // Clear point light selection

                // SHOW MODE TEXT
                modeDisplayText = "GAME MODE";
                modeDisplayTimer = modeDisplayDuration;

                // Capture the mouse for gameplay / camera control
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                Input::SetCameraController(nullptr);

                std::cout << "Mode: GAME" << std::endl;

                // --- WAKE ALL PHYSICS BODIES ---
                for (auto& obj : scene.getObjects())
                {
                    if (obj->hasPhysics())
                        obj->getRigidBody()->activate(true);
                }
            }
            else
            {
                engineMode = EngineMode::Editor;

                // SHOW MODE TEXT
                modeDisplayText = "EDITOR MODE";
                modeDisplayTimer = modeDisplayDuration;

                // Release the mouse so the user can interact with the editor UI
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

                Input::SetCameraController(&cameraController);

                std::cout << "Mode: EDITOR" << std::endl;
            }

        }

        if (!ImGui::GetIO().WantCaptureKeyboard &&
            Input::GetKeyPressed(GLFW_KEY_T))
        {
            engineMode = EngineMode::Test;

            // Clear selections to avoid editor interaction conflicts
            selectedObjects.clear();
            selectedTrigger = nullptr;
            selectedForceGenerator = nullptr;
            selectedPointLight = nullptr;

            // SHOW MODE TEXT
            modeDisplayText = "TEST MODE";
            modeDisplayTimer = modeDisplayDuration;

            // Keep cursor visible for test UI
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            // Disable camera controller in test mode
            Input::SetCameraController(nullptr);

            std::cout << "Mode: TEST" << std::endl;

            for (int i = 0; i < 750; i++)
            {
                GameObject* obj = scene.spawnObject(
                    ShapeType::CUBE,
                    glm::vec3(
                        (i % 10) * 2.0f,
                        10.0f + (i / 100) * 2.0f,
                        (i / 10 % 10) * 2.0f
                    ),
                    glm::vec3(1.0f, 1.0f, 1.0f),
                    1.0f,
                    "Default",
                    "",
                    ""
                );

                if (obj && obj->hasPhysics())
                {
                    obj->getRigidBody()->activate(true);
                }
            }
        }

        if (engineMode == EngineMode::Test)
        {
            static float debugTimer = 0.0f;
            debugTimer += Time::GetDeltaTime();

            if (debugTimer >= 2.0f)
            {
                int activeCount = 0;

                for (auto& obj : scene.getObjects())
                {
                    if (obj->hasPhysics() && obj->getRigidBody()->isActive())
                        activeCount++;
                }

                std::cout << "Active bodies: " << activeCount << std::endl;

                debugTimer = 0.0f;
            }
        }
        if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_V))  // 'V' for Visualize
        {
            renderer.toggleDebugPhysics();
            std::cout << "Debug Physics Wireframes: " << (renderer.isDebugPhysicsEnabled() ? "ON" : "OFF") << std::endl;
        }

        if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_F5))
        {
            // Save current scene to file (overwrites existing)
            scene.setLightState(renderer.getLight().getDirection(),
                renderer.getLight().getColor(),
                renderer.getLight().getIntensity());
            scene.saveToFile("../../assets/scenes/scene_test.json");
        }

        if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_F9))
        {
            selectedObjects.clear(); // Clear selection to avoid dangling pointers if objects get destroyed during load
            PointLightRegistry::getInstance().clearAll();
            if (scene.loadFromFile("../../assets/scenes/scene_test.json"))
            {
                glm::vec3 dir, col;
                float intensity;
                scene.getLightState(dir, col, intensity);
                renderer.getLight().setDirection(dir);
                renderer.getLight().setColor(col);
                renderer.getLight().setIntensity(intensity);
                SetupScripts(scene, camera, physics);
                scene.applyTagScriptsToExistingObjects();
                scene.applyTriggerScriptsToExistingTriggers();
            }
        }



        // Add the elapsed frame time into the accumulator
        accumulator += deltaTime;
        // --- Fixed timestep updates ---
        // ====================
        // Run physics updates in fixed 1/60s steps until caught up with real time
        if (engineMode == EngineMode::Game || engineMode == EngineMode::Test)
        {
            while (accumulator >= fixedDt)
            {
                physics.update(fixedDt); // advance simulation by one fixed step
				TriggerRegistry::getInstance().update(fixedDt); // Update triggers with fixed timestep
                ForceGeneratorRegistry::getInstance().update(fixedDt);

                accumulator -= fixedDt; // remove one step’s worth of time from the bucket
                physicsSteps++; // count how many physics updates ran this second
            }
        }
        else if (engineMode == EngineMode::Editor || engineMode == EngineMode::Test)
        {
            // In editor and test mode, discard accumulator so physics doesn't "catch up"
            accumulator = 0.0;
        }
        scene.update(engineMode);

        if (engineMode == EngineMode::Editor &&
            !selectedObjects.empty() &&
            !ImGui::GetIO().WantCaptureKeyboard &&
            Input::GetKeyPressed(GLFW_KEY_DELETE))
        {
            for (GameObject* obj : selectedObjects)
                scene.requestDestroy(obj);

            selectedObjects.clear();
        }


        if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_G)) {  // Press G to test grid
            std::cout << "\n=== SPATIAL GRID TEST ===" << std::endl;

            // 1. Show grid stats
            scene.printSpatialStats();

            // 2. Test query around camera
            glm::vec3 testPos = camera.getPosition();
            auto nearby = scene.findObjectsInRadius(testPos, 20.0f);

            std::cout << "\nObjects within 20 units of camera:" << std::endl;
            std::cout << "Found: " << nearby.size() << " objects" << std::endl;

            for (GameObject* obj : nearby) {
                glm::vec3 pos = obj->getPosition();
                float dist = glm::distance(pos, testPos);
                std::cout << "  - Distance: " << dist << " units" << std::endl;
            }

            std::cout << "===================\n" << std::endl;
        }
        // ========================
        // Print how many physics steps occurred every second
        physicsTime += deltaTime;

        if (physicsTime >= 1.0)
        {
            //std::cout << "Physics steps per second: " << physicsSteps << std::endl;
            physicsTime = 0.0;
            physicsSteps = 0;
        }
		// Camera mode toggle
        if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_F))
        {
            if (cameraController.getMode() == CameraController::Mode::ORBIT) {
                cameraController.setMode(CameraController::Mode::FREE);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "Camera mode: FREE" << std::endl;
            }
            else {
                cameraController.setMode(CameraController::Mode::ORBIT);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Camera mode: ORBIT" << std::endl;
            }
        }

        // Allow ESC to unlock cursor
        if (Input::GetKeyPressed(GLFW_KEY_ESCAPE))
        {
            if (engineMode == EngineMode::Game)
            {
                // Always unlock mouse in game mode
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

                // Optional: return to editor mode immediately
                engineMode = EngineMode::Editor;

                modeDisplayText = "EDITOR MODE";
                modeDisplayTimer = modeDisplayDuration;

                Input::SetCameraController(&cameraController);
            }
            else
            {
                // In editor mode, just ensure cursor is visible
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

		
        // --- Window
        glfwGetFramebufferSize(window, &fbW, &fbH);

        // Start ImGui frame BEFORE using io.WantCaptureMouse.
        // Otherwise uiWantsMouse is either invalid or from last frame.
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        // ImGui tells us if the UI wants mouse input this frame
        // (hovering, dragging sliders, clicking windows, etc.)
       // Check if mouse is over any ACTUAL panel (not the invisible dockspace)
        bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse;

        GameObject* primarySelection =
            selectedObjects.empty() ? nullptr : selectedObjects.front();

        //Update camera controller
        // Only allow normal camera control if NOT in game mode
        if (engineMode == EngineMode::Editor && !uiWantsMouse)
        {
            cameraController.update(deltaTime);
        }

        bool gizmoCapturingMouse = false;

        // Validate selections are still alive to prevent dangling pointer crashes
        // when items are deleted via UI panels without clearing the selection pointer
        if (selectedForceGenerator)
        {
            auto gens = ForceGeneratorRegistry::getInstance().getAllGenerators();
            if (std::find(gens.begin(), gens.end(), selectedForceGenerator) == gens.end())
                selectedForceGenerator = nullptr;
        }

        if (selectedTrigger)
        {
            auto trigs = TriggerRegistry::getInstance().getAllTriggers();
            if (std::find(trigs.begin(), trigs.end(), selectedTrigger) == trigs.end())
                selectedTrigger = nullptr;
        }

        if (selectedPointLight)
        {
            auto lights = PointLightRegistry::getInstance().getAllLights();
            if (std::find(lights.begin(), lights.end(), selectedPointLight) == lights.end())
                selectedPointLight = nullptr;
        }
        // Priority: ForceGenerator > Trigger > GameObject
        if (selectedForceGenerator)
        {
            gizmoCapturingMouse = gizmo.update(
                window, fbW, fbH,
                camera,
                selectedForceGenerator,
                engineMode == EngineMode::Editor,
                uiWantsMouse
            );
        }
        else if (selectedTrigger)
        {
            gizmoCapturingMouse = gizmo.update(
                window, fbW, fbH,
                camera,
                selectedTrigger,
                engineMode == EngineMode::Editor,
                uiWantsMouse
            );
        }
        else if (selectedPointLight)
        {
            gizmoCapturingMouse = gizmo.update(window, fbW, fbH, camera, selectedPointLight, engineMode == EngineMode::Editor, uiWantsMouse);
        }
        else
        {
            gizmoCapturingMouse = gizmo.update(
                window, fbW, fbH,
                camera,
                primarySelection,
                engineMode == EngineMode::Editor,
                uiWantsMouse
            );
        }

        // Light gizmo always runs alongside object gizmo
        bool lightGizmoCapturing = gizmo.update(
            window, fbW, fbH, camera,
            &renderer.getLight(),
            engineMode == EngineMode::Editor,
            uiWantsMouse);

        gizmoCapturingMouse = gizmoCapturingMouse || lightGizmoCapturing;

        // Gizmo visuals
        if (engineMode == EngineMode::Editor)
        {
            if (selectedForceGenerator)
            {
                gizmo.draw(fbW, fbH, camera, selectedForceGenerator);
            }
            else if (selectedTrigger)
            {
                gizmo.draw(fbW, fbH, camera, selectedTrigger);
            }
            else if (selectedPointLight)
            {
				gizmo.draw(fbW, fbH, camera, selectedPointLight);
			}
            else if (primarySelection)
            {
                gizmo.draw(fbW, fbH, camera, primarySelection);
            }

            // Light gizmo always visible in editor
            gizmo.draw(fbW, fbH, camera, &renderer.getLight());
        }

        // In your main loop, around where you handle input:
        if (engineMode == EngineMode::Game) {
            // Test raycast when you press R key
            if (!ImGui::GetIO().WantCaptureKeyboard &&  Input::GetKeyPressed(GLFW_KEY_R)) {
                PhysicsQuery& query = physics.getQuerySystem();

                // Raycast from camera forward
                glm::vec3 from = camera.getPosition();
                glm::vec3 to = from + camera.getFront() * 100.0f;

                RaycastHit hit;
                if (query.raycast(from, to, hit)) {
                    std::cout << "HIT: " << hit.object->getName()
                        << " at distance " << hit.distance << "m" << std::endl;
                }
                else {
                    std::cout << "MISS" << std::endl;
                }
            }
        }
        // ===============================
        // Editor Ray -> AABB Picking
        // ===============================
        if (engineMode == EngineMode::Editor && 
            !uiWantsMouse &&
            !gizmoCapturingMouse &&
            Input::GetMousePressed(GLFW_MOUSE_BUTTON_LEFT))
        {
            // Mouse position
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);

            // Reverse the rendering pipeline to build a ray from mouse click:
            // 1. Screen pixels (where user clicked)
            // 2. NDC (standard -1 to 1 range)
            // 3. Unproject using inverse camera matrices
            // 4. 3D ray in world space for intersection testing
            float x = (2.0f * static_cast<float>(mouseX)) / fbW - 1.0f;
            float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / fbH;

            // Create a point in clip space at the near clipping plane
            // (x, y) = NDC coordinates of mouse click
            // z = -1.0 = near plane (start of visible depth)
            // w = 1.0 = this is a POINT/position (not a direction yet)
            glm::vec4 rayClip(x, y, -1.0f, 1.0f);

            // Get the camera's projection matrix
            // Aspect ratio is needed to match screen dimensions
            glm::mat4 projection =
                camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

            // Unproject clip space to eye space, then convert to forward-pointing direction vector
            glm::vec4 rayEye = glm::inverse(projection) * rayClip;
            rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

            // Inverse view matrix transforms from eye space -> world space, then normalize to unit direction
            glm::vec3 rayDirection =
                glm::normalize(glm::vec3(glm::inverse(camera.getViewMatrix()) * rayEye));

            // Ray starts at the camera's position in world space
            glm::vec3 rayOrigin = camera.getPosition();

            std::cout << "[Editor Ray]\n";
            std::cout << "  Mouse: (" << mouseX << ", " << mouseY << ")\n";
            std::cout << "  Origin: (" << rayOrigin.x << ", "
                << rayOrigin.y << ", "
                << rayOrigin.z << ")\n";
            std::cout << "  Direction: (" << rayDirection.x << ", "
                << rayDirection.y << ", "
                << rayDirection.z << ")\n";

            // Shoot the laser through the scene. Check every object. Did laser hit any? Keep closest one. What object did laser hit, if any.
            // Test ray against all scene objects to find closest hit
            float closestHit = FLT_MAX;
            GameObject* hitObject = nullptr;

            float closestTriggerHit = FLT_MAX;
            Trigger* hitTrigger = nullptr;

            float closestForceHit = FLT_MAX;
            ForceGenerator* hitForceGenerator = nullptr;

            // Loop through every object in the scene
            for (const auto& obj : scene.getObjects())
            {
                // Get object's world position and size
                const glm::vec3& position = obj->getPosition();
                const glm::vec3& scale = obj->getScale();

                // Build AABB (bounding box) from object's transform
                // Assumes object origin is at center
                glm::vec3 aabbMin = position - scale * 0.5f;
                glm::vec3 aabbMax = position + scale * 0.5f;

                float hitDistance;
                // Test if ray intersects this object's bounding box
                if (RayIntersectsAABB(
                    rayOrigin,
                    rayDirection,
                    aabbMin,
                    aabbMax,
                    hitDistance))
                {
                    // Keep track of the closest object hit so far
                    if (hitDistance < closestHit)
                    {
                        closestHit = hitDistance;
                        hitObject = obj.get();
                    }
                }
            }

            // Trigger Raycast
            for (Trigger* trigger : TriggerRegistry::getInstance().getAllTriggers())
            {
                glm::vec3 pos = trigger->getPosition();
                glm::vec3 size = trigger->getSize();

                glm::vec3 aabbMin = pos - size;
                glm::vec3 aabbMax = pos + size;

                float hitDistance;
                if (RayIntersectsAABB(
                    rayOrigin,
                    rayDirection,
                    aabbMin,
                    aabbMax,
                    hitDistance))
                {
                    if (hitDistance < closestTriggerHit)
                    {
                        closestTriggerHit = hitDistance;
                        hitTrigger = trigger;
                    }
                }
            }

            // Force Generator Raycast (sphere-based picking)
            for (ForceGenerator* gen : ForceGeneratorRegistry::getInstance().getAllGenerators())
            {
                glm::vec3 center = gen->getPosition();

                // If radius is 0 (infinite), give it a small clickable size
                float radius = gen->getRadius();
                if (radius <= 0.0f)
                    radius = 1.0f;

                // Ray-sphere intersection
                glm::vec3 oc = rayOrigin - center;

                float a = glm::dot(rayDirection, rayDirection);
                float b = 2.0f * glm::dot(oc, rayDirection);
                float c = glm::dot(oc, oc) - radius * radius;

                float discriminant = b * b - 4 * a * c;

                if (discriminant >= 0.0f)
                {
                    float t = (-b - sqrt(discriminant)) / (2.0f * a);

                    if (t > 0.0f && t < closestForceHit)
                    {
                        closestForceHit = t;
                        hitForceGenerator = gen;
                    }
                }
            }

            // Point Light Raycast
            float closestLightHit = FLT_MAX;
            PointLight* hitPointLight = nullptr;

            for (PointLight* light : PointLightRegistry::getInstance().getAllLights())
            {
                if (!light || !light->isEnabled()) continue;

                glm::vec3 center = light->getPosition();
                float pickRadius = 1.0f;

                glm::vec3 oc = rayOrigin - center;
                float a = glm::dot(rayDirection, rayDirection);
                float b = 2.0f * glm::dot(oc, rayDirection);
                float c = glm::dot(oc, oc) - pickRadius * pickRadius;
                float discriminant = b * b - 4 * a * c;

                if (discriminant >= 0.0f)
                {
                    float t = (-b - sqrt(discriminant)) / (2.0f * a);
                    if (t > 0.0f && t < closestLightHit)
                    {
                        closestLightHit = t;
                        hitPointLight = light;
                    }
                }
            }

            // Decide whether object or trigger is closer
            bool hitSomething = false;

            // Priority: ForceGenerator > Trigger > GameObject
            if (hitForceGenerator && (!hitTrigger || closestForceHit < closestTriggerHit) &&
                (!hitObject || closestForceHit < closestHit))
            {
                selectedForceGenerator = hitForceGenerator;
                selectedTrigger = nullptr;
                selectedObjects.clear();
                hitSomething = true;
            }
            else if (hitPointLight && (!hitTrigger || closestLightHit < closestTriggerHit) &&
                (!hitObject || closestLightHit < closestHit))
            {
                selectedPointLight = hitPointLight;
                selectedForceGenerator = nullptr;
                selectedTrigger = nullptr;
                selectedObjects.clear();
                hitSomething = true;
            }
            else if (hitTrigger && (!hitObject || closestTriggerHit < closestHit))
            {
                selectedForceGenerator = nullptr;
                selectedTrigger = hitTrigger;
                selectedObjects.clear();
                hitSomething = true;
            }
            else if (hitObject)
            {
                selectedForceGenerator = nullptr;
                selectedTrigger = nullptr;
                selectedObjects.clear();
                selectedObjects.push_back(hitObject);
                hitSomething = true;
            }

            if (!hitSomething)
            {
                selectedObjects.clear();
                selectedTrigger = nullptr;
                selectedForceGenerator = nullptr; // IMPORTANT
                selectedPointLight = nullptr;
            }


        }

        // Build Debug UI Context
        // This packages up read-only engine data and approved debug commands
        // so DebugUI can display stats and issue requests without owning systems.
        DebugUIContext uiContext;

        // Pass editor selection into the UI so Inspector can display it.
        // DebugUI does not decide selection — it only reacts to it.
        uiContext.selectedObject = primarySelection;

        // Timing / performance data
        uiContext.time.deltaTime = Time::GetDeltaTime();
        uiContext.time.fps = Time::GetFPS();

        // Physics debug data
        uiContext.physics.rigidBodyCount = physics.getRigidBodyCount();
        uiContext.physics.physicsEnabled = true;

        // Populate available materials from the registry
        uiContext.physics.availableMaterials = MaterialRegistry::getInstance().getAllMaterialNames();
        
        // generic spawn function
        uiContext.scene.spawnObject =
            [&scene](ShapeType type, const glm::vec3& position,
                const glm::vec3& size, float mass,
                const std::string& materialName,
                const std::string& texturePath,
                const std::string& specularPath)  // ADD THIS
            {
                return scene.spawnObject(type, position, size, mass, materialName, texturePath, specularPath);
            };

        // Spawn for objects without physics
        uiContext.scene.spawnRenderObject =
            [&scene](ShapeType type, const glm::vec3& pos, const glm::vec3& size,
                const std::string& tex, const std::string& specularPath)  // ADD THIS
            {
                return scene.spawnRenderObject(type, pos, size, tex, specularPath);
            };

        // Register custom material command
        uiContext.scene.registerMaterial =
            [](const std::string& name, float friction, float restitution)
            {
                PhysicsMaterial customMat(name, friction, restitution);
                MaterialRegistry::getInstance().registerMaterial(customMat);
            };
		// Set object scale command (handles both render and physics resizing)
        uiContext.scene.setObjectScale =
            [&scene](GameObject* obj, const glm::vec3& scale) {
            scene.setObjectScale(obj, scale);
            };

        // Destroy object (deferred, editor-safe)
        uiContext.scene.destroyObject =
            [&scene, &selectedObjects](GameObject* obj)
            {
                if (!obj) return;

                // Remove from selection list if present
                selectedObjects.erase(
                    std::remove(selectedObjects.begin(), selectedObjects.end(), obj),
                    selectedObjects.end()
                );

                scene.requestDestroy(obj);
            };


        // Get available textures command
        uiContext.scene.getAvailableTextures =
            []() -> std::vector<std::string>
            {
                return FileUtils::getTextureFiles("textures");
            };

		// Lighting commands
        uiContext.lighting.getLight = [&renderer]() -> DirectionalLight& {
            return renderer.getLight();
            };

        // Get available models command
        uiContext.scene.getAvailableModels =
            []() -> std::vector<std::string> {
            return FileUtils::getModelFiles("models");
            };

        // Model loading command
        uiContext.scene.loadAndSpawnModel =
            [&scene](const std::string& filepath, const glm::vec3& pos, const glm::vec3& meshScale,
                bool enablePhysics,
                float mass,
                const glm::vec3& physicsBoxScale,
                const std::string& materialName) -> GameObject* {
            return scene.loadAndSpawnModel(filepath, pos, meshScale, enablePhysics, mass, physicsBoxScale, materialName);
            };

        uiContext.scene.setObjectPhysicsScale = [&scene](GameObject* obj, const glm::vec3& scale) {
            scene.setObjectPhysicsScale(obj, scale);
            };
        // ===== Constraint System Commands =====
        auto& registry = ConstraintRegistry::getInstance();

        // Update constraint view state
        uiContext.constraints.totalConstraints = registry.getConstraintCount();
        uiContext.constraints.allConstraints = registry.getAllConstraints();

        // Count active/broken
        uiContext.constraints.activeConstraints = 0;
        uiContext.constraints.brokenConstraints = 0;
        for (Constraint* c : uiContext.constraints.allConstraints) {
            if (c->isBroken()) {
                uiContext.constraints.brokenConstraints++;
            }
            else {
                uiContext.constraints.activeConstraints++;
            }
        }

        // Count by type
        uiContext.constraints.fixedCount = registry.findConstraintsByType(ConstraintType::FIXED).size();
        uiContext.constraints.hingeCount = registry.findConstraintsByType(ConstraintType::HINGE).size();
        uiContext.constraints.sliderCount = registry.findConstraintsByType(ConstraintType::SLIDER).size();
        uiContext.constraints.springCount = registry.findConstraintsByType(ConstraintType::SPRING).size();
        uiContext.constraints.dof6Count = registry.findConstraintsByType(ConstraintType::GENERIC_6DOF).size();

        // === Creation Commands ===

        uiContext.constraintCommands.createFixed =
            [&registry](GameObject* objA, GameObject* objB) -> Constraint* {
            auto constraint = ConstraintPreset::createFixed(objA, objB);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createHinge =
            [&registry](GameObject* objA, GameObject* objB,
                const glm::vec3& worldPivot, const glm::vec3& worldAxis) -> Constraint* {
                    auto constraint = ConstraintPreset::createHinge(objA, objB, worldPivot, worldAxis);
                    return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createHingeAdvanced =
            [&registry](GameObject* objA, GameObject* objB, const HingeParams& params) -> Constraint* {
            auto constraint = ConstraintPreset::createHinge(objA, objB, params);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createSlider =
            [&registry](GameObject* objA, GameObject* objB, const SliderParams& params) -> Constraint* {
            auto constraint = ConstraintPreset::createSlider(objA, objB, params);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createSpring =
            [&registry](GameObject* objA, GameObject* objB, float stiffness, float damping) -> Constraint* {
            auto constraint = ConstraintPreset::createSpring(objA, objB, stiffness, damping);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createSpringAdvanced =
            [&registry](GameObject* objA, GameObject* objB, const SpringParams& params) -> Constraint* {
            auto constraint = ConstraintPreset::createSpring(objA, objB, params);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        uiContext.constraintCommands.createGeneric6Dof =
            [&registry](GameObject* objA, GameObject* objB, const Generic6DofParams& params) -> Constraint* {
            auto constraint = ConstraintPreset::createGeneric6Dof(objA, objB, params);
            return constraint ? registry.addConstraint(std::move(constraint)) : nullptr;
            };

        // === Management Commands ===

        uiContext.constraintCommands.removeConstraint =
            [&registry](Constraint* constraint) {
            registry.removeConstraint(constraint);
            };

        uiContext.constraintCommands.removeConstraintByName =
            [&registry](const std::string& name) -> bool {
            return registry.removeConstraint(name);
            };

        uiContext.constraintCommands.removeConstraintsForObject =
            [&registry](GameObject* obj) {
            registry.removeConstraintsForObject(obj);
            };

        uiContext.constraintCommands.clearAllConstraints =
            [&registry]() {
            registry.clearAll();
            };

        // === Query Commands ===

        uiContext.constraintCommands.findConstraintByName =
            [&registry](const std::string& name) -> Constraint* {
            return registry.findConstraintByName(name);
            };

        uiContext.constraintCommands.findConstraintsForObject =
            [&registry](GameObject* obj) -> std::vector<Constraint*> {
            return registry.findConstraintsByObject(obj);
            };

        uiContext.constraintCommands.findConstraintsByType =
            [&registry](ConstraintType type) -> std::vector<Constraint*> {
            return registry.findConstraintsByType(type);
            };
        // ===== Trigger System Commands ===== (ADD THIS ENTIRE SECTION)
        auto& triggerRegistry = TriggerRegistry::getInstance();

        // Update trigger view state
        uiContext.triggers.totalTriggers = triggerRegistry.getTriggerCount();
        uiContext.triggers.allTriggers = triggerRegistry.getAllTriggers();

        // Count enabled/disabled
        uiContext.triggers.enabledTriggers = 0;
        uiContext.triggers.disabledTriggers = 0;
        for (Trigger* t : uiContext.triggers.allTriggers) {
            if (t->isEnabled()) {
                uiContext.triggers.enabledTriggers++;
            }
            else {
                uiContext.triggers.disabledTriggers++;
            }
        }

        // Count by type
        uiContext.triggers.teleportCount = triggerRegistry.findTriggersByType(TriggerType::TELEPORT).size();
        uiContext.triggers.speedZoneCount = triggerRegistry.findTriggersByType(TriggerType::SPEED_ZONE).size();
        uiContext.triggers.customCount = triggerRegistry.findTriggersByType(TriggerType::EVENT).size();

        // === Creation Commands ===
        uiContext.triggerCommands.createTrigger =
            [&triggerRegistry](const std::string& name, TriggerType type,
                const glm::vec3& position, const glm::vec3& size) -> Trigger* {
                    return triggerRegistry.createTrigger(name, type, position, size);
            };

        // === Management Commands ===
        uiContext.triggerCommands.removeTrigger =
            [&triggerRegistry](Trigger* trigger) {
            triggerRegistry.removeTrigger(trigger);
            };

        uiContext.triggerCommands.removeTriggerByName =
            [&triggerRegistry](const std::string& name) -> bool {
            return triggerRegistry.removeTrigger(name);
            };

        uiContext.triggerCommands.clearAllTriggers =
            [&triggerRegistry]() {
            triggerRegistry.clearAll();
            };

        // === Query Commands ===
        uiContext.triggerCommands.findTriggerByName =
            [&triggerRegistry](const std::string& name) -> Trigger* {
            return triggerRegistry.findTriggerByName(name);
            };

        uiContext.triggerCommands.findTriggersByType =
            [&triggerRegistry](TriggerType type) -> std::vector<Trigger*> {
            return triggerRegistry.findTriggersByType(type);
            };

        uiContext.triggerCommands.findTriggersContainingObject =
            [&triggerRegistry](GameObject* obj) -> std::vector<Trigger*> {
            return triggerRegistry.findTriggersContainingObject(obj);
            };

        // === Type-Specific Commands ===
        uiContext.triggerCommands.setTeleportDestination =
            [](Trigger* trigger, const glm::vec3& dest) {
            if (trigger) trigger->setTeleportDestination(dest);
            };

        uiContext.triggerCommands.setForce =
            [](Trigger* trigger, const glm::vec3& direction, float magnitude) {
            if (trigger) trigger->setForce(direction, magnitude);
            };
        uiContext.triggerCommands.updateTriggerPosition =
            [](Trigger* trigger, const glm::vec3& position) {
            if (trigger) trigger->setPosition(position);
            };

        uiContext.triggerCommands.updateTriggerSize =
            [](Trigger* trigger, const glm::vec3& size) {
            if (trigger) trigger->setSize(size);
            };

        uiContext.triggerCommands.setTriggerEnabled =
            [](Trigger* trigger, bool enabled) {
            if (trigger) trigger->setEnabled(enabled);
            };
        uiContext.applyTriggerScripts = [&scene]() {
            scene.applyTriggerScriptsToExistingTriggers();
            };

        // Draw Debug UI (logic only) and TestUI
        if (engineMode == EngineMode::Editor)
        {
            debugUI.draw(uiContext);
        }
        else if (engineMode == EngineMode::Test)
        {
            testUI.draw();
        }

        // Draw SceneSavePanel
        if (engineMode == EngineMode::Editor)
        {
            DrawSceneSaveLoadPanel(scene, engineMode, renderer.getLight(), [&](){
                selectedObjects.clear();
                selectedTrigger = nullptr;
                selectedForceGenerator = nullptr;
                selectedPointLight = nullptr;
            });
        }

        // Mode change fade timer
        if (modeDisplayTimer > 0.0f)
        {
            modeDisplayTimer -= deltaTime;

            float alpha = std::clamp(modeDisplayTimer / modeDisplayDuration, 0.0f, 1.0f);

            ImGui::SetNextWindowBgAlpha(0.0f);
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;

            // Tell ImGui to place window at center of screen
            // Pivot 0.5,0.5 means the window is centered around that point
            ImGui::SetNextWindowPos(
                ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f)
            );

            ImGui::Begin("ModeDisplay",
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::TextColored(
                ImVec4(1.0f, 1.0f, 1.0f, alpha),
                "%s",
                modeDisplayText.c_str()
            );

            ImGui::End();
        }

        // End ImGui frame
        ImGui::Render();

        // Camera occlusion — fade walls between camera and player
        renderer.clearAlphaOverrides();

        if (engineMode == EngineMode::Game)
        {
            GameObject* player = nullptr;
            auto players = scene.findObjectsByTag("player");
            if (!players.empty()) player = players[0];

            if (player)
            {
                glm::vec3 camPos = camera.getPosition();
                glm::vec3 playerPos = player->getPosition();
                glm::vec3 rayDir = glm::normalize(playerPos - camPos);
                float rayLen = glm::distance(camPos, playerPos);

                for (const auto& obj : scene.getObjects())
                {
                    if (obj.get() == player) continue;

                    const glm::vec3& pos = obj->getPosition();
                    const glm::vec3& scale = obj->getScale();
                    glm::vec3 aabbMin = pos - scale * 0.5f;
                    glm::vec3 aabbMax = pos + scale * 0.5f;

                    float hitDist;
                    if (RayIntersectsAABB(camPos, rayDir, aabbMin, aabbMax, hitDist))
                    {
                        if (hitDist < rayLen)
                        {
                            renderer.setObjectAlpha(obj.get(), 0.3f);
                        }
                    }
                }
            }
        }

        // --- Render ---
        if (engineMode == EngineMode::Editor)
        {
            renderer.draw(fbW, fbH, camera, scene.getObjects(), primarySelection, selectedObjects);
        }
        else
        {
            renderer.draw(fbW, fbH, camera, scene.getObjects(), nullptr, {});
        }
        renderer.drawTriggerDebug(TriggerRegistry::getInstance().getAllTriggers(), camera, fbW, fbH);
        renderer.drawForceGeneratorDebug(ForceGeneratorRegistry::getInstance().getAllGenerators(), camera, fbW, fbH);
        renderer.drawPointLightDebug(PointLightRegistry::getInstance().getAllLights(), camera, fbW, fbH);
        renderer.drawConstraintDebug(ConstraintRegistry::getInstance().getAllConstraints(), camera, fbW, fbH);
        renderer.uploadPointLights(PointLightRegistry::getInstance().getAllLights());

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

		//limit FPS if enabled
        Time::WaitForNextFrame();
    }

    std::cout << "Exiting..." << std::endl;

    // ImGui shutdown
    ConstraintTemplateRegistry::getInstance().save();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    renderer.cleanup();
    physics.cleanup();
    glfwTerminate();
    return 0;
}
