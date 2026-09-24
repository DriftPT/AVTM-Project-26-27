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

    std::vector<PathPoint> rawPath = {
        { -34.0f,  20.0f },  // P0: canto inferior esquerdo
        { -34.0f,  -8.0f },  // P1: sobe
        { -16.0f,  -8.0f },  // P2: vira à direita
        { -16.0f, -22.0f },  // P3: sobe mais (torre do meio, mais alta)
        {   2.0f, -22.0f },  // P4: topo, vira à direita
        {   2.0f,   3.0f },  // P5: desce (braço direito de cima)
        {  20.0f,   3.0f },  // P6: vira à direita
        {  20.0f,  20.0f },  // P7: desce até à base
    };

    // Bounding box original para centrar na mesa
    float minX = rawPath[0].x, maxX = rawPath[0].x;
    float minZ = rawPath[0].z, maxZ = rawPath[0].z;
    for (const auto& p : rawPath) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }
    float centerX = (minX + maxX) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    // Centra na origem da mesa e escala as estradas (apenas mundando os pontos dos rawPaths)
    trackPath.clear();
    for (const auto& path : rawPath) {
        trackPath.push_back({(path.x - centerX) * cfg.roadScale, (path.z - centerZ) * cfg.roadScale});
    }

    buildPathTrack(trackPath);

    // Define a posição da linha de meta
    const PathPoint& a = trackPath.back();
    const PathPoint& b = trackPath.front();
    cfg.finishLinePosx = (a.x + b.x) * 0.5f;
    cfg.finishLinePosz = (a.z + b.z) * 0.5f;

    float halfRoad = cfg.roadWidth * 0.5f; // metade da largura da estrada
    cheerioY = cfg.tableHeight * 0.5f + cfg.roadHeight + cfg.cheerioTubeRadius;

    cheerios.clear();

    const auto& Path = trackPath;

    // Loop Exterior da pista
    std::vector<PathPoint> outerRing = {
        { Path[0].x - halfRoad, Path[0].z + halfRoad }, // exterior de P0 (baixo-esquerda)
        { Path[1].x - halfRoad, Path[1].z - halfRoad }, // exterior de P1 (cima-esquerda)
        { Path[2].x - halfRoad, Path[2].z - halfRoad }, // exterior de P2 (cima-esquerda)
        { Path[3].x - halfRoad, Path[3].z - halfRoad }, // exterior de P3 (cima-esquerda)
        { Path[4].x + halfRoad, Path[4].z - halfRoad }, // exterior de P4 (cima-direita)
        { Path[5].x + halfRoad, Path[5].z - halfRoad }, // exterior de P5 (cima-direita)
        { Path[6].x + halfRoad, Path[6].z - halfRoad }, // exterior de P6 (cima-direita)
        { Path[7].x + halfRoad, Path[7].z + halfRoad }, // exterior de P7 (baixo-direita)
    };

    for (size_t i = 0; i < outerRing.size(); ++i) {
        size_t next = (i + 1) % outerRing.size(); // fecha o ciclo (último -> primeiro)
        placeCheeriosOnSegment(outerRing[i].x, outerRing[i].z,
            outerRing[next].x, outerRing[next].z,
            cfg.cheerioSpacing);
    }

    // Loop Interior da pista
    std::vector<PathPoint> innerRing = {
        { Path[0].x + halfRoad, Path[0].z - halfRoad }, // interior de P0
        { Path[1].x + halfRoad, Path[1].z + halfRoad }, // interior de P1
        { Path[2].x + halfRoad, Path[2].z + halfRoad }, // interior de P2
        { Path[3].x + halfRoad, Path[3].z + halfRoad }, // interior de P3
        { Path[4].x - halfRoad, Path[4].z + halfRoad }, // interior de P4
        { Path[5].x - halfRoad, Path[5].z + halfRoad }, // interior de P5
        { Path[6].x - halfRoad, Path[6].z + halfRoad }, // interior de P6
        { Path[7].x - halfRoad, Path[7].z - halfRoad }, // interior de P7
    };

    for (size_t i = 0; i < innerRing.size(); ++i) {
        size_t next = (i + 1) % innerRing.size();
        placeCheeriosOnSegment(innerRing[i].x, innerRing[i].z,
            innerRing[next].x, innerRing[next].z,
            cfg.cheerioSpacing);
    }
}

void Environment::buildPathTrack(const std::vector<PathPoint>& pts) {
    roadBlocks.clear();
    float halfRoad = cfg.roadWidth * 0.5f; // metade da largura da estrada
    
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t j = (i + 1) % pts.size(); // fecha o ciclo (último -> primeiro)
        roadBlocks.push_back(makeSegment(pts[i].x, pts[i].z, pts[j].x, pts[j].z, halfRoad)); // cria um bloco de estrada e guarda-o
    }
}

Environment::RoadBlock Environment::makeSegment(float x0, float z0, float x1, float z1, float halfRoad) {
    RoadBlock roadBlock;
    roadBlock.centerX = (x0 + x1) * 0.5f;
    roadBlock.centerZ = (z0 + z1) * 0.5f;

    bool horizontal = fabs(x1 - x0) > fabs(z1 - z0); // verifica se o segmento é horizontal ou vertical

    if (horizontal) {
        roadBlock.halfLengthX = fabs(x1 - x0) * 0.5f + halfRoad; // +halfRoad para tapar os cantos
        roadBlock.halfLengthZ = halfRoad;
    }
    else {
        roadBlock.halfLengthX = halfRoad;
        roadBlock.halfLengthZ = fabs(z1 - z0) * 0.5f; // como já se tapou na horizontal não é preciso na vertical
    }
    return roadBlock;
}

