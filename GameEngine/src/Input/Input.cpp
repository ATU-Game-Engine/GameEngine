#include "../include/Input/Input.h"
#include "../include/Input/CameraController.h"
#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/core/imgui.h"


// keysDown[key]     → true while key is being held
// keysPressed[key]  → true for ONE frame when key transitions RELEASED → PRESSED
// keysReleased[key] → true for ONE frame when key transitions PRESSED → RELEASED
// They are reset every frame for pressed/released transitions.
// 1024 stores key states

// @brief Tracks whether each key is currently held down
static bool keysDown[1024] = { false }; //is currently down
// @brief True for exactly one frame when a key transitions from up to down
static bool keysPressed[1024] = { false };//was pressed this frame
// @brief True for exactly one frame when a key transitions from down to up
static bool keysReleased[1024] = { false }; //was released this frame

// @brief Tracks whether each mouse button is currently held down
static bool mouseButtonsDown[8] = { false }; //is currently down
// @brief True for exactly one frame when a mouse button transitions from up to down.
static bool mouseButtonsPressed[8] = { false }; //was pressed this frame
// @brief True for exactly one frame when a mouse button transitions from down to up.
static bool mouseButtonsReleased[8] = { false }; //was released this frame

// GLFW window pointer, stored so the input module knows which window it belongs to.
// It’s the Input system’s saved reference to the game window.
// Without it, we can’t read input from GLFW.
// @brief GLFW window handle registered via Input::Initialize(). Required to read input events
static GLFWwindow* internalWindow = nullptr;

// Pointer to active CameraController
// @brief Optional CameraController that receives forwarded mouse-move events in editor mode
static CameraController* s_CameraController = nullptr;

static double lastMouseX = 0.0;
static double lastMouseY = 0.0;
static double mouseDeltaX = 0.0;
static double mouseDeltaY = 0.0;
static bool firstMouse = true;

// KeyCallback
// This function updates the key states every time a key is pressed or released
// so the engine knows exactly what happened.
// This is called directly by GLFW every time a key's state changes.
// We only update states here; the engine will read them later.
// GLFW calls KeyCallback -> Update the key states ->  Engine reads them.

/**
 * @brief GLFW key event callback. Updates per-key down/pressed/released state.
 *
 * Called directly by GLFW whenever a key's state changes on the registered window.
 * The event is first forwarded to ImGui so the UI can consume it, then the
 * relevant state arrays are updated:
 *
 *  - GLFW_PRESS:   sets keysDown; sets keysPressed only if the key was not
 *                  already down (prevents auto-repeat events being treated as
 *                  fresh presses).
 *  - GLFW_RELEASE: clears keysDown; sets keysReleased.
 *
 * Keys outside the [0, 1024) range are silently ignored.
 *
 * @param window   The GLFW window that received the event.
 * @param key      GLFW key token (e.g. GLFW_KEY_W).
 * @param scancode Platform-specific scancode (forwarded to ImGui, otherwise unused).
 * @param action   GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT.
 * @param mods     Modifier key bitmask (forwarded to ImGui, otherwise unused).
 */
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{   
    // Forward event to ImGui
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

    // Cant press a key out of bounds
    // Ignore keys outside our array range
    if (key < 0 || key >= 1024) return;

    // Key was pressed this frame
    if (action == GLFW_PRESS)
    {
        // Only mark "pressed" if it was NOT already down (prevents repeats)
        if (!keysDown[key])
            keysPressed[key] = true;// just pressed
         keysDown[key] = true;
    }
    // Key was released this frame
    else if (action == GLFW_RELEASE)
    {
        keysDown[key] = false;
        keysReleased[key] = true;// just released
    }
}

