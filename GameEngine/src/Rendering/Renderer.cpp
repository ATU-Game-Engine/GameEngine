#include <GL/glew.h>
#include "../include/Rendering/Renderer.h"
#include "../include./Rendering/Texture.h"
#include "../include/Rendering/MeshFactory.h"
#include <iostream>
#include <fstream>   
#include <sstream> 
#include "../include/Rendering/Camera.h"
#include "../include/Scene/Transform.h"
#include "../include/Scene/GameObject.h"
#include "../include/Physics/Trigger.h"
#include "../include/Physics/TriggerRegistry.h"
#include "../include/Physics/ForceGenerator.h"
#include "../include/Rendering/PointLight.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

Renderer::Renderer() : shadowMap(4096, 4096), skyboxEnabled(false), debugPhysicsEnabled(false) {
	mainLight.setDirection(glm::vec3(0.3f, -1.0f, 0.5f));
	mainLight.setIntensity(0.8f);
}

Renderer::~Renderer() {
}

void Renderer::initialize() {
	// Create shader programs
	shaderManager.createProgram("shaders/basic.vert", "shaders/basic.frag", "main");
	shaderManager.createProgram("shaders/shadow_depth.vert", "shaders/shadow_depth.frag", "shadow");

	shadowMap.initialize();

	cubeMesh = MeshFactory::createCube();
	sphereMesh = MeshFactory::createSphere();
	cylinderMesh = MeshFactory::createCylinder();
}

bool Renderer::loadSkybox(const std::vector<std::string>& faces) {
	if (skybox.loadCubemap(faces)) {
		skyboxEnabled = true;
		return true;
	}
	return false;
}

void Renderer::renderShadowPass(
	const Camera& camera,
	const std::vector<std::unique_ptr<GameObject>>& objects)
{
	// Calculate light space matrix
	glm::vec3 sceneCenter = glm::vec3(0.0f, 0.0f, 0.0f);  // Could be dynamic based on objects
	float sceneRadius = 50.0f;  // Could be calculated from scene bounds
	glm::mat4 lightSpaceMatrix = mainLight.getLightSpaceMatrix(sceneCenter, sceneRadius);

	// Bind shadow map for writing
	shadowMap.bindForWriting();

	// Use shadow shader
	unsigned int shadowShader = shaderManager.getProgram("shadow");
	glUseProgram(shadowShader);

	int lightSpaceLoc = glGetUniformLocation(shadowShader, "lightSpaceMatrix");
	int modelLoc = glGetUniformLocation(shadowShader, "model");

	glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

	// Render all objects (only their depth)
	for (const auto& obj : objects) {
		glm::mat4 model = Transform::model(
			obj->getPosition(),
			obj->getRotation(),
			obj->getScale()
		);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

		// Use the object's render mesh
		Mesh* mesh = obj->getRender().getRenderMesh();
		if (!mesh) mesh = &cubeMesh;

		mesh->draw();
	}

	// Unbind shadow map
	shadowMap.unbind();
}

