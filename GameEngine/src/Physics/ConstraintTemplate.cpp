/**
 * @file ConstraintTemplate.cpp
 * @brief Implementation of ConstraintTemplate and ConstraintTemplateRegistry.
 *
 * A ConstraintTemplate is a reusable blueprint that stores a constraint type
 * and its full parameter set (hinge limits, spring stiffness, etc.) without
 * being bound to any specific GameObjects. Templates are created once, saved
 * to disk, and then applied to arbitrary object pairs at runtime via
 * applyTemplate(), which delegates to the appropriate ConstraintPreset factory.
 *
 * ConstraintTemplateRegistry is a singleton that manages the in-memory
 * collection of templates and handles serialisation to a simple key=value
 * text file. It also provides smart pivot helpers that compute body-local
 * attachment points from object scale, removing the need to hard-code
 * offsets when applying templates to objects of varying sizes.
 */
#include "../include/Physics/ConstraintTemplate.h"
#include "../include/Physics/ConstraintPreset.h"
#include "../include/Scene/GameObject.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// Initialize static instance
ConstraintTemplateRegistry* ConstraintTemplateRegistry::instance = nullptr;

// ConstraintTemplate 
/**
 * @brief Constructs a named template of a given constraint type.
 *
 * All parameter structs (hingeParams, sliderParams, etc.) are value-initialised
 * to their defaults. Populate the relevant struct before passing the template
 * to addTemplate().
 *
 * @param templateName  Human-readable identifier used for lookup and file I/O.
 * @param templateType  Determines which parameter struct is active and which
 *                      ConstraintPreset factory applyTemplate() will call.
 */
ConstraintTemplate::ConstraintTemplate(const std::string& templateName, ConstraintType templateType)
    : name(templateName), type(templateType)
{
}

// ConstraintTemplateRegistry 
/**
 * @brief Constructs the registry with the default templates file path.
 *
 * The file path is used by save() and load() when no explicit path is given.
 */
ConstraintTemplateRegistry::ConstraintTemplateRegistry()
    : templatesFilePath("constraint_templates.txt")
{
}

ConstraintTemplateRegistry::~ConstraintTemplateRegistry() {
}

/**
 * @brief Returns the global ConstraintTemplateRegistry instance.
 *
 * Created on first call and never destroyed — lives for the application lifetime.
 *
 * @return Reference to the singleton instance.
 */
ConstraintTemplateRegistry& ConstraintTemplateRegistry::getInstance() {
    if (!instance) {
        instance = new ConstraintTemplateRegistry();
    }
    return *instance;
}

//  Template Management 
/**
 * @brief Adds a new template or replaces an existing one with the same name.
 *
 * Name collision results in an in-place update rather than a duplicate entry,
 * so it is safe to call addTemplate() repeatedly when reloading defaults or
 * refreshing a template from the editor.
 *
 * @param templ  The template to store. Copied into the internal vector.
 */
void ConstraintTemplateRegistry::addTemplate(const ConstraintTemplate& templ) {
    // Check if template with same name exists
    for (auto& existing : templates) {
        if (existing.name == templ.name) {
            std::cout << "Updating existing template: " << templ.name << std::endl;
            existing = templ;
            return;
        }
    }

    templates.push_back(templ);
    std::cout << "Added new template: " << templ.name << std::endl;
}

/**
 * @brief Removes the template with the given name.
 *
 * @param name  Name of the template to remove.
 * @return      True if found and removed, false if no template with that name exists.
 */
bool ConstraintTemplateRegistry::removeTemplate(const std::string& name) {
    auto it = std::find_if(templates.begin(), templates.end(),
        [&name](const ConstraintTemplate& t) { return t.name == name; });

    if (it != templates.end()) {
        templates.erase(it);
        std::cout << "Removed template: " << name << std::endl;
        return true;
    }

    return false;
}

/**
 * @brief Removes all templates from the in-memory collection.
 *
 * Does not affect the templates file on disk. Call save() afterward if
 * the cleared state should be persisted.
 */
void ConstraintTemplateRegistry::clearAllTemplates() {
    templates.clear();
    std::cout << "Cleared all constraint templates" << std::endl;
}

//  Queries 
/**
 * @brief Returns a const pointer to the template with the given name.
 *
 * Linear scan — template lists are expected to be small (< 100 entries)
 * so an unordered_map is not warranted here.
 *
 * @param name  Template name to search for.
 * @return      Pointer into the internal vector, or nullptr if not found.
 *              Valid until the next addTemplate / removeTemplate call.
 */