// Mouse movement callback
/**
 * @brief GLFW cursor position callback. Computes mouse deltas and forwards to CameraController.
 *
 * Called by GLFW every time the cursor moves. Behaviour:
 *  - On the first event (after init or a tracking reset), the position is stored
 *    as the baseline with no delta applied, preventing a large jump.
 *  - On subsequent events, the per-frame delta is computed from the previous position.
 *  - If a CameraController is registered and ImGui is not capturing the mouse,
 *    the raw position is forwarded to CameraController::processMouse() only
 *    while the right mouse button is held. Otherwise, mouse tracking is reset on
 *    the controller so it does not jump when the button is next pressed.
 *
 * @param window The GLFW window that received the event.
 * @param xpos   Current cursor X position in screen pixels.
 * @param ypos   Current cursor Y position in screen pixels.
 */
static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

    if (firstMouse)
    {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
    }

    mouseDeltaX = xpos - lastMouseX;
    mouseDeltaY = ypos - lastMouseY;

    lastMouseX = xpos;
    lastMouseY = ypos;

    // Only forward to CameraController in Editor mode
    if (s_CameraController)
    {
        if (!ImGui::GetIO().WantCaptureMouse &&
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            s_CameraController->processMouse(xpos, ypos);
        }
        else
        {
            s_CameraController->resetMouseTracking();
        }
    }
}

/**
 * @brief GLFW mouse button callback. Updates per-button down/pressed/released state.
 *
 * Mirrors the logic of KeyCallback for the eight supported mouse buttons.
 * The event is forwarded to ImGui before any internal state is modified.
 * Buttons outside the [0, 8) range are silently ignored.
 *
 * @param window The GLFW window that received the event.
 * @param button GLFW mouse button token (e.g. GLFW_MOUSE_BUTTON_LEFT).
 * @param action GLFW_PRESS or GLFW_RELEASE.
 * @param mods   Modifier key bitmask (forwarded to ImGui, otherwise unused).
 */
static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    if (button < 0 || button >= 8) return;

    if (action == GLFW_PRESS)
    {
        if (!mouseButtonsDown[button])
            mouseButtonsPressed[button] = true;

        mouseButtonsDown[button] = true;
    }
    else if (action == GLFW_RELEASE)
    {
        mouseButtonsDown[button] = false;
        mouseButtonsReleased[button] = true;
    }
}

/**
 * @brief GLFW scroll callback. Forwards the event to ImGui.
 *
 * Scroll state is not currently tracked internally; this callback exists
 * solely to keep ImGui's scroll handling functional.
 *
 * @param window  The GLFW window that received the event.
 * @param xoffset Horizontal scroll delta.
 * @param yoffset Vertical scroll delta.
 */
static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

/**
 * @brief GLFW character callback. Forwards Unicode input to ImGui for text entry.
 *
 * @param window The GLFW window that received the event.
 * @param c      Unicode code point of the character that was typed.
 */
static void CharCallback(GLFWwindow* window, unsigned int c)
{
    ImGui_ImplGlfw_CharCallback(window, c);
}


// Initialize
// Registers the GLFW key callback and stores the window pointer.
// Must be called ONCE after creating the window.
// This function connects your engine’s input system to the GLFW window
// so it starts receiving keyboard events.

/**
 * @brief Initialises the input system and registers all GLFW callbacks.
 *
 * Must be called exactly once after the GLFW window has been created, before
 * the main loop begins. Stores the window handle internally and registers
 * key, cursor position, mouse button, scroll, and character callbacks so
 * GLFW begins routing input events into the engine's state arrays.
 *
 * @param window The GLFW window whose input events should be tracked.
 */
void Input::Initialize(GLFWwindow* window)
{
    // Remember which window we are using for input
    internalWindow = window;

    // GLFW knows whenever a key is pressed or released on this window,
    // and it calls our KeyCallback function.”
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetCharCallback(window, CharCallback);
}

// BeginFrame
// Called once per frame at the start of the engine loop.
// Clears pressed/released states so they only last ONE frame.

/**
 * @brief Clears single-frame input state. Must be called once at the start of each frame.
 *
 * Resets keysPressed, keysReleased, mouseButtonsPressed, mouseButtonsReleased,
 * and the mouse delta accumulators to zero/false. Without this call, "pressed"
 * and "released" flags would remain set indefinitely after the triggering event.
 */
