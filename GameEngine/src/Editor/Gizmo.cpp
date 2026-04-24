#include <GL/glew.h>
#include "../include/Editor/Gizmo.h"
#include <GLFW/glfw3.h>
#include "../External/imgui/core/imgui.h"
#include "../include/Rendering/Camera.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/Trigger.h"
#include "../include/Physics/ForceGenerator.h"
#include "../include/Rendering/DirectionalLight.h"
#include <glm/gtc/matrix_transform.hpp>
#include "../include/Rendering/PointLight.h"

/** @brief Default constructor. Initialises EditorGizmo with no active state. */
EditorGizmo::EditorGizmo() {}

/**
 * @brief Returns the unit direction vector for a given world axis.
 *
 * @param a The axis to query (X, Y, or Z).
 * @return glm::vec3 Unit vector along the given axis, or zero vector for Axis::None.
 */
glm::vec3 EditorGizmo::axisDir(Axis a)
{
    switch (a)
    {
    case Axis::X: return glm::vec3(1, 0, 0);
    case Axis::Y: return glm::vec3(0, 1, 0);
    case Axis::Z: return glm::vec3(0, 0, 1);
    default:      return glm::vec3(0, 0, 0);
    }
}

// worldToScreen():
// Converts world position -> clip space -> NDC -> screen pixels
// Clip = proj * view * world
// NDC = clip.xyz / clip.w
// screen.x = (ndc.x * 0.5 + 0.5) * width
// screen.y = (1 - (ndc.y * 0.5 + 0.5)) * height
// Returns false if clip.w <= 0 (behind camera)

/**
 * @brief Projects a world-space position into screen-space pixel coordinates.
 *
 * Applies the full MVP transform:
 *   clip  = proj * view * world
 *   NDC   = clip.xyz / clip.w
 *   px.x  = (ndc.x * 0.5 + 0.5) * fbW
 *   px.y  = (1 - (ndc.y * 0.5 + 0.5)) * fbH
 *
 * @param world    World-space position to project.
 * @param view     Camera view matrix.
 * @param proj     Camera projection matrix.
 * @param fbW      Framebuffer width in pixels.
 * @param fbH      Framebuffer height in pixels.
 * @param outScreen Output screen-space pixel coordinate.
 * @return true if the point is in front of the camera (clip.w > 0), false otherwise.
 */
bool EditorGizmo::worldToScreen(const glm::vec3& world,
    const glm::mat4& view,
    const glm::mat4& proj,
    int fbW, int fbH,
    glm::vec2& outScreen)
{
    glm::vec4 clip = proj * view * glm::vec4(world, 1.0f);

    // If w is <= 0, point is behind the camera (not drawable in a stable way)
    if (clip.w <= 0.00001f)
        return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    // Convert [-1..1] to pixels
    outScreen.x = (ndc.x * 0.5f + 0.5f) * fbW;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * fbH;
    return true;
}

// Distance from point P to segment AB in 2D.
// Used so that axis lines are clickable with a tolerance in pixels.

/**
 * @brief Computes the minimum 2D distance from a point to a line segment.
 *
 * Used to determine whether the mouse cursor is close enough to an axis
 * line to register as a hover/click, with a configurable pixel tolerance.
 *
 * @param p Point to test (e.g., mouse cursor position in screen pixels).
 * @param a Start of the line segment.
 * @param b End of the line segment.
 * @return float Shortest distance from p to the segment AB.
 */
float EditorGizmo::distancePointToSegment2D(const glm::vec2& p,
    const glm::vec2& a,
    const glm::vec2& b)
{
    glm::vec2 ab = b - a;
    float abLen2 = glm::dot(ab, ab);

    // If A and B are basically the same point, distance is to A
    if (abLen2 < 0.00001f)
        return glm::length(p - a);

    // Project P onto AB, get parameter t in [0..1]
    float t = glm::dot(p - a, ab) / abLen2;
    t = glm::clamp(t, 0.0f, 1.0f);

    glm::vec2 closest = a + t * ab;
    return glm::length(p - closest);
}

// buildMouseRay():
// Creates the same ray already done for picking:
// mouse pixels -> NDC -> eye -> world direction
// outOrigin = camera position
// outDir    = normalized world direction

/**
 * @brief Constructs a world-space ray from the current mouse cursor position.
 *
 * Converts the mouse pixel position through NDC and eye space back into
 * a world-space ray origin (camera position) and normalised direction.
 *
 *   NDC  = mouse pixels -> [-1, 1]
 *   eye  = inverse(proj) * NDC clip point
 *   dir  = normalise(inverse(view) * eye direction)
 *
 * @param window    Active GLFW window (used to query cursor position).
 * @param fbW       Framebuffer width in pixels.
 * @param fbH       Framebuffer height in pixels.
 * @param camera    Active scene camera.
 * @param outOrigin Output ray origin (camera world position).
 * @param outDir    Output normalised ray direction in world space.
 * @return true always (reserved for future failure cases).
 */
bool EditorGizmo::buildMouseRay(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    glm::vec3& outOrigin,
    glm::vec3& outDir)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // Screen -> NDC
    float x = (2.0f * static_cast<float>(mouseX)) / fbW - 1.0f;
    float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / fbH;

    // Mouse point in clip space on near plane (z = -1)
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    // Unproject to eye space
    glm::mat4 projection = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;

    // Convert from point to direction:
    // z = -1 (forward), w = 0 (direction)
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // Eye -> World
    glm::mat4 invView = glm::inverse(camera.getViewMatrix());
    outDir = glm::normalize(glm::vec3(invView * rayEye));
    outOrigin = camera.getPosition();

    return true;
}