const ConstraintTemplate* ConstraintTemplateRegistry::getTemplate(const std::string& name) const {
    for (const auto& templ : templates) {
        if (templ.name == name) {
            return &templ;
        }
    }
    return nullptr;
}

/**
 * @brief Returns the names of all registered templates in storage order.
 *
 * Intended for populating editor dropdowns and debug listings.
 *
 * @return  Vector of name strings copied from the internal templates.
 */
std::vector<std::string> ConstraintTemplateRegistry::getTemplateNames() const {
    std::vector<std::string> names;
    names.reserve(templates.size());

    for (const auto& templ : templates) {
        names.push_back(templ.name);
    }

    return names;
}

/**
 * @brief Returns true if a template with the given name is registered.
 *
 * @param name  Name to check.
 */
bool ConstraintTemplateRegistry::hasTemplate(const std::string& name) const {
    return getTemplate(name) != nullptr;
}

//  Smart Pivot Calculation 
/**
 * @brief Computes a body-local attachment point scaled to the object's dimensions.
 *
 * When useEdge is true, relativeOffset is treated as a direction vector and
 * scaled by the object's half-extents, placing the pivot at the surface of
 * the bounding box. This is useful for hinges where the pivot should sit at
 * the edge of a door or panel rather than at the object's centre.
 *
 * When useEdge is false, relativeOffset is returned directly as a centre-
 * relative offset without any scaling.
 *
 * @param obj             The object to compute the pivot for. Returns (0,0,0)
 *                        if null.
 * @param relativeOffset  Direction/offset in the object's local space.
 * @param useEdge         True to scale by half-extents (edge attachment),
 *                        false for a raw offset (centre attachment).
 * @return                Body-local pivot position.
 */
glm::vec3 ConstraintTemplateRegistry::calculateSmartPivot(GameObject* obj, const glm::vec3& relativeOffset, bool useEdge) {
    if (!obj) return glm::vec3(0.0f);

    glm::vec3 scale = obj->getScale();

    // For edge attachment (like hinges), use half-extents
    if (useEdge) {
        return glm::vec3(
            relativeOffset.x * scale.x * 0.5f,
            relativeOffset.y * scale.y * 0.5f,
            relativeOffset.z * scale.z * 0.5f
        );
    }

    // For center attachment, just use the offset directly
    return relativeOffset;
}

/**
 * @brief Computes a sensible hinge pivot position on objA given the hinge axis.
 *
 * Identifies the dominant component of the axis vector and places the pivot
 * at the edge of objA's bounding box perpendicular to that axis:
 *   - Y-axis hinge (e.g. door): pivot placed at the left (-X) edge.
 *   - X-axis hinge: pivot placed at the front (-Z) edge.
 *   - Z-axis hinge: pivot placed at the left (-X) edge.
 *
 * This prevents the hinge pivot from sitting at the object centre, which
 * would cause the object to swing through itself.
 *
 * @param objA  The primary body. Returns (0,0,0) if null.
 * @param objB  The secondary body (unused currently, reserved for future
 *              relative-offset calculation).
 * @param axis  The intended hinge rotation axis in local space.
 * @return      Body-local pivot position for objA.
 */
glm::vec3 ConstraintTemplateRegistry::calculateHingePivot(GameObject* objA, GameObject* objB, const glm::vec3& axis) {
    if (!objA) return glm::vec3(0.0f);

    glm::vec3 scale = objA->getScale();

    // Determine which axis is the hinge axis and place pivot at edge
    glm::vec3 pivot(0.0f);

    // Find dominant axis
    float maxComponent = 0.0f;
    int dominantAxis = 0;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(axis[i]) > maxComponent) {
            maxComponent = std::abs(axis[i]);
            dominantAxis = i;
        }
    }

    // Place pivot at edge perpendicular to hinge axis
    // For a vertical hinge (Y axis), place at X edge
    // For a horizontal hinge (X or Z), place at appropriate edge
    if (dominantAxis == 1) { // Y-axis hinge
        pivot.x = -scale.x * 0.5f; // Left edge
    }
    else if (dominantAxis == 0) { // X-axis hinge
        pivot.z = -scale.z * 0.5f; // Front edge
    }
    else { // Z-axis hinge
        pivot.x = -scale.x * 0.5f; // Left edge
    }

    return pivot;
}

