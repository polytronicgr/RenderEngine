#include "Terrain.h"

#include "Mesh.h"

#include "datatables/MeshTable.h"
#include "datatables/ProgramTable.h"
#include "datatables/VegetationTable.h"

#include "Scene.h"
#include "Time.h"
#include "WorldConfig.h"

#include <random>

#include <iostream>

#include "CascadeShadowMaps.h"

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
}

Engine::Terrain::~Terrain()
{
	delete tileObject;
}

// ====================================================================================================================

void Engine::Terrain::render(Engine::Camera * camera)
{
	unsigned int previousRadius = renderRadius;

	// Render cascade shadow maps
	for (unsigned int i = 0; i < Engine::CascadeShadowMaps::getInstance().getCascadeLevels(); i++)
	{
		Engine::CascadeShadowMaps::getInstance().saveCurrentFBO();
		Engine::CascadeShadowMaps::getInstance().beginShadowRender(i);

		glBindVertexArray(tileObject->getMesh()->vao);
		tiledRendering(camera, terrainShadowMapShader, &Terrain::terrainShadowMapRender);
		tiledRendering(camera, waterShadowMapShader, &Terrain::waterShadowMapRender);
		renderRadius = 4;
		tiledRendering(camera, treeShadowMapShader, &Terrain::treesShadowMapRender);
		renderRadius = previousRadius;
		
		Engine::CascadeShadowMaps::getInstance().endShadowRender();
	}

	glBindVertexArray(tileObject->getMesh()->vao);

	// RENDER TERRAIN
	tiledRendering(camera, terrainActiveShader, &Terrain::terrainRender);

	// RENDER WATER
	// enable blending to produce water transparency
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	//glBlendColor(1.0f, 1.0f, 1.0f, 0.6f);
	tiledRendering(camera, waterActiveShader, &Terrain::waterRender);
	//glDisable(GL_BLEND);

	// RENDER TREES
	renderRadius = renderRadius < 5? renderRadius : 5;
	tiledRendering(camera, treeActiveShader, &Terrain::treesRender);
	renderRadius = previousRadius;
}

// ====================================================================================================================

void Engine::Terrain::tiledRendering(Engine::Camera * camera, Program * prog, void (Terrain::*func)(Engine::Camera * cam, int i, int j))
{
	glm::vec3 cameraPosition = camera->getPosition();

	int x = -int((floor(cameraPosition.x)) / tileWidth);
	int y = -int((floor(cameraPosition.z)) / tileWidth);

	int xStart = x - renderRadius;
	int xEnd = x + renderRadius;
	int yStart = y - renderRadius;
	int yEnd = y + renderRadius;

	glUseProgram(prog->getProgramId());

	// Culling parameters
	glm::vec3 fwd = camera->getForwardVector();
	fwd.y = 0;
	fwd = glm::normalize(fwd);
	int px = -int(renderRadius), py = px;

	for (int i = xStart; i < xEnd; i++, px++)
	{
		for (int j = yStart; j < yEnd; j++, py++)
		{
			// Culling (skips almost half)
			glm::vec3 test(i - x, 0, j - y);
			if (abs(px) > 2 && abs(py) > 2 && glm::dot(glm::normalize(test), -fwd) <= 0)
				continue;

			(this->*func)(camera, i, j);
		}
	}
}

// ====================================================================================================================

void Engine::Terrain::terrainShadowMapRender(Engine::Camera * camera, int i, int j)
{
	float poxX = i * tileWidth;
	float posZ = j * tileWidth;
	tileObject->setTranslation(glm::vec3(poxX, 0.0f, posZ));

	terrainShadowMapShader->setUniformGridPosition(i, j);
	terrainShadowMapShader->setUniformLightDepthMatrix(Engine::CascadeShadowMaps::getInstance().getShadowProjectionMat() * tileObject->getModelMatrix());

	terrainShadowMapShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

	glDrawElements(GL_PATCHES, 6, GL_UNSIGNED_INT, (void*)0);
}

void Engine::Terrain::waterShadowMapRender(Engine::Camera * camera, int i, int j)
{
	float poxX = i * tileWidth;
	float posZ = j * tileWidth;
	tileObject->setTranslation(glm::vec3(poxX, 0.0f, posZ));
	waterShadowMapShader->setUniformLightDepthMatrix(Engine::CascadeShadowMaps::getInstance().getShadowProjectionMat() * tileObject->getModelMatrix());
	waterShadowMapShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
}

