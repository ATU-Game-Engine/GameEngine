#include "../include/Input/CameraController.h"
#include "../include/Input/Input.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>


/**
 * @brief Constructs a CameraController with configurable movement and sensitivity parameters.
 *
 * Defaults to Mode::ORBIT on construction. All motion state (velocity, angles,
 * mouse tracking) is initialised to zero/idle values.
 *
 * @param cam         Reference to the Camera this controller will drive.
 * @param speed       Movement speed in world units per second (free mode).
 * @param sensitivity Mouse sensitivity in degrees per pixel (free mode rotation).
 */
CameraController::CameraController(Camera& cam, float speed, float sensitivity)
	: camera(cam),             //Store reference to camera
	currentMode(Mode::ORBIT), //Default to orbital mode
	moveSpeed(speed),           //how fast camera moves in free mode
	mouseSensitivity(sensitivity), //how sensitive mouse is for rotation
	velocity(0.0f),     //intial velocity (0,0,0)
    acceleration(10.0f),      // Fast acceleration
    deceleration(15.0f),      // Faster deceleration for responsive stop
	minPitch(-89.0f),         // Prevent gimbal lock at bottom
	maxPitch(89.0f),        //prevents gimbol lock at top
	orbitalRadius(20.0f),  //radius for orbit mode
	orbitalSpeed(1.0f),     //speed of orbiting
	orbitalAngle(0.0f),     //starting angle
	orbitalCenter(0.0f),    //center point to orbit around
	firstMouse(true),   //first mouse movement flag
	lastMouseX(0.0),  //last mouse x position
	lastMouseY(0.0) //last mouse y position
                {
}

//takes in delta time to make movement frame rate independent
/**
 * @brief Dispatches the per-frame update to the appropriate mode handler.
 *
 * Should be called once per frame with the elapsed time since the last frame.
 * Branching on Mode::FREE calls updateFreeMode(); Mode::ORBIT calls
 * updateOrbitMode(). All movement is scaled by @p deltaTime to remain
 * frame-rate independent.
 *
 * @param deltaTime Elapsed time since the last frame, in seconds.
 */
void CameraController::update(float deltaTime) {
    switch (currentMode) {
    case Mode::FREE:
        updateFreeMode(deltaTime);
        break;

    case Mode::ORBIT:
        updateOrbitMode(deltaTime);
        break;
    }
}

/**
 * @brief Updates the camera each frame when in free-fly mode.
 *
 * Reads WASD / Space / Left-Ctrl keyboard input and constructs a target velocity
 * in camera-relative space (WASD along the camera's front/right vectors; Space
 * and Left-Ctrl along world Y). Movement uses smooth acceleration and
 * deceleration via lerp so the camera eases in and out rather than starting
 * and stopping instantly:
 *
 *  - While input is held:  velocity lerps toward targetVelocity at @c acceleration rate.
 *  - While no input is held: velocity lerps toward zero at @c deceleration rate.
 *
 * The lerp factor is clamped to [0, 1] to prevent overshooting.
 *
 * @param deltaTime Elapsed time since the last frame, in seconds.
 */
void CameraController::updateFreeMode(float deltaTime) {
    
	//calculate target velocity based on input( where we want to be moving)
    glm::vec3 targetVelocity(0.0f);//(0,0,0)

    // Forward/Backward
    if (Input::GetKeyDown(GLFW_KEY_W))
        targetVelocity += camera.getFront() * moveSpeed; //(0,0,-1) * 5 = (0,0,-5)
    if (Input::GetKeyDown(GLFW_KEY_S))
        targetVelocity -= camera.getFront() * moveSpeed;

    // Right/Left
    if (Input::GetKeyDown(GLFW_KEY_D))
        targetVelocity += camera.getRight() * moveSpeed; //(1,0,0) * 5 = (5,0,0) 
                                                         //(0,0,-5) + (5,0,0) = (5,0,-5)
    if (Input::GetKeyDown(GLFW_KEY_A))
        targetVelocity -= camera.getRight() * moveSpeed;

    // Up/Down (world space)
    if (Input::GetKeyDown(GLFW_KEY_SPACE))
        targetVelocity += glm::vec3(0.0f, 1.0f, 0.0f) * moveSpeed;
    if (Input::GetKeyDown(GLFW_KEY_LEFT_CONTROL))
        targetVelocity -= glm::vec3(0.0f, 1.0f, 0.0f) * moveSpeed;


    //Smooth acceleration/deceleration(how quickly )
    float lerpFactor;
    if (glm::length(targetVelocity) > 0.001f) {
        //Accelerating toward target
        lerpFactor = acceleration * deltaTime;
    }
    else {
        // Decelerating to stop
        lerpFactor = deceleration * deltaTime;
    }

    // Clamp lerp factor to [0, 1] to prevent overshooting
    lerpFactor = glm::clamp(lerpFactor, 0.0f, 1.0f);

    // Smoothly interpolate velocity
    velocity = glm::mix(velocity, targetVelocity, lerpFactor);

    // Apply velocity to position (delta-time based movement)
    if (glm::length(velocity) > 0.001f) {
        camera.setPosition(camera.getPosition() + velocity * deltaTime);
    }
}