//  Apply Template 
/**
 * @brief Creates a Constraint by applying a named template to two GameObjects.
 *
 * Looks up the template, selects the matching ConstraintPreset factory, and
 * optionally auto-calculates pivot positions when the template's pivot is
 * near zero (indicating it should be derived from object scale rather than
 * stored as an absolute offset).
 *
 * Breaking thresholds stored in the template are forwarded to the new
 * Constraint after creation.
 *
 * The returned Constraint is not automatically registered — pass it to
 * ConstraintRegistry::addConstraint() to make it active in the simulation.
 *
 * @param templateName  Name of the template to apply.
 * @param objA          Primary body. Must have a physics component.
 * @param objB          Secondary body, or nullptr where supported (e.g. hinge
 *                      anchored to the world).
 * @return              Owning pointer to the new Constraint, or nullptr on
 *                      failure (template not found, objA missing physics, or
 *                      the underlying preset returns null).
 */
std::unique_ptr<Constraint> ConstraintTemplateRegistry::applyTemplate(
    const std::string& templateName,
    GameObject* objA,
    GameObject* objB)
{
    const ConstraintTemplate* templ = getTemplate(templateName);
    if (!templ) {
        std::cerr << "Error: Template '" << templateName << "' not found" << std::endl;
        return nullptr;
    }

    if (!objA || !objA->hasPhysics()) {
        std::cerr << "Error: Cannot apply template - objA missing or no physics" << std::endl;
        return nullptr;
    }

    std::unique_ptr<Constraint> constraint;

    switch (templ->type) {
    case ConstraintType::FIXED:
        constraint = ConstraintPreset::createFixed(objA, objB);
        break;

    case ConstraintType::HINGE: {
        HingeParams params = templ->hingeParams;

        // Recalculate pivots based on object size if using relative positioning
        if (glm::length(params.pivotA) < 0.01f) {
            params.pivotA = calculateHingePivot(objA, objB, params.axisA);
        }

        if (objB && glm::length(params.pivotB) < 0.01f) {
            params.pivotB = calculateHingePivot(objB, objA, params.axisB);
        }

        constraint = ConstraintPreset::createHinge(objA, objB, params);
        break;
    }

    case ConstraintType::SLIDER: {
        SliderParams params = templ->sliderParams;
        constraint = ConstraintPreset::createSlider(objA, objB, params);
        break;
    }

    case ConstraintType::SPRING: {
        SpringParams params = templ->springParams;
        constraint = ConstraintPreset::createSpring(objA, objB, params);
        break;
    }

    case ConstraintType::GENERIC_6DOF: {
        Generic6DofParams params = templ->dofParams;
        constraint = ConstraintPreset::createGeneric6Dof(objA, objB, params);
        break;
    }
    }

    if (constraint) {
        constraint->setName(templ->name);

        if (templ->breakable) {
            constraint->setBreakingThreshold(templ->breakForce, templ->breakTorque);
        }

        std::cout << "Applied template '" << templateName << "' to create constraint" << std::endl;
    }

    return constraint;
}

//  File I/O (Simple Text Format) 
/**
 * @brief Serialises all templates to a simple key=value text file.
 *
 * Format uses TEMPLATE_START / TEMPLATE_END delimiters with one key=value
 * pair per line. Type-specific parameters are prefixed with the type name
 * (e.g. hinge_pivotA, spring_stiffness0) to avoid key collisions.
 *
 * Only the types with non-trivial parameters (Hinge, Slider, Spring) write
 * type-specific lines; Fixed and Generic6DOF rely on common fields only.
 *
 * @param filepath  Destination file path. Parent directory must exist.
 * @return          True on success, false if the file could not be opened.
 */
