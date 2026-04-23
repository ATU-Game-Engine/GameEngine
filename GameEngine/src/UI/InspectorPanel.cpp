#include "../include/UI/InspectorPanel.h"
#include "../include/Physics/Constraint.h"
#include "../include/Physics/ConstraintRegistry.h"
#include "../include/Physics/PhysicsMaterial.h"
#include "../External/imgui/core/imgui.h"
#include <glm/gtc/constants.hpp>
#include <vector>
#include <string>
#include <cmath>

// helpers
static glm::vec3 QuatToEulerRad(const glm::quat& q)
{
    glm::vec3 euler;

    float sinp = 2.0f * (q.w * q.x - q.z * q.y);
    if (std::abs(sinp) >= 1.0f)
        euler.x = std::copysign(glm::half_pi<float>(), sinp);
    else
        euler.x = std::asin(sinp);

    float siny_cosp = 2.0f * (q.w * q.y + q.x * q.z);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.x * q.x);
    euler.y = std::atan2(siny_cosp, cosy_cosp);

    float sinr_cosp = 2.0f * (q.w * q.z + q.y * q.x);
    float cosr_cosp = 1.0f - 2.0f * (q.z * q.z + q.x * q.x);
    euler.z = std::atan2(sinr_cosp, cosr_cosp);

    return euler;
}

static glm::quat EulerRadToQuat(const glm::vec3& euler)
{
    glm::quat qx = glm::angleAxis(euler.x, glm::vec3(1, 0, 0));
    glm::quat qy = glm::angleAxis(euler.y, glm::vec3(0, 1, 0));
    glm::quat qz = glm::angleAxis(euler.z, glm::vec3(0, 0, 1));
    return qy * qx * qz;
}

static const char* ConstraintTypeToString(ConstraintType type)
{
    switch (type)
    {
    case ConstraintType::FIXED:        return "Fixed";
    case ConstraintType::HINGE:        return "Hinge";
    case ConstraintType::SLIDER:       return "Slider";
    case ConstraintType::SPRING:       return "Spring";
    case ConstraintType::GENERIC_6DOF: return "Generic 6DOF";
    default:                           return "Unknown";
    }
}


