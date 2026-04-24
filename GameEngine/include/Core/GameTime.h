#ifndef TIME_H
#define TIME_H

#include <chrono>


//Time management class for tracking frame time, delta time, and FPS
/**
 * @brief Static time management utility for tracking delta time, total time, and FPS.
 *
 * Call Initialize() once at startup, Update() at the start of each frame,
 * and WaitForNextFrame() at the end if FPS limiting is enabled.
 */
class Time {
private:
    using Clock = std::chrono::high_resolution_clock; //High-precision timer
    using TimePoint = std::chrono::time_point<Clock>; //A point in time

    //timing data
    static TimePoint lastFrameTime; //time when the last frame started
    static TimePoint startTime; //time when the application started
    static float deltaTime; //time since last frame
    static float totalTime; //time since application started
    static int frameCount; //number of renderered frames in a second
    static float fps; //current frames per second(updates every seciobd)
    static float fpsUpdateTimer;//accumulator for fps calcs (so fps isnt calculated every frame )

    //FPS limiting
    static bool fpsLimitEnabled;
    static float targetFrameTime;  //Target time per frame (1/targetFPS)

public:
    //Initialize the time system (call once at startup)
    static void Initialize();

    //Update delta time (call once per frame at start of game loop)
    static void Update();

    //getters
    static float GetDeltaTime() { return deltaTime; }      //Time since last frame (seconds)
    static float GetTotalTime() { return totalTime; }      //Time since start (seconds)
    static float GetFPS() { return fps; }                   //Current FPS
    static int GetFrameCount() { return frameCount; }      //Total frames rendered

    //FPS Limiting
    static void SetTargetFPS(float targetFPS);
    static void EnableFPSLimit(bool enable) { fpsLimitEnabled = enable; }
    static bool IsFPSLimitEnabled() { return fpsLimitEnabled; }

    //Wait if frame finished too quickly (for FPS limiting)
    static void WaitForNextFrame();
};

#endif //TIME_H