bool ConstraintTemplateRegistry::saveToFile(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filepath << std::endl;
        return false;
    }

    file << "# Constraint Templates\n";
    file << "# Format: NAME|TYPE|PARAMS...\n\n";
    file << "COUNT=" << templates.size() << "\n\n";

    for (const auto& templ : templates) {
        file << "TEMPLATE_START\n";
        file << "name=" << templ.name << "\n";
        file << "type=" << static_cast<int>(templ.type) << "\n";
        file << "breakable=" << (templ.breakable ? 1 : 0) << "\n";
        file << "breakForce=" << templ.breakForce << "\n";
        file << "breakTorque=" << templ.breakTorque << "\n";
        file << "description=" << templ.description << "\n";

        // Type-specific parameters
        switch (templ.type) {
        case ConstraintType::HINGE: {
            const auto& p = templ.hingeParams;
            file << "hinge_pivotA=" << p.pivotA.x << "," << p.pivotA.y << "," << p.pivotA.z << "\n";
            file << "hinge_pivotB=" << p.pivotB.x << "," << p.pivotB.y << "," << p.pivotB.z << "\n";
            file << "hinge_axisA=" << p.axisA.x << "," << p.axisA.y << "," << p.axisA.z << "\n";
            file << "hinge_axisB=" << p.axisB.x << "," << p.axisB.y << "," << p.axisB.z << "\n";
            file << "hinge_useLimits=" << (p.useLimits ? 1 : 0) << "\n";
            file << "hinge_lowerLimit=" << p.lowerLimit << "\n";
            file << "hinge_upperLimit=" << p.upperLimit << "\n";
            file << "hinge_useMotor=" << (p.useMotor ? 1 : 0) << "\n";
            file << "hinge_motorVelocity=" << p.motorTargetVelocity << "\n";
            file << "hinge_motorImpulse=" << p.motorMaxImpulse << "\n";
            break;
        }
        case ConstraintType::SLIDER: {
            const auto& p = templ.sliderParams;
            file << "slider_framePosA=" << p.frameAPos.x << "," << p.frameAPos.y << "," << p.frameAPos.z << "\n";
            file << "slider_framePosB=" << p.frameBPos.x << "," << p.frameBPos.y << "," << p.frameBPos.z << "\n";
            file << "slider_useLimits=" << (p.useLimits ? 1 : 0) << "\n";
            file << "slider_lowerLimit=" << p.lowerLimit << "\n";
            file << "slider_upperLimit=" << p.upperLimit << "\n";
            file << "slider_useMotor=" << (p.useMotor ? 1 : 0) << "\n";
            file << "slider_motorVelocity=" << p.motorTargetVelocity << "\n";
            file << "slider_motorForce=" << p.motorMaxForce << "\n";
            break;
        }
        case ConstraintType::SPRING: {
            const auto& p = templ.springParams;
            file << "spring_pivotA=" << p.pivotA.x << "," << p.pivotA.y << "," << p.pivotA.z << "\n";
            file << "spring_pivotB=" << p.pivotB.x << "," << p.pivotB.y << "," << p.pivotB.z << "\n";
            for (int i = 0; i < 6; ++i) {
                file << "spring_enabled" << i << "=" << (p.enableSpring[i] ? 1 : 0) << "\n";
                file << "spring_stiffness" << i << "=" << p.stiffness[i] << "\n";
                file << "spring_damping" << i << "=" << p.damping[i] << "\n";
            }
            break;
        }
        default:
            break;
        }

        file << "TEMPLATE_END\n\n";
    }

    file.close();
    std::cout << "Saved " << templates.size() << " templates to " << filepath << std::endl;
    return true;
}
/**
 * @brief Loads templates from a key=value text file, replacing the current set.
 *
 * Clears the in-memory collection before parsing. Lines beginning with '#'
 * and empty lines are skipped. Each template block is delimited by
 * TEMPLATE_START and TEMPLATE_END tokens. Unknown keys are silently ignored
 * so older files remain forward-compatible.
 *
 * Returns false (non-fatal) if the file does not exist — this is expected on
 * first run before any templates have been saved.
 *
 * @param filepath  Source file path.
 * @return          True if the file was opened and parsed successfully.
 */