// rayPlaneIntersection():
// Solve ray-plane intersection:
// plane: dot((P - planePoint), planeNormal) = 0
// ray:   P = ro + t * rd
//
// Substitute and solve for t:
// t = dot((planePoint - ro), planeNormal) / dot(rd, planeNormal)
//
// If denom is 0 => ray parallel to plane.

/**
 * @brief Computes the intersection point of a ray with an infinite plane.
 *
 * Solves:  dot((P - planePoint), planeNormal) = 0
 *          P = ro + t * rd
 *
 * Substituting and rearranging:
 *   t = dot(planePoint - ro, planeNormal) / dot(rd, planeNormal)
 *
 * @param ro         Ray origin in world space.
 * @param rd         Normalised ray direction in world space.
 * @param planePoint Any point on the plane.
 * @param planeNormal Normal vector of the plane (need not be unit length).
 * @param outHit     Output intersection point if the ray hits the plane.
 * @return true if an intersection exists (ray not parallel to plane and hit is in front of origin).
 */
bool EditorGizmo::rayPlaneIntersection(const glm::vec3& ro,
    const glm::vec3& rd,
    const glm::vec3& planePoint,
    const glm::vec3& planeNormal,
    glm::vec3& outHit)
{
    float denom = glm::dot(rd, planeNormal);
    if (std::abs(denom) < 0.00001f)
        return false;

    float t = glm::dot(planePoint - ro, planeNormal) / denom;

    // negative t means intersection behind ray origin
    if (t < 0.0f)
        return false;

    outHit = ro + rd * t;
    return true;
}

// update():
// Handles:
// - which axis is hovered
// - click on axis starts drag
// - while dragging: compute new hit point on drag plane,
//   project delta onto axis, and set object position.

/**
 * @brief Processes per-frame gizmo interaction for a selected GameObject.
 *
 * Handles the full gizmo lifecycle each frame:
 *  - Hover detection: projects axis line segments to screen and tests mouse proximity.
 *  - Drag start: on click, selects the hot axis and constructs a stable drag plane whose
 *    normal is chosen by finding the camera vector most perpendicular to the active axis.
 *  - Drag update: casts a ray to the drag plane each frame, projects the world-space delta
 *    onto the active axis, and updates the object's position accordingly.
 *  - Drag end: releases state when the mouse button is released.
 *
 * @param window         Active GLFW window.
 * @param fbW            Framebuffer width in pixels.
 * @param fbH            Framebuffer height in pixels.
 * @param camera         Active scene camera.
 * @param selectedObject The GameObject to be moved.
 * @param editorMode     Whether the editor is in edit mode; gizmo is inactive otherwise.
 * @param uiWantsMouse   True if ImGui is consuming mouse input; prevents drag start.
 * @return true if the gizmo is actively consuming mouse input (dragging or just started drag).
 */
