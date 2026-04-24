#include <chrono>
#include <thread>
#include <iostream>
#include "../include/Core/GameTime.h"

// GameTime comment test for including in project


// Initialize static members
Time::TimePoint Time::lastFrameTime;
Time::TimePoint Time::startTime;
float Time::deltaTime = 0.0f;
float Time::totalTime = 0.0f;
int Time::frameCount = 0;
float Time::fps = 0.0f;
float Time::fpsUpdateTimer = 0.0f;
bool Time::fpsLimitEnabled = false;
float Time::targetFrameTime = 0.0f;

/**
 * @brief Resets all time state. Must be called once before the main loop
 */
void Time::Initialize() {
    startTime = Clock::now();
    lastFrameTime = startTime;
    deltaTime = 0.0f;
    totalTime = 0.0f;
    frameCount = 0;
    fps = 0.0f;
    fpsUpdateTimer = 0.0f;
}

/**
 * @brief Updates delta time, total time, and FPS. Call once at the start of each frame
 *
 * Delta time is capped at 0.25s to prevent the physics accumulator spiralling
 * if a frame takes unusually long. FPS is recalculated once per second
 */
void Time::Update() {
    //Calculate time since last frame
    //get current time
    TimePoint currentTime = Clock::now();
    //calculate time since lastfrme started 
    std::chrono::duration<float> frameDuration = currentTime - lastFrameTime;
    
    //convert duration to seconds(float)
    deltaTime = frameDuration.count();

    // Cap delta time to prevent spiral of death (max 0.25s = 4 FPS minimum)
    //prevents physics from doing to much catch up
    if (deltaTime > 0.25f) {
        deltaTime = 0.25f;
    }

    // Update total time
    //calculate time since app started running
    std::chrono::duration<float> totalDuration = currentTime - startTime;
    //convert to seconds
    totalTime = totalDuration.count();

    // Update frame count
    frameCount++;

    // Update FPS counter every second

    //add current frames time to the accumulator
    fpsUpdateTimer += deltaTime;
    if (fpsUpdateTimer >= 1.0f) {
        //calculate the fps after a second has passed
        fps = frameCount / fpsUpdateTimer;
        //reset counters for the next second
        frameCount = 0;
        fpsUpdateTimer = 0.0f;
    }

    //store time last frame started for next delta calc
    lastFrameTime = currentTime;
}

/**
 * @brief Sets the target frame rate and enables the frame limiter
 *
 * Pass 0 or a negative value to disable limiting and run uncapped
 *
 * @param targetFPS Desired frames per second
 */
void Time::SetTargetFPS(float targetFPS) {
    if (targetFPS > 0.0f) {
        //convert fps to frame time
        //60fps = 0.0166 seconds per frame
        targetFrameTime = 1.0f / targetFPS;
        fpsLimitEnabled = true;
    }
    else {
        //target fps 0 or negative to disable limiter
        fpsLimitEnabled = false;
    }
}

/**
 * @brief Blocks until the current frame has met its target duration.
 *
 * Sleeps for most of the remaining time then spin-locks for the final
 * millisecond to achieve precise timing. Does nothing if the limiter
 * is disabled or the frame already ran long
 */
void Time::WaitForNextFrame() {
    //check if  fps limit is on to decide if next frame should wait or not
    if (!fpsLimitEnabled)
        return;
    while (true) {
        //calculate how long this frame took
        TimePoint now = Clock::now();
        std::chrono::duration<float> frameDuration = now - lastFrameTime;
        float actualFrameTime = frameDuration.count();

        // If frame finished early
        //calculate how long until next frame 
        float remainingTime = targetFrameTime - actualFrameTime;

        if (remainingTime <= 0.0f) {
            break; // Target time reached
        }

        // If significant time remains, sleep
        if (remainingTime > 0.001f) {// More than 1ms left
            float sleepTime = remainingTime * 0.001f;// Leave 1ms for spin-lock
            std::this_thread::sleep_for(std::chrono::duration<float>(sleepTime));
        }
        //otherwise spin lock for precise timing
    }
}