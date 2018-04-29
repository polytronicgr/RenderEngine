#define _USE_MATH_DEFINES

#include "Terrain.h"

#include "Mesh.h"
#include "datatables/MeshTable.h"
#include "datatables/ProgramTable.h"

#include "WorldConfig.h"

#include <cmath>

#include <iostream>
#include <chrono>

inline double clamp(double value)
{
	return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
}

Engine::Terrain::Terrain()
{
	tileWidth = 1.0f;
	renderRadius = 7;
	initialize();
}

Engine::Terrain::Terrain(float tileWidth, unsigned int renderRadius)
{
	this->tileWidth = tileWidth;
	this->renderRadius = renderRadius;
	initialize();

	Engine::RenderableNotifier::getInstance().registerRenderable(this);
}

Engine::Terrain::Terrain(const Terrain & other)
{

}

void Engine::Terrain::render(Engine::Camera * camera)
{
	drawTerrainLayer(camera);
	drawWaterLayer(camera);
}

void Engine::Terrain::drawTerrainLayer(Engine::Camera * camera)
{
	glm::vec3 cameraPosition = camera->getPosition();

	int x = -int((floor(cameraPosition.x)) / tileWidth);
	int y = -int((floor(cameraPosition.z)) / tileWidth);

	int xStart = x - renderRadius;
	int xEnd = x + renderRadius;
	int yStart = y - renderRadius;
	int yEnd = y + renderRadius;

	glUseProgram(terrainActiveShader->getProgramId());
	glBindVertexArray(tileObject->getMesh()->vao);

	for (int i = xStart; i < xEnd; i++)
	{
		for (int j = yStart; j < yEnd; j++)
		{
			float poxX = i * tileWidth;
			float posZ = j * tileWidth;
			tileObject->setTranslation(glm::vec3(poxX, 0.0f, posZ));

			terrainActiveShader->setUniformGridPosition(i, j);
			terrainActiveShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

			glDrawElements(GL_PATCHES, 6, GL_UNSIGNED_INT, (void*)0);
		}
	}
}

void Engine::Terrain::drawWaterLayer(Engine::Camera * camera)
{
	glm::vec3 cameraPosition = camera->getPosition();

	int x = -int((floor(cameraPosition.x)) / tileWidth);
	int y = -int((floor(cameraPosition.z)) / tileWidth);

	int xStart = x - renderRadius;
	int xEnd = x + renderRadius;
	int yStart = y - renderRadius;
	int yEnd = y + renderRadius;

	glUseProgram(waterActiveShader->getProgramId());
	glBindVertexArray(tileObject->getMesh()->vao);

	for (int i = xStart; i < xEnd; i++)
	{
		for (int j = yStart; j < yEnd; j++)
		{
			float poxX = i * tileWidth;
			float posZ = j * tileWidth;
			tileObject->setTranslation(glm::vec3(poxX, Engine::Settings::waterHeight * 4.5f, posZ));

			waterActiveShader->setUniformGridPosition(i, j);
			waterActiveShader->setTimeUniform(waterMovementSignal);
			waterMovementSignal += 0.00001f;
			waterActiveShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

			glDrawElements(GL_PATCHES, 6, GL_UNSIGNED_INT, (void*)0);
		}
	}
}

void Engine::Terrain::initialize()
{
	waterMovementSignal = 0;

	terrainShadingShader = dynamic_cast<Engine::ProceduralTerrainProgram*>(Engine::ProgramTable::getInstance().getProgramByName(Engine::ProceduralTerrainProgram::PROGRAM_NAME));
	terrainWireShader = dynamic_cast<Engine::ProceduralTerrainProgram*>(Engine::ProgramTable::getInstance().getProgramByName(Engine::ProceduralTerrainProgram::PROGRAM_NAME,
		Engine::ProceduralTerrainProgram::WIRE_DRAW_MODE));

	terrainActiveShader = terrainShadingShader;

	waterShadingShader = dynamic_cast<Engine::ProceduralWaterProgram*>(Engine::ProgramTable::getInstance().getProgramByName(Engine::ProceduralWaterProgram::PROGRAM_NAME));
	waterWireShader = dynamic_cast<Engine::ProceduralWaterProgram*>(Engine::ProgramTable::getInstance().getProgramByName(Engine::ProceduralWaterProgram::PROGRAM_NAME, 
		Engine::ProceduralTerrainProgram::WIRE_DRAW_MODE));

	waterActiveShader = waterShadingShader;

	createTileMesh();
}

void Engine::Terrain::createTileMesh()
{
	float vertices[12];
	vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
	vertices[3] = 1.0f; vertices[4] = 0.0f; vertices[5] = 0.0f;
	vertices[6] = 0.0f; vertices[7] = 0.0f; vertices[8] = 1.0f;
	vertices[9] = 1.0f; vertices[10] = 0.0f; vertices[11] = 1.0f;

	unsigned int faces[6];
	faces[0] = 0; faces[1] = 1; faces[2] = 2;
	faces[3] = 1; faces[4] = 3; faces[5] = 2;

	float normals[12];
	normals[0] = 0.0f; normals[1] = 1.0f; normals[2] = 0.0f;
	normals[3] = 0.0f; normals[4] = 1.0f; normals[5] = 0.0f;
	normals[6] = 0.0f; normals[7] = 1.0f; normals[8] = 0.0f;
	normals[9] = 0.0f; normals[10] = 1.0f; normals[11] = 0.0f;

	float uv[8];
	uv[0] = 0.0f; uv[1] = 0.0f;
	uv[2] = 1.0f; uv[3] = 0.0f;
	uv[4] = 0.0f; uv[5] = 1.0f;
	uv[6] = 1.0f; uv[7] = 1.0f;

	Engine::Mesh plane(2, 4, faces, vertices, 0, normals, uv, 0);
	Engine::MeshTable::getInstance().addMeshToCache("terrain_tile", plane);
	Engine::Mesh * planeMesh = Engine::MeshTable::getInstance().getMesh("terrain_tile");
	
	terrainShadingShader->configureMeshBuffers(planeMesh);
	waterShadingShader->configureMeshBuffers(planeMesh);

	tileObject = new Engine::Object(planeMesh);
	if(tileWidth != 1.0f)
		tileObject->setScale(glm::vec3(tileWidth, tileWidth, tileWidth));
}

void Engine::Terrain::notifyRenderModeUpdate(Engine::RenderMode mode)
{
	switch (mode)
	{
	case Engine::RenderMode::RENDER_MODE_WIRE:
		terrainActiveShader = terrainWireShader;
		waterActiveShader = waterWireShader;
		break;
	default:
		terrainActiveShader = terrainShadingShader;
		waterActiveShader = waterShadingShader;
		break;
	}
}