void Renderer::drawGameObject(const GameObject& obj, int modelLoc, int colorLoc) {
	glm::mat4 model = Transform::model(
		obj.getPosition(),
		obj.getRotation(),
		obj.getScale()
	);
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

	// Use render component's mesh
	Mesh* mesh = obj.getRender().getRenderMesh();
	if (!mesh) mesh = &cubeMesh;  // Fallback safety

	glm::vec3 color(1.0f, 0.5f, 0.2f);

	bool hasTexture = !obj.getTexturePath().empty();
	Texture* texture = nullptr;
	Texture* specularTexture = nullptr;

	if (hasTexture) {
		texture = textureManager.loadTexture(obj.getTexturePath());
	}

	// Load specular map if exists
	std::string specularPath = obj.getRender().getSpecularTexturePath();
	bool hasSpecular = !specularPath.empty();
	if (hasSpecular) {
		specularTexture = textureManager.loadTexture(specularPath);
	}

	unsigned int mainShader = shaderManager.getProgram("main");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	int texLoc = glGetUniformLocation(mainShader, "textureSampler");
	int useSpecLoc = glGetUniformLocation(mainShader, "useSpecularMap");
	int specLoc = glGetUniformLocation(mainShader, "specularMap");

	if (texture) {
		texture->bind(0);
		glUniform1i(texLoc, 0);
		glUniform1i(useTexLoc, 1);
	}
	else {
		glUniform1i(useTexLoc, 0);
		glUniform3f(colorLoc, color.r, color.g, color.b);
	}

	if (texture) {
		texture->bind(0);
		glUniform1i(texLoc, 0);
		glUniform1i(useTexLoc, 1);

		// IMPORTANT: prevent shader’s "edge black" early-return from triggering
		glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
	}
	else {
		glUniform1i(useTexLoc, 0);
		glUniform3f(colorLoc, color.r, color.g, color.b);
	}

	if (specularTexture) {
		specularTexture->bind(2);
		glUniform1i(specLoc, 2);
		glUniform1i(useSpecLoc, 1);
	}
	else {
		glUniform1i(useSpecLoc, 0);
	}

	// Load normal map if exists
	std::string normalPath = obj.getRender().getNormalTexturePath();
	bool hasNormal = !normalPath.empty();
	Texture* normalTexture = nullptr;
	if (hasNormal) {
		normalTexture = textureManager.loadTexture(normalPath);
	}

	int useNormalLoc = glGetUniformLocation(mainShader, "useNormalMap");
	int normalLoc = glGetUniformLocation(mainShader, "normalMap");

	glUniform1i(normalLoc, 3);  // Always assign unit 3 to prevent sampler conflicts

	if (normalTexture) {
		normalTexture->bind(3);
		glUniform1i(normalLoc, 3);
		glUniform1i(useNormalLoc, 1);
	}
	else {
		glUniform1i(useNormalLoc, 0);
	}


	int uvTilingLoc = glGetUniformLocation(mainShader, "uvTiling");
	glUniform3f(uvTilingLoc, 1.0f, 1.0f, 1.0f);

	mesh->draw();

	if (texture) {
		texture->unbind();
	}
}

void Renderer::draw(int windowWidth,
	int windowHeight,
	const Camera& camera,
	const std::vector<std::unique_ptr<GameObject>>& objects,
	const GameObject* primarySelection,
	const std::vector<GameObject*>& selectedObjects) {
	if (windowHeight == 0)
		return;

	// SHADOW PASS - Render from light's perspective
	renderShadowPass(camera, objects);

	// MAIN PASS - Render scene normally
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	// Draw skybox first
	float aspectRatio = (float)windowWidth / (float)windowHeight;
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(aspectRatio);

	if (skyboxEnabled) {
		skybox.draw(view, projection);
	}

	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	int modelLoc = glGetUniformLocation(mainShader, "model");
	int viewLoc = glGetUniformLocation(mainShader, "view");
	int projectionLoc = glGetUniformLocation(mainShader, "projection");
	int colorLoc = glGetUniformLocation(mainShader, "objectColor");
	int lightDirLoc = glGetUniformLocation(mainShader, "lightDir");
	int viewPosLoc = glGetUniformLocation(mainShader, "viewPos");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);

	// Set lighting uniforms from DirectionalLight
	glm::vec3 cameraPos = camera.getPosition();

	glUniform3fv(lightDirLoc, 1, &mainLight.getDirection()[0]);
	glUniform3fv(viewPosLoc, 1, &cameraPos[0]);
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);


	// Calculate and pass light space matrix
	glm::vec3 sceneCenter = glm::vec3(0.0f, 0.0f, 0.0f);
	float sceneRadius = 50.0f;
	glm::mat4 lightSpaceMatrix = mainLight.getLightSpaceMatrix(sceneCenter, sceneRadius);
	int lightSpaceMatrixLoc = glGetUniformLocation(mainShader, "lightSpaceMatrix");
	glUniformMatrix4fv(lightSpaceMatrixLoc, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

	// Bind shadow map texture to texture unit 1
	shadowMap.bindForReading(1);
	int shadowMapLoc = glGetUniformLocation(mainShader, "shadowMap");
	glUniform1i(shadowMapLoc, 1);


	for (const auto& obj : objects)
	{
		bool isSelected =
			std::find(selectedObjects.begin(),
				selectedObjects.end(),
				obj.get()) != selectedObjects.end();

		glUniform1i(glGetUniformLocation(mainShader, "uIsSelected"),
			isSelected ? 1 : 0);

		glUniform3f(glGetUniformLocation(mainShader, "uHighlightColor"),
			0.0f, 1.0f, 1.0f); // cyan

		glUniform1f(glGetUniformLocation(mainShader, "uHighlightStrength"),
			0.6f);

		drawGameObject(*obj, modelLoc, colorLoc);

		// Outline ONLY for primary selection
		if (primarySelection && obj.get() == primarySelection)
		{
			drawOutlineOnly(*obj, modelLoc, colorLoc);
		}

		if (debugPhysicsEnabled)
		{
			drawDebugCollisionShape(*obj, modelLoc, colorLoc);
		}
	}
}

