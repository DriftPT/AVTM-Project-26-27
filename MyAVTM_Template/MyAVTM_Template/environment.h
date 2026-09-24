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
        float tableWidth = 150.0f;
		float tableDepth = 120.0f;
        float tableHeight = 0.6f;

        // Circuito
        float roadWidth = 12.0f;     // largura de cada bloco de estrada
        float roadHeight = 0.08f;   // espessura da estrada (fica em cima da mesa)

        // Cheerios (margens)
        float cheerioOuterRadius = 0.9f;  // raio exterior do anel
        float cheerioTubeRadius = 0.28f;  // espessura do tubo
        float cheerioSpacing = 2.0f;      // distância entre cheerios
        float cheerioYOffset = 0.05f;     // espaçamento acima da estrada

        // Manteigas e Laranjas
        int numButters = 7;
        int numOranges = 7;
        float orangeDefaultRadius = 0.8f;

		// Linha de meta
		float finishLinePosx = 0.0f; 
		float finishLinePosz = 0.0f; 
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
    };
    
    struct Butter {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float sizeX = 1.2f, sizeY = 0.5f, sizeZ = 1.6f;
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
    const std::vector<float>& getCheerioPosX()    const { return cheerioX; }
    const std::vector<float>& getCheerioPosZ()    const { return cheerioZ; }
    const std::vector<Butter>& getButters()       const { return butters; }
    const std::vector<Orange>& getOranges()       const { return oranges; }
    float getCheerioRadius()                      const { return cfg.cheerioOuterRadius; }
    float getFinishLinePosx()                     const { return cfg.finishLinePosx; }
    float getFinishLinePosz()                     const { return cfg.finishLinePosz; }

    float getRoadSurfaceY() const {
        return (cfg.tableHeight * 0.5f) + cfg.roadHeight;
    }

private:
    Config cfg;
    float totalPlayTime = 0.0f;

    std::vector<PathPoint> trackPath;
    std::vector<RoadBlock> roadBlocks;
    std::vector<float> cheerioX, cheerioZ;
    float cheerioY = 0.0f;

    std::vector<Orange> oranges;
    std::vector<Butter> butters;

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
};

#endif // _ENVIRONMENT_H_