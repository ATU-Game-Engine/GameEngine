# 3D Game Engine

A custom 3D game engine built from scratch in C++ as a Final Year Project, featuring real-time rendering, physics simulation, and a complete editor toolset.

---

## Features

### Rendering System
- OpenGL-based rendering with Phong lighting model
- Shadow mapping with Percentage Closer Filtering (PCF)
- Normal and specular mapping for surface detail
- Point light system with up to 16 simultaneous coloured lights
- Skybox system with cubemap texture support
- Model loading from .obj files with auto-normalisation
- Texture caching system and procedural mesh generation
- External GLSL shaders modifiable without recompilation
- Wireframe debug visualisation (toggle with V key)

### Physics Integration
- Bullet Physics integration for realistic simulation
- Collision detection with multiple shape types (box, sphere, cylinder, capsule)
- Physics materials system with customisable friction and restitution
- Independent hitbox scaling separate from visual mesh
- Spatial grid optimisation for efficient proximity queries
- Rigid body dynamics with dynamic and static objects
- Trigger system for collision events without physical response
- Force generator system for area-based forces
- Constraint system for joints and connections between objects
- Physics stress testing and test mode

### Editor & Tools
- ImGui-based editor with docking layout
- Editor and game mode switching (E key) with visual indicator
- Object selection via raycasting with visual highlight
- Transform gizmo for translating objects with axis-constrained dragging
- Inspector panel for live property editing
- Runtime model importer with dropdown selection
- Scene save and load system with named scenes in JSON format
- Scene manager panel for creating, renaming, loading and deleting scenes
- Performance statistics display (FPS, frame time)

### Camera System
- Free-fly camera with WASD movement and mouse look
- Editor camera with right-click mouse look
- Third-person orbit camera with player follow

### Architecture
- Component-based GameObject system for modularity
- Separate rendering and physics meshes for optimisation
- Fixed timestep game loop for stable physics updates
- Full scene serialisation for rendering and physics state
- Input management with pressed, held and released states
- Scripting system foundation for gameplay logic

---

## Tech Stack

| Category | Technology |
|----------|-----------|
| **Language** | C++ |
| **Graphics API** | OpenGL 4.x |
| **Shading Language** | GLSL |
| **Physics Engine** | Bullet Physics |
| **UI Framework** | Dear ImGui |
| **Build System** | CMake |

**Libraries:**
- GLFW (windowing and OpenGL context)
- GLAD (OpenGL function loader)
- GLM (mathematics)
- stb_image (image loading)
- tinyobjloader (model loading)

---

## Team

This project was developed as a collaborative Final Year Project:

- **Radoslaw Rodak** - Rendering Systems
- **Brian Walsh** - Framework Architecture
- **Liam Ryan** - Physics Integration

---

## Academic Context

This engine was developed as a Final Year Project for B.Sc. (Hons) Software Development at Atlantic Technological University (ATU) Galway, demonstrating practical application of:

- Real-time graphics programming
- Physics simulation algorithms
- Software architecture patterns
- Collaborative development practices
- Modern C++ techniques

**Supervisor:** Dr Aaron Hurley

---

## Build Instructions

### Prerequisites
- CMake 3.15 or higher
- Visual Studio 2022
- vcpkg installed at `C:\vcpkg`

### Steps

1. Clone the repository:
   `git clone https://github.com/ATU-Game-Engine/GameEngine`

2. Open `GameEngine.sln` in Visual Studio 2022

3. Open the **Terminal** in Visual Studio (View → Terminal)

4. Navigate into the project folder:
   `cd GameEngine`

5. Configure the build:
   `cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows`

6. Build the project:
   `cmake --build build`

7. Navigate to the executable:
   `cd build/debug`

8. Run the engine:
   `.\GameEngine.exe`

---

## License

This project is for academic purposes.

---

## Acknowledgments

- Thanks to our project supervisor Dr Aaron Hurley for guidance and feedback
- External libraries used are credited to their respective authors