// Draws a black wire overlay on top of the object (if mesh has edge indices).
// Does NOT change your friend's base draw code.
void Renderer::drawOutlineOnly(const GameObject& obj, int modelLoc, int colorLoc)
{
	// Build model matrix
	glm::mat4 model = Transform::model(obj.getPosition(), obj.getRotation(), obj.getScale());
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

	Mesh* mesh = obj.getRender().getRenderMesh();
	if (!mesh) mesh = &cubeMesh;

	// Force "edge shader path" to black using your existing fragment test
	unsigned int mainShader = shaderManager.getProgram("main");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	glUniform1i(useTexLoc, 0);
	glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);

	// Render as wireframe overlay
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-0.5f, -0.5f);
	glLineWidth(2.0f);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	mesh->draw();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glDisable(GL_POLYGON_OFFSET_LINE);
}

void Renderer::drawDebugCollisionShape(const GameObject& obj, int modelLoc, int colorLoc) {
	glm::vec3 debugScale = obj.getScale();

	btRigidBody* rb = obj.getRigidBody();
	if (rb && rb->getCollisionShape()) {
		btCollisionShape* shape = rb->getCollisionShape();
		if (shape->getShapeType() == BOX_SHAPE_PROXYTYPE) {
			btBoxShape* box = static_cast<btBoxShape*>(shape);
			btVector3 half = box->getHalfExtentsWithMargin();
			debugScale = glm::vec3(
				half.x() * 2.0f,
				half.y() * 2.0f,
				half.z() * 2.0f
			);
		}
	}
	glm::mat4 model = Transform::model(
		obj.getPosition(),
		obj.getRotation(),
		debugScale
	);
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

	// Pick mesh
	Mesh* mesh = nullptr;
	switch (obj.getShapeType()) {
	case ShapeType::CUBE:
		mesh = &cubeMesh;
		break;
	case ShapeType::SPHERE:
		mesh = &sphereMesh;
		break;
	case ShapeType::CAPSULE:
		mesh = &cylinderMesh;
		break;
	}

	if (!mesh) return;

	// Disable depth test so wireframe is always visible
	glDisable(GL_DEPTH_TEST);

	// Set light to super bright
	unsigned int mainShader = shaderManager.getProgram("main");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");
	glUniform3f(lightColorLoc, 10.0f, 10.0f, 10.0f);  // Super bright light


	// Draw as bright cyan wireframe
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	glUniform1i(useTexLoc, 0);
	glUniform3f(colorLoc, 0.0f, 1.0f, 1.0f);  // Cyan

	glLineWidth(1.5f);

	// Draw using polygon mode instead of edge indices
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  // Wireframe mode
	mesh->draw();  // Use regular draw (faces), but in LINE mode
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // Back to normal

	// Restore light color
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);

	// Re-enable depth test
	glEnable(GL_DEPTH_TEST);
}

