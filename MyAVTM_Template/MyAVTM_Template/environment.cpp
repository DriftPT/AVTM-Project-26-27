/*
* Environment.cpp - Implementa a classe Environment, que representa
* o ambiente de jogo, incluindo a mesa, estrada e cheerios.
*/

#include <cstring>
#include <cmath>
#include <string>

#include <GL/glew.h>

#include "renderer.h"
#include "mathUtility.h"
#include "model.h"

#include "Environment.h"

Environment::Environment() {}

// ------------------------ init ------------------------
void Environment::init(Renderer& renderer, const Config& config) {

    cfg = config;

    buildTrackLayout();
    spawnButters();
    spawnOranges();
}

void Environment::buildTrackLayout() {

    roadBlocks.clear();
    cheerioX.clear();
    cheerioZ.clear();

    // Metade do comprimento e largura do circuito, para facilitar os cálculos (pois o centro do circuito é a origem)
    float outerHalfX = cfg.trackLength * 0.5f; // metade do comprimento do circuito
    float outerHalfZ = cfg.trackWidth * 0.5f; // metade da largura do circuito
    float halfRoad = cfg.roadWidth * 0.5f; // metade da largura da estrada

    // Estrada do Topo
    RoadBlock top;
    top.centerX = 0.0f;
    top.centerZ = -(outerHalfZ - halfRoad);
    top.halfLengthX = outerHalfX;
    top.halfLengthZ = halfRoad;
    roadBlocks.push_back(top);

    // Estrada de baixo
    RoadBlock bottom = top;
    bottom.centerZ = (outerHalfZ - halfRoad);
    roadBlocks.push_back(bottom);

    // Estrada da esquerda 
    RoadBlock left;
    left.centerX = -(outerHalfX - halfRoad);
    left.centerZ = 0.0f;
    left.halfLengthX = halfRoad;
    left.halfLengthZ = outerHalfZ - cfg.roadWidth;
    roadBlocks.push_back(left);

    // Estrada da direita
    RoadBlock right = left;
    right.centerX = (outerHalfX - halfRoad);
    roadBlocks.push_back(right);

    // Defenir a altura dos cheerios 
    float tableTop = cfg.tableHeight * 0.5f; // A divisão por 2 é porque a mesa é centrada na origem
    cheerioY = tableTop + cfg.roadHeight + cfg.cheerioTubeRadius + cfg.cheerioYOffset;

    // margem extreior e interior
    placeCheeriosOnRectangle(outerHalfX, outerHalfZ, cfg.cheerioSpacing);
    placeCheeriosOnRectangle(outerHalfX - cfg.roadWidth, outerHalfZ - cfg.roadWidth, cfg.cheerioSpacing);
}

void Environment::placeCheeriosOnRectangle(float halfX, float halfZ, float spacing) {

    float sidesXZ[4][4] = {
        { -halfX, -halfZ,  halfX, -halfZ }, // topo
        {  halfX, -halfZ,  halfX,  halfZ }, // direita
        {  halfX,  halfZ, -halfX,  halfZ }, // baixo
        { -halfX,  halfZ, -halfX, -halfZ }  // esquerda
    };

    for (int i = 0; i < 4; ++i) {
        float x0 = sidesXZ[i][0], z0 = sidesXZ[i][1];
        float x1 = sidesXZ[i][2], z1 = sidesXZ[i][3];

        float len = sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)); // distância entre os dois pontos
        int steps = (int)(len / spacing);
        if (steps < 1) steps = 1;

        for (int j = 0; j < steps; ++j) {
            float t = (float)j / (float)steps; // parâmetro de interpolação entre 0 e 1 para distribuir os cheerios uniformemente sem sobreposição nos cantos
            cheerioX.push_back(x0 + t * (x1 - x0));
            cheerioZ.push_back(z0 + t * (z1 - z0));
        }
    }
}

