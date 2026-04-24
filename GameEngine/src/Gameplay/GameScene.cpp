#include "../include/Gameplay/GameScene.h"
#include "../include/Gameplay/PlayerController.h"
#include "../include/Gameplay/EnemyAI.h"
#include "../include/Scene/Scene.h"
#include "../include/Rendering/Camera.h"
#include "../include/Physics/Physics.h"
#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/Trigger.h"
#include "../include/Physics/ForceGeneratorRegistry.h"
#include "../include/Physics/ForceGenerator.h"

/**
 * @brief Populates the scene with the initial set of game objects and force generators.
 *
 * Called once during game startup before the main loop begins. Responsible for
 * spawning the static world geometry and runtime entities that should exist from
 * the first frame:
 *
 *  - **Ground plane**: a large flat cube centred at the origin, used as the
 *    static collision floor.
 *  - **Player capsule**: spawned above the origin with friction/restitution tuned
 *    for responsive on-ground feel and no bouncing. Adding the "player" tag fires
 *    the wireTagCallback registered in SetupScripts(), which immediately attaches
 *    a PlayerController script.
 *  - **Wind force generator**: a cylindrical wind area centred at (0, 2, 0) that
 *    applies a continuous force in the +X direction to any physics objects within
 *    its radius.
 *
 * @param scene   The scene to populate with spawned objects.
 * @param camera  The active scene camera (passed through to scripts that need it).
 * @param physics The physics world (provides the query system for ground detection).
 */
void SetupGameScene(Scene& scene, Camera& camera, Physics& physics)
{
    // Ground
    scene.spawnObject(ShapeType::CUBE, glm::vec3(0, -0.25f, 0), glm::vec3(100.0f, 0.5f, 100.0f), 0.0f, "Default");

    // Player
    GameObject* player = scene.spawnObject(
        ShapeType::CAPSULE,
		glm::vec3(0.0f, 3.0f, 0.0f),// position
		glm::vec3(0.5f, 1.0f, 0.5f),// size (radius, height, unused)
        1.0f, "Default"
    );
    player->setName("Player");
    player->getRigidBody()->setFriction(0.8f);
    player->getRigidBody()->setRestitution(0.0f);
    player->getRigidBody()->setContactProcessingThreshold(0.0f);
    // script is now attached
    // SetupScripts() registers "player" -> PlayerController, so adding this
    // tag here fires the attachment immediately through wireTagCallback.
    player->addTag("player");


    auto& forceRegistry = ForceGeneratorRegistry::getInstance();
    forceRegistry.createWind(
        "wind_corridor",
        glm::vec3(0.0f, 2.0f, 0.0f),  // center of the wind area
        8.0f,                            // radius
        glm::vec3(1.0f, 0.0f, 0.0f),    // direction (blowing toward +X)
        100.0f                            // strength
    );
}

/**
 * @brief Registers all tag-to-script and trigger-to-script bindings for the game.
 *
 * Must be called once during game startup, after SetupGameScene() but before
 * any scene file is loaded. Each registration maps a string tag or trigger tag
 * to a pair of attach/detach lambdas, so the binding is live for the entire
 * session:
 *
 *  - Any object that receives a registered tag — at spawn time, via
 *    loadFromFile(), or at runtime through the Inspector — will have the
 *    corresponding script attached immediately.
 *  - Removing the tag detaches the script via the corresponding remove lambda.
 *
 * After all bindings are registered, applyTagScriptsToExistingObjects() and
 * applyTriggerScriptsToExistingTriggers() are called to retroactively attach
 * scripts to objects and triggers that were loaded from file before these
 * registrations existed.
 *
 * **Registered tag scripts:**
 *  - "player"  → PlayerController (receives camera + physics query references)
 *  - "enemy"   → EnemyAI (receives scene reference and tuning parameters)
 *
 * **Registered trigger scripts:**
 *  - "bounce_pad"     → on enter: spawns a short-lived explosion ForceGenerator
 *                       at the trigger's position, launching the entering object.
 *  - "sphere_spawner" → on enter: spawns a sphere above the trigger position.
 *
 * @param scene   The scene whose tag/trigger registries will be populated.
 * @param camera  Captured by the "player" lambda and passed to PlayerController.
 * @param physics Captured by the "player" lambda; provides the physics query system.
 */
void SetupScripts(Scene& scene, Camera& camera, Physics& physics)
{
    // Each call maps a tag string to a lambda that attaches the right script.
    // From this point on, ANY object that gets this tag added — whether at
    // spawn time, from loadFromFile(), or at runtime via the Inspector —
    // will immediately have the script attached.
    scene.registerTagScript("player",
        [&](GameObject* obj) {
            obj->addScript<PlayerController>(&camera, &physics.getQuerySystem());
        },
        [](GameObject* obj) {
            obj->removeScript<PlayerController>();
        }
    );

    // FORMAT:
      //   scene.registerTriggerScript("your_tag", [&](Trigger* t) {
      //       t->setOnEnterCallback([](GameObject* obj) { /* your logic */ });
      //       // optionally: t->setOnExitCallback(...);
      //       // optionally: t->setOnStayCallback(...);
      //   });
    
    scene.registerTriggerScript("bounce_pad",
        [](Trigger* t) {
            t->setOnEnterCallback([t](GameObject* obj) {  // ← capture t
                if (!obj->hasPhysics()) return;

                ForceGeneratorRegistry::getInstance().createExplosion(
                    "bounce_" + obj->getName(),
                    t->getPosition(),
                    t->getSize().y * 2.0f,
                    400.0f
                );
                });
        }
    );
    scene.registerTriggerScript("sphere_spawner",
        [&scene](Trigger* t) {
            t->setOnEnterCallback([&scene, t](GameObject* obj) {
                if (!obj->hasPhysics()) return;

                // Spawn 5 spheres in a spread above the trigger
                for (int i = 0; i < 1; i++)
                {
                    float offsetX = (i - 2) * 1.5f; // spread them out: -3, -1.5, 0, 1.5, 3
                    glm::vec3 spawnPos = t->getPosition() + glm::vec3(0.0f, t->getSize().y + 3.0f, 0.0f);

                    GameObject* sphere = scene.spawnObject(
                        ShapeType::SPHERE,
                        spawnPos,
                        glm::vec3(2.0f),  // radius
                        100.0f,             // mass
                        "Default"
                    );
                    sphere->setName("SpawnedSphere_" + std::to_string(i));
                }
                });
        }
    );

    scene.registerTagScript("enemy",
        [&scene](GameObject* obj) {
            obj->addScript<EnemyAI>(&scene, 15.0f, 4.0f, 1.5f, 1.0f);
        },
        [](GameObject* obj) { obj->removeScript<EnemyAI>(); }
    );
    // One-time pass to attach scripts to any objects that were loaded
    // from file and already had tags set before registerTagScript() was called.
    // Must be called AFTER all registerTagScript() calls above.
    scene.applyTagScriptsToExistingObjects();
    scene.applyTriggerScriptsToExistingTriggers();
}