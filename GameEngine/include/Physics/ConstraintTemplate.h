/**
 * @file ConstraintTemplate.h
 * @brief Declares ConstraintTemplate (a reusable constraint blueprint) and
 *        ConstraintTemplateRegistry (the singleton that stores and applies them).
 *
 * A ConstraintTemplate bundles a constraint type and its full parameter set
 * without being bound to specific GameObjects. Templates are created once,
 * persisted to a simple key=value text file, and applied to arbitrary object
 * pairs at runtime via applyTemplate(), which delegates to the appropriate
 * ConstraintPreset factory.
 *
 * The registry also provides smart pivot helpers that derive body-local
 * attachment points from object scale, so templates remain correct when
 * applied to objects of varying sizes.
 */
#ifndef CONSTRAINTTEMPLATE_H
#define CONSTRAINTTEMPLATE_H

#include "ConstraintParams.h"
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class GameObject;
class Constraint;

/**
 * @brief A reusable constraint configuration that can be applied to any
 *        pair of GameObjects.
 *
 * Stores the constraint type and all type-specific parameters (hingeParams,
 * sliderParams, springParams, dofParams) along with optional breaking thresholds
 * and a human-readable description. Only the parameter struct matching `type`
 * is meaningful when the template is applied; the others are value-initialised
 * to defaults and ignored.
 *
 * Pivots stored in the parameter structs are treated as relative offsets —
 * ConstraintTemplateRegistry::applyTemplate() recalculates them based on the
 * target objects' actual scale at apply time when the stored pivot is near zero.
 */
struct ConstraintTemplate {
    std::string name;  ///< Unique identifier used for lookup and file I/O.
    ConstraintType type;  ///< Determines which parameter struct is used on apply.

    // Type-specific parameters
    HingeParams hingeParams; ///< Used when type == HINGE.
    SliderParams sliderParams; ///< Used when type == SLIDER.
    SpringParams springParams;  ///< Used when type == SPRING.
    Generic6DofParams dofParams; ///< Used when type == GENERIC_6DOF.

    // Breaking settings
    bool breakable = false;  ///< True if the created constraint should have a break threshold.
    float breakForce = 1000.0f;  ///< Maximum linear impulse (N·s) before the joint breaks.
    float breakTorque = 1000.0f;  ///< Maximum angular impulse (N·m·s) before the joint breaks.

    // Metadata
    std::string description;  ///< Optional human-readable description shown in the editor.

    /**
     * @brief Default constructor — value-initialises all parameter structs.
     *
     * Required so ConstraintTemplate can be used as a local variable during
     * file parsing without an explicit initialiser.
     */
    ConstraintTemplate() = default;

    /**
    * @brief Constructs a named template of a given constraint type.
    *
    * @param templateName  Unique name for registry lookup and serialisation.
    * @param templateType  Determines which ConstraintPreset factory is called
    *                      by applyTemplate().
    */
    ConstraintTemplate(const std::string& templateName, ConstraintType templateType);
};

/**
 * @brief Singleton registry that stores, persists, and applies constraint templates.
 *
 * Manages an in-memory list of ConstraintTemplate objects and handles
 * serialisation to a simple key=value text file (constraint_templates.txt by
 * default). Provides smart pivot helpers that compute body-local attachment
 * points from object scale, so templates do not need hard-coded offsets.
 *
 * Non-copyable. Obtain the instance via ConstraintTemplateRegistry::getInstance().
 */
class ConstraintTemplateRegistry {
private:
    static ConstraintTemplateRegistry* instance; ///< Singleton pointer.

    std::vector<ConstraintTemplate> templates;  ///< In-memory template list.
    std::string templatesFilePath;    ///< Default file path for save() / load().

    /// Private constructor — sets the default file path and seeds no templates.
    ConstraintTemplateRegistry();

    /**
    * @brief Computes a body-local attachment point scaled to the object's bounds.
    *
    * When useEdge is true, relativeOffset is scaled by the object's half-extents
    * to place the pivot at the surface of the bounding box (useful for hinges
    * that should sit at the edge of a door panel rather than its centre).
    *
    * @param obj             Object to compute the pivot for. Returns (0,0,0) if null.
    * @param relativeOffset  Direction or offset in local space.
    * @param useEdge         True to scale by half-extents; false for a raw offset.
    * @return                Body-local pivot position.
    */
    glm::vec3 calculateSmartPivot(GameObject* obj, const glm::vec3& relativeOffset, bool useEdge = false);

    /**
    * @brief Computes a sensible hinge pivot on objA given the hinge axis.
    *
    * Places the pivot at the edge of objA's bounding box perpendicular to the
    * dominant axis component, preventing the hinge pivot from sitting at the
    * object centre (which would cause it to swing through itself).
    *
    * @param objA  Primary body. Returns (0,0,0) if null.
    * @param objB  Secondary body (reserved for future relative-offset use).
    * @param axis  The intended hinge rotation axis in local space.
    * @return      Body-local pivot position for objA.
    */
    glm::vec3 calculateHingePivot(GameObject* objA, GameObject* objB, const glm::vec3& axis);

public:
    /**
     * @brief Destructor — no dynamic resources to release (templates are value types).
     */
    ~ConstraintTemplateRegistry();

