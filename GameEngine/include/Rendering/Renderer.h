#ifndef RENDERER_H
#define RENDERER_H

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "../include/Rendering/Camera.h"
#include "../include/Rendering/Mesh.h"
#include "../include/Scene/GameObject.h"
#include "../include/Rendering/Texture.h"
#include "../include/Rendering/DirectionalLight.h"
#include "../include/Rendering/Skybox.h"
#include "ShadowMap.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include <string>
#include <memory>  // for std::unique_ptr
#include <unordered_map>
#include "../Physics/TriggerRegistry.h"
#include "../Physics/ForceGeneratorRegistry.h"
#include "../Physics/ForceGenerator.h"
#include "../Physics/Trigger.h"
#include "../Rendering/PointLight.h"
#include "../Physics/ConstraintRegistry.h"
#include "../Physics/Constraint.h"

class Renderer {
private:
    
    ShaderManager shaderManager;
    ShadowMap shadowMap;
    Mesh cubeMesh;
    Mesh sphereMesh;
    Mesh cylinderMesh;
    bool debugPhysicsEnabled;
    TextureManager textureManager;
    DirectionalLight mainLight;
    Skybox skybox;
    bool skyboxEnabled;
    std::unordered_map<GameObject*, float> objectAlphas;

   
    void drawGameObject(const GameObject& obj, int modelLoc, int colorLoc);// draw a single game object
    void drawOutlineOnly(const GameObject& obj, int modelLoc, int colorLoc);
    void drawDebugCollisionShape(const GameObject& obj, int modelLoc, int colorLoc);
    void renderShadowPass( 
        const Camera& camera,
        const std::vector<std::unique_ptr<GameObject>>& objects);


public:
    Renderer();
    ~Renderer();

    void initialize();
    void draw(
        int windowWidth,
        int windowHeight,
        const Camera& camera,
        const std::vector<std::unique_ptr<GameObject>>& objects,
        const GameObject* primarySelection,
        const std::vector<GameObject*>& selectedObjects
    );

    void drawTriggerDebug(const std::vector<Trigger*>& triggers, const Camera& camera, int fbW, int fbH);
    void drawForceGeneratorDebug(const std::vector<ForceGenerator*>& generators, const Camera& camera, int fbW, int fbH);
    void drawPointLightDebug(const std::vector<PointLight*>& lights, const Camera& camera, int fbW, int fbH);
    void drawConstraintDebug(const std::vector<Constraint*>& constraints, const Camera& camera, int fbW, int fbH);
    void uploadPointLights(const std::vector<PointLight*>& lights);
    void setObjectAlpha(GameObject* obj, float alpha) { objectAlphas[obj] = alpha; }
    void clearAlphaOverrides() { objectAlphas.clear(); }
    
    void cleanup();

    // Light control
    DirectionalLight& getLight() { return mainLight; }

    // Skybox control
    bool loadSkybox(const std::vector<std::string>& faces);
    void toggleSkybox() { skyboxEnabled = !skyboxEnabled; }

    Mesh* getCubeMesh() { return &cubeMesh; } 
    Mesh* getSphereMesh() { return &sphereMesh; }
    Mesh* getCylinderMesh() { return &cylinderMesh; }

    // Debug physics toggle
    void toggleDebugPhysics() { debugPhysicsEnabled = !debugPhysicsEnabled; }
    bool isDebugPhysicsEnabled() const { return debugPhysicsEnabled; }
};

#endif