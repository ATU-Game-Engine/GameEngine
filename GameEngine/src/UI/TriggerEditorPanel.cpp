#include "../include/UI/TriggerEditorPanel.h"
#include "../include/Physics/Trigger.h"
#include "../include/Physics/TriggerRegistry.h"
#include "../External/imgui/core/imgui.h"
#include <algorithm>
#include <iostream>

static const char* TriggerTypeToString(TriggerType type)
{
    switch (type)
    {
    case TriggerType::TELEPORT:   return "Teleport";
    case TriggerType::SPEED_ZONE: return "Speed Zone";
    case TriggerType::EVENT:     return "Custom";
    default:                      return "Unknown";
    }
}

static TriggerType IndexToTriggerType(int index)
{
    switch (index)
    {
    case 0:  return TriggerType::TELEPORT;
    case 1:  return TriggerType::SPEED_ZONE;
    default: return TriggerType::EVENT;
    }
}

void DrawTriggerEditorPanel(DebugUIContext& context)
{
    ImGui::Begin("Trigger Editor");

    // All persistent UI state as statics - same pattern as other panels
    static Trigger* selectedTrigger = nullptr;
    static char     newTriggerName[128] = "Trigger_1";
    static int      selectedTypeIndex = 0;
    static float    newPosition[3] = { 0.0f, 2.0f, 0.0f };
    static float    newSize[3] = { 2.0f, 2.0f, 2.0f };
    static float    teleportDest[3] = { 0.0f, 0.0f, 0.0f };
    static float    forceDir[3] = { 0.0f, 1.0f, 0.0f };
    static float    forceMag = 10.0f;

    static int maxUses = -1;
    static int selectedUseIndex = 0;

	// assign behaviour to event triggers
    static char behaviourTagBuf[64] = "";
	// Tag management for new trigger creation
    static char newTrigTagInput[64] = "";
    static std::vector<std::string> newTriggerTags;
    // Refresh list every frame
    const std::vector<Trigger*> allTriggers = TriggerRegistry::getInstance().getAllTriggers();

    // Validate selectedTrigger is still alive
    if (selectedTrigger)
    {
        bool stillValid = std::find(allTriggers.begin(), allTriggers.end(),
            selectedTrigger) != allTriggers.end();
        if (!stillValid)
            selectedTrigger = nullptr;
    }

    if (ImGui::BeginTabBar("TriggerEditorTabs"))
    {
        // ================= CREATE TAB =================
        if (ImGui::BeginTabItem("Create"))
        {
            ImGui::SeparatorText("New Trigger");
            ImGui::InputText("Name", newTriggerName, sizeof(newTriggerName));

            const char* triggerTypes[] =
            {  "Teleport", "Speed Zone", "EVENT" };
            ImGui::Combo("Type", &selectedTypeIndex, triggerTypes, IM_ARRAYSIZE(triggerTypes));

            ImGui::Separator();
            ImGui::TextWrapped("Description:");
            switch (selectedTypeIndex)
            {
            case 0: ImGui::TextWrapped("Instantly moves objects to a destination."); break;
            case 1: ImGui::TextWrapped("Applies force to objects entering."); break;
            case 2: ImGui::TextWrapped("Fires enter/exit/stay callbacks only. "
                "Behavior must be set in code via setOnEnterCallback()."); break;
            }

            ImGui::Separator();
            ImGui::DragFloat3("Position", newPosition, 0.1f);
            ImGui::DragFloat3("Size (Half Extents)", newSize, 0.1f, 0.1f, 100.0f);

            ImGui::Separator();

            ImGui::SeparatorText("Usage");

            const char* useOptions[] = { "Infinite", "1", "2", "3", "5", "10" };
            ImGui::Combo("Uses", &selectedUseIndex, useOptions, IM_ARRAYSIZE(useOptions));

            switch (selectedUseIndex)
            {
            case 0: maxUses = -1; break;
            case 1: maxUses = 1; break;
            case 2: maxUses = 2; break;
            case 3: maxUses = 3; break;
            case 4: maxUses = 5; break;
            case 5: maxUses = 10; break;
            }

            ImGui::SeparatorText("Type-Specific Settings");

            TriggerType selectedType = IndexToTriggerType(selectedTypeIndex);
            if (selectedType == TriggerType::TELEPORT)
            {
                ImGui::DragFloat3("Teleport Destination", teleportDest, 0.1f);
            }
            else if (selectedType == TriggerType::SPEED_ZONE)
            {
                ImGui::DragFloat3("Force Direction", forceDir, 0.01f, -1.0f, 1.0f);
                ImGui::DragFloat("Force Magnitude", &forceMag, 0.5f, 0.0f, 1000.0f);
            }
            else
            {
                ImGui::SeparatorText("Behaviour");
                ImGui::InputText("Behaviour##BehaviourTagCreate", behaviourTagBuf, sizeof(behaviourTagBuf));
                ImGui::TextDisabled("Must match a tag registered via registerTriggerScript()");
                ImGui::TextDisabled("e.g.  bounce_pad   sphere_spawner");
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Object Filter (Optional)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextDisabled("Only objects with ALL listed tags will activate this trigger");
                ImGui::Spacing();

                if (newTriggerTags.empty())
                {
                    ImGui::TextDisabled("No tags — trigger affects all objects");
                }
                else
                {
                    ImGui::Text("Required tags:");
                    for (int i = 0; i < static_cast<int>(newTriggerTags.size()); ++i)
                    {
                        if (i > 0) ImGui::SameLine();
                        ImGui::PushID(i);
                        std::string chipLabel = newTriggerTags[i] + "  x##NewTrigTag";
                        if (ImGui::SmallButton(chipLabel.c_str()))
                            newTriggerTags.erase(newTriggerTags.begin() + i);
                        ImGui::PopID();
                    }
                }

                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputText("##NewTrigTagInput", newTrigTagInput, sizeof(newTrigTagInput));
                ImGui::SameLine();
                if (ImGui::Button("Add##NewTrigAddTag") && newTrigTagInput[0] != '\0')
                {
                    if (std::find(newTriggerTags.begin(), newTriggerTags.end(),
                        std::string(newTrigTagInput)) == newTriggerTags.end())
                        newTriggerTags.push_back(std::string(newTrigTagInput));
                    newTrigTagInput[0] = '\0';
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear All##NewTrigClearTags"))
                    newTriggerTags.clear();
            }
            ImGui::Separator();

            if (ImGui::Button("Create Trigger", ImVec2(-1, 0)))
            {
                if (context.triggerCommands.createTrigger)
                {
                    Trigger* newTrigger = context.triggerCommands.createTrigger(
                        newTriggerName,
                        selectedType,
                        glm::vec3(newPosition[0], newPosition[1], newPosition[2]),
                        glm::vec3(newSize[0], newSize[1], newSize[2]));

                    if (newTrigger)
                    {
                        newTrigger->setMaxUses(maxUses);

                        if (selectedType == TriggerType::TELEPORT)
                            context.triggerCommands.setTeleportDestination(newTrigger,
                                glm::vec3(teleportDest[0], teleportDest[1], teleportDest[2]));
                        else if (selectedType == TriggerType::SPEED_ZONE)
                            context.triggerCommands.setForce(newTrigger,
                                glm::vec3(forceDir[0], forceDir[1], forceDir[2]), forceMag);
                        else if (selectedType == TriggerType::EVENT && behaviourTagBuf[0] != '\0')
                        {
                            newTrigger->setBehaviourTag(std::string(behaviourTagBuf));
                            if (context.applyTriggerScripts)
                                context.applyTriggerScripts();
                        }
						// Apply tags
                        for (const auto& tag : newTriggerTags)
                            newTrigger->requireTag(tag);
                        newTriggerTags.clear();

                        selectedTrigger = newTrigger;

                        // Reset fields
                        strcpy(newTriggerName, "Trigger_1");
                        newPosition[0] = 0.0f; newPosition[1] = 2.0f; newPosition[2] = 0.0f;
                        newSize[0] = 2.0f;     newSize[1] = 2.0f;     newSize[2] = 2.0f;
                        behaviourTagBuf[0] = '\0';
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // ================= LIST TAB =================
        if (ImGui::BeginTabItem("List"))
        {
            if (allTriggers.empty())
            {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No triggers in scene");
            }
            else
            {
                ImGui::Text("Total triggers: %zu", allTriggers.size());
                ImGui::Separator();

                for (Trigger* trigger : allTriggers)
                {
                    if (!trigger) continue;

                    bool isSelected = (selectedTrigger == trigger);
                    ImGui::PushID(static_cast<int>(trigger->getID()));

                    if (ImGui::Selectable(trigger->getName().c_str(), isSelected))
                        selectedTrigger = trigger;

                    ImGui::PopID();
                }
            }

            ImGui::EndTabItem();
        }

        // ================= PROPERTIES TAB =================
        if (ImGui::BeginTabItem("Properties"))
        {
            if (selectedTrigger)
            {
                Trigger* trigger = selectedTrigger;

                ImGui::SeparatorText("Trigger Properties");
                ImGui::Text("ID: %llu", static_cast<unsigned long long>(trigger->getID()));
                ImGui::Text("Type: %s", TriggerTypeToString(trigger->getType()));

                ImGui::Separator();

                // Editable name - sync buffer when selection changes
                static char    nameBuffer[128] = {};
                static Trigger* lastEdited = nullptr;
                if (lastEdited != trigger)
                {
                    strncpy(nameBuffer, trigger->getName().c_str(), sizeof(nameBuffer) - 1);
                    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                    lastEdited = trigger;
                }
                if (ImGui::InputText("Name##edit", nameBuffer, sizeof(nameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue))
                    trigger->setName(nameBuffer);
                ImGui::SameLine();
                ImGui::TextDisabled("(press Enter)");

                ImGui::Separator();

                glm::vec3 pos = trigger->getPosition();
                if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
                    trigger->setPosition(pos);

                glm::vec3 size = trigger->getSize();
                if (ImGui::DragFloat3("Size (Half Extents)", &size.x, 0.1f, 0.1f, 100.0f))
                    trigger->setSize(size);

                ImGui::SeparatorText("Usage");

                int maxUses = trigger->getMaxUses();
                int useCount = trigger->getUseCount();

                if (maxUses < 0)
                {
                    ImGui::Text("Uses: %d / Infinite", useCount);
                }
                else
                {
                    ImGui::Text("Uses: %d / %d", useCount, maxUses);
                }

                ImGui::Separator();

                bool enabled = trigger->isEnabled();
                if (ImGui::Checkbox("Enabled", &enabled))
                    trigger->setEnabled(enabled);

                ImGui::Separator();
                ImGui::SeparatorText("Type-Specific");

                if (trigger->getType() == TriggerType::TELEPORT)
                {
                    glm::vec3 dest = trigger->getTeleportDestination();
                    if (ImGui::DragFloat3("Destination", &dest.x, 0.1f))
                        trigger->setTeleportDestination(dest);
                }
                else if (trigger->getType() == TriggerType::SPEED_ZONE){
                    glm::vec3 dir = trigger->getForceDirection();
                    float mag = trigger->getForceMagnitude();
                    bool changed = false;
                    changed |= ImGui::DragFloat3("Force Direction", &dir.x, 0.01f, -1.0f, 1.0f);
                    changed |= ImGui::DragFloat("Force Magnitude", &mag, 0.5f, 0.0f, 1000.0f);
                    if (changed)
                        trigger->setForce(dir, mag);
                }
                else {
                    ImGui::SeparatorText("Behaviour");

                    static char behaviourEditBuf[64] = {};
                    static Trigger* lastBehaviourEdited = nullptr;
                    if (lastBehaviourEdited != trigger)
                    {
                        strncpy(behaviourEditBuf, trigger->getBehaviourTag().c_str(),
                            sizeof(behaviourEditBuf) - 1);
                        behaviourEditBuf[sizeof(behaviourEditBuf) - 1] = '\0';
                        lastBehaviourEdited = trigger;
                    }
                    if (ImGui::InputText("Behaviour Tag##EditBehaviour", behaviourEditBuf,sizeof(behaviourEditBuf), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        trigger->setBehaviourTag(std::string(behaviourEditBuf));
                        if (context.applyTriggerScripts)
                            context.applyTriggerScripts();
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(press Enter)");
                    ImGui::TextDisabled("Must match a registerTriggerScript() binding");

                    if (trigger->getBehaviourTag().empty())
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                            "No behaviour tag set — trigger will do nothing");
                }

                ImGui::Separator();

                if (ImGui::CollapsingHeader("Object Filter", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (trigger->hasTagFilter()) {
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "Active — only tagged objects affected");
                    }
                    else {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No filter — affects all objects");
                    }

                    ImGui::Spacing();

                    // Show current required tags as removable chips
                    const auto& reqTags = trigger->getRequiredTags();
                    if (reqTags.empty()) {
                        ImGui::TextDisabled("No required tags set");
                    }
                    else {
                        ImGui::Text("Required tags:");
                        std::vector<std::string> tagVec(reqTags.begin(), reqTags.end());
                        for (int i = 0; i < static_cast<int>(tagVec.size()); ++i) {
                            if (i > 0) ImGui::SameLine();
                            ImGui::PushID(i);
                            std::string chipLabel = tagVec[i] + "  x##TrigTag";
                            if (ImGui::SmallButton(chipLabel.c_str()))
                                trigger->removeRequiredTag(tagVec[i]);
                            ImGui::PopID();
                        }
                    }

                    // Add tag input
                    static char trigTagInput[64] = "";
                    ImGui::SetNextItemWidth(140.0f);
                    ImGui::InputText("##TrigNewTag", trigTagInput, sizeof(trigTagInput));
                    ImGui::SameLine();
                    if (ImGui::Button("Add##TrigAddTag") && trigTagInput[0] != '\0') {
                        trigger->requireTag(std::string(trigTagInput));
                        trigTagInput[0] = '\0';
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear All##TrigClearTags")) {
                        trigger->clearRequiredTags();
                    }

                    ImGui::TextDisabled("Object must have ALL listed tags to activate trigger");
                }
                ImGui::Separator();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

                if (ImGui::Button("Delete Trigger", ImVec2(-1, 0)))
                {
                    if (context.triggerCommands.removeTrigger)
                    {
                        context.triggerCommands.removeTrigger(trigger);
                        selectedTrigger = nullptr;
                    }
                }

                ImGui::PopStyleColor(3);
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No trigger selected");
                ImGui::TextDisabled("Select a trigger from the List tab.");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}