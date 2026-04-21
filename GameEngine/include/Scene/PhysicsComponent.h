#ifndef PHYSICSCOMPONENT_H
#define PHYSICSCOMPONENT_H

#include "Component.h"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class TransformComponent;

/**
 * @brief Component that connects a GameObject to the Bullet Physics simulation.
 *
 * This component:
 * - Wraps a btRigidBody pointer (owned by Physics system)
 * - Syncs transform data between physics and GameObject
 * - Stores physics material properties
 *
 * Objects without this component are render-only (no physics).
 */
class PhysicsComponent : public Component {
private:
    btRigidBody* rigidBody;      // Not owned - Physics system manages this
    std::string materialName;

public:
    PhysicsComponent(btRigidBody* body, const std::string& material)
        : rigidBody(body), materialName(material) {
    }

    btRigidBody* getRigidBody() const { return rigidBody; }
    const std::string& getMaterialName() const { return materialName; }
    void setMaterialName(const std::string& name) { materialName = name; }
    /**
     * @brief Sync physics simulation state TO the GameObject's transform.
     * Call this after physics simulation has stepped.
     */
    void syncToTransform(TransformComponent& transform);

    /**
     * @brief Sync GameObject's transform state TO the physics simulation.
     * Call this when manually moving/rotating the object.
     */
    void syncFromTransform(const TransformComponent& transform);

    void setRigidBody(btRigidBody* newBody) {
        rigidBody = newBody;
    }
};

#endif // PHYSICSCOMPONENT_H