void Environment::spawnButters() {
    butters.clear();
    float tableTop = cfg.tableHeight * 0.5f;

    for (int i = 0; i < cfg.numButters; ++i) {
        Butter butter_i;
        int blockIdx = rand() % roadBlocks.size();
        const RoadBlock& road = roadBlocks[blockIdx];

        // Calcular os limites (mínimo e máximo) onde o centro da manteiga cabe
        float minX = (road.centerX - road.halfLengthX) + (butter_i.sizeX * 0.5f);
        float maxX = (road.centerX + road.halfLengthX) - (butter_i.sizeX * 0.5f);

        float minZ = (road.centerZ - road.halfLengthZ) + (butter_i.sizeZ * 0.5f);
        float maxZ = (road.centerZ + road.halfLengthZ) - (butter_i.sizeZ * 0.5f);

		// criar um número aleatório entre 0 e 1, 0 é no inicio e 1 o fim
        float t_x = (float)rand() / (float)RAND_MAX;
        float t_z = (float)rand() / (float)RAND_MAX;
        
        // Interpolar diretamente entre o mínimo e o máximo
        butter_i.x = minX + t_x * (maxX - minX);
        butter_i.z = minZ + t_z * (maxZ - minZ);

        butter_i.y = tableTop + cfg.roadHeight + (butter_i.sizeY * 0.5f);

        butters.push_back(butter_i);
    }
}

void Environment::spawnOranges() {
    oranges.resize(cfg.numOranges);
    for (auto& orange : oranges) {
        resetOrange(orange);
		// Adicionar uma pequena variação aleatória à posição inicial (como se fosse 2 segundos
        // de movimento) para evitar que todas as laranjas apareçam exatamente no mesmo ponto
        orange.x += orange.vx * (((float)rand() / (float)RAND_MAX) * 2.0f);
        orange.z += orange.vz * (((float)rand() / (float)RAND_MAX) * 2.0f);
    }
}

void Environment::resetOrange(Orange& orange) {
    orange.radius = cfg.orangeDefaultRadius;
    float tableTop = cfg.tableHeight * 0.5f;
    orange.y = tableTop + cfg.roadHeight + orange.radius;

    // Atribuir tipo aleatório
    int rType = rand() % 3;
    if (rType == 0) {
        orange.type = OrangeType::SLOW;
        orange.baseSpeed = 7.0f;
        orange.radius = cfg.orangeDefaultRadius * 1.2f;
    }
    else if (rType == 1) {
        orange.type = OrangeType::NORMAL;
        orange.baseSpeed = 9.0f;
        orange.radius = cfg.orangeDefaultRadius;
    }
    else {
        orange.type = OrangeType::FAST;
        orange.baseSpeed = 12.0f;
        orange.radius = cfg.orangeDefaultRadius * 0.8f;
    }

    int side = rand() % 4; //escolher lado da mesa
	float halfTableX = cfg.tableWidth * 0.45f; // 0.45f para não sair da mesa
    float halfTableZ = cfg.tableDepth * 0.45f;

    float dirX = 0.0f, dirZ = 0.0f; // direção (ainda não normalizada)

    if (side == 0) { // Topo
        orange.x = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * halfTableX;
        orange.z = -halfTableZ;
        dirX = (((float)rand() / (float)RAND_MAX) - 0.5f); // desvio aleatório
        dirZ = 1.0f;
    }
    else if (side == 1) { // Baixo
        orange.x = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * halfTableX;
        orange.z = halfTableZ;
        dirX = (((float)rand() / (float)RAND_MAX) - 0.5f);
        dirZ = -1.0f;
    }
    else if (side == 2) { // Esquerda
        orange.x = -halfTableX;
        orange.z = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * halfTableZ;
        dirX = 1.0f;
        dirZ = (((float)rand() / (float)RAND_MAX) - 0.5f);
    }
    else { // Direita
        orange.x = halfTableX;
        orange.z = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * halfTableZ;
        dirX = -1.0f;
        dirZ = (((float)rand() / (float)RAND_MAX) - 0.5f);
    }

	// Normalizar a direção para que a velocidade seja consistente
    float dirLen = sqrtf(dirX * dirX + dirZ * dirZ);
    dirX /= dirLen;
    dirZ /= dirLen;

    orange.vx = dirX * orange.baseSpeed;
    orange.vz = dirZ * orange.baseSpeed;
    orange.speed = orange.baseSpeed;
    orange.dirX = dirX;
    orange.dirZ = dirZ;
    orange.active = true;
}

