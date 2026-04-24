#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include "../Rendering/Camera.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

/**
 * @brief Drives a Camera in either free-fly or orbital mode.
 *
 * Call update() once per frame and processMouse() from the cursor callback.
 * Switching modes via setMode() resets the relevant motion state automatically.
 */
class CameraController {

public:
    enum class Mode {
        ORBIT,
        FREE
    };

private:
    Camera& camera;
    Mode currentMode;

    // Movement settings
    float moveSpeed;           // Units per second
    float mouseSensitivity;    // Degrees per pixel
    glm::vec3 velocity;        // Current velocity for smooth acceleration
    float acceleration;        // How fast we reach target speed
    float deceleration;        // How fast we slow down

    // Rotation constraints
    float minPitch;            // Minimum pitch angle (looking down)
    float maxPitch;            // Maximum pitch angle (looking up)

    // Orbital mode settings
    float orbitalRadius;
    float orbitalSpeed;
    float orbitalAngle;
    glm::vec3 orbitalCenter;

    // Mouse tracking
    bool firstMouse;
    double lastMouseX;
    double lastMouseY;


private:
    void updateFreeMode(float deltaTime);
    void updateOrbitMode(float deltaTime);

public:
    CameraController(Camera& cam, float speed = 5.0f, float sensitivity = 0.1f);

    // Main update (call every frame with delta time)
    void update(float deltaTime);

    // Process mouse movement (call from mouse callback)
    void processMouse(double xPos, double yPos);

    // Mode switching
    void setMode(Mode mode);
    Mode getMode() const { return currentMode; }

    // Settings
    void setMoveSpeed(float speed) { moveSpeed = speed; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }
    void setPitchConstraints(float min, float max);
    void setAcceleration(float accel) { acceleration = accel; }
    void setDeceleration(float decel) { deceleration = decel; }

    // Orbital mode settings
    void setOrbitalCenter(const glm::vec3& center) { orbitalCenter = center; }
    void setOrbitalRadius(float radius) { orbitalRadius = radius; }
    void setOrbitalSpeed(float speed) { orbitalSpeed = speed; }

    void resetMouseTracking();
};

#endif // CAMERA_CONTROLLER_H