void Engine::Terrain::terrainRender(Engine::Camera * camera, int i, int j)
{
	Engine::DirectionalLight * dl = Engine::SceneManager::getInstance().getActiveScene()->getDirectionalLight();

	float poxX = i * tileWidth;
	float posZ = j * tileWidth;
	tileObject->setTranslation(glm::vec3(poxX, 0.0f, posZ));

	terrainActiveShader->setUniformGridPosition(i, j);
	terrainActiveShader->setUniformDepthTexture(Engine::CascadeShadowMaps::getInstance().getDepthTexture0());
	terrainActiveShader->setUniformDepthTexture1(Engine::CascadeShadowMaps::getInstance().getDepthTexture1());
	terrainActiveShader->setUniformLightDepthMatrix(Engine::CascadeShadowMaps::getInstance().getDepthMatrix0() * tileObject->getModelMatrix());
	terrainActiveShader->setUniformLightDepthMatrix1(Engine::CascadeShadowMaps::getInstance().getDepthMatrix1() * tileObject->getModelMatrix());
	terrainActiveShader->setUniformLightDirection(dl->getDirection());

	terrainActiveShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

	glDrawElements(GL_PATCHES, 6, GL_UNSIGNED_INT, (void*)0);
}

void Engine::Terrain::waterRender(Engine::Camera * camera, int i, int j)
{
	Engine::DirectionalLight * dl = Engine::SceneManager::getInstance().getActiveScene()->getDirectionalLight();

	float poxX = i * tileWidth;
	float posZ = j * tileWidth;
	tileObject->setTranslation(glm::vec3(poxX, Engine::Settings::waterHeight * tileWidth * 1.5f, posZ));

	waterActiveShader->setUniformGridPosition(i, j);
	waterActiveShader->setTimeUniform(Engine::Time::timeSinceBegining);
	waterActiveShader->setUniformLightDepthMatrix(Engine::CascadeShadowMaps::getInstance().getDepthMatrix0() * tileObject->getModelMatrix());
	waterActiveShader->setUniformLightDepthMatrix1(Engine::CascadeShadowMaps::getInstance().getDepthMatrix1() * tileObject->getModelMatrix());
	waterActiveShader->setUniformDepthTexture(Engine::CascadeShadowMaps::getInstance().getDepthTexture0());
	waterActiveShader->setUniformDepthTexture1(Engine::CascadeShadowMaps::getInstance().getDepthTexture1());
	waterActiveShader->setUniformLightDirection(dl->getDirection());
	waterActiveShader->onRenderObject(tileObject, camera->getViewMatrix(), camera->getProjectionMatrix());

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
}

void Engine::Terrain::treesShadowMapRender(Engine::Camera * camera, int i, int j)
{
	// We have to travel way to much to make this seed system not to work...
	unsigned int seed = (i << 16) | j;
	float posX = i * tileWidth;
	float posZ = j * tileWidth;

	std::uniform_real_distribution<float> dTerrain(0.0f, 1.0f);
	std::default_random_engine eTerrain(seed);

	// 16 trees per tile
	size_t spawnTrees = 16;
	size_t numTypeOfTrees = treeTypes.size();
	size_t equalAmount = spawnTrees / numTypeOfTrees;
	equalAmount = equalAmount < 1 ? 1 : equalAmount;

	Engine::CascadeShadowMaps & csm = Engine::CascadeShadowMaps::getInstance();

	size_t treeToSpawn = 0;
	unsigned int z = 0;
	while(z < spawnTrees)
	{
		Engine::Object * randomTree = treeTypes[treeToSpawn % numTypeOfTrees];
		treeToSpawn++;
		glBindVertexArray(randomTree->getMesh()->vao);
		for (unsigned int k = 0; k < equalAmount; k++)
		{
			z++;
			float uOffset = dTerrain(eTerrain);
			float vOffset = dTerrain(eTerrain);

			float treePosX = posX + uOffset * tileWidth;
			float treePosZ = posZ + vOffset * tileWidth;

			randomTree->setTranslation(glm::vec3(treePosX, 0.0f, treePosZ));

			float u = abs(i + uOffset);
			float v = abs(j + vOffset);

			treeShadowMapShader->setUniformTileUV(u, v);
			treeShadowMapShader->setUniformLightDepthMat(csm.getShadowProjectionMat() *  randomTree->getModelMatrix());
			treeShadowMapShader->onRenderObject(randomTree, camera->getViewMatrix(), camera->getProjectionMatrix());

			glDrawElements(GL_TRIANGLES, randomTree->getMesh()->getNumFaces() * 3, GL_UNSIGNED_INT, (void*)0);
		}
	}
}