/**
 * @brief Updates the camera each frame when in orbit mode.
 *
 * Advances the orbital angle by @c orbitalSpeed each second and repositions
 * the camera on a circular path of radius @c orbitalRadius around
 * @c orbitalCenter. The camera is then oriented to face the center by
 * deriving yaw and pitch from the resulting direction vector.
 *
 * Position is computed as:
 *   x = center.x + sin(angle) * radius
 *   z = center.z + cos(angle) * radius
 *   y = center.y + 1.8           (fixed eye-height offset)
 *
 * @param deltaTime Elapsed time since the last frame, in seconds.
 */
void CameraController::updateOrbitMode(float deltaTime) {
    // Update orbital angle (rotates around center)
    orbitalAngle += orbitalSpeed * deltaTime;

    // Calculate new camera position in circular path
    float x = orbitalCenter.x + sin(orbitalAngle) * orbitalRadius;
    float z = orbitalCenter.z + cos(orbitalAngle) * orbitalRadius;
    float y = orbitalCenter.y + 1.8f;  // Match your current offset

    camera.setPosition(glm::vec3(x, y, z));

    // Make camera look at the center
    glm::vec3 direction = glm::normalize(orbitalCenter - camera.getPosition());

    // Calculate yaw and pitch from direction vector
    float yaw = glm::degrees(atan2(direction.z, direction.x));
    float pitch = glm::degrees(asin(direction.y));

    camera.setYaw(yaw);
    camera.setPitch(pitch);
}

/**
 * @brief Processes a raw mouse position event and updates the camera rotation (free mode only).
 *
 * Ignored entirely when in Mode::ORBIT. On the first call after a mode switch
 * (or after resetMouseTracking()), the current position is stored as the
 * baseline and no rotation is applied — this prevents a sudden camera jump
 * from the cursor's prior position to wherever it currently sits.
 *
 * Mouse delta is scaled by @c mouseSensitivity and passed to Camera::rotate().
 * Pitch is then clamped to [@c minPitch, @c maxPitch] to prevent gimbal lock.
 *
 * @param xPos Current cursor X position in screen pixels.
 * @param yPos Current cursor Y position in screen pixels.
 */
void CameraController::processMouse(double xPos, double yPos) {
    //Only process mouse in free mode
    if (currentMode != Mode::FREE)
        return;

    // Handle first mouse movement to prevent camera jump
    if (firstMouse) {
        lastMouseX = xPos;
        lastMouseY = yPos;
        firstMouse = false;
        return;
    }

    // Calculate mouse offset from last frame
    double xOffset = xPos - lastMouseX;
    double yOffset = lastMouseY - yPos;  // Reversed: y goes bottom to top

    lastMouseX = xPos;
    lastMouseY = yPos;

    // Apply sensitivity (convert pixels to degrees)
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    // Update camera rotation
    camera.rotate(static_cast<float>(xOffset), static_cast<float>(yOffset));

    //Apply pitch constraints to prevent gimbal lock
    float currentPitch = camera.getPitch();
    if (currentPitch > maxPitch) {
        camera.setPitch(maxPitch);
    }
    if (currentPitch < minPitch) {
        camera.setPitch(minPitch);
    }
}

/**
 * @brief Switches the controller to a new operating mode.
 *
 * Performs the necessary state resets for each transition:
 *  - Entering Mode::FREE: velocity is zeroed and mouse tracking is reset so
 *    the camera does not lurch on the first mouse event.
 *  - Entering Mode::ORBIT: no reset currently applied (orbital angle is
 *    preserved across mode switches).
 *
 * @param mode The mode to switch to (Mode::FREE or Mode::ORBIT).
 */
void CameraController::setMode(Mode mode) {
    currentMode = mode;

    if (mode == Mode::FREE) {
        // Reset velocity when entering free mode
        velocity = glm::vec3(0.0f);
        firstMouse = true;  // Reset mouse tracking
    }
    else if (mode == Mode::ORBIT) {
        // Reset orbital angle currently not ressiting
      
    }
}

/**
 * @brief Resets the mouse baseline so the next movement event is treated as the first.
 *
 * Call this whenever the cursor is warped or hidden/shown to prevent a
 * sudden camera jump caused by a large positional discontinuity in the
 * mouse input stream.
 */
void CameraController::resetMouseTracking()
{
    firstMouse = true;
}

/**
 * @brief Sets the minimum and maximum allowed pitch angles for free-fly mode.
 *
 * Pitch is clamped to this range inside processMouse() after each rotation.
 * The defaults of [-89°, 89°] prevent gimbal lock; tighter values can be used
 * to restrict vertical look range for specific gameplay needs.
 *
 * @param min Minimum pitch in degrees (typically negative, e.g. -89°).
 * @param max Maximum pitch in degrees (typically positive, e.g. 89°).
 */
void CameraController::setPitchConstraints(float min, float max) {
    minPitch = min;
    maxPitch = max;
}