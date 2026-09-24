#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_

#include <vector>

// Forward declarations
class Renderer;
class gmu;

class Environment {
public:

    Environment();

    struct Config {
        // Mesa 
        float tableWidth = 160.0f;
		float tableDepth = 130.0f;
        float tableHeight = 0.6f;

        // Circuito
        float roadWidth = 16.0f;     // largura de cada bloco de estrada
        float roadHeight = 0.08f;   // espessura da estrada (fica em cima da mesa)

        // Cheerios (margens)
        float cheerioOuterRadius = 0.9f;  // raio exterior do anel
        float cheerioTubeRadius = 0.3f;  // espessura do tubo
        float cheerioSpacing = 2.0f;      // distância entre cheerios
		float cheerioScale = 0.6f;        // escala do cheerio em relação ao tamanho original

        // Manteigas e Laranjas
        int numButters = 10;
        int numOranges = 7;
        float orangeDefaultRadius = 0.8f;

		// Linha de meta
		float finishLinePosx = 0.0f; 
		float finishLinePosz = 0.0f; 
    };

    struct AABB {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;
    };

    enum class OrangeType { NORMAL, FAST, SLOW };

    struct Orange {
        OrangeType type = OrangeType::NORMAL;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float vx = 0.0f, vz = 0.0f; // velocidade linear
		float speed = 0.0f; // velocidade escalar
		float baseSpeed = 0.0f; // velocidade base (dependendo do tipo)
		float dirX = 0.0f, dirZ = 0.0f; //direção de movimento (normalizada)
        float radius = 1.0f;
        float rotationAngle = 0.0f; // ângulo de rotação enquanto rola
        bool active = true;
        AABB box;

        void updateAABB() {
            box.minX = x - radius; box.maxX = x + radius;
            box.minY = y - radius; box.maxY = y + radius;
            box.minZ = z - radius; box.maxZ = z + radius;
        }
    };
    
    struct Butter {
        int id = 0; 
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float sizeX = 1.2f, sizeY = 0.5f, sizeZ = 1.6f;
        float shakeTimer = 0.0f;
        AABB box;

        void updateAABB() {
            box.minX = x - sizeX * 0.5f; box.maxX = x + sizeX * 0.5f;
            box.minY = y - sizeY * 0.5f; box.maxY = y + sizeY * 0.5f;
            box.minZ = z - sizeZ * 0.5f; box.maxZ = z + sizeZ * 0.5f;
        }
    };

    struct Cheerio {
        int id = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float halfXZ = 0.9f;         // meia-extensão em X e Z
        float halfY = 0.3f;          // meia-extensão em Y
        AABB box;

        void updateAABB() {
            box.minX = x - halfXZ; box.maxX = x + halfXZ;
            box.minY = y - halfY;  box.maxY = y + halfY;
            box.minZ = z - halfXZ; box.maxZ = z + halfXZ;
        }
    };

    // Blocos da estrada e path points para auxílio no posicionamento
    struct PathPoint { float x, z; };
    
    struct RoadBlock {
        float centerX, centerZ;
        float halfLengthX, halfLengthZ;
    };

    void init(Renderer& renderer, const Config& config = Config());
    void update(float deltaTime);
    void render(Renderer& renderer, gmu& mu);

    const std::vector<RoadBlock>& getRoadBlocks() const { return roadBlocks; }
    const std::vector<Cheerio>& getCheerios()     const { return cheerios; }
    const std::vector<Butter>& getButters()       const { return butters; }
    const std::vector<Orange>& getOranges()       const { return oranges; }
    float getCheerioRadius()                      const { return cfg.cheerioOuterRadius; }
    float getFinishLinePosx()                     const { return cfg.finishLinePosx; }
    float getFinishLinePosz()                     const { return cfg.finishLinePosz; }

    float getRoadSurfaceY() const {
        return (cfg.tableHeight * 0.5f) + cfg.roadHeight;
    }
    void toggleBoundingBoxes() { showBoundingBoxes = !showBoundingBoxes; }
    void nudgeCheerio(int id, float dx, float dz);
    void hitButter(int id);

private:
    Config cfg;
    float totalPlayTime = 0.0f;

    std::vector<PathPoint> trackPath;
    std::vector<RoadBlock> roadBlocks;
    std::vector<Orange> oranges;
    std::vector<Butter> butters;
    std::vector<Cheerio> cheerios;

    float cheerioY = 0.0f;
    bool showBoundingBoxes = false;

    void buildTrackLayout();
    RoadBlock makeSegment(float x0, float z0, float x1, float z1, float halfRoad);
    void buildPathTrack(const std::vector<PathPoint>& pts);
    void placeCheeriosOnSegment(float x0, float z0, float x1, float z1, float spacing);
    void spawnButters();
    void spawnOranges();
    void resetOrange(Orange& orange);

    void drawTable(Renderer& renderer, gmu& mu);
    void drawRoad(Renderer& renderer, gmu& mu);
    void drawFinishLine(Renderer& renderer, gmu& mu);
    void drawCheerios(Renderer& renderer, gmu& mu);
    void drawOranges(Renderer& renderer, gmu& mu);
    void drawButters(Renderer& renderer, gmu& mu);
    void drawBoundingBoxes(Renderer& renderer, gmu& mu);
    void drawBox(Renderer& renderer, gmu& mu, const AABB& b);
};

#endif // _ENVIRONMENT_H_