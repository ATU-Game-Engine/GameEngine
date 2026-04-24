#pragma once
#pragma once
#include "../../include/Scene/ScriptComponent.h"
#include <glm/glm.hpp>

class Scene;

/**
 * @brief Simple enemy AI that detects the player via spatial query,
 *        chases them, and applies a knockback impulse on contact.
 *
 * Usage (direct attach):
 *   enemy->addScript<EnemyAI>(&scene);
 *
 * Usage (tag-driven, recommended):
 *   scene.registerTagScript("enemy", [&](GameObject* obj) {
 *       obj->addScript<EnemyAI>(&scene);
 *   }, [](GameObject* obj) {
 *       obj->removeScript<EnemyAI>();
 *   });
 *   enemy->addTag("enemy"); // script attaches automatically
 *
 * The enemy object must have a physics body. It will lock its own
 * rotation axes on start so it doesn't tip over during movement.
 *
 * @param scene           Scene reference for spatial queries
 * @param detectionRadius How far away the enemy can spot the player
 * @param speed           XZ movement speed (units/sec)
 * @param attackRange     Distance at which the enemy transitions to attacking
 * @param attackCooldown  Seconds between attack hits
 */
class EnemyAI : public ScriptComponent
{
public:
    EnemyAI(Scene* scene,
        float detectionRadius = 15.0f,
        float speed = 4.0f,
        float attackRange = 1.5f,
        float attackCooldown = 1.0f);

    void onStart()                    override;
    void onUpdate(float dt)           override;
    void onFixedUpdate(float fixedDt) override;
    void onDestroy()                  override;

    // Runtime tuning
    void setSpeed(float s) { speed = s; }
    void setDetectionRadius(float r) { detectionRadius = r; }
    void setAttackRange(float r) { attackRange = r; }
    void setAttackCooldown(float c) { attackCooldown = c; }

private:
    enum class State { IDLE, CHASING, ATTACKING };

    Scene* scene;
    float       detectionRadius;
    float       speed;
    float       attackRange;
    float       attackCooldown;

    State       state = State::IDLE;
    GameObject* target = nullptr; // cached player pointer (non-owning)
    float       attackTimer = 0.0f;    // counts down between hits
    float       idleTimer = 0.0f;    // throttles spatial scans in IDLE

    void tickIdle(float dt);
    void tickChasing(float dt);
    void tickAttacking(float dt);

    // Fires once per attack interval when player is in range
    void onAttack();

    // Drives the rigid body toward a world position, preserving Y for gravity
    void moveToward(const glm::vec3& targetPos);

    // Zeroes XZ velocity, keeps Y so gravity continues working
    void stopMovement();

    static const char* stateName(State s);
};