bool ConstraintTemplateRegistry::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "Note: No template file found at " << filepath << " (this is normal for first run)" << std::endl;
        return false;
    }

    templates.clear();

    std::string line;
    ConstraintTemplate currentTemplate;
    bool inTemplate = false;

    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        if (line == "TEMPLATE_START") {
            inTemplate = true;
            currentTemplate = ConstraintTemplate();
            continue;
        }

        if (line == "TEMPLATE_END") {
            if (inTemplate) {
                templates.push_back(currentTemplate);
                inTemplate = false;
            }
            continue;
        }

        if (!inTemplate) continue;

        // Parse key=value pairs
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Parse common fields
        if (key == "name") currentTemplate.name = value;
        else if (key == "type") currentTemplate.type = static_cast<ConstraintType>(std::stoi(value));
        else if (key == "breakable") currentTemplate.breakable = (std::stoi(value) != 0);
        else if (key == "breakForce") currentTemplate.breakForce = std::stof(value);
        else if (key == "breakTorque") currentTemplate.breakTorque = std::stof(value);
        else if (key == "description") currentTemplate.description = value;

        // Parse hinge parameters
        else if (key == "hinge_pivotA") {
            std::istringstream ss(value);
            char comma;
            ss >> currentTemplate.hingeParams.pivotA.x >> comma
                >> currentTemplate.hingeParams.pivotA.y >> comma
                >> currentTemplate.hingeParams.pivotA.z;
        }
        else if (key == "hinge_axisA") {
            std::istringstream ss(value);
            char comma;
            ss >> currentTemplate.hingeParams.axisA.x >> comma
                >> currentTemplate.hingeParams.axisA.y >> comma
                >> currentTemplate.hingeParams.axisA.z;
        }
        else if (key == "hinge_useLimits") currentTemplate.hingeParams.useLimits = (std::stoi(value) != 0);
        else if (key == "hinge_lowerLimit") currentTemplate.hingeParams.lowerLimit = std::stof(value);
        else if (key == "hinge_upperLimit") currentTemplate.hingeParams.upperLimit = std::stof(value);
        else if (key == "hinge_useMotor") currentTemplate.hingeParams.useMotor = (std::stoi(value) != 0);
        else if (key == "hinge_motorVelocity") currentTemplate.hingeParams.motorTargetVelocity = std::stof(value);
        else if (key == "hinge_motorImpulse") currentTemplate.hingeParams.motorMaxImpulse = std::stof(value);

        // Parse slider parameters
        else if (key == "slider_useLimits") currentTemplate.sliderParams.useLimits = (std::stoi(value) != 0);
        else if (key == "slider_lowerLimit") currentTemplate.sliderParams.lowerLimit = std::stof(value);
        else if (key == "slider_upperLimit") currentTemplate.sliderParams.upperLimit = std::stof(value);

        // Parse spring parameters
        else if (key.find("spring_enabled") == 0) {
            int axis = std::stoi(key.substr(14)); // "spring_enabled"
            currentTemplate.springParams.enableSpring[axis] = (std::stoi(value) != 0);
        }
        else if (key.find("spring_stiffness") == 0) {
            int axis = std::stoi(key.substr(16));
            currentTemplate.springParams.stiffness[axis] = std::stof(value);
        }
        else if (key.find("spring_damping") == 0) {
            int axis = std::stoi(key.substr(14));
            currentTemplate.springParams.damping[axis] = std::stof(value);
        }
    }

    file.close();
    std::cout << "Loaded " << templates.size() << " templates from " << filepath << std::endl;
    return true;
}
/**
 * @brief Saves templates to the default file path (constraint_templates.txt).
 *
 * Convenience wrapper around saveToFile() using the path set at construction.
 *
 * @return  True on success.
 */
bool ConstraintTemplateRegistry::save() {
    return saveToFile(templatesFilePath);
}
/**
 * @brief Loads templates from the default file path (constraint_templates.txt).
 *
 * Convenience wrapper around loadFromFile() using the path set at construction.
 *
 * @return  True if the file was found and loaded successfully.
 */
bool ConstraintTemplateRegistry::load() {
    return loadFromFile(templatesFilePath);
}

// Default Templates
/**
 * @brief Populates the registry with built-in default templates.
 *
 * Currently empty — add common presets here (e.g. "door_hinge", "piston",
 * "suspension") so new scenes have sensible starting points without requiring
 * a saved template file.
 */
void ConstraintTemplateRegistry::initializeDefaults() {
 }

//  Debug 
/**
 * @brief Prints a summary of all registered templates to stdout.
 *
 * Lists each template's name, type integer, optional description, and
 * breaking thresholds if the template is breakable.
 */
void ConstraintTemplateRegistry::printTemplates() const {
    std::cout << "\n=== Constraint Templates ===" << std::endl;
    std::cout << "Total templates: " << templates.size() << std::endl;

    for (const auto& templ : templates) {
        std::cout << "\n- " << templ.name << " (" << static_cast<int>(templ.type) << ")" << std::endl;
        if (!templ.description.empty()) {
            std::cout << "  Description: " << templ.description << std::endl;
        }
        if (templ.breakable) {
            std::cout << "  Breakable: force=" << templ.breakForce
                << ", torque=" << templ.breakTorque << std::endl;
        }
    }

    std::cout << "===========================\n" << std::endl;
}