//  DrawInspectorPanel
void DrawInspectorPanel(DebugUIContext& context)
{
    ImGui::Begin("Inspector");

    if (!context.selectedObject)
    {
        ImGui::TextDisabled("No object selected.");
        ImGui::TextDisabled("Click an object in the scene to inspect it.");
        ImGui::End();
        return;
    }

    glm::vec3 pos = context.selectedObject->getPosition();
    glm::vec3 scale = context.selectedObject->getScale();
    glm::quat rot = context.selectedObject->getRotation();

    // Identity 
    ImGui::Text("Object ID: %llu", context.selectedObject->getID());

    {
        static char     nameBuffer[64] = "";
        static uint64_t lastInspectedID = 0;

        if (context.selectedObject->getID() != lastInspectedID)
        {
            lastInspectedID = context.selectedObject->getID();
            strncpy(nameBuffer, context.selectedObject->getName().c_str(),
                sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        }

        if (ImGui::InputText("Name##InspectorName", nameBuffer,
            IM_ARRAYSIZE(nameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue))
            context.selectedObject->setName(std::string(nameBuffer));

        ImGui::TextDisabled("Press Enter to confirm");
    }

    // Tags 
    if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& tags = context.selectedObject->getTags();

        if (tags.empty())
            ImGui::TextDisabled("No tags assigned");
        else
        {
            std::vector<std::string> tagVec(tags.begin(), tags.end());
            for (int i = 0; i < static_cast<int>(tagVec.size()); ++i)
            {
                if (i > 0) ImGui::SameLine();
                ImGui::PushID(i);
                std::string chipLabel = tagVec[i] + " x##InspTag";
                if (ImGui::SmallButton(chipLabel.c_str()))
                    context.selectedObject->removeTag(tagVec[i]);
                ImGui::PopID();
            }
        }

        static char inspTagInput[64] = "";
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##InspNewTag", inspTagInput, IM_ARRAYSIZE(inspTagInput));
        ImGui::SameLine();
        if (ImGui::Button("Add##InspAddTag") && inspTagInput[0] != '\0')
        {
            context.selectedObject->addTag(std::string(inspTagInput));
            inspTagInput[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All##InspClearTags"))
            context.selectedObject->clearTags();
    }

    ImGui::Separator();

    // Transform 
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (context.selectedObject->isRenderOnly())
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Physics: Disabled (Render Only)");
        else
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Physics: Enabled");

        // Position
        float posArr[3] = { pos.x, pos.y, pos.z };
        if (ImGui::DragFloat3("Position", posArr, 0.05f))
            context.selectedObject->setPosition(glm::vec3(posArr[0], posArr[1], posArr[2]));

        // Rotation
        glm::vec3 eulerDeg = glm::degrees(QuatToEulerRad(rot));
        float rotArr[3] = { eulerDeg.x, eulerDeg.y, eulerDeg.z };
        if (ImGui::DragFloat3("Rotation (deg)", rotArr, 0.5f))
        {
            glm::vec3 newRad = glm::radians(glm::vec3(rotArr[0], rotArr[1], rotArr[2]));
            context.selectedObject->setRotation(EulerRadToQuat(newRad));
        }

        // Scale
        float scaleArr[3] = { scale.x, scale.y, scale.z };
        if (ImGui::DragFloat3("Scale", scaleArr, 0.05f, 0.01f, 1000.0f))
        {
            if (context.scene.setObjectScale)
                context.scene.setObjectScale(context.selectedObject,
                    glm::vec3(scaleArr[0], scaleArr[1], scaleArr[2]));
        }

        // Physics hitbox scale
        if (context.selectedObject->hasPhysics())
        {
            glm::vec3 physScale = context.selectedObject->getPhysicsScale();
            float physScaleArr[3] = { physScale.x, physScale.y, physScale.z };

            if (ImGui::DragFloat3("Hit Box Scale", physScaleArr, 0.05f, 0.1f, 2.0f))
            {
                if (context.scene.setObjectPhysicsScale)
                    context.scene.setObjectPhysicsScale(context.selectedObject,
                        glm::vec3(physScaleArr[0], physScaleArr[1], physScaleArr[2]));
            }
            ImGui::TextDisabled("Collision box scale multiplier");
        }
    }

    //  Textures 
    if (ImGui::CollapsingHeader("Textures"))
    {
        static std::vector<std::string> inspTextures;
        static bool inspTexturesLoaded = false;
        if (!inspTexturesLoaded && context.scene.getAvailableTextures)
        {
            inspTextures = context.scene.getAvailableTextures();
            inspTexturesLoaded = true;
        }

        if (!inspTextures.empty())
        {
            std::vector<const char*> texNames;
            texNames.push_back("None");
            for (const auto& t : inspTextures)
                texNames.push_back(t.c_str());

            // Diffuse
            std::string currentDiffuse = context.selectedObject->getTexturePath();
            int diffuseIdx = 0;
            for (int i = 0; i < (int)inspTextures.size(); i++)
                if (inspTextures[i] == currentDiffuse) { diffuseIdx = i + 1; break; }

            if (ImGui::Combo("Diffuse##InspDiffuse", &diffuseIdx,
                texNames.data(), (int)texNames.size()))
            {
                context.selectedObject->setTexturePath(
                    diffuseIdx > 0 ? inspTextures[diffuseIdx - 1] : "");
            }

            // Specular
            std::string currentSpecular =
                context.selectedObject->getRender().getSpecularTexturePath();
            int specularIdx = 0;
            for (int i = 0; i < (int)inspTextures.size(); i++)
                if (inspTextures[i] == currentSpecular) { specularIdx = i + 1; break; }

            if (ImGui::Combo("Specular##InspSpecular", &specularIdx,
                texNames.data(), (int)texNames.size()))
            {
                context.selectedObject->getRender().setSpecularTexturePath(
                    specularIdx > 0 ? inspTextures[specularIdx - 1] : "");
            }

            // Normal map
            std::string currentNormal =
                context.selectedObject->getRender().getNormalTexturePath();
            int normalIdx = 0;
            for (int i = 0; i < (int)inspTextures.size(); i++)
                if (inspTextures[i] == currentNormal) { normalIdx = i + 1; break; }

            if (ImGui::Combo("Normal Map##InspNormal", &normalIdx,
                texNames.data(), (int)texNames.size()))
            {
                context.selectedObject->getRender().setNormalTexturePath(
                    normalIdx > 0 ? inspTextures[normalIdx - 1] : "");
            }
        }
        else
        {
            ImGui::TextDisabled("No textures found in textures/ folder");
        }
    }

    // Physics Material
    if (ImGui::CollapsingHeader("Physics Material") && context.selectedObject->hasPhysics())
    {
        btRigidBody* body = context.selectedObject->getRigidBody();
        std::string currentMat = context.selectedObject->getMaterialName();

        const auto& materials = context.physics.availableMaterials;
        if (!materials.empty())
        {
            int selectedMatIdx = -1;
            for (int i = 0; i < (int)materials.size(); i++)
                if (materials[i] == currentMat) { selectedMatIdx = i; break; }

            std::vector<const char*> matNames;
            for (const auto& m : materials) matNames.push_back(m.c_str());

            if (ImGui::Combo("Preset##InspMat", &selectedMatIdx,
                matNames.data(), (int)matNames.size()))
            {
                const std::string& newMat = materials[selectedMatIdx];
                const PhysicsMaterial& mat =
                    MaterialRegistry::getInstance().getMaterial(newMat);

                body->setFriction(mat.friction);
                body->setRestitution(mat.restitution);
                body->activate(true);
                context.selectedObject->getPhysics()->setMaterialName(newMat);
            }

            if (selectedMatIdx >= 0)
                ImGui::TextDisabled("Preset active: %s", currentMat.c_str());
            else
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                    "Custom override (no preset active)");
        }

        ImGui::Separator();
        ImGui::Text("Active Friction:    %.3f", body->getFriction());
        ImGui::Text("Active Restitution: %.3f", body->getRestitution());

        ImGui::Separator();
        ImGui::TextDisabled("Manual Override");
        ImGui::TextDisabled("Clears the active preset name");

        float friction = body->getFriction();
        float restitution = body->getRestitution();

        if (ImGui::SliderFloat("Friction##InspFric", &friction, 0.0f, 2.0f))
        {
            body->setFriction(friction);
            context.selectedObject->getPhysics()->setMaterialName("Custom");
        }
        if (ImGui::SliderFloat("Restitution##InspRest", &restitution, 0.0f, 1.0f))
        {
            body->setRestitution(restitution);
            context.selectedObject->getPhysics()->setMaterialName("Custom");
        }
    }

    //  Constraints 
    if (ImGui::CollapsingHeader("Constraints"))
    {
        auto constraints =
            ConstraintRegistry::getInstance().findConstraintsByObject(
                context.selectedObject);

        if (constraints.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "No constraints attached");
        }
        else
        {
            ImGui::Text("Attached Constraints: %d", (int)constraints.size());
            ImGui::Separator();

            for (int i = 0; i < (int)constraints.size(); ++i)
            {
                Constraint* c = constraints[i];
                ImGui::PushID(i);

                std::string label = c->getName().empty()
                    ? ("Constraint " + std::to_string(i))
                    : c->getName();

                if (ImGui::TreeNode(label.c_str()))
                {
                    // read only info
                    ImGui::Text("Type: %s", ConstraintTypeToString(c->getType()));

                    GameObject* bodyA = c->getBodyA();
                    GameObject* bodyB = c->getBodyB();
                    if (bodyA) ImGui::Text("Connected to A: ID %llu", bodyA->getID());
                    if (bodyB) ImGui::Text("Connected to B: ID %llu", bodyB->getID());
                    else       ImGui::Text("Connected to: World");

                    // enabled toggle
                    bool enabled = !c->isBroken();
                    if (ImGui::Checkbox("Enabled", &enabled))
                        c->setEnabled(enabled);
                    // Breakable settings
                    ImGui::Spacing();
                    ImGui::SeparatorText("Breaking");

                    bool isBreakable = c->isBreakable();
                    if (ImGui::Checkbox("Breakable##InspBreakable", &isBreakable))
                    {
                        if (isBreakable)
                            c->setBreakingThreshold(1000.0f, 1000.0f);
                        else
                            c->setBreakingThreshold(INFINITY, INFINITY);
                    }

                    if (c->isBreakable())
                    {
                        float breakForce = c->getBreakForce();
                        if (ImGui::DragFloat("Break Force##InspBreakForce", &breakForce, 10.0f, 0.0f, 100000.0f))
                            c->setBreakingThreshold(breakForce, c->getBreakTorque());

                        float breakTorque = c->getBreakTorque();
                        if (ImGui::DragFloat("Break Torque##InspBreakTorque", &breakTorque, 10.0f, 0.0f, 100000.0f))
                            c->setBreakingThreshold(c->getBreakForce(), breakTorque);
                    }
					// Type-specific editing
                    ImGui::Spacing();
                    ImGui::SeparatorText("Properties");
                    switch (c->getType())
                    {   
                        case ConstraintType::HINGE:
                        {
                            float angleDeg = glm::degrees(c->getHingeAngle());
                            ImGui::Text("Current Angle: %.1f deg", angleDeg);

                            btHingeConstraint* hinge =
                                static_cast<btHingeConstraint*>(c->getBulletConstraint());

                            static float      hingeLower = -90.0f;
                            static float      hingeUpper = 90.0f;
                            static bool       hingeUseLimits = false;
                            static bool       hingeMotorOn = false;
                            static float      hingeMotorVel = 1.0f;
                            static float      hingeMotorImp = 10.0f;
                            static Constraint* lastHinge = nullptr; // track which constraint is open

                            if (c != lastHinge && hinge)
                            {
                                lastHinge = c;
                                hingeUseLimits = hinge->hasLimit();
                                if (hingeUseLimits)
                                {
                                    hingeLower = glm::degrees(hinge->getLowerLimit());
                                    hingeUpper = glm::degrees(hinge->getUpperLimit());
                                }
                                // getEnableMotor is not publicly exposed in Bullet
                                // default to false and let the user toggle it on
                                hingeMotorOn = false;
                                hingeMotorVel = 1.0f;
                                hingeMotorImp = 10.0f;
                            }

                            if (ImGui::Checkbox("Use Limits##HingeLimits", &hingeUseLimits))
                            {
                                if (hingeUseLimits)
                                    c->setAngleLimits(glm::radians(hingeLower), glm::radians(hingeUpper));
                                else
                                    c->setAngleLimits(1.0f, -1.0f); // lower > upper = no limit in Bullet
                            }

                            if (hingeUseLimits)
                            {
                                bool limitsChanged = false;
                                limitsChanged |= ImGui::DragFloat("Lower (deg)##HingeLower", &hingeLower, 0.5f, -180.0f, 0.0f);
                                limitsChanged |= ImGui::DragFloat("Upper (deg)##HingeUpper", &hingeUpper, 0.5f, 0.0f, 180.0f);
                                if (limitsChanged)
                                    c->setAngleLimits(glm::radians(hingeLower), glm::radians(hingeUpper));
                            }

                            ImGui::Spacing();
                            if (ImGui::Checkbox("Motor##HingeMotor", &hingeMotorOn))
                            {
                                if (hingeMotorOn) c->enableMotor(hingeMotorVel, hingeMotorImp);
                                else              c->disableMotor();
                            }

                            if (hingeMotorOn)
                            {
                                bool motorChanged = false;
                                motorChanged |= ImGui::DragFloat("Target Velocity##HingeVel", &hingeMotorVel, 0.05f, -20.0f, 20.0f);
                                motorChanged |= ImGui::DragFloat("Max Impulse##HingeImp", &hingeMotorImp, 0.5f, 0.0f, 500.0f);
                                if (motorChanged)
                                    c->enableMotor(hingeMotorVel, hingeMotorImp);
                            }
                            break;
                        }

                        case ConstraintType::SLIDER:
                        {
                            ImGui::Text("Current Position: %.2f", c->getSliderPosition());

                            btSliderConstraint* slider =
                                static_cast<btSliderConstraint*>(c->getBulletConstraint());

                            static float       sliderLower = -5.0f;
                            static float       sliderUpper = 5.0f;
                            static bool        sliderUseLimits = false;
                            static bool        sliderMotorOn = false;
                            static float       sliderMotorVel = 1.0f;
                            static float       sliderMotorForce = 10.0f;
                            static Constraint* lastSlider = nullptr; // track which constraint is open

                            if (c != lastSlider && slider)
                            {
                                lastSlider = c;
                                sliderLower = slider->getLowerLinLimit();
                                sliderUpper = slider->getUpperLinLimit();
                                sliderUseLimits = sliderLower <= sliderUpper;
                                sliderMotorOn = slider->getPoweredLinMotor();
                                sliderMotorVel = slider->getTargetLinMotorVelocity();
                                sliderMotorForce = slider->getMaxLinMotorForce();
                            }

                            if (ImGui::Checkbox("Use Limits##SliderLimits", &sliderUseLimits))
                            {
                                if (sliderUseLimits) c->setLinearLimits(sliderLower, sliderUpper);
                                else                 c->setLinearLimits(1.0f, -1.0f);
                            }

                            if (sliderUseLimits)
                            {
                                bool limitsChanged = false;
                                limitsChanged |= ImGui::DragFloat("Lower##SliderLower", &sliderLower, 0.1f, -100.0f, 0.0f);
                                limitsChanged |= ImGui::DragFloat("Upper##SliderUpper", &sliderUpper, 0.1f, 0.0f, 100.0f);
                                if (limitsChanged)
                                    c->setLinearLimits(sliderLower, sliderUpper);
                            }

                            ImGui::Spacing();
                            if (ImGui::Checkbox("Motor##SliderMotor", &sliderMotorOn))
                            {
                                if (sliderMotorOn)  c->enableLinearMotor(sliderMotorVel, sliderMotorForce);
                                else if (slider)    slider->setPoweredLinMotor(false);
                            }

                            if (sliderMotorOn)
                            {
                                bool motorChanged = false;
                                motorChanged |= ImGui::DragFloat("Target Velocity##SliderVel", &sliderMotorVel, 0.05f, -20.0f, 20.0f);
                                motorChanged |= ImGui::DragFloat("Max Force##SliderForce", &sliderMotorForce, 0.5f, 0.0f, 500.0f);
                                if (motorChanged)
                                    c->enableLinearMotor(sliderMotorVel, sliderMotorForce);
                            }
                            break;
                        }

                        case ConstraintType::SPRING:
                        {
                            ImGui::TextDisabled("0-2 = Linear XYZ    3-5 = Angular XYZ");

                            btGeneric6DofSpringConstraint* spring =
                                static_cast<btGeneric6DofSpringConstraint*>(c->getBulletConstraint());

                            static bool  springEnabled[6] = {};
                            static float springStiffness[6] = { 100,100,100,100,100,100 };
                            static float springDamping[6] = { 0.3f,0.3f,0.3f,0.3f,0.3f,0.3f };

                            const char* axisNames[6] = {
                                "Linear X","Linear Y","Linear Z",
                                "Angular X","Angular Y","Angular Z"
                            };

                            for (int axis = 0; axis < 6; ++axis)
                            {
                                ImGui::PushID(axis);
                                ImGui::SeparatorText(axisNames[axis]);

                                bool changed = false;
                                changed |= ImGui::Checkbox("Active##SpringActive", &springEnabled[axis]);
                                if (springEnabled[axis])
                                {
                                    changed |= ImGui::DragFloat("Stiffness##SpringStiff", &springStiffness[axis], 1.0f, 0.0f, 10000.0f);
                                    changed |= ImGui::DragFloat("Damping##SpringDamp", &springDamping[axis], 0.01f, 0.0f, 1.0f);
                                }

                                if (changed && spring)
                                {
                                    spring->enableSpring(axis, springEnabled[axis]);
                                    if (springEnabled[axis])
                                    {
                                        c->setSpringStiffness(axis, springStiffness[axis]);
                                        c->setSpringDamping(axis, springDamping[axis]);
                                    }
                                }
                                ImGui::PopID();
                            }

                            ImGui::Spacing();
                            if (ImGui::Button("Reset Equilibrium Point##SpringEq", ImVec2(-1, 0)))
                                if (spring) spring->setEquilibriumPoint();
                            ImGui::TextDisabled("Resets spring rest position to current object position");
                            break;
                        }

                        case ConstraintType::FIXED:
                            ImGui::TextDisabled("Fixed constraints have no editable properties");
                            break;

                        case ConstraintType::GENERIC_6DOF:
                            ImGui::TextDisabled("Use Constraint Creator panel for 6DOF limit editing");
                            break;

                    default: break;
                    } 
					// remove constraint button
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1));
                    if (ImGui::Button("Remove Constraint", ImVec2(-1, 0)))
                        if (context.constraintCommands.removeConstraint)
                            context.constraintCommands.removeConstraint(c);
                    ImGui::PopStyleColor(3);

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

    // Delete 
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1));

    if (ImGui::Button("Delete Object", ImVec2(-1, 0)))
        if (context.scene.destroyObject)
            context.scene.destroyObject(context.selectedObject);

    ImGui::PopStyleColor(3);

    ImGui::End();
}