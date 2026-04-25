
#include "../include/Rendering/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


/**
 * @brief Constructs a Camera with the given position, orientation and projection settings.
 *
 * Initialises all camera vectors by calling updateCameraVectors() after storing
 * the provided parameters.
 *
 * @param pos       Initial world-space position of the camera.
 * @param upDir     The world up direction, typically (0, 1, 0).
 * @param fieldOfView Vertical field of view in degrees.
 * @param yawAngle  Initial horizontal rotation angle in degrees.
 *                  -90 points the camera along the negative Z axis by default.
 * @param pitchAngle Initial vertical rotation angle in degrees.
 *                   Positive values look upward.
 */
Camera::Camera(const glm::vec3& pos,
    const glm::vec3& upDir,
    float fieldOfView,
    float yawAngle,
    float pitchAngle)
    : position(pos),       // Initialize camera position
	worldUp(upDir), //world up direction
    fov(fieldOfView), //Initialize field of view
	yaw(yawAngle),      //initialize yaw angle
	pitch(pitchAngle){  
    updateCameraVectors();
}


/**
 * @brief Returns the view matrix for this camera.
 *
 * The view matrix transforms world-space coordinates into camera (eye) space.
 * It is constructed using the camera position, a look-at target of
 * (position + front), and the camera's local up vector.
 *
 * @return glm::mat4 The view matrix.
 */
glm::mat4 Camera::getViewMatrix() const {
	//position of camera, target (position + front), up vector (where i am,what im looking at , which way is up)
	return glm::lookAt(position, position + front, up);
}




/**
 * @brief Returns the perspective projection matrix for this camera.
 *
 * Converts 3D camera-space coordinates into clip space using a perspective
 * projection. Objects closer to the near plane appear larger; objects beyond
 * the far plane are clipped.
 *
 * @param aspectRatio Width divided by height of the viewport.
 * @param nearPlane   Distance to the near clipping plane. Fragments closer
 *                    than this are discarded.
 * @param farPlane    Distance to the far clipping plane. Fragments further
 *                    than this are discarded.
 * @return glm::mat4 The perspective projection matrix.
 */
//projection of matrix with aspect ratio and near and far plane parameters
//aspect ratio is width/height of viewport/screen
//nearPlane closest distance from camera that will be rendered ,prevents clipping
//farPlane farthest distance from camera that will be rendered ,prevents clipping
glm::mat4 Camera::getProjectionMatrix(float aspectRatio,
	float nearPlane,
	float farPlane) const {
	//glm::perspective expects fov in radians, converts the 3d units to ndc space
	return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

/**
 * @brief Recalculates the camera's front, right and up vectors from yaw and pitch.
 *
 * Models the camera as sitting at the centre of a unit sphere. The yaw angle
 * rotates horizontally around the Y axis and the pitch angle tilts vertically.
 *
 * - front.x = cos(yaw) * cos(pitch)  — horizontal X component
 * - front.y = sin(pitch)             — vertical component
 * - front.z = sin(yaw) * cos(pitch)  — horizontal Z component
 *
 * cos(pitch) scales the horizontal components so they shrink correctly as the
 * camera looks further up or down. The right and up vectors are then derived
 * from the new front via cross products with the world up direction.
 *
 * Called automatically after any change to yaw or pitch.
 */
void Camera::updateCameraVectors() {
	//calculate new front vector for the camera based on yaw and pitch angles
    glm::vec3 newFront;

	//camera is at the center of a sphere pointed outwards to a point on the sphere
	//the point is controlled by yaw and pitch angles
	//yaw affects x and z components (horizontal rotation)
	//pitch affects y component (vertical rotation) and the x and z components are scaled by cos(pitch) to account for vertical tilt(when you look down the horizontal shrinks)
	//think of sin pitch as the height on the sphere and cos pitch as the radius of the horizontal circle at that height 
	//and cos yaw and sin yaw as the x and z coordinates on that horizontal circle
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

	//recalculate right and up vectors with updated front vector
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}


//movement methods
/**
 * @brief Moves the camera along its local forward axis.
 * @param distance Distance to move. Positive moves forward, negative moves backward.
 */
void Camera::moveForward(float distance) {
    position += front * distance;
}

/**
 * @brief Moves the camera along its local right axis.
 * @param distance Distance to move. Positive moves right, negative moves left.
 */
void Camera::moveRight(float distance) {
    position += right * distance;
}

/**
 * @brief Moves the camera along its local up axis.
 * @param distance Distance to move. Positive moves up, negative moves down.
 */
void Camera::moveUp(float distance) {
    position += up * distance;
}

/**
 * @brief Rotates the camera by applying deltas to the yaw and pitch angles.
 *
 * Typically called with mouse delta values each frame. After updating the
 * angles, updateCameraVectors() is called to keep the direction vectors
 * in sync.
 *
 * @param dx Change in yaw (horizontal rotation) in degrees.
 * @param dy Change in pitch (vertical rotation) in degrees.
 */
void Camera::rotate(float dx, float dy) {
    yaw += dx;
    pitch += dy;

	//update the camera vectors based on the new yaw and pitch values
    updateCameraVectors();  
}


/**
 * @brief Sets the camera's world-space position directly.
 * @param pos New position.
 */
void Camera::setPosition(const glm::vec3& pos) {
    position = pos;
}

/**
 * @brief Sets the camera's vertical field of view.
 * @param fieldOfView New field of view in degrees.
 */
void Camera::setFov(float fieldOfView) {
    fov = fieldOfView;
}

/**
 * @brief Sets the yaw angle and refreshes the camera vectors.
 * @param yawAngle New horizontal rotation angle in degrees.
 */
void Camera::setYaw(float yawAngle) {
    yaw = yawAngle;
    updateCameraVectors();
}

/**
 * @brief Sets the pitch angle and refreshes the camera vectors.
 * @param pitchAngle New vertical rotation angle in degrees.
 */
void Camera::setPitch(float pitchAngle) {
    pitch = pitchAngle;
    updateCameraVectors();
}