// ------------------------ Update -------------------------
void Environment::update(float deltaTime) {
    float limitX = cfg.tableWidth * 0.5f;
    float limitZ = cfg.tableDepth * 0.5f;

    totalPlayTime += deltaTime;

	// Aumentar a velocidade das laranjas com o tempo com um maximo de 2.5x
    float speedMultiplier = 1.0f + totalPlayTime * 0.02f;
    if (speedMultiplier > 2.5f) speedMultiplier = 2.5f;
    for (auto& orange : oranges) {
        if (!orange.active) continue;

		// Atualizar a velocidade com base no multiplicador de tempo
        orange.speed = orange.baseSpeed * speedMultiplier;
        orange.vx = orange.dirX * orange.speed;
        orange.vz = orange.dirZ * orange.speed;

        // Movimento retilíneo
        orange.x += orange.vx * deltaTime;
        orange.z += orange.vz * deltaTime;

		// Atualizar o ângulo de rotação com base na velocidade linear e no raio da laranja
        float angularSpeedRad = orange.speed / orange.radius;                   // ω = v / r
        float angularSpeedDeg = angularSpeedRad * (180.0f / 3.14159f);
        orange.rotationAngle += angularSpeedDeg * deltaTime;
        if (orange.rotationAngle > 360.0f) {
            orange.rotationAngle -= 360.0f;
        }

        // Respawn ao sair da mesa
        if (fabs(orange.x) > limitX || fabs(orange.z) > limitZ) {
            resetOrange(orange);
        }
    }
}

// ------------------------ Render ------------------------
void Environment::render(Renderer& renderer, gmu& mu) {
    drawTable(renderer, mu);
    drawRoad(renderer, mu);
    drawFinishLine(renderer, mu);
    drawCheerios(renderer, mu);
    drawButters(renderer, mu);
    drawOranges(renderer, mu);
}