bool EditorGizmo::update(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    GameObject* selectedObject,
    bool editorMode,
    bool uiWantsMouse)
{
    // Reset hovered axis each frame
    hotAxis = Axis::None;

    if (fbW == 0 || fbH == 0) return false;
    // not in editor mode or no selection exists,
    // stop any dragging and do nothing.
    if (!editorMode || !selectedObject)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    // Prepare matrices (for projection to screen space)
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    // Gizmo origin is the selected object's position
    glm::vec3 originW = selectedObject->getPosition();

    // Project origin to screen
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return false;

    // Choose a world-space length for axes.
    // Scale it based on distance to camera so the gizmo
    // stays about the same size on screen.
    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    // Compute world endpoints for each axis
    glm::vec3 xEndW = originW + glm::vec3(1, 0, 0) * axisLenWorld;
    glm::vec3 yEndW = originW + glm::vec3(0, 1, 0) * axisLenWorld;
    glm::vec3 zEndW = originW + glm::vec3(0, 0, 1) * axisLenWorld;

    // Project endpoints to screen
    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(xEndW, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(yEndW, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(zEndW, view, proj, fbW, fbH, zEndS);

    // Current mouse position in screen pixels
    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec2 mouseS(mouse.x, mouse.y);

    // Hover test:
    // Compute distance from mouse to each axis line segment on screen.
    // Pick the closest axis within threshold.
    float best = axisPickThresholdPx;

    if (okX)
    {
        float d = distancePointToSegment2D(mouseS, originS, xEndS);
        if (d < best) { best = d; hotAxis = Axis::X; }
    }
    if (okY)
    {
        float d = distancePointToSegment2D(mouseS, originS, yEndS);
        if (d < best) { best = d; hotAxis = Axis::Y; }
    }
    if (okZ)
    {
        float d = distancePointToSegment2D(mouseS, originS, zEndS);
        if (d < best) { best = d; hotAxis = Axis::Z; }
    }

    // Mouse state from ImGui
    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // -----------------------------
    // Begin drag
    // -----------------------------
    if (!dragging)
    {
        // Don’t start dragging if ImGui UI is already using the mouse
        // (clicking a slider, window, etc.)
        if (!uiWantsMouse && mouseClicked && hotAxis != Axis::None)
        {
            activeAxis = hotAxis;
            dragging = true;

            // Store starting object pos so movement is relative
            dragStartObjPos = selectedObject->getPosition();

            // Define the drag plane:
            // Want to move along ONE axis, but mouse movement is 2D.
            // Create a plane that:
            //  - goes through the object position
            //  - is oriented so ray hits produce stable movement along the axis
            
            // planeNormal = normalize(cross(axis, cameraForward))
            // That produces a plane that contains the axis and faces the camera.
            glm::vec3 axis = axisDir(activeAxis);
            glm::vec3 camF = camera.getFront();
            glm::vec3 camR = camera.getRight();
            glm::vec3 camU = camera.getUp();

            // Choose the most perpendicular camera vector to the axis
            // This prevents the plane from being edge-on to the camera
            float dotF = std::abs(glm::dot(axis, camF));
            float dotR = std::abs(glm::dot(axis, camR));
            float dotU = std::abs(glm::dot(axis, camU));

            glm::vec3 perpVec;
            if (dotF < dotR && dotF < dotU) {
                perpVec = camF;  // Forward is most perpendicular
            }
            else if (dotR < dotU) {
                perpVec = camR;  // Right is most perpendicular
            }
            else {
                perpVec = camU;  // Up is most perpendicular
            }

            // Create plane normal from axis × perpendicular vector
            glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

            // Ensure plane faces camera (dot product with camera forward should be positive)
            if (glm::dot(planeN, -camF) < 0.0f) {
                planeN = -planeN;
            }


            // Ray from mouse
            glm::vec3 ro, rd, hit;
            buildMouseRay(window, fbW, fbH, camera, ro, rd);

            // Find where the ray hits our drag plane
            if (!rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
            {
                // If no hit, cancel dragging
                dragging = false;
                activeAxis = Axis::None;
                return false;
            }

            // Store initial hit point to compare later
            dragStartHitPoint = hit;

            // Return true: gizmo is now capturing mouse
            return true;
        }

        // Not dragging yet
        return false;
    }

    // -----------------------------
    // End drag
    // -----------------------------
    if (dragging && !mouseDown)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    // -----------------------------
    // Drag update
    // -----------------------------
    if (dragging)
    {
        glm::vec3 axis = axisDir(activeAxis);
        glm::vec3 camF = camera.getFront();
        glm::vec3 camR = camera.getRight();
        glm::vec3 camU = camera.getUp();

        // Choose most perpendicular camera vector
        float dotF = std::abs(glm::dot(axis, camF));
        float dotR = std::abs(glm::dot(axis, camR));
        float dotU = std::abs(glm::dot(axis, camU));

        glm::vec3 perpVec;
        if (dotF < dotR && dotF < dotU) {
            perpVec = camF;
        }
        else if (dotR < dotU) {
            perpVec = camR;
        }
        else {
            perpVec = camU;
        }

        glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

        if (glm::dot(planeN, -camF) < 0.0f) {
            planeN = -planeN;
        }

        // Build ray from current mouse position
        glm::vec3 ro, rd, hit;
        buildMouseRay(window, fbW, fbH, camera, ro, rd);

        // Find new hit point on plane
        if (rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
        {
            // Delta in world space between current mouse-plane hit
            // and the starting hit
            glm::vec3 delta = hit - dragStartHitPoint;

            // Project that delta onto the active axis:
            // t = dot(delta, axis)
            // This gives how far along the axis has been moved.
            float t = glm::dot(delta, axis);

            // New object position:
            // start position + axis * amount
            glm::vec3 newPos = dragStartObjPos + axis * t;
            selectedObject->setPosition(newPos);
        }

        // While dragging capture the mouse
        return true;
    }

    return false;
}

// draw():
// Renders axis lines as overlay using ImGui foreground draw list.
// This avoids needing any OpenGL rendering changes.

/**
 * @brief Renders the translation gizmo for a selected GameObject as an ImGui overlay.
 *
 * Draws three colour-coded axis lines (X = red, Y = green, Z = blue) and a centre dot
 * using ImGui's background draw list so no OpenGL state changes are required.
 * Hovered and active axes are tinted yellow to provide visual feedback.
 * Axis length is scaled by camera distance to maintain a consistent on-screen size.
 *
 * @param fbW            Framebuffer width in pixels.
 * @param fbH            Framebuffer height in pixels.
 * @param camera         Active scene camera.
 * @param selectedObject The GameObject whose gizmo should be drawn.
 */
void EditorGizmo::draw(int fbW, int fbH, const Camera& camera, GameObject* selectedObject)
{
    if (!selectedObject) return;
    if (fbW == 0 || fbH == 0) return;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = selectedObject->getPosition();
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return;

    // Pick axis length in world units based on camera distance
    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    // Screen endpoints
    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(originW + glm::vec3(1, 0, 0) * axisLenWorld, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(originW + glm::vec3(0, 1, 0) * axisLenWorld, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(originW + glm::vec3(0, 0, 1) * axisLenWorld, view, proj, fbW, fbH, zEndS);

    // Draw on top of everything
    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    // Choose colors per axis:
    // X = red, Y = green, Z = blue
    // If hovered or active, tint to yellow-ish to show feedback.
    auto axisColor = [&](Axis a) -> ImU32
        {
            ImU32 base =
                (a == Axis::X) ? IM_COL32(230, 80, 80, 255) :
                (a == Axis::Y) ? IM_COL32(80, 230, 80, 255) :
                (a == Axis::Z) ? IM_COL32(80, 140, 230, 255) :
                IM_COL32(255, 255, 255, 255);

            // highlight if hovered or active
            if (dragging && a == activeAxis) return IM_COL32(255, 255, 180, 255);
            if (!dragging && a == hotAxis)   return IM_COL32(255, 255, 180, 255);
            return base;
        };

    // Draw each axis line
    if (okX)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(xEndS.x, xEndS.y), axisColor(Axis::X), axisLineThickness);
    if (okY)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(yEndS.x, yEndS.y), axisColor(Axis::Y), axisLineThickness);
    if (okZ)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(zEndS.x, zEndS.y), axisColor(Axis::Z), axisLineThickness);

    // Draw a small center dot so origin is visible
    dl->AddCircleFilled(ImVec2(originS.x, originS.y), 4.0f, IM_COL32(240, 240, 240, 255));
}

// Same logic as GameObject gizmo but operates on Trigger instead.
// We duplicate instead of abstracting to avoid modifying existing systems.

/**
 * @brief Processes per-frame gizmo interaction for a selected Trigger.
 *
 * Identical drag-plane interaction logic to the GameObject overload, but
 * operates on a Trigger object. Kept as a separate overload rather than a
 * template to avoid modifying existing engine systems.
 *
 * @param window          Active GLFW window.
 * @param fbW             Framebuffer width in pixels.
 * @param fbH             Framebuffer height in pixels.
 * @param camera          Active scene camera.
 * @param selectedTrigger The Trigger to be moved.
 * @param editorMode      Whether the editor is in edit mode.
 * @param uiWantsMouse    True if ImGui is consuming mouse input.
 * @return true if the gizmo is actively consuming mouse input.
 */
bool EditorGizmo::update(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    Trigger* selectedTrigger,
    bool editorMode,
    bool uiWantsMouse)
{
    hotAxis = Axis::None;

    if (fbW == 0 || fbH == 0) return false;
    // If no trigger selected or not in editor mode, stop interaction
    if (!editorMode || !selectedTrigger)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    // Use trigger position instead of GameObject
    glm::vec3 originW = selectedTrigger->getPosition();

    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return false;

    // Keep gizmo size consistent on screen
    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    // Build axis endpoints
    glm::vec3 xEndW = originW + glm::vec3(1, 0, 0) * axisLenWorld;
    glm::vec3 yEndW = originW + glm::vec3(0, 1, 0) * axisLenWorld;
    glm::vec3 zEndW = originW + glm::vec3(0, 0, 1) * axisLenWorld;

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(xEndW, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(yEndW, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(zEndW, view, proj, fbW, fbH, zEndS);

    // Mouse position
    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec2 mouseS(mouse.x, mouse.y);

    float best = axisPickThresholdPx;

    // Detect hovered axis
    if (okX)
    {
        float d = distancePointToSegment2D(mouseS, originS, xEndS);
        if (d < best) { best = d; hotAxis = Axis::X; }
    }
    if (okY)
    {
        float d = distancePointToSegment2D(mouseS, originS, yEndS);
        if (d < best) { best = d; hotAxis = Axis::Y; }
    }
    if (okZ)
    {
        float d = distancePointToSegment2D(mouseS, originS, zEndS);
        if (d < best) { best = d; hotAxis = Axis::Z; }
    }

    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // Start dragging
    if (!dragging)
    {
        if (!uiWantsMouse && mouseClicked && hotAxis != Axis::None)
        {
            activeAxis = hotAxis;
            dragging = true;

            // Store starting trigger position
            dragStartObjPos = selectedTrigger->getPosition();

            glm::vec3 axis = axisDir(activeAxis);
            glm::vec3 camF = camera.getFront();
            glm::vec3 camR = camera.getRight();
            glm::vec3 camU = camera.getUp();

            // Pick a stable plane for dragging
            float dotF = std::abs(glm::dot(axis, camF));
            float dotR = std::abs(glm::dot(axis, camR));
            float dotU = std::abs(glm::dot(axis, camU));

            glm::vec3 perpVec;
            if (dotF < dotR && dotF < dotU) perpVec = camF;
            else if (dotR < dotU) perpVec = camR;
            else perpVec = camU;

            glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

            // Ensure plane faces camera
            if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

            glm::vec3 ro, rd, hit;
            buildMouseRay(window, fbW, fbH, camera, ro, rd);

            if (!rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
            {
                dragging = false;
                activeAxis = Axis::None;
                return false;
            }

            // Store initial mouse-plane hit
            dragStartHitPoint = hit;
            return true;
        }

        return false;
    }

    // Stop dragging
    if (dragging && !mouseDown)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    // Drag update
    if (dragging)
    {
        glm::vec3 axis = axisDir(activeAxis);
        glm::vec3 camF = camera.getFront();
        glm::vec3 camR = camera.getRight();
        glm::vec3 camU = camera.getUp();

        float dotF = std::abs(glm::dot(axis, camF));
        float dotR = std::abs(glm::dot(axis, camR));
        float dotU = std::abs(glm::dot(axis, camU));

        glm::vec3 perpVec;
        if (dotF < dotR && dotF < dotU) perpVec = camF;
        else if (dotR < dotU) perpVec = camR;
        else perpVec = camU;

        glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

        if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

        glm::vec3 ro, rd, hit;
        buildMouseRay(window, fbW, fbH, camera, ro, rd);

        if (rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
        {
            glm::vec3 delta = hit - dragStartHitPoint;

            float t = glm::dot(delta, axis);

            glm::vec3 newPos = dragStartObjPos + axis * t;

            // Move trigger instead of GameObject
            selectedTrigger->setPosition(newPos);
        }

        return true;
    }

    return false;
}

// Draw gizmo for trigger using same visual logic as GameObject

/**
 * @brief Renders the translation gizmo for a selected Trigger as an ImGui overlay.
 *
 * Uses the same visual style as the GameObject overload (colour-coded axis lines,
 * yellow hover/active highlight, centre dot). Kept separate to avoid modifying
 * existing engine systems.
 *
 * @param fbW             Framebuffer width in pixels.
 * @param fbH             Framebuffer height in pixels.
 * @param camera          Active scene camera.
 * @param selectedTrigger The Trigger whose gizmo should be drawn.
 */
void EditorGizmo::draw(int fbW, int fbH, const Camera& camera, Trigger* selectedTrigger)
{
    if (!selectedTrigger) return;
    if (fbW == 0 || fbH == 0) return;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    // Use trigger position
    glm::vec3 originW = selectedTrigger->getPosition();
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return;

    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(originW + glm::vec3(1, 0, 0) * axisLenWorld, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(originW + glm::vec3(0, 1, 0) * axisLenWorld, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(originW + glm::vec3(0, 0, 1) * axisLenWorld, view, proj, fbW, fbH, zEndS);

    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    auto axisColor = [&](Axis a) -> ImU32
        {
            ImU32 base =
                (a == Axis::X) ? IM_COL32(230, 80, 80, 255) :
                (a == Axis::Y) ? IM_COL32(80, 230, 80, 255) :
                (a == Axis::Z) ? IM_COL32(80, 140, 230, 255) :
                IM_COL32(255, 255, 255, 255);

            if (dragging && a == activeAxis) return IM_COL32(255, 255, 180, 255);
            if (!dragging && a == hotAxis)   return IM_COL32(255, 255, 180, 255);
            return base;
        };

    if (okX)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(xEndS.x, xEndS.y), axisColor(Axis::X), axisLineThickness);
    if (okY)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(yEndS.x, yEndS.y), axisColor(Axis::Y), axisLineThickness);
    if (okZ)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(zEndS.x, zEndS.y), axisColor(Axis::Z), axisLineThickness);

    dl->AddCircleFilled(ImVec2(originS.x, originS.y), 4.0f, IM_COL32(240, 240, 240, 255));
}

// =======================================================
// FORCE GENERATOR GIZMO
// =======================================================

// Same logic as Trigger gizmo but operates on ForceGenerator instead.

/**
 * @brief Processes per-frame gizmo interaction for a selected ForceGenerator.
 *
 * Identical drag-plane interaction logic to the Trigger overload, but
 * operates on a ForceGenerator object. Kept as a separate overload rather
 * than a template to avoid modifying existing engine systems.
 *
 * @param window                 Active GLFW window.
 * @param fbW                    Framebuffer width in pixels.
 * @param fbH                    Framebuffer height in pixels.
 * @param camera                 Active scene camera.
 * @param selectedForceGenerator The ForceGenerator to be moved.
 * @param editorMode             Whether the editor is in edit mode.
 * @param uiWantsMouse           True if ImGui is consuming mouse input.
 * @return true if the gizmo is actively consuming mouse input.
 */
bool EditorGizmo::update(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    ForceGenerator* selectedForceGenerator,
    bool editorMode,
    bool uiWantsMouse)
{
    hotAxis = Axis::None;

    if (fbW == 0 || fbH == 0) return false;
    // If no generator selected or not in editor mode, stop interaction
    if (!editorMode || !selectedForceGenerator)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = selectedForceGenerator->getPosition();

    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return false;

    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    glm::vec3 xEndW = originW + glm::vec3(1, 0, 0) * axisLenWorld;
    glm::vec3 yEndW = originW + glm::vec3(0, 1, 0) * axisLenWorld;
    glm::vec3 zEndW = originW + glm::vec3(0, 0, 1) * axisLenWorld;

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(xEndW, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(yEndW, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(zEndW, view, proj, fbW, fbH, zEndS);

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec2 mouseS(mouse.x, mouse.y);

    float best = axisPickThresholdPx;

    if (okX)
    {
        float d = distancePointToSegment2D(mouseS, originS, xEndS);
        if (d < best) { best = d; hotAxis = Axis::X; }
    }
    if (okY)
    {
        float d = distancePointToSegment2D(mouseS, originS, yEndS);
        if (d < best) { best = d; hotAxis = Axis::Y; }
    }
    if (okZ)
    {
        float d = distancePointToSegment2D(mouseS, originS, zEndS);
        if (d < best) { best = d; hotAxis = Axis::Z; }
    }

    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (!dragging)
    {
        if (!uiWantsMouse && mouseClicked && hotAxis != Axis::None)
        {
            activeAxis = hotAxis;
            dragging = true;

            dragStartObjPos = selectedForceGenerator->getPosition();

            glm::vec3 axis = axisDir(activeAxis);
            glm::vec3 camF = camera.getFront();
            glm::vec3 camR = camera.getRight();
            glm::vec3 camU = camera.getUp();

            float dotF = std::abs(glm::dot(axis, camF));
            float dotR = std::abs(glm::dot(axis, camR));
            float dotU = std::abs(glm::dot(axis, camU));

            glm::vec3 perpVec;
            if (dotF < dotR && dotF < dotU) perpVec = camF;
            else if (dotR < dotU) perpVec = camR;
            else perpVec = camU;

            glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

            if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

            glm::vec3 ro, rd, hit;
            buildMouseRay(window, fbW, fbH, camera, ro, rd);

            if (!rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
            {
                dragging = false;
                activeAxis = Axis::None;
                return false;
            }

            dragStartHitPoint = hit;
            return true;
        }

        return false;
    }

    if (dragging && !mouseDown)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    if (dragging)
    {
        glm::vec3 axis = axisDir(activeAxis);
        glm::vec3 camF = camera.getFront();
        glm::vec3 camR = camera.getRight();
        glm::vec3 camU = camera.getUp();

        float dotF = std::abs(glm::dot(axis, camF));
        float dotR = std::abs(glm::dot(axis, camR));
        float dotU = std::abs(glm::dot(axis, camU));

        glm::vec3 perpVec;
        if (dotF < dotR && dotF < dotU) perpVec = camF;
        else if (dotR < dotU) perpVec = camR;
        else perpVec = camU;

        glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));

        if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

        glm::vec3 ro, rd, hit;
        buildMouseRay(window, fbW, fbH, camera, ro, rd);

        if (rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
        {
            glm::vec3 delta = hit - dragStartHitPoint;

            float t = glm::dot(delta, axis);

            glm::vec3 newPos = dragStartObjPos + axis * t;

            // Move force generator
            selectedForceGenerator->setPosition(newPos);
        }

        return true;
    }

    return false;
}


// Draw gizmo for ForceGenerator

/**
 * @brief Renders the translation gizmo for a selected ForceGenerator as an ImGui overlay.
 *
 * Uses the same visual style as the other object gizmo overloads (colour-coded
 * axis lines, yellow hover/active highlight, centre dot).
 *
 * @param fbW                    Framebuffer width in pixels.
 * @param fbH                    Framebuffer height in pixels.
 * @param camera                 Active scene camera.
 * @param selectedForceGenerator The ForceGenerator whose gizmo should be drawn.
 */
void EditorGizmo::draw(int fbW, int fbH, const Camera& camera, ForceGenerator* selectedForceGenerator)
{
    if (!selectedForceGenerator) return;
    if (fbW == 0 || fbH == 0) return;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = selectedForceGenerator->getPosition();
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return;

    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(originW + glm::vec3(1, 0, 0) * axisLenWorld, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(originW + glm::vec3(0, 1, 0) * axisLenWorld, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(originW + glm::vec3(0, 0, 1) * axisLenWorld, view, proj, fbW, fbH, zEndS);

    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    auto axisColor = [&](Axis a) -> ImU32
        {
            ImU32 base =
                (a == Axis::X) ? IM_COL32(230, 80, 80, 255) :
                (a == Axis::Y) ? IM_COL32(80, 230, 80, 255) :
                (a == Axis::Z) ? IM_COL32(80, 140, 230, 255) :
                IM_COL32(255, 255, 255, 255);

            if (dragging && a == activeAxis) return IM_COL32(255, 255, 180, 255);
            if (!dragging && a == hotAxis)   return IM_COL32(255, 255, 180, 255);
            return base;
        };

    if (okX)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(xEndS.x, xEndS.y), axisColor(Axis::X), axisLineThickness);
    if (okY)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(yEndS.x, yEndS.y), axisColor(Axis::Y), axisLineThickness);
    if (okZ)
        dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(zEndS.x, zEndS.y), axisColor(Axis::Z), axisLineThickness);

    dl->AddCircleFilled(ImVec2(originS.x, originS.y), 4.0f, IM_COL32(240, 240, 240, 255));
}

// Fixed sky position where the gizmo lives
static const glm::vec3 LIGHT_GIZMO_ORIGIN = glm::vec3(0.0f, 15.0f, 0.0f);
// Distance from the gizmo origin to the draggable direction handle in world units.
static const float LIGHT_GIZMO_HANDLE_DIST = 5.0f;

/**
 * @brief Processes per-frame gizmo interaction for a DirectionalLight.
 *
 * Unlike the translation gizmos, this gizmo adjusts the light's direction rather
 * than a position. A single draggable handle sits at a fixed sky-space origin offset
 * by the current light direction. Dragging the handle updates the light direction to
 * point from the fixed gizmo origin toward the new handle position.
 *
 * The drag plane always faces the camera (normal = -cameraFront), which keeps
 * movement intuitive regardless of the current view angle.
 *
 * @param window       Active GLFW window.
 * @param fbW          Framebuffer width in pixels.
 * @param fbH          Framebuffer height in pixels.
 * @param camera       Active scene camera.
 * @param light        The DirectionalLight whose direction should be edited.
 * @param editorMode   Whether the editor is in edit mode.
 * @param uiWantsMouse True if ImGui is consuming mouse input.
 * @return true if the gizmo is actively consuming mouse input.
 */
bool EditorGizmo::update(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    DirectionalLight* light,
    bool editorMode,
    bool uiWantsMouse)
{
    hotAxis = Axis::None;

    if (fbW == 0 || fbH == 0) return false;

    if (!editorMode || !light)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    // Handle sits at origin + direction * distance
    glm::vec3 handleW = LIGHT_GIZMO_ORIGIN + glm::normalize(light->getDirection()) * LIGHT_GIZMO_HANDLE_DIST;

    glm::vec2 handleS;
    if (!worldToScreen(handleW, view, proj, fbW, fbH, handleS))
        return false;

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec2 mouseS(mouse.x, mouse.y);

    // Hover: check if mouse is near the handle circle
    float distToHandle = glm::length(mouseS - handleS);
    bool hovering = distToHandle < 12.0f;  // pixel radius for hit

    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (!lightDragging)
    {
        if (!uiWantsMouse && mouseClicked && hovering)
        {
            lightDragging = true;

            dragStartObjPos = handleW;

            // Drag plane faces camera, passes through handle
            glm::vec3 planeN = -camera.getFront();
            glm::vec3 ro, rd, hit;
            buildMouseRay(window, fbW, fbH, camera, ro, rd);

            if (!rayPlaneIntersection(ro, rd, handleW, planeN, hit))
            {
                lightDragging = false;
                activeAxis = Axis::None;
                return false;
            }

            dragStartHitPoint = hit;
            return true;
        }
        return false;
    }

    if (lightDragging && !mouseDown)
    {
        lightDragging = false;
        activeAxis = Axis::None;
        return false;
    }

    if (lightDragging)
    {
        glm::vec3 planeN = -camera.getFront();
        glm::vec3 ro, rd, hit;
        buildMouseRay(window, fbW, fbH, camera, ro, rd);

        if (rayPlaneIntersection(ro, rd, dragStartHitPoint, planeN, hit))
        {
            // New direction = from gizmo origin to where the handle was dragged
            glm::vec3 newDir = hit - LIGHT_GIZMO_ORIGIN;
            if (glm::length(newDir) > 0.001f)
            {
                light->setDirection(glm::normalize(newDir));
            }
        }

        return true;
    }

    return false;
}

/**
 * @brief Renders the direction gizmo for a DirectionalLight as an ImGui overlay.
 *
 * Draws a yellow arrow line from a fixed sky-space origin to a draggable circular
 * handle positioned along the light's current direction. A "SUN" text label is
 * drawn beside the handle for clarity. The handle brightens when being dragged.
 *
 * @param fbW    Framebuffer width in pixels.
 * @param fbH    Framebuffer height in pixels.
 * @param camera Active scene camera.
 * @param light  The DirectionalLight whose gizmo should be drawn.
 */
void EditorGizmo::draw(int fbW, int fbH, const Camera& camera, DirectionalLight* light)
{
    if (!light) return;
    if (fbW == 0 || fbH == 0) return;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = LIGHT_GIZMO_ORIGIN;
    glm::vec3 handleW = originW + glm::normalize(light->getDirection()) * LIGHT_GIZMO_HANDLE_DIST;

    glm::vec2 originS, handleS;
    bool okO = worldToScreen(originW, view, proj, fbW, fbH, originS);
    bool okH = worldToScreen(handleW, view, proj, fbW, fbH, handleS);

    if (!okO || !okH) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    // Yellow arrow line for the sun
    ImU32 col = lightDragging ? IM_COL32(255, 255, 100, 255) : IM_COL32(255, 220, 50, 255);
    dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(handleS.x, handleS.y), col, 2.5f);

    // Small circle at origin
    dl->AddCircleFilled(ImVec2(originS.x, originS.y), 4.0f, IM_COL32(255, 220, 50, 180));

    // Draggable handle circle at tip
    dl->AddCircleFilled(ImVec2(handleS.x, handleS.y), 8.0f, col);
    dl->AddCircle(ImVec2(handleS.x, handleS.y), 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);

    // Label
    dl->AddText(ImVec2(handleS.x + 10.0f, handleS.y - 8.0f), IM_COL32(255, 220, 50, 255), "SUN");
}

/**
 * @brief Processes per-frame gizmo interaction for a selected PointLight.
 *
 * Uses the same axis-constrained drag-plane approach as the GameObject and
 * Trigger overloads, but moves the PointLight's world position instead.
 *
 * @param window       Active GLFW window.
 * @param fbW          Framebuffer width in pixels.
 * @param fbH          Framebuffer height in pixels.
 * @param camera       Active scene camera.
 * @param light        The PointLight to be moved.
 * @param editorMode   Whether the editor is in edit mode.
 * @param uiWantsMouse True if ImGui is consuming mouse input.
 * @return true if the gizmo is actively consuming mouse input.
 */
bool EditorGizmo::update(GLFWwindow* window,
    int fbW, int fbH,
    const Camera& camera,
    PointLight* light,
    bool editorMode,
    bool uiWantsMouse)
{
    hotAxis = Axis::None;
    if (fbW == 0 || fbH == 0) return false;


    if (!editorMode || !light)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = light->getPosition();
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return false;

    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    glm::vec3 xEndW = originW + glm::vec3(1, 0, 0) * axisLenWorld;
    glm::vec3 yEndW = originW + glm::vec3(0, 1, 0) * axisLenWorld;
    glm::vec3 zEndW = originW + glm::vec3(0, 0, 1) * axisLenWorld;

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(xEndW, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(yEndW, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(zEndW, view, proj, fbW, fbH, zEndS);

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec2 mouseS(mouse.x, mouse.y);

    float best = axisPickThresholdPx;

    if (okX) { float d = distancePointToSegment2D(mouseS, originS, xEndS); if (d < best) { best = d; hotAxis = Axis::X; } }
    if (okY) { float d = distancePointToSegment2D(mouseS, originS, yEndS); if (d < best) { best = d; hotAxis = Axis::Y; } }
    if (okZ) { float d = distancePointToSegment2D(mouseS, originS, zEndS); if (d < best) { best = d; hotAxis = Axis::Z; } }

    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (!dragging)
    {
        if (!uiWantsMouse && mouseClicked && hotAxis != Axis::None)
        {
            activeAxis = hotAxis;
            dragging = true;
            dragStartObjPos = light->getPosition();

            glm::vec3 axis = axisDir(activeAxis);
            glm::vec3 camF = camera.getFront();
            glm::vec3 camR = camera.getRight();
            glm::vec3 camU = camera.getUp();

            float dotF = std::abs(glm::dot(axis, camF));
            float dotR = std::abs(glm::dot(axis, camR));
            float dotU = std::abs(glm::dot(axis, camU));

            glm::vec3 perpVec;
            if (dotF < dotR && dotF < dotU) perpVec = camF;
            else if (dotR < dotU) perpVec = camR;
            else perpVec = camU;

            glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));
            if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

            glm::vec3 ro, rd, hit;
            buildMouseRay(window, fbW, fbH, camera, ro, rd);

            if (!rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
            {
                dragging = false;
                activeAxis = Axis::None;
                return false;
            }

            dragStartHitPoint = hit;
            return true;
        }
        return false;
    }

    if (dragging && !mouseDown)
    {
        dragging = false;
        activeAxis = Axis::None;
        return false;
    }

    if (dragging)
    {
        glm::vec3 axis = axisDir(activeAxis);
        glm::vec3 camF = camera.getFront();
        glm::vec3 camR = camera.getRight();
        glm::vec3 camU = camera.getUp();

        float dotF = std::abs(glm::dot(axis, camF));
        float dotR = std::abs(glm::dot(axis, camR));
        float dotU = std::abs(glm::dot(axis, camU));

        glm::vec3 perpVec;
        if (dotF < dotR && dotF < dotU) perpVec = camF;
        else if (dotR < dotU) perpVec = camR;
        else perpVec = camU;

        glm::vec3 planeN = glm::normalize(glm::cross(axis, perpVec));
        if (glm::dot(planeN, -camF) < 0.0f) planeN = -planeN;

        glm::vec3 ro, rd, hit;
        buildMouseRay(window, fbW, fbH, camera, ro, rd);

        if (rayPlaneIntersection(ro, rd, dragStartObjPos, planeN, hit))
        {
            glm::vec3 delta = hit - dragStartHitPoint;
            float t = glm::dot(delta, axis);
            light->setPosition(dragStartObjPos + axis * t);
        }

        return true;
    }

    return false;
}

/**
 * @brief Renders the translation gizmo for a selected PointLight as an ImGui overlay.
 *
 * Draws the same colour-coded axis lines as the other object gizmos, but
 * replaces the plain centre dot with a filled circle coloured to match the
 * light's RGB colour, making it easy to identify the light source at a glance.
 *
 * @param fbW    Framebuffer width in pixels.
 * @param fbH    Framebuffer height in pixels.
 * @param camera Active scene camera.
 * @param light  The PointLight whose gizmo should be drawn.
 */
void EditorGizmo::draw(int fbW, int fbH, const Camera& camera, PointLight* light)
{
    if (!light) return;
    if (fbW == 0 || fbH == 0) return;

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(static_cast<float>(fbW) / fbH);

    glm::vec3 originW = light->getPosition();
    glm::vec2 originS;
    if (!worldToScreen(originW, view, proj, fbW, fbH, originS))
        return;

    float distToCam = glm::length(camera.getPosition() - originW);
    float axisLenWorld = glm::clamp(distToCam * 0.15f, 0.5f, 6.0f);

    glm::vec2 xEndS, yEndS, zEndS;
    bool okX = worldToScreen(originW + glm::vec3(1, 0, 0) * axisLenWorld, view, proj, fbW, fbH, xEndS);
    bool okY = worldToScreen(originW + glm::vec3(0, 1, 0) * axisLenWorld, view, proj, fbW, fbH, yEndS);
    bool okZ = worldToScreen(originW + glm::vec3(0, 0, 1) * axisLenWorld, view, proj, fbW, fbH, zEndS);

    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    auto axisColor = [&](Axis a) -> ImU32 {
        ImU32 base =
            (a == Axis::X) ? IM_COL32(230, 80, 80, 255) :
            (a == Axis::Y) ? IM_COL32(80, 230, 80, 255) :
            (a == Axis::Z) ? IM_COL32(80, 140, 230, 255) :
            IM_COL32(255, 255, 255, 255);
        if (dragging && a == activeAxis) return IM_COL32(255, 255, 180, 255);
        if (!dragging && a == hotAxis)   return IM_COL32(255, 255, 180, 255);
        return base;
        };

    if (okX) dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(xEndS.x, xEndS.y), axisColor(Axis::X), axisLineThickness);
    if (okY) dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(yEndS.x, yEndS.y), axisColor(Axis::Y), axisLineThickness);
    if (okZ) dl->AddLine(ImVec2(originS.x, originS.y), ImVec2(zEndS.x, zEndS.y), axisColor(Axis::Z), axisLineThickness);

    // Draw a yellow circle to represent the light source
    glm::vec3 col = light->getColour();
    ImU32 lightCol = IM_COL32(
        (int)(col.r * 255),
        (int)(col.g * 255),
        (int)(col.b * 255),
        255);
    dl->AddCircleFilled(ImVec2(originS.x, originS.y), 6.0f, lightCol);
    dl->AddCircle(ImVec2(originS.x, originS.y), 6.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
}