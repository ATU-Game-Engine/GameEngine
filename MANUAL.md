# Game Engine Manual

This document describes how to use the engine editor, including all controls, panels and features.

---

## Getting Started

On launch the engine starts in **Editor Mode**. The viewport is in the centre of the screen with panels docked on the left, right and bottom. The skybox and scene will be visible immediately.

---

## Engine Modes

The engine has three modes switchable at runtime.

| Key | Mode | Description |
|-----|------|-------------|
| `E` | Editor | Default mode. UI panels active, object selection enabled, physics paused |
| `E` (again) | Game | Physics active, cursor captured, camera control enabled |
| `T` | Test | Spawns 750 physics objects for stress testing |
| `ESC` | Editor | Returns to editor mode from game mode |

A mode label fades in at the centre of the screen when switching.

---

## Camera Controls

### Editor Mode (Orbit Camera)
- **Right Click + Drag** — rotate camera around scene centre
- **F** — switch to Free camera mode

### Free Camera Mode
- **WASD** — move camera
- **Space** — move up
- **Ctrl** — move down
- **Mouse** — look around
- **F** — switch back to Orbit mode

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `E` | Toggle Editor / Game mode |
| `V` | Toggle wireframe physics debug visualisation |
| `F` | Toggle camera mode (Orbit / Free) |
| `F5` | Save current scene to file |
| `F9` | Load scene from file |
| `Delete` | Delete selected object |
| `ESC` | Return to Editor mode |

---

## Left Panel

### Inspector
Displays properties of the currently selected object. Click any object in the viewport to select it.

- **Name** — rename the object (press Enter to confirm)
- **Tags** — add or remove gameplay tags
- **Transform** — edit position, rotation and scale with drag sliders
- **Hit Box Scale** — adjust the physics collision box independently from the visual mesh
- **Textures** — assign or change diffuse, specular and normal map textures at runtime
- **Constraints** — view and remove any physics constraints attached to the object
- **Delete Object** — removes the object from the scene

### Stats
- Displays current FPS, delta time, rigid body count and physics status
- **Reset Layout** button restores the default panel docking layout

---

## Right Panel

### Spawn
Creates new objects in the scene.

- **Enable Physics** — toggle whether the spawned object has a physics body
- **Shape** — choose Cube, Sphere or Capsule
- **Dimensions** — set size, radius or height depending on shape
- **Position** — world position to spawn at
- **Mass** — 0 = static, any value > 0 = dynamic
- **Material** — choose a preset physics material or define a custom one with friction and restitution values
- **Diffuse Texture** — assign a colour texture
- **Specular Map** — assign a specular intensity map
- **Normal Map** — assign a normal map for surface detail
- **Name** — optional custom name, auto-generated if left blank
- **Tags** — add gameplay tags before spawning
- **Spawn Object** — creates the object in the scene

### Model Importer
Loads .obj model files at runtime.

- **Model File** — dropdown list of all .obj files found in the `models/` folder
- **Position** — world position to place the model
- **Scale** — visual scale of the model
- **Enable Physics** — optionally add a physics body
- **Mass** — 0 = static
- **Box Scale Factor** — adjusts the collision box relative to the model bounds
- **Material** — physics material preset
- **Load & Spawn Model** — loads and places the model in the scene

> Place .obj files in the `models/` folder. Restart the engine to refresh the list.

---

## Bottom Panel

### Lighting
Controls the directional light (sun).

- **Direction** — XYZ direction vector for the light
- **Color** — RGB colour of the light
- **Intensity** — brightness multiplier
- **Presets** — Noon Sun, Sunset, Night

The directional light also has a **yellow arrow gizmo** visible in the viewport. Drag the circle at the tip of the arrow to rotate the light direction interactively.

### Point Lights
Manages coloured point lights in the scene.

**Active Lights** — lists all current lights. Expand each to edit:
- Position, Colour, Intensity, Radius
- Enable/Disable toggle
- Remove button

**Create Light:**
- Enter a name, position, colour, intensity and radius
- Click **Create**

Point lights can also be selected by clicking their wireframe sphere in the viewport (visible when debug mode is on with `V`). Once selected, use the axis gizmo to drag them to a new position.

### Constraint Creator
Creates physics constraints between two objects.

- **Object A / Object B** — select objects from the scene
- **Constraint Type** — Fixed, Hinge, Slider, Spring, Generic 6DOF
- **Parameters** — type-specific settings appear based on selection
- **Create Constraint** — links the two objects
- **Templates** — save and load constraint configurations as presets

### Trigger Editor
Creates and manages trigger volumes in the scene.

- **Create** — add a new trigger with a name, type, position and size
- **List** — view all active triggers
- **Properties** — edit the selected trigger's position, size, type and destination
- **Types available** — Teleport, Speed Zone, Custom Event

Triggers can be selected by clicking them in the viewport and repositioned using the gizmo.

### Force Generators
Creates area-based physics forces.

- **Types** — Wind, Gravity Well, Vortex, Explosion
- **Position** — centre of the force area
- **Radius** — affected area (0 = infinite)
- **Strength** — force magnitude
- **Type-specific options:**
  - Wind: direction vector
  - Gravity Well: minimum distance to avoid infinite force
  - Vortex: axis and pull strength
  - Explosion: one-shot, fires immediately on creation

Force generators can be selected and repositioned via the viewport gizmo.

### Scene Manager
Saves and loads named scenes.

- **Scene Name** — enter a name for the scene
- **Save Scene** — saves current scene state to disk as JSON
- **New Scene** — clears the current scene
- **Scenes dropdown** — select a previously saved scene
- **Load Scene** — loads the selected scene
- **Rename Scene** — renames the selected scene
- **Delete Scene** — removes the selected scene file

> Scene operations are only available in Editor Mode.

---

## Viewport Interaction

- **Left Click** — select an object, trigger, force generator or point light
- **Drag gizmo axes** — move the selected item along X, Y or Z axis
- **V key** — toggle wireframe debug view showing physics collision shapes and point light spheres
- **Directional light gizmo** — always visible in editor, drag the yellow handle to rotate the sun

---

## Debug Wireframe Mode (V Key)

When active, draws cyan wireframes over all physics collision shapes, coloured wireframe spheres at point light positions, and coloured outlines for triggers and force generators. Useful for checking that collision shapes match visual geometry.

---

## Scene Files

Scenes are saved as JSON files in `assets/scenes/`. Each scene stores:
- All GameObjects with transform, physics and texture data
- Point lights with position, colour, intensity and radius
- Directional light direction, colour and intensity
- Triggers and force generators

Use `F5` to quick-save and `F9` to quick-load the default scene file.