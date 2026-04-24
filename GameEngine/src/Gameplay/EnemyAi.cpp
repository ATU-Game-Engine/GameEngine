#pragma once
#include "../../include/Gameplay/EnemyAI.h"
#include "../../include/Scene/GameObject.h"
#include "../../include/Scene/Scene.h"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <iostream>

EnemyAI::EnemyAI(Scene* scene,
    float detectionRadius,
    float speed,
    float attackRange,
    float attackCooldown)
    : scene(scene)
    , detectionRadius(detectionRadius)
    , speed(speed)
    , attackRange(attackRange)
    , attackCooldown(attackCooldown)
{
}

void EnemyAI::onStart()
{
    std::cout << "[EnemyAI] Started on '" << owner->getName() << "'" << std::endl;

    if (owner->hasPhysics())
    {
        // Prevent the capsule from tipping over while moving
        owner->getRigidBody()->setAngularFactor(btVector3(0, 0, 0));
    }
}

// Per-frame state dispatch

void EnemyAI::onUpdate(float dt)
{
    switch (state)
    {
    case State::IDLE:      tickIdle(dt);      break;
    case State::CHASING:   tickChasing(dt);   break;
    case State::ATTACKING: tickAttacking(dt); break;
    }
}

// Physics movement runs in onFixedUpdate so velocity changes are applied
// at the same rate as the physics step (1/60s).
void EnemyAI::onFixedUpdate(float /*fixedDt*/)
{
    if (state == State::CHASING && target)
        moveToward(target->getPosition());
    else
        stopMovement();
}

// State handlers

// IDLE — throttle spatial scans to every 0.5 s to avoid querying every frame
void EnemyAI::tickIdle(float dt)
{
    idleTimer -= dt;
    if (idleTimer > 0.0f) return;
    idleTimer = 0.5f;

    // Spatial query: nearest object tagged "player" within detection radius
    target = scene->findNearestObject(
        owner->getPosition(),
        detectionRadius,
        [](GameObject* obj) { return obj->hasTag("player"); }
    );

    if (target)
    {
        std::cout << "[EnemyAI] '" << owner->getName()
            << "' spotted player — chasing!" << std::endl;
        state = State::CHASING;
    }
}

// Chasing  move toward player each fixed update, transition on range changes
void EnemyAI::tickChasing(float dt)
{
    if (!target)
    {
        state = State::IDLE;
        return;
    }

    float dist = glm::length(target->getPosition() - owner->getPosition());

    // 20% hysteresis on the outer edge prevents flip-flopping at the boundary
    if (dist > detectionRadius * 1.2f)
    {
        std::cout << "[EnemyAI] Lost player" << std::endl;
        target = nullptr;
        state = State::IDLE;
        return;
    }

    if (dist <= attackRange)
    {
        state = State::ATTACKING;
        attackTimer = 0.0f; // fire immediately on first contact
    }
}

// ATTACKING — hit the player, wait for cooldown, then decide whether to
// keep attacking or resume chasing if the player stepped back
void EnemyAI::tickAttacking(float dt)
{
    if (!target)
    {
        state = State::IDLE;
        return;
    }

    attackTimer -= dt;
    if (attackTimer > 0.0f) return;

    float dist = glm::length(target->getPosition() - owner->getPosition());

    if (dist <= attackRange)
    {
        onAttack();
        attackTimer = attackCooldown;
    }
    else if (dist > detectionRadius * 1.2f)
    {
        // Player is completely out of range
        target = nullptr;
        state = State::IDLE;
    }
    else
    {
        // Player backed off just outside attack range — resume chasing
        state = State::CHASING;
    }
}

// Attack

void EnemyAI::onAttack()
{
    if (!target || !target->hasPhysics()) return;

    std::cout << "[EnemyAI] '" << owner->getName() << "' attacks player!" << std::endl;

    // Direction from enemy to player, with a slight upward lift for feel
    glm::vec3 knockDir = target->getPosition() - owner->getPosition();
    knockDir.y = 0.0f;

    float len = glm::length(knockDir);
    if (len < 0.001f) knockDir = glm::vec3(1, 0, 0); // fallback if exactly overlapping
    else              knockDir /= len;

    knockDir.y = 0.4f; // add upward component after normalising XZ

    btRigidBody* playerBody = target->getRigidBody();
    if (playerBody)
    {
        const float knockbackStrength = 30.0f;
        playerBody->activate(true); // wake if sleeping
        playerBody->applyCentralImpulse(btVector3(
            knockDir.x * knockbackStrength,
            knockDir.y * knockbackStrength,
            knockDir.z * knockbackStrength
        ));
    }
}

// Movement helpers 

void EnemyAI::moveToward(const glm::vec3& targetPos)
{
    if (!owner->hasPhysics()) return;
    btRigidBody* rb = owner->getRigidBody();
    if (!rb) return;

    glm::vec3 dir = targetPos - owner->getPosition();
    dir.y = 0.0f; // ignore height difference, let gravity handle Y

    float dist = glm::length(dir);
    if (dist < 0.01f) return;
    dir /= dist; // normalise

    // Overwrite XZ, preserve Y so gravity continues to apply
    btVector3 current = rb->getLinearVelocity();
    rb->setLinearVelocity(btVector3(dir.x * speed, current.y(), dir.z * speed));
    rb->activate(true);
}

void EnemyAI::stopMovement()
{
    if (!owner->hasPhysics()) return;
    btRigidBody* rb = owner->getRigidBody();
    if (!rb) return;

    btVector3 vel = rb->getLinearVelocity();
    rb->setLinearVelocity(btVector3(0.0f, vel.y(), 0.0f));
}

// Cleanup
void EnemyAI::onDestroy()
{
    std::cout << "[EnemyAI] '" << owner->getName() << "' destroyed" << std::endl;
    target = nullptr;
}

const char* EnemyAI::stateName(State s)
{
    switch (s)
    {
    case State::IDLE:      return "IDLE";
    case State::CHASING:   return "CHASING";
    case State::ATTACKING: return "ATTACKING";
    default:               return "UNKNOWN";
    }
}