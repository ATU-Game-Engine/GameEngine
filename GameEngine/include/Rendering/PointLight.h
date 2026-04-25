#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include <glm/glm.hpp>
#include <string>

class PointLight {
private:
    static uint64_t nextID;
    uint64_t id;

    std::string name;
    glm::vec3   position;
    glm::vec3   colour;
    float       intensity;
    float       radius;
    bool        enabled;

public:
    PointLight(const std::string& name,
        const glm::vec3& position,
        const glm::vec3& colour = glm::vec3(1.0f),
        float intensity = 1.0f,
        float radius = 10.0f)
        : name(name), position(position), colour(colour),
        intensity(intensity), radius(radius), enabled(true),
        id(nextID++) {
    }

    // Getters
    uint64_t          getID()        const { return id; }
    const std::string& getName()     const { return name; }
    glm::vec3         getPosition()  const { return position; }
    glm::vec3         getColour()    const { return colour; }
    float             getIntensity() const { return intensity; }
    float             getRadius()    const { return radius; }
    bool              isEnabled()    const { return enabled; }

    // Setters
    void setName(const std::string& n) { name = n; }
    void setPosition(const glm::vec3& p) { position = p; }
    void setColour(const glm::vec3& c) { colour = c; }
    void setIntensity(float i) { intensity = i; }
    void setRadius(float r) { radius = r; }
    void setEnabled(bool e) { enabled = e; }
};

#endif