void Environment::drawTable(Renderer& renderer, gmu& mu) {
	renderer.setTexUnit(1, 2); // TU1 = texmap1 -> lightwood.tga
	renderer.setTexUnit(2, 3); // TU2 = texmap2 -> table.jpg

    // Material da mesa aplicado ao Cubo (Mesh 0)
    float amb[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    float diff[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    float spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    memcpy(renderer.myMeshes[0].mat.ambient, amb, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.diffuse, diff, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.specular, spec, 4 * sizeof(float));
    renderer.myMeshes[0].mat.shininess = 20.0f;

    dataMesh data;
    data.meshID = 0;   // Cubo
	data.texMode = 3;  // multitexturing (usa texmap/TU0 e texmap2/TU2)

    mu.pushMatrix(gmu::MODEL);
    mu.scale(gmu::MODEL, cfg.tableWidth, cfg.tableHeight, cfg.tableDepth);
    mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

    mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
    mu.computeNormalMatrix3x3();

    data.vm = mu.get(gmu::VIEW_MODEL);
    data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
    data.normal = mu.getNormalMatrix();
    renderer.renderMesh(data);

    mu.popMatrix(gmu::MODEL);
}

void Environment::drawRoad(Renderer& renderer, gmu& mu) {
    renderer.setTexUnit(2, 4);

    // Configurar material da pista no Mesh 0
    float amb[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    float diff[] = { 0.9f,  0.9f,  0.9f,  1.0f };
    float spec[] = { 0.2f,  0.2f,  0.2f,  1.0f };
    memcpy(renderer.myMeshes[0].mat.ambient, amb, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.diffuse, diff, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.specular, spec, 4 * sizeof(float));
    renderer.myMeshes[0].mat.shininess = 20.0f;

    dataMesh data;
    data.meshID = 0; // Cubo base
    data.texMode = 1; // usa diffuse * texel (usa texmap2/TU2)

    float roadCenterY = cfg.tableHeight * 0.5f + cfg.roadHeight * 0.5f;

    for (const RoadBlock& road : roadBlocks) {

        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, road.centerX, roadCenterY, road.centerZ);
        mu.scale(gmu::MODEL, road.halfLengthX * 2.0f, cfg.roadHeight, road.halfLengthZ * 2.0f);
        mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        mu.computeNormalMatrix3x3();

        data.vm = mu.get(gmu::VIEW_MODEL);
        data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
        data.normal = mu.getNormalMatrix();
        renderer.renderMesh(data);

        mu.popMatrix(gmu::MODEL);
    }
}

void Environment::drawFinishLine(Renderer& renderer, gmu& mu) {
    renderer.setTexUnit(0, 5);

    float amb[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    float diff[] = { 0.9f,  0.9f,  0.9f,  1.0f }; 
    float spec[] = { 0.2f,  0.2f,  0.2f,  1.0f };
    memcpy(renderer.myMeshes[0].mat.ambient, amb, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.diffuse, diff, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.specular, spec, 4 * sizeof(float));
    renderer.myMeshes[0].mat.shininess = 40.0f;

    dataMesh data;
    data.meshID = 0; // Cubo base
	data.texMode = 2; // diffuse substituída pelo texel (usa texmap/TU0)

    float outerHalfZ = cfg.trackWidth * 0.5f;
    float halfRoad = cfg.roadWidth * 0.5f;

    // Coordenadasdo meio da estrada da esquerda
    float posX = -3.0f;
    float posZ = (outerHalfZ - halfRoad);
    // Eleva ligeiramente acima da estrada
    float posY = cfg.tableHeight * 0.5f + cfg.roadHeight + 0.005f;

    float lineLengthX = 2.0f;                // Espessura visual da faixa na pista
    float lineWidthZ = cfg.roadWidth;        // Atravessa a estrada de um lado ao outro
    float lineHeightY = 0.01f;

    mu.pushMatrix(gmu::MODEL);
    mu.translate(gmu::MODEL, posX, posY, posZ);
    mu.scale(gmu::MODEL, lineLengthX, lineHeightY, lineWidthZ);
    mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

    mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
    mu.computeNormalMatrix3x3();

    data.vm = mu.get(gmu::VIEW_MODEL);
    data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
    data.normal = mu.getNormalMatrix();
    renderer.renderMesh(data);

    mu.popMatrix(gmu::MODEL);
}


void Environment::drawCheerios(Renderer& renderer, gmu& mu) {

    float ambC[] = { 0.30f, 0.15f, 0.05f, 1.0f };
	float diffC[] = { 0.85f, 0.55f, 0.15f, 1.0f }; // cor laranja amarelada
    float specC[] = { 0.40f, 0.40f, 0.40f, 1.0f };
    memcpy(renderer.myMeshes[5].mat.ambient, ambC, 4 * sizeof(float));
    memcpy(renderer.myMeshes[5].mat.diffuse, diffC, 4 * sizeof(float));
    memcpy(renderer.myMeshes[5].mat.specular, specC, 4 * sizeof(float));
    renderer.myMeshes[5].mat.shininess = 50.0f;

    dataMesh data;
    data.meshID = 5; // Torus base
	data.texMode = 0; // sem textura, só o material (cor do cheerio)

    for (size_t i = 0; i < cheerioX.size(); ++i) {

        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, cheerioX[i], cheerioY, cheerioZ[i]);
        mu.scale(gmu::MODEL, 0.6f, 0.6f, 0.6f);

        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        mu.computeNormalMatrix3x3();

        data.vm = mu.get(gmu::VIEW_MODEL);
        data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
        data.normal = mu.getNormalMatrix();
        renderer.renderMesh(data);

        mu.popMatrix(gmu::MODEL);
    }
}

void Environment::drawButters(Renderer& renderer, gmu& mu) {

	float ambB[] = { 0.30f, 0.28f, 0.10f, 1.0f };
	float diffB[] = { 0.95f, 0.88f, 0.38f, 1.0f }; // cor amarela
    float specB[] = { 0.40f, 0.40f, 0.40f, 1.0f };
    memcpy(renderer.myMeshes[0].mat.ambient, ambB, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.diffuse, diffB, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.specular, specB, 4 * sizeof(float));
    renderer.myMeshes[0].mat.shininess = 30.0f;

    dataMesh data;
    data.meshID = 0; // Cubo base
	data.texMode = 0; // sem textura, só o material (cor da manteiga)

    for (const auto& butter : butters) {

        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, butter.x, butter.y, butter.z);
        mu.scale(gmu::MODEL, butter.sizeX, butter.sizeY, butter.sizeZ);
        mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        mu.computeNormalMatrix3x3();

        data.vm = mu.get(gmu::VIEW_MODEL);
        data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
        data.normal = mu.getNormalMatrix();
        renderer.renderMesh(data);

        mu.popMatrix(gmu::MODEL);
    }
}

void Environment::drawOranges(Renderer& renderer, gmu& mu) {

    renderer.setTexUnit(2, 6); // TU2 = texmap2

    float ambO[] = { 0.35f, 0.20f, 0.05f, 1.0f };
    float specO[] = { 0.40f, 0.40f, 0.40f, 1.0f };
    memcpy(renderer.myMeshes[2].mat.ambient, ambO, 4 * sizeof(float));
    memcpy(renderer.myMeshes[2].mat.specular, specO, 4 * sizeof(float));
    renderer.myMeshes[2].mat.shininess = 30.0f;

    dataMesh data;
    data.meshID = 2; // Esfera base
    data.texMode = 1; // usa diffuse * texel (usa texmap2/TU2)

    for (const auto& orange : oranges) {
        if (!orange.active) continue;

        if (orange.type == OrangeType::SLOW) {
            // Tonalidade esverdeada/amarelada
            renderer.myMeshes[2].mat.diffuse[0] = 0.65f;
            renderer.myMeshes[2].mat.diffuse[1] = 0.90f;
            renderer.myMeshes[2].mat.diffuse[2] = 0.35f;
        }
        else if (orange.type == OrangeType::NORMAL) {
            // Tonalidade laranja viva tradicional
            renderer.myMeshes[2].mat.diffuse[0] = 1.00f;
            renderer.myMeshes[2].mat.diffuse[1] = 0.60f;
            renderer.myMeshes[2].mat.diffuse[2] = 0.20f;
        }
        else { // FAST
            // Tonalidade avermelhada
            renderer.myMeshes[2].mat.diffuse[0] = 1.00f;
            renderer.myMeshes[2].mat.diffuse[1] = 0.25f;
            renderer.myMeshes[2].mat.diffuse[2] = 0.15f;
        }

        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, orange.x, orange.y, orange.z);

		// Calcular o eixo de rotação para a laranja
		float axisX = orange.vz / orange.speed; // eixo perpendicular à direção do movimento já normalizado
        float axisZ = -orange.vx / orange.speed;
        mu.rotate(gmu::MODEL, orange.rotationAngle, axisX, 0.0f, axisZ);

        mu.scale(gmu::MODEL, orange.radius, orange.radius, orange.radius);
        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        mu.computeNormalMatrix3x3();

        data.vm = mu.get(gmu::VIEW_MODEL);
        data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
        data.normal = mu.getNormalMatrix();
        renderer.renderMesh(data);

        mu.popMatrix(gmu::MODEL);
    }
}