void Renderer::drawTriggerDebug(const std::vector<Trigger*>& triggers, const Camera& camera, int fbW, int fbH)
{

	if (!debugPhysicsEnabled) return;
	if (fbW == 0 || fbH == 0) return; // Avoid division by zero in aspect ratio

	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	float aspect = (float)fbW / (float)fbH;
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(aspect);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "projection"), 1, GL_FALSE, &projection[0][0]);

	int modelLoc = glGetUniformLocation(mainShader, "model");
	int colorLoc = glGetUniformLocation(mainShader, "objectColor");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");

	glDisable(GL_DEPTH_TEST);
	glUniform1i(useTexLoc, 0);
	glUniform3f(lightColorLoc, 10.0f, 10.0f, 10.0f);
	glLineWidth(1.5f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (Trigger* trigger : triggers)
	{
		if (!trigger || !trigger->isEnabled()) continue;

		// Yellow for teleport, green for speed zone, white for others
		switch (trigger->getType())
		{
		case TriggerType::TELEPORT:    glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); break;
		case TriggerType::SPEED_ZONE:  glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f); break;
		default:                       glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f); break;
		}

		glm::mat4 model = glm::translate(glm::mat4(1.0f), trigger->getPosition());
		model = glm::scale(model, trigger->getSize());
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

		cubeMesh.draw();
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);
}

