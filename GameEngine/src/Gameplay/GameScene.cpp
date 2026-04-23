#include "../include/Gameplay/GameScene.h"
#include "../include/Gameplay/PlayerController.h"
#include "../include/Scene/Scene.h"
#include "../include/Rendering/Camera.h"
#include "../include/Physics/Physics.h"
#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/Trigger.h"
#include "../include/Physics/ForceGeneratorRegistry.h"
#include "../include/Physics/ForceGenerator.h"
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
    
    // One-time pass to attach scripts to any objects that were loaded
    // from file and already had tags set before registerTagScript() was called.
    // Must be called AFTER all registerTagScript() calls above.
    scene.applyTagScriptsToExistingObjects();
    scene.applyTriggerScriptsToExistingTriggers();
}