void Input::BeginFrame()
{
    for (int i = 0; i < 1024; i++)
    {
        keysPressed[i] = false;
        keysReleased[i] = false;
    }

    for (int i = 0; i < 8; i++)
    {
        mouseButtonsPressed[i] = false;
        mouseButtonsReleased[i] = false;
    }

    mouseDeltaX = 0.0;
    mouseDeltaY = 0.0;

}

// Query Functions
// These are what the engine and gameplay systems call.

// Returns true every frame the key is physically held down.
/**
 * @brief Returns true every frame the key is physically held down.
 *
 * Use for continuous actions such as movement or camera pan.
 *
 * @param key GLFW key token (e.g. GLFW_KEY_W).
 * @return true if the key is currently down.
 */
bool Input::GetKeyDown(int key)
{
    return keysDown[key];
}

// Returns true ONLY on the frame the key transitions from UP -> DOWN.
/**
 * @brief Returns true only on the single frame a key transitions from up to down.
 *
 * Use for one-shot actions such as jumping, toggling a menu, or firing.
 * Guaranteed to be true for at most one frame per press.
 *
 * @param key GLFW key token (e.g. GLFW_KEY_SPACE).
 * @return true on the frame the key was first pressed.
 */
bool Input::GetKeyPressed(int key)
{
    return keysPressed[key];
}

// Returns true ONLY on the frame the key transitions from DOWN -> UP.
/**
 * @brief Returns true only on the single frame a key transitions from down to up.
 *
 * Use for actions that trigger on key release (e.g. confirming a held charge).
 *
 * @param key GLFW key token.
 * @return true on the frame the key was released.
 */
bool Input::GetKeyReleased(int key)
{
    return keysReleased[key];
}

// Allow engine to set active camera controller
/**
 * @brief Registers a CameraController to receive forwarded mouse-move events.
 *
 * When set, CursorPosCallback forwards cursor position events to the controller
 * while the right mouse button is held and ImGui is not capturing the mouse.
 * Pass nullptr to stop forwarding.
 *
 * @param controller Pointer to the active CameraController, or nullptr to unregister.
 */
void Input::SetCameraController(CameraController* controller)
{
    s_CameraController = controller;
}

/**
 * @brief Returns true every frame the given mouse button is physically held down.
 *
 * @param button GLFW mouse button token (e.g. GLFW_MOUSE_BUTTON_LEFT).
 * @return true if the button is currently down. Returns false for out-of-range values.
 */
bool Input::GetMouseDown(int button)
{
    if (button < 0 || button >= 8) return false;
    return mouseButtonsDown[button];
}

/**
 * @brief Returns true only on the single frame a mouse button transitions from up to down.
 *
 * @param button GLFW mouse button token.
 * @return true on the frame the button was first pressed. Returns false for out-of-range values.
 */
bool Input::GetMousePressed(int button)
{
    if (button < 0 || button >= 8) return false;
    return mouseButtonsPressed[button];
}

/**
 * @brief Returns true only on the single frame a mouse button transitions from down to up.
 *
 * @param button GLFW mouse button token.
 * @return true on the frame the button was released. Returns false for out-of-range values.
 */
bool Input::GetMouseReleased(int button)
{
    if (button < 0 || button >= 8) return false;
    return mouseButtonsReleased[button];
}

/**
 * @brief Returns the horizontal mouse movement since the last frame, in pixels.
 *
 * Positive values indicate rightward movement. Reset to 0.0 by BeginFrame().
 *
 * @return Horizontal cursor delta in screen pixels.
 */
double Input::GetMouseDeltaX()
{
    return mouseDeltaX;
}

/**
 * @brief Returns the vertical mouse movement since the last frame, in pixels.
 *
 * Positive values indicate downward movement (screen-space convention).
 * Reset to 0.0 by BeginFrame().
 *
 * @return Vertical cursor delta in screen pixels.
 */
double Input::GetMouseDeltaY()
{
    return mouseDeltaY;
}