// Coloca cheerios espaçados de forma uniforme numa linha reta entre dois pontos
void Environment::placeCheeriosOnSegment(float x0, float z0, float x1, float z1, float spacing) {
    float len = sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
    int steps = (int)(len / spacing);
    if (steps < 1) steps = 1;

    for (int i = 0; i < steps; ++i) {
        float t = (float)i / (float)steps; 

        Cheerio cheerio;
        cheerio.id = (int)cheerios.size();   // id = posição no vetor
        cheerio.x = x0 + t * (x1 - x0);
        cheerio.y = cheerioY;
        cheerio.z = z0 + t * (z1 - z0);
        cheerio.halfXZ = cfg.cheerioOuterRadius;
        cheerio.halfY = cfg.cheerioTubeRadius;
        cheerio.updateAABB();

        cheerios.push_back(cheerio);
    }
}

void Environment::spawnButters() {
    butters.clear();
    float tableTop = cfg.tableHeight * 0.5f;

    for (int i = 0; i < cfg.numButters; ++i) {
        Butter butter_i;
        butter_i.id = (int)butters.size();
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
        
        butter_i.updateAABB();
        butters.push_back(butter_i);
    }
}

void Environment::spawnOranges() {
    oranges.resize(cfg.numOranges);
    for (size_t i = 0; i < oranges.size(); ++i) {
        oranges[i].id = (int)i;
        resetOrange(oranges[i]);
		// Adicionar uma pequena variação aleatória à posição inicial (como se fosse 2 segundos
        // de movimento) para evitar que todas as laranjas apareçam exatamente no mesmo ponto
        oranges[i].x += oranges[i].vx * (((float)rand() / (float)RAND_MAX) * 2.0f);
        oranges[i].z += oranges[i].vz * (((float)rand() / (float)RAND_MAX) * 2.0f);
        oranges[i].updateAABB();
    }
}

void Environment::resetOrange(Orange& orange) {
    orange.radius = cfg.orangeDefaultRadius;
    float tableTop = cfg.tableHeight * 0.5f;

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
    orange.y = tableTop + cfg.roadHeight + orange.radius;

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
    orange.updateAABB();
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
        orange.updateAABB();

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

    for (auto& b : butters) {
        if (b.shakeTimer > 0.0f) {
            b.shakeTimer -= deltaTime;
            if (b.shakeTimer < 0.0f) b.shakeTimer = 0.0f;
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
    if (showBoundingBoxes)
        drawBoundingBoxes(renderer, mu);
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

    float posX = cfg.finishLinePosx;
    float posZ = cfg.finishLinePosz;
    float posY = cfg.tableHeight * 0.5f + cfg.roadHeight + 0.005f;
	cfg.finishLinePosx = posX;
	cfg.finishLinePosz = posZ;

    float lineLengthX = 2.0f;
    float lineWidthZ = cfg.roadWidth;
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

    for (const auto& c : cheerios) {
        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, c.x, c.y, c.z);
        mu.scale(gmu::MODEL, cfg.cheerioScale, cfg.cheerioScale, cfg.cheerioScale);

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
        if (butter.shakeTimer > 0.0f) {
            float amplitude = 15.0f * (butter.shakeTimer / 0.8f); // O quão exagerado é o abanão em cada instante (0.8=segundos totais)
            float angle = sinf(butter.shakeTimer * 40.0f) * amplitude; // O ângulo de abanamento o 40 é o número de ondas por segundo
            mu.rotate(gmu::MODEL, angle, 0.0f, 1.0f, 0.0f); // abana em torno do eixo Y
        }
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
// ------------------------ Extras ------------------------

//TODO: REVER
void Environment::drawBox(Renderer& renderer, gmu& mu, const AABB& b) {
    dataMesh data;
    data.meshID = 0;   // cubo
    data.texMode = 0;  // só material

    mu.pushMatrix(gmu::MODEL);
    mu.translate(gmu::MODEL, b.minX, b.minY, b.minZ);
    mu.scale(gmu::MODEL, b.maxX - b.minX, b.maxY - b.minY, b.maxZ - b.minZ);

    mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
    mu.computeNormalMatrix3x3();

    data.vm = mu.get(gmu::VIEW_MODEL);
    data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
    data.normal = mu.getNormalMatrix();
    renderer.renderMesh(data);

    mu.popMatrix(gmu::MODEL);
}

void Environment::drawBoundingBoxes(Renderer& renderer, gmu& mu) {
    // Verde puro: com diffuse/specular a 0, o shader devolve o ambient
    float amb[]  = { 0.0f, 1.0f, 0.0f, 1.0f };
    float zero[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    memcpy(renderer.myMeshes[0].mat.ambient,  amb,  4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.diffuse,  zero, 4 * sizeof(float));
    memcpy(renderer.myMeshes[0].mat.specular, zero, 4 * sizeof(float));
    renderer.myMeshes[0].mat.shininess = 1.0f;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);

    for (const auto& c : cheerios) drawBox(renderer, mu, c.box);
    for (const auto& b : butters)  drawBox(renderer, mu, b.box);
    for (const auto& o : oranges)  if (o.active) drawBox(renderer, mu, o.box);

    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Environment::nudgeCheerio(int id, float dx, float dz) {
    if (id < 0 || id >= (int)cheerios.size()) return;
    cheerios[id].x += dx;
    cheerios[id].z += dz;
    cheerios[id].updateAABB();
}

void Environment::hitButter(int id) {
    if (id < 0 || id >= (int)butters.size()) return;
    butters[id].shakeTimer = 0.8f; // dura 0.8s
}

void Environment::hitOrange(int id) {
    if (id < 0 || id >= (int)oranges.size()) return;
    resetOrange(oranges[id]);
}