void Engine::Terrain::treesRender(Engine::Camera * camera, int i, int j)
{
	Engine::DirectionalLight * dl = Engine::SceneManager::getInstance().getActiveScene()->getDirectionalLight();

	// We have to travel way to much to make this seed system not to work...
	unsigned int seed = (i << 16) | j;
	float posX = i * tileWidth;
	float posZ = j * tileWidth;

	std::uniform_real_distribution<float> dTerrain(0.0f, 1.0f);
	std::default_random_engine eTerrain(seed);

	// 16 trees per tile
	size_t spawnTrees = 16;
	size_t numTypeOfTrees = treeTypes.size();
	size_t equalAmount = spawnTrees / numTypeOfTrees;
	equalAmount = equalAmount < 1 ? 1 : equalAmount;

	Engine::CascadeShadowMaps & csm = Engine::CascadeShadowMaps::getInstance();

	size_t treeToSpawn = 0;
	unsigned int z = 0;
	while(z < spawnTrees)
	{
		Engine::Object * randomTree = treeTypes[treeToSpawn % numTypeOfTrees];
		treeToSpawn++;
		glBindVertexArray(randomTree->getMesh()->vao);
		for (unsigned int k = 0; k < equalAmount; k++, z++)
		{
			float uOffset = dTerrain(eTerrain);
			float vOffset = dTerrain(eTerrain);

			float treePosX = posX + uOffset * tileWidth;
			float treePosZ = posZ + vOffset * tileWidth;

			randomTree->setTranslation(glm::vec3(treePosX, 0.0f, treePosZ));

			float u = abs(i + uOffset);
			float v = abs(j + vOffset);

			treeActiveShader->setUniformTileUV(u, v);
			treeActiveShader->setUniformLightDir(dl->getDirection());
			treeActiveShader->setUniformLightDepthMat(csm.getDepthMatrix0() * randomTree->getModelMatrix());
			treeActiveShader->setUniformLightDepthMat1(csm.getDepthMatrix1() * randomTree->getModelMatrix());
			treeActiveShader->setUniformDepthMap(csm.getDepthTexture0());
			treeActiveShader->setUniformDepthMap1(csm.getDepthTexture1());
			treeActiveShader->onRenderObject(randomTree, camera->getViewMatrix(), camera->getProjectionMatrix());

			glDrawElements(GL_TRIANGLES, randomTree->getMesh()->getNumFaces() * 3, GL_UNSIGNED_INT, (void*)0);
		}
	}

}

// ====================================================================================================================

