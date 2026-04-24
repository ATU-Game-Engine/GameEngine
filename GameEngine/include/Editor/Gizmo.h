#pragma once

// Gizmo goal:
// - Draw editor gizmos (translate/rotate/scale) using ImGui overlay
// - No renderer changes 
// - Handle picking + dragging in editor mode only
//
// This file declares an EditorGizmo helper that:
// 1) Draws axis lines on screen for a selected object
// 2) Detects mouse hover/click on an axis
// 3) When dragging, moves the object along that world axis

/**
 * @brief ImGui-based editor gizmo for translating scene objects.
 *
 * Draws colour-coded axis lines over a selected object, detects mouse hover
 * and click on each axis, and moves the object along that axis while dragging
 * Operates entirely as a 2D ImGui overlay — no renderer changes required
 * Only active in editor mode; returns true from update() while the mouse is
 * captured so the engine can suppress normal picking during a drag
 */

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera;
class GameObject;
class Trigger;
class ForceGenerator;
struct GLFWwindow;
class DirectionalLight;
class PointLight;

class EditorGizmo
{
public:
    // Start with Translate
    // @brief Supported gizmo modes. Only Translate is currently implemented
    enum class Mode { Translate /*, Rotate, Scale */ };

    // Which axis is hovered/active
    // @brief Which axis the mouse is hovering or dragging
    enum class Axis { None, X, Y, Z };

    EditorGizmo();

    void setMode(Mode m) { mode = m; }
    Mode getMode() const { return mode; }

    // update():
    // - Runs input + math logic:
    //   - hover detection
    //   - mouse click -> begin drag
    //   - mouse move -> update object position
    //   - mouse release -> stop drag
    //
    // Returns true if the gizmo is currently using the mouse (dragging)
    // so the engine can disable editor picking during drag.

     /**
     * @brief Handles hover detection, drag start, drag update, and drag end for a GameObject
     * @return true while the gizmo is capturing the mouse (i.e. dragging).
     */
    bool update(
        GLFWwindow* window,
        int fbW, int fbH,
        const Camera& camera,
        GameObject* selectedObject,
        bool editorMode,
        bool uiWantsMouse);

    /** @brief Trigger overload — same behaviour as the GameObject version. */
    bool update(
        GLFWwindow* window,
        int fbW, int fbH,
        const Camera& camera,
        Trigger* selectedTrigger,
        bool editorMode,
        bool uiWantsMouse);

    // ForceGenerator gizmo support
    /** @brief ForceGenerator overload — same behaviour as the GameObject version. */
    bool update(GLFWwindow* window,
        int fbW, int fbH,
        const Camera& camera,
        ForceGenerator* selectedForceGenerator,
        bool editorMode,
        bool uiWantsMouse);
    
    // DirectionalLight gizmo support
     /**
     * @brief DirectionalLight overload — drags a single handle to change light direction
     * @return true while the gizmo is capturing the mouse.
     */
    bool update(GLFWwindow* window, int fbW, int fbH,
        const Camera& camera,
        DirectionalLight* light,
        bool editorMode,
        bool uiWantsMouse);

    // PointLight gizmo support 
    /** @brief PointLight overload — same behaviour as the GameObject version. */
    bool update(GLFWwindow* window, int fbW, int fbH,
        const Camera& camera,
        PointLight* light,
        bool editorMode,
        bool uiWantsMouse);


    // draw():
    // - Renders the axis lines as a 2D overlay via ImGui
    // - Call after update(), before ImGui::Render()
    /** @brief Draws the translation gizmo for a GameObject. Call after update(), before ImGui::Render(). */
    void draw(int fbW, int fbH, const Camera& camera, GameObject* selectedObject);

    // Trigger gizmo
    /** @brief Draws the translation gizmo for a Trigger. */
    void draw(int fbW, int fbH, const Camera& camera, Trigger* selectedTrigger);

    // ForceGenerator gizmo
    /** @brief Draws the translation gizmo for a ForceGenerator. */
    void draw(int fbW, int fbH, const Camera& camera, ForceGenerator* selectedForceGenerator);

    // DirectionalLight gizmo
    /** @brief Draws the direction gizmo for a DirectionalLight. */
    void draw(int fbW, int fbH, const Camera& camera, DirectionalLight* light);

    // PointLight gizmo 
    /** @brief Draws the translation gizmo for a PointLight. */
    void draw(int fbW, int fbH, const Camera& camera, PointLight* light);

    // Expose dragging state
    /// @brief Returns true while an axis drag is in progress
    bool isDragging() const { return dragging; }

private:
    // Current gizmo mode
    Mode mode = Mode::Translate;

    // Hot axis is what the mouse is hovering this frame
    Axis hotAxis = Axis::None;

    // Active axis is the axis clicked on and are dragging
    Axis activeAxis = Axis::None;

    // True while dragging along an axis
    bool dragging = false;

    // Separate dragging state for directional light, since it doesn't fit the same pattern as objects/triggers/force generators
    bool lightDragging = false;

    // Store the object's position at the moment drag begins
    // Spply relative movement cleanly.
    glm::vec3 dragStartObjPos{ 0.0f };

    // Store where the mouse-ray hit the drag plane at drag start
    // Compute mouse delta in world space.
    glm::vec3 dragStartHitPoint{ 0.0f };

    // Tuning values
    float axisPickThresholdPx = 10.0f; // how close mouse must be to an axis line (in pixels)
    float axisLineThickness = 3.0f;  // line thickness for axis draw

private:
    // Converts a world space position -> screen pixel position
    // Returns false if point is behind the camera / invalid.
    /// @brief Projects a world-space point to screen pixels. Returns false if behind the camera
    static bool worldToScreen(
        const glm::vec3& world,
        const glm::mat4& view,
        const glm::mat4& proj,
        int fbW, int fbH,
        glm::vec2& outScreen);

    // Distance from a point to a line segment in 2D.
    // Used for "hover detection" on the axis line.
    /// @brief Returns the minimum distance from point p to segment ab in 2D screen space
    static float distancePointToSegment2D(
        const glm::vec2& p,
        const glm::vec2& a,
        const glm::vec2& b);

    // Builds a ray from the mouse position through the camera:
    // rayOrigin = camera position
    // rayDir    = direction in world space (normalized)
    /// @brief Constructs a world-space ray from the current mouse position through the camera
    static bool buildMouseRay(
        GLFWwindow* window,
        int fbW, int fbH,
        const Camera& camera,
        glm::vec3& outOrigin,
        glm::vec3& outDir);

    // Intersects a ray with a plane.
    // Plane is defined by: point on plane + plane normal.
    /// @brief Intersects a ray with a plane defined by a point and normal. Returns false if parallel
    static bool rayPlaneIntersection(
        const glm::vec3& ro,
        const glm::vec3& rd,
        const glm::vec3& planePoint,
        const glm::vec3& planeNormal,
        glm::vec3& outHit);

    // Helper: returns unit direction for the chosen axis
    /// @brief Returns the unit direction vector for the given axis
    static glm::vec3 axisDir(Axis a);
};