    /// Non-copyable.
    ConstraintTemplateRegistry(const ConstraintTemplateRegistry&) = delete;
    ConstraintTemplateRegistry& operator=(const ConstraintTemplateRegistry&) = delete;


    /**
     * @brief Returns the global ConstraintTemplateRegistry instance.
     *
     * Created on first call, lives for the application lifetime.
     *
     * @return Reference to the singleton.
     */
    static ConstraintTemplateRegistry& getInstance();

    // Template management

     /**
     * @brief Adds a new template or replaces an existing one with the same name.
     *
     * Safe to call repeatedly — name collision results in an in-place update
     * rather than a duplicate entry.
     *
     * @param templ  Template to store. Copied into the internal vector.
     */
    void addTemplate(const ConstraintTemplate& templ);

    /**
     * @brief Removes the template with the given name.
     *
     * @param name  Name of the template to remove.
     * @return      True if found and removed, false otherwise.
     */
    bool removeTemplate(const std::string& name);

    /**
     * @brief Removes all templates from the in-memory collection.
     *
     * Does not affect the templates file on disk.
     */
    void clearAllTemplates();

    // Queries
     /**
     * @brief Returns a const pointer to the named template, or nullptr if not found.
     *
     * Pointer is valid until the next addTemplate / removeTemplate call.
     *
     * @param name  Case-sensitive template name.
     */
    const ConstraintTemplate* getTemplate(const std::string& name) const;

    /**
    * @brief Returns the names of all registered templates in storage order.
    *
    * Intended for populating editor dropdowns.
    *
    * @return  Vector of name strings.
    */
    std::vector<std::string> getTemplateNames() const;

    /// Returns a const reference to the full template list.
    const std::vector<ConstraintTemplate>& getAllTemplates() const { return templates; }

    /**
     * @brief Returns true if a template with the given name is registered.
     *
     * @param name  Name to check.
     */
    bool hasTemplate(const std::string& name) const;
    /// Returns the total number of registered templates.
    int getTemplateCount() const { return templates.size(); }

    /**
     * @brief Creates a Constraint by applying a named template to two GameObjects.
     *
     * Looks up the template, selects the correct ConstraintPreset factory,
     * optionally auto-calculates pivot positions when the stored pivot is near
     * zero, and forwards breaking thresholds to the new Constraint.
     *
     * The returned Constraint is not automatically registered — pass it to
     * ConstraintRegistry::addConstraint() to make it active.
     *
     * @param templateName  Name of the template to apply.
     * @param objA          Primary body. Must have a physics component.
     * @param objB          Secondary body, or nullptr where supported (e.g.
     *                      world-anchored hinge).
     * @return              Owning pointer to the new Constraint, or nullptr on
     *                      failure (template not found, missing physics, etc.).
     */
    std::unique_ptr<Constraint> applyTemplate(
        const std::string& templateName,
        GameObject* objA,
        GameObject* objB = nullptr
    );

    /**
    * @brief Saves all templates to a key=value text file at the given path.
    *
    * Each template is wrapped in TEMPLATE_START / TEMPLATE_END delimiters.
    * Type-specific parameters are prefixed with the type name to avoid key
    * collisions (e.g. hinge_pivotA, spring_stiffness0).
    *
    * @param filepath  Destination file path. Parent directory must exist.
    * @return          True on success, false if the file could not be opened.
    */
    bool saveToFile(const std::string& filepath);

    /**
     * @brief Loads templates from a key=value text file, replacing the current set.
     *
     * Returns false (non-fatal) if the file does not exist — expected on first run.
     *
     * @param filepath  Source file path.
     * @return          True if the file was found and parsed successfully.
     */
    bool loadFromFile(const std::string& filepath);


    // Default file operations

    /**
     * @brief Saves templates to the default file path (constraint_templates.txt).
     *
     * @return  True on success.
     */
    bool save(); // Save to default location

    /**
    * @brief Loads templates from the default file path (constraint_templates.txt).
    *
    * @return  True if the file was found and loaded.
    */
    bool load(); // Load from default location

    // Initialize with a few useful starter templates
    /**
     * @brief Populates the registry with built-in starter templates.
     *
     * Currently empty — add common presets here (e.g. "door_hinge", "piston",
     * "suspension") so new scenes have sensible starting points without a
     * saved template file.
     */
    void initializeDefaults();

    // Debug
     /**
     * @brief Prints a summary of all registered templates to stdout, including
     *        name, type, description, and breaking thresholds.
     */
    void printTemplates() const;
};

#endif // CONSTRAINTTEMPLATE_H