void Engine::Terrain::initialize()
{
	Engine::RenderableNotifier::getInstance().registerRenderable(this);

	// TERRAIN SHADERS INSTANCING
	const std::string & terrainProgName = Engine::ProceduralTerrainProgram::PROGRAM_NAME;
	// shadow map
	terrainShadowMapShader = dynamic_cast<Engine::ProceduralTerrainProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(terrainProgName,
		Engine::ProceduralTerrainProgram::SHADOW_MAP)
	);
	// g-buffers
	terrainShadingShader = dynamic_cast<Engine::ProceduralTerrainProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(terrainProgName)
	);
	// wire mode
	terrainWireShader = dynamic_cast<Engine::ProceduralTerrainProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(terrainProgName,
		Engine::ProceduralTerrainProgram::WIRE_DRAW_MODE)
	);

	terrainActiveShader = terrainShadingShader;

	// WATER SHADERS INSTANCING
	const std::string & waterProgName = Engine::ProceduralWaterProgram::PROGRAM_NAME;
	// shadow map
	waterShadowMapShader = dynamic_cast<Engine::ProceduralWaterProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(waterProgName,
		Engine::ProceduralWaterProgram::SHADOW_MAP)
	);
	// g-buffers
	waterShadingShader = dynamic_cast<Engine::ProceduralWaterProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(waterProgName)
	);
	// wire mode
	waterWireShader = dynamic_cast<Engine::ProceduralWaterProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(waterProgName,
		Engine::ProceduralWaterProgram::WIRE_DRAW_MODE)
	);

	waterActiveShader = waterShadingShader;

	// TREE SHADERS INSTANCING
	const std::string & treeProgName = Engine::TreeProgram::PROGRAM_NAME;
	// shading
	treeShader = dynamic_cast<Engine::TreeProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(treeProgName)
	);
	// wire mode
	treeWireShader = dynamic_cast<Engine::TreeProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(treeProgName, Engine::TreeProgram::WIRE_MODE)
	);
	// shadow map
	treeShadowMapShader = dynamic_cast<Engine::TreeProgram*>
	(
		Engine::ProgramTable::getInstance().getProgramByName(treeProgName, Engine::TreeProgram::SHADOW_MAP)
	);

	treeActiveShader = treeShader;

	// MESHES INSTANCING
	createTileMesh();

	// TREES INSTANCING
	addTrees();
}

void Engine::Terrain::createTileMesh()
{
	float vertices[12];
	vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
	vertices[3] = 1.0f; vertices[4] = 0.0f; vertices[5] = 0.0f;
	vertices[6] = 0.0f; vertices[7] = 0.0f; vertices[8] = 1.0f;
	vertices[9] = 1.0f; vertices[10] = 0.0f; vertices[11] = 1.0f;

	unsigned int faces[6];
	faces[0] = 0; faces[1] = 2; faces[2] = 1;
	faces[3] = 1; faces[4] = 2; faces[5] = 3;

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

void Engine::Terrain::addTrees()
{
	std::uniform_int_distribution<unsigned int> d(0, 50000);
	std::default_random_engine e(0);

	std::uniform_real_distribution<float> leafColor(0.0f, 1.0f);
	std::default_random_engine eLeaf(0);

	for (int i = 0; i < 8; i++)
	{
		Engine::TreeGenerationData treeData;
		treeData.treeName = std::string("CherryTree") + std::to_string(i);
		treeData.emissiveLeaf = leafColor(eLeaf) > 0.8f;
		treeData.startTrunkColor = glm::vec3(0.2f, 0.1f, 0.0f);
		treeData.endTrunkColor = treeData.startTrunkColor;
		treeData.leafColor = glm::vec3(leafColor(eLeaf), leafColor(eLeaf), 1.0 - leafColor(eLeaf));
		treeData.maxBranchesSplit = 4;
		treeData.maxBranchRotation = glm::vec3(20.0f, 10.0f, 10.0f);
		treeData.minBranchRotation = glm::vec3(-20.0f, -10.0f, -10.0f);
		treeData.maxDepth = 7;
		treeData.rotateMainTrunk = false;
		treeData.scalingFactor = glm::vec3(0.85, 1.0, 0.85);
		treeData.seed = d(e);
		treeData.startBranchingDepth = 2;

		Engine::Mesh * m = Engine::VegetationTable::getInstance().generateFractalTree(treeData, true);

		std::cout << "Tree: " << m->getNumFaces() << " faces, " << m->getNumVertices() << " vertices" << std::endl;

		treeShader->configureMeshBuffers(m);
		treeWireShader->configureMeshBuffers(m);
		treeShadowMapShader->configureMeshBuffers(m);

		Engine::Object * tree = new Engine::Object(m);
		
		treeTypes.push_back(tree);
	}
}

// ====================================================================================================================

void Engine::Terrain::notifyRenderModeUpdate(Engine::RenderMode mode)
{
	switch (mode)
	{
	case Engine::RenderMode::RENDER_MODE_WIRE:
		terrainActiveShader = terrainWireShader;
		waterActiveShader = waterWireShader;
		treeActiveShader = treeWireShader;
		break;
	default:
		terrainActiveShader = terrainShadingShader;
		waterActiveShader = waterShadingShader;
		treeActiveShader = treeShader;
		break;
	}
}