void Renderer::drawForceGeneratorDebug(const std::vector<ForceGenerator*>& generators, const Camera& camera, int fbW, int fbH)
{
	if (!debugPhysicsEnabled) return;
	if (fbW == 0 || fbH == 0) return;

	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	float aspect = (float)fbW / (float)fbH;
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(aspect);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "projection"), 1, GL_FALSE, &projection[0][0]);

	int modelLoc = glGetUniformLocation(mainShader, "model");
	int colorLoc = glGetUniformLocation(mainShader, "objectColor");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");

	glDisable(GL_DEPTH_TEST);
	glUniform1i(useTexLoc, 0);
	glUniform3f(lightColorLoc, 10.0f, 10.0f, 10.0f);
	glLineWidth(1.5f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (ForceGenerator* gen : generators)
	{
		if (!gen || !gen->isEnabled()) continue;

		switch (gen->getType())
		{
		case ForceGeneratorType::WIND:         glUniform3f(colorLoc, 0.5f, 0.8f, 1.0f); break; // light blue
		case ForceGeneratorType::GRAVITY_WELL: glUniform3f(colorLoc, 0.8f, 0.0f, 1.0f); break; // purple
		case ForceGeneratorType::VORTEX:       glUniform3f(colorLoc, 1.0f, 0.5f, 0.0f); break; // orange
		case ForceGeneratorType::EXPLOSION:    glUniform3f(colorLoc, 1.0f, 0.2f, 0.0f); break; // red
		default:                               glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f); break;
		}

		// Generators use radius so draw as sphere scaled to diameter
		float r = gen->getRadius();
		glm::mat4 model = glm::translate(glm::mat4(1.0f), gen->getPosition());
		model = glm::scale(model, glm::vec3(r * 2.0f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

		sphereMesh.draw();
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);
}

void Renderer::drawPointLightDebug(const std::vector<PointLight*>& lights, const Camera& camera, int fbW, int fbH)
{
	if (!debugPhysicsEnabled) return;
	if (fbW == 0 || fbH == 0) return;

	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	float aspect = (float)fbW / (float)fbH;
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(aspect);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "projection"), 1, GL_FALSE, &projection[0][0]);

	int modelLoc = glGetUniformLocation(mainShader, "model");
	int colorLoc = glGetUniformLocation(mainShader, "objectColor");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");

	glDisable(GL_DEPTH_TEST);
	glUniform1i(useTexLoc, 0);
	glUniform3f(lightColorLoc, 10.0f, 10.0f, 10.0f);
	glLineWidth(1.5f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (PointLight* light : lights)
	{
		if (!light || !light->isEnabled()) continue;

		// Use the light's own colour for the wireframe
		glm::vec3 col = light->getColour();
		glUniform3f(colorLoc, col.r, col.g, col.b);

		// Draw sphere
		glm::mat4 model = glm::translate(glm::mat4(1.0f), light->getPosition());
		model = glm::scale(model, glm::vec3(1.0f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

		sphereMesh.draw();
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);
}

void Renderer::drawConstraintDebug(const std::vector<Constraint*>& constraints,const Camera& camera,int fbW, int fbH)
{
	if (!debugPhysicsEnabled) return;
	if (constraints.empty()) return;
	if (fbW == 0 || fbH == 0) return;

	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	float aspect = (float)fbW / (float)fbH;
	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(aspect);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(mainShader, "projection"), 1, GL_FALSE, &projection[0][0]);

	int modelLoc = glGetUniformLocation(mainShader, "model");
	int colorLoc = glGetUniformLocation(mainShader, "objectColor");
	int useTexLoc = glGetUniformLocation(mainShader, "useTexture");
	int lightColorLoc = glGetUniformLocation(mainShader, "lightColor");

	glDisable(GL_DEPTH_TEST);
	glUniform1i(useTexLoc, 0);
	glUniform3f(lightColorLoc, 10.0f, 10.0f, 10.0f);
	glLineWidth(2.0f);

	// Build a  reusable line VAO, two positions, updated per segment
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(glm::vec3), nullptr, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

	// Helper upload two endpoints and draw a single line segment
	auto drawLine = [&](const glm::vec3& a, const glm::vec3& b)
		{
			glm::vec3 pts[2] = { a, b };
			glm::mat4 identity(1.0f);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &identity[0][0]);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pts), pts);
			glDrawArrays(GL_LINES, 0, 2);
		};

	// Helper draw a small axis-aligned cross at a world position
	auto drawCross = [&](const glm::vec3& p, float size)
		{
			drawLine(p - glm::vec3(size, 0, 0), p + glm::vec3(size, 0, 0));
			drawLine(p - glm::vec3(0, size, 0), p + glm::vec3(0, size, 0));
			drawLine(p - glm::vec3(0, 0, size), p + glm::vec3(0, 0, size));
		};

	const float markerSize = 0.15f;

	for (Constraint* c : constraints)
	{
		if (!c) continue;

		GameObject* A = c->getBodyA();
		GameObject* B = c->getBodyB();
		if (!A) continue;

		glm::vec3 posA = A->getPosition();
		glm::vec3 posB = B ? B->getPosition() : glm::vec3(0.0f);

		
		glm::vec3 col;
		switch (c->getType())
		{
		case ConstraintType::FIXED:        col = { 0.6f, 0.1f, 0.9f }; break; //  purple
		case ConstraintType::HINGE:        col = { 0.9f, 0.1f, 0.5f }; break; //  pink
		case ConstraintType::SLIDER:       col = { 0.0f, 0.7f, 0.6f }; break; // teal
		case ConstraintType::SPRING:       col = { 0.8f, 1.0f, 0.1f }; break; //  lime
		case ConstraintType::GENERIC_6DOF: col = { 0.3f, 0.2f, 0.9f }; break; // indigo
		default:                           col = { 0.5f, 0.5f, 0.5f }; break; // grey
		}

		// Dim for broken/disabled constraints
		if (c->isBroken())
			col *= 0.3f;

		glUniform3f(colorLoc, col.r, col.g, col.b);

		//  Connection line
		drawLine(posA, posB);

		// Cross markers at each body
		drawCross(posA, markerSize);
		drawCross(posB, markerSize);

		//  Hinge: axis line through bodyA
		if (c->getType() == ConstraintType::HINGE)
		{
			// Same pink as the hinge so it reads as part of it
			glUniform3f(colorLoc, 0.9f, 0.1f, 0.5f);
			drawLine(posA - glm::vec3(0, 0.4f, 0), posA + glm::vec3(0, 0.4f, 0));
			glUniform3f(colorLoc, col.r, col.g, col.b); // restore
		}

		// Slider: dashed line along the slide axis
		if (c->getType() == ConstraintType::SLIDER)
		{
			float segLen = glm::length(posB - posA);
			if (segLen > 0.001f)
			{
				glm::vec3 dir = (posB - posA) / segLen;
				float     dashLen = 0.15f, gap = 0.15f;
				for (float t = 0.0f; t < segLen; t += dashLen + gap)
				{
					glm::vec3 start = posA + dir * t;
					glm::vec3 end = posA + dir * std::min(t + dashLen, segLen);
					drawLine(start, end);
				}
			}
		}

		//  Spring: zigzag coil between the two bodies 
		if (c->getType() == ConstraintType::SPRING)
		{
			glm::vec3 dir = posB - posA;
			float     len = glm::length(dir);

			if (len > 0.001f)
			{
				glm::vec3 normDir = dir / len;
				glm::vec3 up = std::abs(normDir.y) < 0.9f
					? glm::vec3(0, 1, 0)
					: glm::vec3(1, 0, 0);
				glm::vec3 perp = glm::normalize(glm::cross(normDir, up)) * 0.12f;

				int       coils = 6;
				glm::vec3 prev = posA;

				for (int i = 1; i <= coils * 2; ++i)
				{
					float     t = (float)i / (float)(coils * 2);
					glm::vec3 offset = (i % 2 == 0) ? perp : -perp;
					glm::vec3 cur = posA + dir * t + offset;
					drawLine(prev, cur);
					prev = cur;
				}
				drawLine(prev, posB);
			}
		}
	}

	// Cleanup
	glBindVertexArray(0);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);

	glLineWidth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glUniform3fv(lightColorLoc, 1, &mainLight.getFinalColor()[0]);
}

void Renderer::uploadPointLights(const std::vector<PointLight*>& lights)
{
	unsigned int mainShader = shaderManager.getProgram("main");
	glUseProgram(mainShader);

	int count = 0;
	for (PointLight* l : lights)
	{
		if (!l || !l->isEnabled()) continue;
		if (count >= 16) break;

		std::string base = "pointLightPositions[" + std::to_string(count) + "]";
		glUniform3fv(glGetUniformLocation(mainShader, ("pointLightPositions[" + std::to_string(count) + "]").c_str()), 1, &l->getPosition()[0]);
		glUniform3fv(glGetUniformLocation(mainShader, ("pointLightColours[" + std::to_string(count) + "]").c_str()), 1, &l->getColour()[0]);
		glUniform1f(glGetUniformLocation(mainShader, ("pointLightIntensities[" + std::to_string(count) + "]").c_str()), l->getIntensity());
		glUniform1f(glGetUniformLocation(mainShader, ("pointLightRadii[" + std::to_string(count) + "]").c_str()), l->getRadius());
		count++;
	}

	glUniform1i(glGetUniformLocation(mainShader, "numPointLights"), count);
}

void Renderer::cleanup() {
	std::cout << "Cleaning up renderer..." << std::endl;
	cubeMesh.cleanup();
	skybox.cleanup();
	shadowMap.cleanup();

	textureManager.cleanup();
	shaderManager.cleanup();
	std::cout << "Renderer cleaned up" << std::endl;
}

