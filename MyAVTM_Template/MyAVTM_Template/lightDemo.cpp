//
// AVT 2026: Texturing with Phong Shading and Text rendered with TrueType library
// The text rendering was based on https://dev.to/shreyaspranav/how-to-render-truetype-fonts-in-opengl-using-stbtruetypeh-1p5k
// You can also learn an alternative with FreeType text: https://learnopengl.com/In-Practice/Text-Rendering
// This demo was built for learning purposes only.
// Some code could be severely optimised, but I tried to
// keep as simple and clear as possible.
//
// The code comes with no warranties, use it at your own risk.
// You may use it, or parts of it, wherever you want.
//
// Author: João Madeiras Pereira
//

#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "gestureInput.h"

// include GLEW to access OpenGL 3.3 functions
#include <GL/glew.h>

// GLUT is the toolkit to interface with the OS
#include <GL/freeglut.h>

#include <IL/il.h>

// assimp include files. These three are usually needed.
#include <assimp/Importer.hpp> //OO version Header!
#include <assimp/scene.h>

#include "renderer.h"
#include "shader.h"
#include "mathUtility.h"
#include "model.h"
#include "texture.h"
#include "Environment.h"
#include "meshFromAssimp.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

using namespace std;

#define CAPTION "AVTM 2026 Welcome Demo"
int WindowHandle = 0;
int WinX = 640, WinY = 480;

unsigned int FrameCount = 0;

// Arduino
HANDLE arduinoSerial = INVALID_HANDLE_VALUE;

// File with the font
const string fontPathFile = "fonts/arial.ttf";

// Object of class gmu (Graphics Math Utility) to manage math and matrix operations
gmu mu;

// Object of class renderer to manage the rendering of meshes and ttf-based bitmap text
Renderer renderer;

// Object of class Environment to manage the environment
Environment env;

// Controlo de tempo entre frames, necessário para o env.update(deltaTime)
int lastFrameTime = 0;
// =========================================

// Camera Position
float camX, camY, camZ;

// Camera Spherical Coordinates
float alpha = 57.0f, _beta = 18.0f;
float r = 25.0f;

// Projections Variables
bool orthoProj = false;
bool perspProj = true;
const float PERSPECTIVE_FOV = 53.13f;
const float ORTHO_SIZE = 75.0f;           
const float TOP_CAMERA_HEIGHT = 150.0f;     
const float FOLLOW_CAMERA_DISTANCE = 25.0f;

// Rear-view mirror (top-right corner)
bool showRearMirror = true;
const float MIRROR_WIDTH_RATIO = 0.28f;
const float MIRROR_HEIGHT_RATIO = 0.28f;
const int MIRROR_MARGIN = 10;
const float MIRROR_FOV = PERSPECTIVE_FOV;

// Car starting position (start line)
float CAR_START_X = 0;
float CAR_START_Z = 0;
const float CAR_START_ANGLE = -90.0f;

float carX = CAR_START_X;
float carZ = CAR_START_Z;
float carAngle = CAR_START_ANGLE;

// Top of the floor in the current scene
float carBaseY = env.getRoadSurfaceY();

// Scale of the entire car
const float CAR_SCALE = 0.8f;

// Auxiliary variables
bool fixedCamera = false;
bool trackingCamera = true;
float cameraOrientation = 0.0f;
bool globalIllumination = true;
bool candlesIllumination = false;

// Mouse Tracking Variables
int startX, startY, tracking = 0;

// Frame counting and FPS computation
#define FPS 60
long myTime, timebase = 0, frame = 0;
char s[32];

// Global Illumination - Directional
float globalLightPos[4] = {0.2f, 1.0f, 0.3f, 0.0f};

// 6 Candles Lights Positions - Pontual
float candles[6][4] = {
	{-54.5f, env.getRoadSurfaceY() + 1.6f, 20.5f, 1.0f},
	{-47.7f, env.getRoadSurfaceY() + 1.6f, -40.9f, 1.0f},
	{0.0f, env.getRoadSurfaceY() + 1.6f, -40.9f, 1.0f},
	{54.5f, env.getRoadSurfaceY() + 1.6f, -13.6f, 1.0f},
	{54.5f, env.getRoadSurfaceY() + 1.6f, 38.2f, 1.0f},
	{0.0f, env.getRoadSurfaceY() + 1.6f, 38.2f, 1.0f}};

// Spotlight
bool carHeadlights = false;
float localHeadlights[2][4] = {
	{-1.0f, 1.0f, 2.71f, 1.0f},
	{1.0f, 1.0f, 2.71f, 1.0f}};
float spotCosCutOff = 0.93;

// Fog
bool fogEnabled = false;
void togglefog() { fogEnabled = !fogEnabled; }
float fogColor[3] = {0.5f, 0.6f, 0.7f};

bool fontLoaded = false;

// ==== ASSIMP Importer ==========
// Create an instance of the Importer class
Assimp::Importer importer;

// the global Assimp scene object
const aiScene *scene;

// scale factor for the Assimp model to fit in the window
float scaleFactor = 1.0;

// by default if there is a normal map then bump effect is implemented. press key "b" to enable/disable normal mapping
bool normalMapKey = TRUE;

// .obj Visability
bool objVisible = false;

// ------------------------------------------------------------
// Mesh IDs inside renderer.myMeshes
#define MESH_CUBE 0
#define MESH_PAWN 1
#define MESH_SPHERE 2
#define MESH_CYLINDER 3
#define MESH_CONE 4
#define MESH_TORUS 5
#define TEX_ENGINE_OFF 9
#define TEX_ENGINE_ON  10

// ------------------------------------------------------------
// Gameplay HUD

const int MAX_LIVES = 5;
int lives = MAX_LIVES;
bool gameOver = false;
bool paused = false;
bool gameStarted = false;

// Pontos
const int POINTS_CHEERIO_HIT = -10;
const int POINTS_BUTTER_HIT = -15;
const int POINTS_LIFE_LOST = -100;
const int POINTS_ORB_COLLECTED = +100;
int points = 0;
int lastScoreChange = 0;
float scoreChangeTimer = 0.0f;
const float SCORE_CHANGE_DISPLAY_TIME = 1.5f;

// Evita descontar pontos em todos os frames enquanto o carro está encostado ao obstáculo
bool wasHittingCheerio = false;
bool wasHittingButter = false;

// ------------------------------------------------------------
// Reflections
const float ROAD_REFLECTION_ALPHA = 0.85f; // menor = reflexo mais forte
const float TABLE_REFLECTION_ALPHA = 0.7f;

// ------------------------------------------------------------
// Car keyboard state

bool keyW = false;
bool keyS = false;
bool keyA = false;
bool keyD = false;

// ------------------------------------------------------------
// Arduino tilt controls

float arduinoThrottle = 0.0f;  // -1 = trás, +1 = frente
float arduinoSteering = 0.0f;  // -1 = esquerda, +1 = direita

// Configuração da inclinação (graus)
const float TILT_DEADZONE = 10.0f;
const float TILT_MAX_ANGLE = 40.0f;

// ------------------------------------------------------------
// Arduino light sensor
int arduinoLight = 0;
const int LIGHT_THRESHOLD = 100; // Threshold for detecting darkness

// ------------------------------------------------------------
// Arduino switch
bool arduinoIgnition = false;
bool previousArduinoIgnition = false;

bool previousArduinoBoostButton = false;

ma_engine audioEngine;
bool audioInitialized = false;

ma_sound carDrivingSound;
bool drivingSoundInitialized = false;
bool drivingSoundPlaying = false;

// ------------------------------------------------------------
// Car movement

float carVelocity = 0.0f;
float carAcceleration = 0.0f;

// Direction of the car.
// For now the car points towards +Z.
float carDirection[3] = {0.0f, 0.0f, 1.0f};

// Movement values
const float ACCELERATION = 6.0f;
const float DECELERATION = 4.0f;

const float TURN_SPEED = 75.0f; // degrees per second

const float MAX_SPEED = 25.0f;

// Used to calculate the time between frames
int lastUpdateTime = 0;

// ------------------------------------------------------------
// COLISIONS
// ------------------------------------------------------------
// Axis-Aligned Bounding Box structure

Environment::AABB carAABB;

bool showCarAABB = false;
void toggleCarAABB() { showCarAABB = !showCarAABB; }

// Car bounding box dimensions
float CAR_MIN_X = -1.815f * CAR_SCALE;
float CAR_MAX_X = 1.815f * CAR_SCALE;

float CAR_MIN_Y = 0.0f;
float CAR_MAX_Y = 1.85f * CAR_SCALE;

float CAR_MIN_Z = -3.15f * CAR_SCALE;
float CAR_MAX_Z = 2.85f * CAR_SCALE;

// ------------------------------------------------------------
// BOOST + SMOKE
// ------------------------------------------------------------

const float BOOST_DURATION  = 0.35f;  // segundos de aceleração extra
const float BOOST_COOLDOWN  = 1.5f;   // tempo até poder voltar a usar
const float BOOST_ACCEL     = 90.0f;  // aceleração extra durante o boost
const float BOOST_MAX_SPEED = 45.0f;  // velocidade máxima durante o boost
const float BOOST_DECAY     = 40.0f;  // quão depressa volta à velocidade normal

float boostTimer = 0.0f;
float boostCooldown = 0.0f;
struct SmokeParticle
{
	float x, y, z;
	float vx, vy, vz;
	float life, maxLife;
	float startRadius;
};
std::vector<SmokeParticle> smokeParticles;

// função auxiliar para gerar um número aleatório entre a e b
static float frand(float a, float b)
{
	return a + (b - a) * ((float)rand() / (float)RAND_MAX);
}

// ------------------------------------------------------------
// MATERIALS

struct CarMaterial
{
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float shininess;
};

// Car body - blue
CarMaterial bodyMaterial = {
	{0.02f, 0.05f, 0.15f, 1.0f},
	{0.02f, 0.30f, 0.80f, 1.0f},
	{0.60f, 0.70f, 0.90f, 1.0f},
	70.0f};

// Headlights - blueish white
CarMaterial headlightMaterial = {
	{0.12f, 0.14f, 0.18f, 1.0f},
	{0.75f, 0.85f, 1.00f, 1.0f},
	{1.00f, 1.00f, 1.00f, 1.0f},
	100.0f};

// RearLights - red
CarMaterial rearLightMaterial = {
	{0.20f, 0.00f, 0.00f, 1.0f},
	{0.90f, 0.05f, 0.05f, 1.0f},
	{0.80f, 0.30f, 0.30f, 1.0f},
	50.0f};

// Dark color
CarMaterial darkMaterial = {
	{0.03f, 0.03f, 0.03f, 1.0f},
	{0.12f, 0.12f, 0.12f, 1.0f},
	{0.20f, 0.20f, 0.20f, 1.0f},
	20.0f};

// Windshield and windows - light blue transparent glass
CarMaterial glassMaterial = {
	{0.05f, 0.10f, 0.15f, 1.0f},
	{0.15f, 0.35f, 0.45f, 0.7f},
	{0.90f, 0.95f, 1.00f, 1.0f},
	90.0f};

// Bounding box debug material - red
CarMaterial boundingBoxMaterial = {
	{0.3f, 0.0f, 0.0f, 1.0f},
	{1.0f, 0.0f, 0.0f, 1.0f},
	{1.0f, 0.0f, 0.0f, 1.0f},
	10.0f};



float mapTilt(float angle)
{
	float magnitude = fabsf(angle);

	// Ignorar pequenas inclinações
	if (magnitude <= TILT_DEADZONE)
		return 0.0f;

	// Converter o intervalo 5° a 30° para 0 a 1
	float value =
		(magnitude - TILT_DEADZONE) /
		(TILT_MAX_ANGLE - TILT_DEADZONE);

	// Limitar a 100%
	if (value > 1.0f)
		value = 1.0f;

	// Recuperar o sinal original
	return (angle < 0.0f) ? -value : value;
}


bool initArduinoSerial(const char* portName)
{
		arduinoSerial = CreateFileA(portName,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);

    if (arduinoSerial == INVALID_HANDLE_VALUE)
    {
        printf("Could not open Arduino serial port\n");
        return false;
    }

    DCB serialParams = {0};
    serialParams.DCBlength = sizeof(serialParams);

    if (!GetCommState(arduinoSerial, &serialParams))
        return false;

    serialParams.BaudRate = CBR_9600;
    serialParams.ByteSize = 8;
    serialParams.StopBits = ONESTOPBIT;
    serialParams.Parity = NOPARITY;

    if (!SetCommState(arduinoSerial, &serialParams))
        return false;

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;

    SetCommTimeouts(arduinoSerial, &timeouts);

    printf("Arduino connected on %s\n", portName);

    return true;
}

void startBoost();

void updateArduinoControls()
{
	if (arduinoSerial == INVALID_HANDLE_VALUE)
		return;

	static std::string serialBuffer;

	char buffer[64];
	DWORD bytesRead = 0;

	if (ReadFile( arduinoSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
		if (bytesRead > 0) {
			buffer[bytesRead] = '\0';
			serialBuffer += buffer;

			size_t newlinePos;

			while ((newlinePos = serialBuffer.find('\n')) != std::string::npos) {
				std::string line =
					serialBuffer.substr(0, newlinePos);

				serialBuffer.erase(0, newlinePos + 1);

				float pitch, roll;
				int light, ignition, boostButton;

				// Receive pitch, roll and light sensor values
				if (sscanf(line.c_str(), "%f,%f,%d,%d,%d", &pitch, &roll, &light, &ignition, &boostButton) == 5) {

					// Forward / backward acceleration
					arduinoThrottle = mapTilt(pitch);

					// Left / right steering
					arduinoSteering = -mapTilt(roll);

					// Light sensor reading
					arduinoLight = light;

					// Ignition reading
					arduinoIgnition = (ignition == 1);

					// Boost button reading
					bool boostPressed = (boostButton == 1);

					if (boostPressed && !previousArduinoBoostButton) {
						startBoost();
					}

					previousArduinoBoostButton = boostPressed;

					printf(
						"Pitch: %.1f Roll: %.1f | Throttle: %.2f Steering: %.2f | Light: %d | Boost: %d\n",
						pitch,
						roll,
						arduinoThrottle,
						arduinoSteering,
						arduinoLight,
						boostButton
					);
				}
			}
		}
	}
}

/// ::::::::::::::::::::::::::::::::::::::::::::::::CALLBACK FUNCIONS:::::::::::::::::::::::::::::::::::::::::::::::::://///

void timer(int value)
{
	std::ostringstream oss;
	oss << CAPTION << ": " << FrameCount << " FPS @ (" << WinX << "x" << WinY << ")";
	std::string s = oss.str();
	glutSetWindow(WindowHandle);
	glutSetWindowTitle(s.c_str());
	FrameCount = 0;
	glutTimerFunc(1000, timer, 0);
}

void refresh(int value)
{
	// PUT YOUR CODE HERE
	glutPostRedisplay();
	glutTimerFunc(1000 / FPS, refresh, 0);
}

// ------------------------------------------------------------
//
// Reshape Callback Function
//

void changeSize(int w, int h)
{

	float ratio;
	// Prevent a divide by zero, when window is too short
	if (h == 0)
		h = 1;
	WinX = w;
	WinY = h;
	// set the viewport to be the entire window
	glViewport(0, 0, w, h);
	// set the projection matrix
	ratio = (1.0f * w) / h;
	if (perspProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.perspective(PERSPECTIVE_FOV, ratio, 0.1f, 1000.0f);
	}
	else if (orthoProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(-ORTHO_SIZE * ratio, ORTHO_SIZE * ratio, -ORTHO_SIZE, ORTHO_SIZE, -300.0f, 300.0f);
	}
}

// ------------------------------------------------------------
// Camera view 

// Update Projection Function
void updateProjection()
{

	float ratio;
	int w = WinX; // window width
	int h = WinY; // window height
	// set the projection matrix
	ratio = (1.0f * w) / h;
	if (perspProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.perspective(PERSPECTIVE_FOV, ratio, 0.1f, 1000.0f); // mu.perspective(fov, ratio, near, far)
	}
	else if (orthoProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(-ORTHO_SIZE * ratio, ORTHO_SIZE * ratio, -ORTHO_SIZE, ORTHO_SIZE, -300.0f, 300.0f); // mu.ortho(left, right, bottom, top, near, far)
	}
	glutPostRedisplay(); // viewport refresh
}


// Set Camera Over Function
void static setCameraOver()
{
	alpha = 0.0f;
	_beta = 90.0f; // Over Camera Spherical Coordinates
	r = TOP_CAMERA_HEIGHT;
	camX = r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f); // =0
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f); // =0
	camY = r * sin(_beta * 3.14f / 180.0f); // =TOP_CAMERA_HEIGHT
}


// Set Camera Behind the car Function
void static setCameraBehind()
{
	alpha = 180.0f + carAngle + cameraOrientation; 
	_beta = 20.0f; // Behind Camera Spherical Coordinates - alpha horizontal rotation - beta vertical rotation
	camX = carX + r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camZ = carZ + r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camY = carBaseY + r * sin(_beta * 3.14f / 180.0f);

	mu.lookAt(camX, camY, camZ, carX, carBaseY, carZ, 0, 1, 0); // the sum of the car postion makes the camera follow it
}

// ------------------------------------------------------------
// Aux function to render a mesh using the current MODEL matrix.

void drawMesh(int meshID, int texMode = 0)
{
	dataMesh data;

	mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
	mu.computeNormalMatrix3x3();

	data.meshID = meshID;
	data.texMode = texMode;
	data.vm = mu.get(gmu::VIEW_MODEL);
	data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
	data.normal = mu.getNormalMatrix();
	renderer.renderMesh(data);
}

void setMeshMaterial(int meshID, const CarMaterial &material)
{
	memcpy(
		renderer.myMeshes[meshID].mat.ambient,
		material.ambient,
		4 * sizeof(float));

	memcpy(
		renderer.myMeshes[meshID].mat.diffuse,
		material.diffuse,
		4 * sizeof(float));

	memcpy(
		renderer.myMeshes[meshID].mat.specular,
		material.specular,
		4 * sizeof(float));

	renderer.myMeshes[meshID].mat.shininess =
		material.shininess;
}

// Function to update the car's bounding box based on the Assimp model's bounding box
void updateCarBounds()
{
	if (objVisible && scene != nullptr)
	{
		aiVector3D objMin, objMax;
		get_bounding_box(scene, &objMin, &objMax);

		float scale = scaleFactor * 4.5f;

		CAR_MIN_X = objMin.x * scale;
		CAR_MAX_X = objMax.x * scale;

		CAR_MIN_Z = -objMax.y * scale;
		CAR_MAX_Z = -objMin.y * scale;

		CAR_MIN_Y = objMin.z * scale;
		CAR_MAX_Y = objMax.z * scale;
	}
	else
	{
		CAR_MIN_X = -1.815f * CAR_SCALE;
		CAR_MAX_X = 1.815f * CAR_SCALE;

		CAR_MIN_Y = 0.0f;
		CAR_MAX_Y = 1.85f * CAR_SCALE;

		CAR_MIN_Z = -3.15f * CAR_SCALE;
		CAR_MAX_Z = 2.85f * CAR_SCALE;
	}
}

// Function to update the car's Axis-Aligned Bounding Box (AABB) based on its position and orientation
void updateCarAABB()
{
	float angleRad = carAngle * 3.14f / 180.0f;

	float c = cos(angleRad);
	float s = sin(angleRad);

	// Four corners of the local bounding box in the XZ plane
	float localX[4] = {CAR_MIN_X, CAR_MAX_X, CAR_MAX_X, CAR_MIN_X};

	float localZ[4] = {CAR_MIN_Z, CAR_MIN_Z, CAR_MAX_Z, CAR_MAX_Z};

	// Rotate first corner and use it to initialize min/max
	float rotatedX = localX[0] * c + localZ[0] * s;
	float rotatedZ = -localX[0] * s + localZ[0] * c;

	carAABB.minX = carAABB.maxX = carX + rotatedX;
	carAABB.minZ = carAABB.maxZ = carZ + rotatedZ;

	// Rotate the remaining corners
	for (int i = 1; i < 4; i++)
	{
		rotatedX = localX[i] * c + localZ[i] * s;
		rotatedZ = -localX[i] * s + localZ[i] * c;

		float worldX = carX + rotatedX;
		float worldZ = carZ + rotatedZ;

		if (worldX < carAABB.minX)
			carAABB.minX = worldX;

		if (worldX > carAABB.maxX)
			carAABB.maxX = worldX;

		if (worldZ < carAABB.minZ)
			carAABB.minZ = worldZ;

		if (worldZ > carAABB.maxZ)
			carAABB.maxZ = worldZ;
	}

	carAABB.minY = carBaseY + CAR_MIN_Y;
	carAABB.maxY = carBaseY + CAR_MAX_Y;
}

void resetCarToStart()
{
	carX = CAR_START_X;
	carZ = CAR_START_Z;
	carAngle = CAR_START_ANGLE;
	carVelocity = 0.0f;
	carAcceleration = 0.0f;
	updateCarAABB();
}

void addPoints(int delta)
{
	points += delta;
	lastScoreChange = delta;
	scoreChangeTimer = SCORE_CHANGE_DISPLAY_TIME;
}

void loseLife()
{
	addPoints(POINTS_LIFE_LOST);
	lives--;
	if (lives <= 0)
	{
		lives = 0;
		gameOver = true;
	}
	else
	{
		resetCarToStart();
	}
}

void handleCollisions(float oldX, float oldZ)
{
	const auto &cheerios = env.getCheerios();
	const auto &butters = env.getButters();
	const auto &oranges = env.getOranges();
	const auto &orbs = env.getPointOrbs();
	const float CHEERIO_PUSH = 0.8f; // o quanto o cheerio se move quando é atingido

	// CHEERIOS
	bool cheerioHit = false;
	for (const auto &c : cheerios)
	{
		if (env.checkOverlap(carAABB, c.box))
		{
			cheerioHit = true;

			// Empurra ligeiramente o cheerio para longe do carro
			float dx = c.x - carX;
			float dz = c.z - carZ;
			float len = sqrtf(dx * dx + dz * dz);
			if (len > 0.0001f)
			{
				dx /= len;
				dz /= len;
			} // normalização do vetor de deslocamento
			env.nudgeCheerio(c.id, dx * CHEERIO_PUSH, dz * CHEERIO_PUSH);
		}
	}
	// Se o carro colidiu com um cheerio, repõe a posição anterior e para
	if (cheerioHit)
	{
		if (!wasHittingCheerio)
			addPoints(POINTS_CHEERIO_HIT);

		// O carro repõe a posição anterior e para
		carX = oldX;
		carZ = oldZ;
		carVelocity = 0.0f;
		carAcceleration = 0.0f;
		updateCarAABB();
	}
	wasHittingCheerio = cheerioHit;

	// BUTTERS
	bool butterHit = false;
	for (const auto &b : butters)
	{
		if (env.checkOverlap(carAABB, b.box))
		{
			butterHit = true;
			env.hitButter(b.id);
		}
	}
	if (butterHit)
	{
		if (!wasHittingButter)
			addPoints(POINTS_BUTTER_HIT);

		carX = oldX;
		carZ = oldZ;
		carVelocity = 0.0f;
		carAcceleration = -10.0f;
		updateCarAABB();
	}
	wasHittingButter = butterHit;

	// ORANGES
	for (const auto &o : oranges)
	{
		if (o.active && env.checkOverlap(carAABB, o.box))
		{
			env.hitOrange(o.id);
			loseLife();
			break;
		}
	}

	// POINT ORBS
	for (const auto &orb : orbs)
	{
		if (orb.active && env.checkOverlap(carAABB, orb.box))
		{
			env.hitPointOrb(orb.id);
			addPoints(POINTS_ORB_COLLECTED);
			break;
		}
	}

	// Verificar se caiu da mesa
	float halfTableX = env.getTableWidth() * 0.5f;
	float halfTableZ = env.getTableDepth() * 0.5f;
	if (fabs(carX) > halfTableX || fabs(carZ) > halfTableZ)
	{
		loseLife();
	}
}

void startBoost()
{
	if (!gameStarted || paused || gameOver)
		return;
	if (boostTimer > 0.0f || boostCooldown > 0.0f)
		return;
	// com o Arduino ligado, só funciona com a ignição ligada
	if (arduinoSerial != INVALID_HANDLE_VALUE && !arduinoIgnition)
		return;

	boostTimer = BOOST_DURATION;
	boostCooldown = BOOST_COOLDOWN;
}

void spawnSmokeAtExhausts()
{
	float angleRad = carAngle * 3.14159265f / 180.0f;
	float s = sin(angleRad);
	float c = cos(angleRad);

	// posições locais dos escapes (as mesmas do drawCar)
	const float exhaustX[2] = { -0.9f, 0.9f };

	for (int i = 0; i < 2; i++)
	{
		float lx = exhaustX[i] * CAR_SCALE;
		float ly = 0.5f * CAR_SCALE;
		float lz = -3.0f * CAR_SCALE;

		SmokeParticle p;
		p.x = carX + (lx * c + lz * s);
		p.y = carBaseY + ly;
		p.z = carZ + (-lx * s + lz * c);

		// vai para trás do carro, com algum desvio lateral e a subir
		float back = frand(2.0f, 5.0f);
		p.vx = -carDirection[0] * back + frand(-0.8f, 0.8f);
		p.vy = frand(0.8f, 2.0f);
		p.vz = -carDirection[2] * back + frand(-0.8f, 0.8f);

		p.maxLife = frand(0.6f, 1.1f);
		p.life = p.maxLife;
		p.startRadius = frand(0.15f, 0.3f);
		smokeParticles.push_back(p);
	}
}

void updateBoostAndSmoke(float dt)
{
	if (boostCooldown > 0.0f)
		boostCooldown = fmaxf(0.0f, boostCooldown - dt);

	if (boostTimer > 0.0f)
	{
		boostTimer -= dt;
		// 3 partículas por escape por frame enquanto dura o boost
		for (int k = 0; k < 3; k++)
			spawnSmokeAtExhausts();
	}

	for (size_t i = 0; i < smokeParticles.size();)
	{
		SmokeParticle &p = smokeParticles[i];
		p.life -= dt;
		if (p.life <= 0.0f)
		{
			smokeParticles[i] = smokeParticles.back();
			smokeParticles.pop_back();
			continue;
		}
		p.x += p.vx * dt;
		p.y += p.vy * dt;
		p.z += p.vz * dt;
		p.vx *= (1.0f - 1.5f * dt); // o fumo vai abrandando
		p.vz *= (1.0f - 1.5f * dt);
		++i;
	}
}

void drawSmoke()
{
	if (smokeParticles.empty())
		return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE); // translúcido: não escreve no depth buffer

	for (const auto &p : smokeParticles)
	{
		float t = 1.0f - p.life / p.maxLife; // 0 = acabou de nascer, 1 = a desparecer
		float radius = p.startRadius * (1.0f + 3.0f * t); // cresce
		float alpha = 0.55f * (1.0f - t);                 // desvanece

		CarMaterial smoke = {
			{0.30f, 0.55f, 1.00f, 1.0f},
			{0.20f, 0.45f, 1.00f, alpha},
			{0.0f, 0.0f, 0.0f, 1.0f},
			1.0f};
		setMeshMaterial(MESH_SPHERE, smoke); // fumo é mais transpaente a medida que se afasta do carro (tempo de vida da partícula)

		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, p.x, p.y, p.z);
		mu.scale(gmu::MODEL, radius, radius, radius);
		drawMesh(MESH_SPHERE);
		mu.popMatrix(gmu::MODEL);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

// Desenha o ícone do motor no HUD (coordenadas de ecrã, projeção ortográfica já ativa)
void drawEngineIcon(float x, float y, float size)
{
	// Estado atual do motor: sem Arduino, consideramos o motor ligado
	bool engineOn = (arduinoSerial == INVALID_HANDLE_VALUE) || arduinoIgnition;

	// Motor desligado: pisca (0.5s visível, 0.5s escondido)
	if (!engineOn)
	{
		int blink = (glutGet(GLUT_ELAPSED_TIME) / 500) % 2;
		if (blink == 0)
			return;
	}

	renderer.setTexUnit(0, engineOn ? TEX_ENGINE_ON : TEX_ENGINE_OFF);

	CarMaterial iconMaterial = {
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		1.0f};
	setMeshMaterial(MESH_CUBE, iconMaterial);

	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, x + size * 0.5f, y + size * 0.5f, 0.0f);
	mu.scale(gmu::MODEL, size, size, 1.0f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	drawMesh(MESH_CUBE, 5);
	mu.popMatrix(gmu::MODEL);
}

void updateCar(float dt)
{
	// ------------------------------------------------------------
	// INPUT: KEYBOARD + ARDUINO

	float throttleInput = 0.0f;
	float steeringInput = 0.0f;

	if (arduinoSerial == INVALID_HANDLE_VALUE || arduinoIgnition)
	{
		throttleInput = arduinoThrottle;
		steeringInput = arduinoSteering;

		// Keyboard overrides Arduino for acceleration
		if (keyW && !keyS)
			throttleInput = 1.0f;
		else if (keyS && !keyW)
			throttleInput = -1.0f;

		// Keyboard overrides Arduino for steering
		if (keyA && !keyD)
			steeringInput = -1.0f;
		else if (keyD && !keyA)
			steeringInput = 1.0f;
	}

	// ------------------------------------------------------------
	// ACCELERATION

	if (throttleInput > 0.0f)
	{
		// Accelerate forward
		carAcceleration = ACCELERATION * throttleInput;
	}
	else if (throttleInput < 0.0f)
	{
		// Accelerate backwards
		carAcceleration = ACCELERATION * throttleInput;
	}
	else
	{
		// Neither W nor S nor Arduino acceleration:
		// gradually reduce the velocity.

		if (carVelocity > 0.0f)
		{
			carAcceleration = -DECELERATION;
		}
		else if (carVelocity < 0.0f)
		{
			carAcceleration = DECELERATION;
		}
		else
		{
			carAcceleration = 0.0f;
		}
	}

	// ------------------------------------------------------------
	// UPDATE VELOCITY

	float oldVelocity = carVelocity;

	carVelocity += carAcceleration * dt;

	// When no acceleration input is active,
	// stop at exactly zero instead of crossing it.
	if (throttleInput == 0.0f)
	{
		if ((oldVelocity > 0.0f && carVelocity < 0.0f) ||
			(oldVelocity < 0.0f && carVelocity > 0.0f))
		{
			carVelocity = 0.0f;
			carAcceleration = 0.0f;
		}
	}

	// ------------------------------------------------------------
	// BOOST + LIMIT MAXIMUM SPEED

	if (boostTimer > 0.0f)
	{
		// dash: aceleração extra para a frente, a velocidade sobe gradualmente
		carVelocity += BOOST_ACCEL * dt;
		if (carVelocity > BOOST_MAX_SPEED)
			carVelocity = BOOST_MAX_SPEED;
	}
	else if (carVelocity > MAX_SPEED)
	{
		// acabou o boost: volta suavemente à velocidade normal
		carVelocity -= BOOST_DECAY * dt;
		if (carVelocity < MAX_SPEED)
			carVelocity = MAX_SPEED;
	}

	if (carVelocity < -MAX_SPEED)
		carVelocity = -MAX_SPEED;

	// ------------------------------------------------------------
	// STEERING

	// Only turn if the car is moving
	if (fabs(carVelocity) > 0.01f)
	{
		// When reversing, steering direction is reversed
		float directionSign = 1.0f;

		if (carVelocity < 0.0f)
			directionSign = -1.0f;

		// Steering proportional to keyboard or Arduino input
		carAngle -= TURN_SPEED * steeringInput * directionSign * dt;
	}

	// ------------------------------------------------------------
	// CAR DIRECTION

	float angleRad = carAngle * 3.14159265f / 180.0f;

	carDirection[0] = sin(angleRad);
	carDirection[1] = 0.0f;
	carDirection[2] = cos(angleRad);

	// ------------------------------------------------------------
	// UPDATE POSITION

	float oldX = carX;
	float oldZ = carZ;

	carX += carDirection[0] * carVelocity * dt;
	carZ += carDirection[2] * carVelocity * dt;

	updateCarAABB();
	handleCollisions(oldX, oldZ);
}

void drawCabin()
{
	// ROOF
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.0f, 1.75f, -0.4f);
	mu.scale(gmu::MODEL, 2.4f, 0.2f, 2.4f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// FRONT RIGHT PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, -1.1f, 1.5f, 0.7f);
	mu.scale(gmu::MODEL, 0.2f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// FRONT LEFT PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 1.1f, 1.5f, 0.7f);
	mu.scale(gmu::MODEL, 0.2f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// REAR RIGHT PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, -1.1f, 1.5f, -1.5f);
	mu.scale(gmu::MODEL, 0.2f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// REAR LEFT PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 1.1f, 1.5f, -1.50f);
	mu.scale(gmu::MODEL, 0.2f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// LEFT CENTRE PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 1.1f, 1.5f, -0.4f);
	mu.scale(gmu::MODEL, 0.20f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);

	// RIGHT CENTRE PILLAR
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, -1.1f, 1.5f, -0.4f);
	mu.scale(gmu::MODEL, 0.20f, 0.70f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);
	mu.popMatrix(gmu::MODEL);
}

void drawWheel(float x, float y, float z)
{
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, x, y, z);
	mu.rotate(gmu::MODEL, 90.0f, 0.0f, 0.0f, 1.0f);

	// Tire
	mu.pushMatrix(gmu::MODEL);

	mu.scale(gmu::MODEL, 0.33f, 0.33f, 0.33f);
	renderer.setTexUnit(0, 7);
	setMeshMaterial(MESH_TORUS, darkMaterial);
	drawMesh(MESH_TORUS, 2);

	mu.popMatrix(gmu::MODEL);

	// Rim
	mu.pushMatrix(gmu::MODEL);

	mu.scale(gmu::MODEL, 0.4f, 0.12f, 0.4f);
	setMeshMaterial(MESH_CYLINDER, darkMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	mu.popMatrix(gmu::MODEL);
}

void drawSideWindow(float x, float z)
{
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, x, 1.4f, z);
	mu.scale(gmu::MODEL, 0.2f, 0.5f, 0.9f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, glassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);
}

void drawMirror(float side)
{
	float angle = side * 20.0f;

	// Cylindrical support
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, side * 1.2f, 1.3f, 0.75f);
	mu.rotate(gmu::MODEL, -angle, 0.0f, 1.0f, 0.0f);
	mu.rotate(gmu::MODEL, 90.0f, 0.0f, 0.0f, 1.0f);
	mu.scale(gmu::MODEL, 0.05f, 0.1f, 0.05f);

	setMeshMaterial(MESH_CYLINDER, bodyMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	// Body
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, side * 1.4f, 1.35f, 0.73f);
	mu.rotate(gmu::MODEL, angle, 0.0f, 1.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.3f, 0.15f, 0.05f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// Miror
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, side * 1.4f, 1.35f, 0.72f);
	mu.rotate(gmu::MODEL, angle, 0.0f, 1.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.2f, 0.1f, 0.05f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, glassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);
}

void drawSeat(float x, float z)
{
	mu.pushMatrix(gmu::MODEL);

	// Backrest
	mu.translate(gmu::MODEL, x, 1.35f, z);
	mu.scale(gmu::MODEL, 0.65f, 0.40f, 0.20f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, darkMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);
}

void drawCar()
{
	// ------------------------------------------------------------
	// CAR TRANSFORMATION
	//
	// Everything drawn between this push/pop belongs to the car.

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, carX, carBaseY, carZ);
	mu.rotate(gmu::MODEL, carAngle, 0.0f, 1.0f, 0.0f);
	mu.scale(gmu::MODEL, CAR_SCALE, CAR_SCALE, CAR_SCALE);

	// ------------------------------------------------------------
	// MAIN BODY

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.8f, 0.0f);
	mu.scale(gmu::MODEL, 3.2f, 0.7f, 5.4f);

	// Cube is originally defined from 0 to 1,
	// so centre it around its local origin.
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// CABIN

	drawCabin();

	// ------------------------------------------------------------
	// HOOD RAISED

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.2f, 1.7f);
	mu.scale(gmu::MODEL, 1.8f, 0.1f, 1.4f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// SEATS and STEERING WHEEL

	// Front seats
	drawSeat(-0.55f, -0.2f);
	drawSeat(0.55f, -0.2f);

	// Rear seats
	drawSeat(-0.55f, -1.3f);
	drawSeat(0.55f, -1.3f);

	// steering wheel
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.55f, 1.15f, 0.50f);
	mu.rotate(gmu::MODEL, 90.0f, 1.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.15f, 0.15f, 0.15f);
	setMeshMaterial(MESH_TORUS, darkMaterial);
	drawMesh(MESH_TORUS);
	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// WHEELS

	drawWheel(-1.65f, 0.5f, 1.9f); // Front right
	drawWheel(1.65f, 0.5f, 1.9f);  // Front left

	drawWheel(-1.65f, 0.5f, -1.9f); // Rear right
	drawWheel(1.65f, 0.5f, -1.9f);	// Rear left

	// ------------------------------------------------------------
	// BUMPER

	// upper bumper
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.50f, 0.0f);
	mu.scale(gmu::MODEL, 3.3f, 0.2f, 5.7f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// lower front bumper
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.40f, 2.61f);
	mu.scale(gmu::MODEL, 3.31f, 0.1f, 0.5f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, darkMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// FRONT LICENSE PLATE

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.75f, 2.70f);
	mu.scale(gmu::MODEL, 0.8f, 0.25f, 0.02f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	renderer.setTexUnit(0, 8);
	drawMesh(MESH_CUBE, 2);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// RIGHT HEADLIGHT

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -1.0f, 1.0f, 2.71f);
	mu.scale(gmu::MODEL, 0.8f, 0.2f, 0.1f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, headlightMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// LEFT HEADLIGHT

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 1.0f, 1.0f, 2.71f);
	mu.scale(gmu::MODEL, 0.8f, 0.2f, 0.1f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, headlightMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// EXHAUSTS

	// Left Exhaust
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.90f, 0.50f, -3.0f);
	mu.rotate(gmu::MODEL, 90.0f, 1.0f, 0.0f, 0.0f); // Rotate it so its axis points towards the rear.
	mu.scale(gmu::MODEL, 0.20f, 0.20f, 0.20f);

	setMeshMaterial(MESH_CYLINDER, darkMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	// Right Exhaust
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -0.90f, 0.50f, -3.0f);
	mu.rotate(gmu::MODEL, 90.0f, 1.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.20f, 0.20f, 0.20f);

	setMeshMaterial(MESH_CYLINDER, darkMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR LIGHTS

	// Right outer light
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -1.3f, 0.9f, -2.7f);
	mu.scale(gmu::MODEL, 0.15f, 0.15f, 0.1f);
	setMeshMaterial(MESH_SPHERE, rearLightMaterial);
	drawMesh(MESH_SPHERE);

	mu.popMatrix(gmu::MODEL);

	// Right inner light
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -1.0f, 0.85f, -2.7f);
	mu.scale(gmu::MODEL, 0.1f, 0.1f, 0.1f);
	setMeshMaterial(MESH_SPHERE, rearLightMaterial);
	drawMesh(MESH_SPHERE);

	mu.popMatrix(gmu::MODEL);

	// Left outer light
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 1.3f, 0.9f, -2.7f);
	mu.scale(gmu::MODEL, 0.15f, 0.15f, 0.1f);
	setMeshMaterial(MESH_SPHERE, rearLightMaterial);
	drawMesh(MESH_SPHERE);

	mu.popMatrix(gmu::MODEL);

	// Left inner light
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 1.0f, 0.85f, -2.7f);
	mu.scale(gmu::MODEL, 0.1f, 0.1f, 0.1f);
	setMeshMaterial(MESH_SPHERE, rearLightMaterial);
	drawMesh(MESH_SPHERE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR LICENSE PLATE

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.8f, -2.70f);
	mu.scale(gmu::MODEL, 0.85f, 0.30f, 0.01f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	renderer.setTexUnit(0, 8);
	drawMesh(MESH_CUBE, 2);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR SPOILER - RIGHT SUPPORT

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -1.15f, 1.32f, -2.5f);
	mu.rotate(gmu::MODEL, -20.0f, 1.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.08f, 0.35f, 0.08f);

	setMeshMaterial(MESH_CYLINDER, bodyMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR SPOILER - LEFT SUPPORT

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 1.15f, 1.32f, -2.5f);
	mu.rotate(gmu::MODEL, -20.0f, 1.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.08f, 0.35f, 0.08f);

	setMeshMaterial(MESH_CYLINDER, bodyMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR SPOILER - WING

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.55f, -2.55f);
	mu.scale(gmu::MODEL, 3.5f, 0.08f, 0.45f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// GLASS MATERIAL
	// ------------------------------------------------------------
	// FRONT WINDSHIELD

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.4f, 0.7f);
	mu.scale(gmu::MODEL, 2.0f, 0.5f, 0.2f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, glassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// REAR WINDOW

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.4f, -1.5f);
	mu.scale(gmu::MODEL, 2.0f, 0.5f, 0.2f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, glassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// SIDE WINDOWS

	// right windows
	drawSideWindow(-1.1f, 0.15f);
	drawSideWindow(-1.1f, -0.95f);

	// left windows
	drawSideWindow(1.1f, 0.15f);
	drawSideWindow(1.1f, -0.95f);

	// ------------------------------------------------------------
	// MIRRORS

	drawMirror(-1.0f); // Right mirror
	drawMirror(1.0f);  // Left mirror

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	// ------------------------------------------------------------
	// END OF CAR

	mu.popMatrix(gmu::MODEL);
}

void drawCarAABB()
{
	if (!showCarAABB)
		return;

	// Calculate box dimensions
	float sizeX = carAABB.maxX - carAABB.minX;
	float sizeY = carAABB.maxY - carAABB.minY;
	float sizeZ = carAABB.maxZ - carAABB.minZ;

	// Calculate box centre
	float centerX = (carAABB.minX + carAABB.maxX) * 0.5f;
	float centerY = (carAABB.minY + carAABB.maxY) * 0.5f;
	float centerZ = (carAABB.minZ + carAABB.maxZ) * 0.5f;

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, centerX, centerY, centerZ);

	mu.scale(gmu::MODEL, sizeX, sizeY, sizeZ);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, boundingBoxMaterial);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Draw only the edges

	glDisable(GL_CULL_FACE); // So we can see all cube faces/edges

	drawMesh(MESH_CUBE);

	// Restore normal rendering
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	mu.popMatrix(gmu::MODEL);
}

// ------------------------------------------------------------
// Candles Help debugger
//

void drawCandles()
{
	CarMaterial candleBase = {
		{0.35f, 0.30f, 0.22f, 1.0f},
		{0.90f, 0.80f, 0.65f, 1.0f},
		{1.00f, 0.90f, 0.75f, 1.0f},
		40.0f};

	CarMaterial candleFire = {
		{0.30f, 0.15f, 0.00f, 1.0f},
		{1.00f, 0.70f, 0.10f, 1.0f},
		{1.00f, 0.90f, 0.50f, 1.0f},
		100.0f};

	for (int i = 0; i < 6; i++)
	{
		// Cilindro base
		setMeshMaterial(MESH_CYLINDER, candleBase);
		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, candles[i][0], env.getRoadSurfaceY() + 0.7f, candles[i][2]);
		mu.scale(gmu::MODEL, 0.5f, 0.7f, 0.5f);
		drawMesh(MESH_CYLINDER);
		mu.popMatrix(gmu::MODEL);

		// Cilindro pequeno (fogo)
		setMeshMaterial(MESH_CYLINDER, candleFire);
		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, candles[i][0], env.getRoadSurfaceY() + 0.7f + 0.6f, candles[i][2]);
		mu.scale(gmu::MODEL, 0.1f, 0.25f, 0.1f);
		drawMesh(MESH_CYLINDER);
		mu.popMatrix(gmu::MODEL);
	}
}

void updateCarHeadlights(float headlightsWorld[2][4], float coneDirWorld[4])
{
	float angleRad = carAngle * 3.14159265f / 180.0f;

	float s = sin(angleRad);
	float c = cos(angleRad);

	for (int i = 0; i < 2; i++)
	{
		// Aplicar escala do carro
		float localX = localHeadlights[i][0] * CAR_SCALE;
		float localY = localHeadlights[i][1] * CAR_SCALE;
		float localZ = localHeadlights[i][2] * CAR_SCALE;

		// Aplicar a mesma rotação Y usada no carro
		float rotatedX = localX * c + localZ * s;
		float rotatedZ = -localX * s + localZ * c;

		// Aplicar translação do carro
		headlightsWorld[i][0] = carX + rotatedX;
		headlightsWorld[i][1] = carBaseY + localY;
		headlightsWorld[i][2] = carZ + rotatedZ;
		headlightsWorld[i][3] = 1.0f;
	}

	// Direção dos faróis: frente do carro, sem inclinação para baixo
	coneDirWorld[0] = carDirection[0];
	coneDirWorld[1] = 0.0f;
	coneDirWorld[2] = carDirection[2];
	coneDirWorld[3] = 0.0f;
}

// ------------------------------------------------------------
//
// Render stufff
//

// Envia as luzes em eye space. Se mirrored, espelha-as em relação ao plano da estrada
void setSceneLights(bool mirrored)
{
	float planeY = env.getRoadSurfaceY();

	// Global (direcional, w = 0): basta inverter o Y da direção
	float gWorld[4] = {globalLightPos[0], globalLightPos[1], globalLightPos[2], globalLightPos[3]};
	if (mirrored)
		gWorld[1] = -gWorld[1];
	float gloablLightPosEye[4];
	mu.multMatrixPoint(gmu::VIEW, gWorld, gloablLightPosEye);
	renderer.setGlobalLight(globalIllumination, gloablLightPosEye);

	// Faróis
	float carHeadlightsWorld[2][4], coneDirWorld[4], carHeadlightsEye[2][4], coneDirEye[4];
	updateCarHeadlights(carHeadlightsWorld, coneDirWorld);
	if (mirrored)
	{
		for (int i = 0; i < 2; i++)
			carHeadlightsWorld[i][1] = 2.0f * planeY - carHeadlightsWorld[i][1];
		coneDirWorld[1] = -coneDirWorld[1];
	}
	for (int i = 0; i < 2; i++)
		mu.multMatrixPoint(gmu::VIEW, carHeadlightsWorld[i], carHeadlightsEye[i]);
	mu.multMatrixPoint(gmu::VIEW, coneDirWorld, coneDirEye);
	renderer.setSpotLights(carHeadlights, carHeadlightsEye, coneDirEye, spotCosCutOff);

	// Velas
	float candlesEye[6][4];
	for (int i = 0; i < 6; i++)
	{
		float p[4] = {candles[i][0], candles[i][1], candles[i][2], 1.0f};
		if (mirrored)
			p[1] = 2.0f * planeY - p[1];
		mu.multMatrixPoint(gmu::VIEW, p, candlesEye[i]);
	}
	renderer.setCandlesLights(candlesIllumination, candlesEye);
}

// Carro
void drawCarModel()
{
	if (!objVisible)
	{
		drawCar();
		return;
	}

	dataMesh data;
	for (unsigned int n = 0; n < renderer.myMeshes.size(); ++n)
	{
		mu.pushMatrix(gmu::MODEL);

		mu.multMatrix(gmu::MODEL, renderer.myMeshes[n].transform);
		mu.translate(gmu::MODEL, carX, carBaseY, carZ);
		mu.rotate(gmu::MODEL, -90.0f, 1.0f, 0.0f, 0.0f);
		mu.rotate(gmu::MODEL, carAngle, 0.0f, 0.0f, 1.0f);
		mu.scale(gmu::MODEL, scaleFactor * 4.5, scaleFactor * 4.5, scaleFactor * 4.5);

		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		mu.computeNormalMatrix3x3();

		data.meshID = n;
		data.texMode = 0;
		data.model = mu.get(gmu::MODEL);
		data.view = mu.get(gmu::VIEW);
		data.vm = mu.get(gmu::VIEW_MODEL);
		data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		data.normal = mu.getNormalMatrix();
		data.normalMapKey = normalMapKey;

		renderer.renderMeshFromAssimp(data);
		mu.popMatrix(gmu::MODEL);
	}
}

// Tudo o que está em cima da estrada/mesa (o que reflete)
void drawObjectsAboveRoad(bool mirrored)
{
	drawCandles();
	env.renderObjects(renderer, mu);
	drawCarModel();
	if (!mirrored)
		drawCarAABB();
	drawSmoke();
	env.renderTranslucent(renderer, mu); // orbs por último
}

// Tudo o que deve ter sombras (para as orbs nao terem)
void drawShadowCasters()
{
	drawCandles();
	env.renderObjects(renderer, mu);
	drawCarModel();
}

// chamar só de dentro de renderSceneWithReflections()
void renderShadows()
{
	const float planeY = env.getRoadSurfaceY();

	// Plano horizontal y = planeY, em coordenadas do mundo: (0,1,0,-planeY)
	GLfloat plane[4] = {0.0f, 1.0f, 0.0f, -planeY};

	// Luz que projeta a sombra, a global (direcional)
	float shadowMat[16];
	mu.shadow_matrix(shadowMat, plane, globalLightPos);

	// Desenha onde o stencil marca mesa ou estrada (>=1) -> stencil já tem mesa=1/estrada=2 marcado
	glStencilFunc(GL_LEQUAL, 0x1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO); // ao desenhar aqui, "consome" a marca (evita enscurecimento duplo)

	glDisable(GL_DEPTH_TEST); // força a sombra a aparecer achatada contra o chão ("ignora" z-buffer)
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO); // escurece multiplicativamente o que já lá estiver

	renderer.forceConstantShading = true;  // texMode = 4 no shader

	mu.pushMatrix(gmu::MODEL);
	mu.multMatrix(gmu::MODEL, shadowMat);
	drawShadowCasters();
	mu.popMatrix(gmu::MODEL);

	renderer.forceConstantShading = false;

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

void renderSceneWithReflections()
{
	const float planeY = env.getRoadSurfaceY(); // Topo da Estrada

	// Stencil: mesa = 1,  estrada = 2
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_NEVER, 0x1, 0xFF);
	env.renderTable(renderer, mu); // Escreve 1 no stencil buffer onde se desenha a mesa
	glStencilFunc(GL_NEVER, 0x2, 0xFF);
	env.renderRoad(renderer, mu); // Escreve 2 no stencil buffer onde se desenha a estrada

	// renderiza reflexos onde o stencil buffer é maior que 1 (geometria da mesa e estrada)
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_LEQUAL, 0x1, 0xFF);
	setSceneLights(true);

	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.0f, planeY, 0.0f);
	mu.scale(gmu::MODEL, 1.0f, -1.0f, 1.0f);
	mu.translate(gmu::MODEL, 0.0f, -planeY, 0.0f);
	glCullFace(GL_FRONT); // Inverte a face culling para que o reflexo seja desenhado
	drawObjectsAboveRoad(true);
	glCullFace(GL_BACK);
	mu.popMatrix(gmu::MODEL);

	// Repõe as luzes na sua posição normal
	setSceneLights(false);

	// Mesa e estrada semi-transparentes (blending) por cima do reflexo
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE); // Desativa a escrita no Depth Buffer para não apagar o reflexo

	glStencilFunc(GL_EQUAL, 0x1, 0xFF);
	env.renderTable(renderer, mu, TABLE_REFLECTION_ALPHA);

	glStencilFunc(GL_EQUAL, 0x2, 0xFF);
	env.renderRoad(renderer, mu, ROAD_REFLECTION_ALPHA);

	glDepthMask(GL_TRUE); // Restaura a escrita no Depth Buffer
	glDisable(GL_BLEND);

	renderShadows(); // SOMBRAS PLANARES (reaproveita as marcas de stencil: mesa=1, estrada=2)

	// Repõe o stencil buffer para 0, para não afetar o resto da cena
	glStencilFunc(GL_ALWAYS, 0x0, 0xFF);
	glDisable(GL_STENCIL_TEST);

	// Desenha os objetos acima da estrada/mesa
	drawObjectsAboveRoad(false);
}


void renderRearMirror()
{
	if (!showRearMirror || !gameStarted || !trackingCamera)
		return;

	int mirrorW = (int)(WinX * MIRROR_WIDTH_RATIO); // 28% da largura da janela
	int mirrorH = (int)(WinY * MIRROR_HEIGHT_RATIO);
	int mirrorX = WinX - mirrorW - MIRROR_MARGIN;
	int mirrorY = WinY - mirrorH - MIRROR_MARGIN;

	const int FRAME_BORDER = 6;

	int m_viewport[4];
	glGetIntegerv(GL_VIEWPORT, m_viewport);

	// --------------------------------------------------------
	// Screen-space setup (moldura + máscara do stencil)

	mu.pushMatrix(gmu::PROJECTION);
	mu.loadIdentity(gmu::PROJECTION);
	mu.ortho(m_viewport[0], m_viewport[0] + m_viewport[2] - 1, m_viewport[1], m_viewport[1] + m_viewport[3] - 1, -1, 1);

	mu.pushMatrix(gmu::VIEW);
	mu.loadIdentity(gmu::VIEW);
	mu.pushMatrix(gmu::MODEL);

	// Moldura: retângulo sólido, um pouco maior, por trás da máscara 
	mu.loadIdentity(gmu::MODEL);
	mu.translate(gmu::MODEL, mirrorX + mirrorW * 0.5f, mirrorY + mirrorH * 0.5f, 0.0f); // Move o centro do cubo para o centro do espelho
	mu.scale(gmu::MODEL, (float)(mirrorW + FRAME_BORDER * 1.5), (float)(mirrorH + FRAME_BORDER * 1.5), 1.0f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	setMeshMaterial(MESH_CUBE, darkMaterial);
	drawMesh(MESH_CUBE);

	glEnable(GL_STENCIL_TEST);
	glClear(GL_STENCIL_BUFFER_BIT);

	// Máscara do stencil: retângulo do tamanho real do espelho
	mu.loadIdentity(gmu::MODEL);
	mu.translate(gmu::MODEL, mirrorX + mirrorW * 0.5f, mirrorY + mirrorH * 0.5f, 0.0f);
	mu.scale(gmu::MODEL, (float)mirrorW, (float)mirrorH, 1.0f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	// Marca o retângulo do espelho no stencil buffer (valor 1) sem desenhar cor nem profundidade
	glStencilFunc(GL_NEVER, 0x1, 0x1);
	glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);
	mu.popMatrix(gmu::VIEW);
	mu.popMatrix(gmu::PROJECTION);

	// --------------------------------------------------------
	// Limpar profundidade e cor só na zona do espelho
	glEnable(GL_SCISSOR_TEST);
	glScissor(mirrorX, mirrorY, mirrorW, mirrorH);
	glDepthMask(GL_TRUE);
	glClearColor(fogColor[0], fogColor[1], fogColor[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);

	// --------------------------------------------------------
	glStencilFunc(GL_EQUAL, 0x1, 0x1);      // agora só passa onde stencil == 1 (dentro do espelho)
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	glViewport(mirrorX, mirrorY, mirrorW, mirrorH); // a cena vai ser desenhada neste retângulo
	// --------------------------------------------------------
	// Câmara traseira
	mu.pushMatrix(gmu::PROJECTION);
	mu.loadIdentity(gmu::PROJECTION);
	float mirrorRatio = (float)mirrorW / (float)mirrorH;
	mu.perspective(MIRROR_FOV, mirrorRatio, 0.1f, 1000.0f);

	mu.pushMatrix(gmu::VIEW);
	mu.loadIdentity(gmu::VIEW);

	float mirrorEyeY = carBaseY + 1.6f * CAR_SCALE;

	// Distance behind the car
	float mirrorDistance = 3.0f;

	// Camera position behind the car
	float mirrorCamX = carX - carDirection[0] * mirrorDistance;
	float mirrorCamZ = carZ - carDirection[2] * mirrorDistance;

	// Camera target (looking backwards)
	float mirrorTargetX = mirrorCamX - carDirection[0];
	float mirrorTargetZ = mirrorCamZ - carDirection[2];

	mu.lookAt(
		mirrorCamX, mirrorEyeY, mirrorCamZ,
		mirrorTargetX, mirrorEyeY, mirrorTargetZ,
		0, 1, 0
	);

	mu.loadIdentity(gmu::MODEL);

	// --------------------------------------------------------
	// Luzes em eye coords para esta VIEW
	renderer.activateRenderMeshesShaderProg();

	float mGlobalLightEye[4];
	mu.multMatrixPoint(gmu::VIEW, globalLightPos, mGlobalLightEye);
	renderer.setGlobalLight(globalIllumination, mGlobalLightEye);

	float mHeadlightsWorld[2][4], mConeDirWorld[4], mHeadlightsEye[2][4], mConeDirEye[4];
	updateCarHeadlights(mHeadlightsWorld, mConeDirWorld);
	for (int i = 0; i < 2; i++)
		mu.multMatrixPoint(gmu::VIEW, mHeadlightsWorld[i], mHeadlightsEye[i]);
	mu.multMatrixPoint(gmu::VIEW, mConeDirWorld, mConeDirEye);
	renderer.setSpotLights(carHeadlights, mHeadlightsEye, mConeDirEye, spotCosCutOff);

	float mCandlesEye[6][4];
	for (int i = 0; i < 6; i++)
		mu.multMatrixPoint(gmu::VIEW, candles[i], mCandlesEye[i]);
	renderer.setCandlesLights(candlesIllumination, mCandlesEye);

	renderer.setFog(fogEnabled, fogColor);

	renderSceneWithReflections();

	// --------------------------------------------------------
	// Repor o estado anterior
	mu.popMatrix(gmu::VIEW);
	mu.popMatrix(gmu::PROJECTION);

	glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
	glStencilFunc(GL_ALWAYS, 0x0, 0x1);
	glDisable(GL_STENCIL_TEST);
}



// ------------------------------------------------------------
// AUDIO FUNCTIONS

void initAudio()
{
	if (ma_engine_init(NULL, &audioEngine) != MA_SUCCESS)
	{
		printf("Failed to initialize audio\n");
		return;
	}

	audioInitialized = true;
	printf("Audio initialized successfully\n");

	if (ma_sound_init_from_file(
		&audioEngine,
		"assets/sounds/car_driving.wav",
		0, NULL, NULL,
		&carDrivingSound
	) == MA_SUCCESS)
	{
		ma_sound_set_looping(&carDrivingSound, MA_TRUE);
		drivingSoundInitialized = true;
		printf("Driving sound loaded\n");
	}
	else
	{
		printf("Failed to load driving sound\n");
	}
}


void playCarStartSound()
{
	if (audioInitialized)
	{
		ma_engine_play_sound(
			&audioEngine,
			"assets/sounds/car_start_eng.wav",
			NULL
		);
	}
}

void playCarStopSound()
{
	if (audioInitialized)
	{
		ma_engine_play_sound(
			&audioEngine,
			"assets/sounds/car_stop.wav",
			NULL
		);
	}
}


void updateCarAudio()
{
	if (!drivingSoundInitialized)
		return;

	bool engineOn = (arduinoSerial == INVALID_HANDLE_VALUE || arduinoIgnition);

	bool isDriving =
		engineOn &&
		gameStarted &&
		!paused &&
		!gameOver;

	if (isDriving && !drivingSoundPlaying)
	{
		ma_sound_start(&carDrivingSound);
		drivingSoundPlaying = true;
	}
	else if (!isDriving && drivingSoundPlaying)
	{
		ma_sound_stop(&carDrivingSound);
		ma_sound_seek_to_pcm_frame(&carDrivingSound, 0);
		drivingSoundPlaying = false;
	}
}


void cleanupAudio()
{
	if (drivingSoundInitialized)
	{
		ma_sound_uninit(&carDrivingSound);
		drivingSoundInitialized = false;
	}

	if (audioInitialized)
	{
		ma_engine_uninit(&audioEngine);
		audioInitialized = false;
	}
}



void handleGestures(); // defined in Events from the Keyboard

// Render the simulation scene
void renderSim(void)
{
	FrameCount++;
	handleGestures();

	// CAR UPDATE / DELTA TIME
	int currentTime = glutGet(GLUT_ELAPSED_TIME);

	float dt = 0.0f;
	if (lastUpdateTime != 0)
	{
		dt = (currentTime - lastUpdateTime) / 1000.0f;
	}

	lastUpdateTime = currentTime;

	// Avoid huge jumps if the program freezes momentarily
	if (dt > 0.05f)
		dt = 0.05f;

	// Read physical controller
	updateArduinoControls();

	// Control lighting based on the Arduino light sensor
	if (arduinoSerial != INVALID_HANDLE_VALUE)
	{
		if (arduinoLight < LIGHT_THRESHOLD)
		{
			// Darkness: turn off global illumination and turn on headlights
			globalIllumination = false;
			carHeadlights = true;
		}
		else
		{
			// Daylight: turn on global illumination and turn off headlights
			globalIllumination = true;
			carHeadlights = false;
		}
	}

	// Detect ignition transition from OFF to ON
	if (arduinoSerial != INVALID_HANDLE_VALUE)
	{
		if (arduinoIgnition && !previousArduinoIgnition)
		{
			playCarStartSound();

			if (!gameStarted)
			{
				// Start the game
				gameStarted = true;
			}
			else if (gameOver)
			{
				// Restart the game
				lives = MAX_LIVES;
				points = 0;
				lastScoreChange = 0;
				scoreChangeTimer = 0.0f;

				env.gameOver();
				gameOver = false;

				resetCarToStart();
				env.resetPointOrbs();
			}
		}

		// ON -> OFF
		if (!arduinoIgnition && previousArduinoIgnition)
		{
			playCarStopSound();
		}

		previousArduinoIgnition = arduinoIgnition;
	}

	// update the environment and car
	if (!paused && !gameOver && gameStarted)
	{
		env.update(dt);
		updateCar(dt);
		updateBoostAndSmoke(dt); 
		if (scoreChangeTimer > 0.0f)
			scoreChangeTimer -= dt;
	}

	// Update audio
	updateCarAudio();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	renderer.activateRenderMeshesShaderProg(); // use the required GLSL program to draw the meshes with illumination

	// load identity matrices
	mu.loadIdentity(gmu::VIEW);
	mu.loadIdentity(gmu::MODEL);
	// set the camera using a function similar to gluLookAt
	if (trackingCamera)
	{
		// Camera tracking the car
		setCameraBehind();
	}
	else if (fixedCamera)
	{
		// Top camera. 
		mu.lookAt(camX, camY, camZ, 0, 0, 0, 0, 0, -1); // (0,0,-1)->up 
	}
	else
	{
		// Normal camera
		mu.lookAt(camX, camY, camZ, 0, 0, 0, 0, 1, 0);
	}

	// ----------------------------
	// Lights + Fog + Scene
	// ----------------------------
	setSceneLights(false);
	renderer.setFog(fogEnabled, fogColor);

	if (camY > env.getRoadSurfaceY())
	{
		renderSceneWithReflections();
	}
	else
	{
		env.renderTable(renderer, mu, 1.0f);
		env.renderRoad(renderer, mu, 1.0f);
		drawObjectsAboveRoad(false);
	}
	renderRearMirror();

	if (fontLoaded)
	{
		glDisable(GL_DEPTH_TEST);
		// the glyph contains transparent background colors and non-transparent for the actual character pixels. So we use the blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		int m_viewport[4];
		glGetIntegerv(GL_VIEWPORT, m_viewport);

		// viewer at origin looking down at  negative z direction
		mu.loadIdentity(gmu::MODEL);
		mu.loadIdentity(gmu::VIEW);
		mu.pushMatrix(gmu::PROJECTION);
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(m_viewport[0], m_viewport[0] + m_viewport[2] - 1, m_viewport[1], m_viewport[1] + m_viewport[3] - 1, -1, 1);
		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		float hudPvm[16];
		memcpy(hudPvm, mu.get(gmu::PROJ_VIEW_MODEL), 16 * sizeof(float));

		// Ícone do motor (por baixo dos pontos)
		renderer.activateRenderMeshesShaderProg();
		renderer.setFog(false, fogColor);          // sem nevoeiro no HUD
		renderer.setGlobalLight(true, globalLightPos);  // luz suficiente para não escurecer o ícone
		float iconSize = 100.0f;
		float iconX = WinX * 0.5f - iconSize * 0.5f;
		float iconY = WinY - 20.0f - iconSize;   // 20 px de margem ao topo

		drawEngineIcon(iconX, iconY, iconSize);
		

		// Vidas (canto superior esquerdo)
		std::ostringstream livesStream;
		livesStream << "Lives: " << lives;

		TextCommand livesCmd = {livesStream.str(), {20.0f, WinY - 40.0f}, 0.4f};
		livesCmd.pvm = hudPvm;
		renderer.renderText(livesCmd);

		// Pontos (logo abaixo das vidas)
		std::ostringstream pointsStream;
		pointsStream << "Score: " << points;
		if (scoreChangeTimer > 0.0f)
		{
			pointsStream << "  (" << (lastScoreChange > 0 ? "+" : "") << lastScoreChange << ")";
		}
		TextCommand pointsCmd = {pointsStream.str(), {20.0f, WinY - 70.0f}, 0.4f};
		pointsCmd.pvm = hudPvm;
		renderer.renderText(pointsCmd);

		// Mensagens centrais: Game Over e Paused
		if (!gameStarted)
		{
			TextCommand menuCmd = {std::string("AVTM Racing 2026!\n Start the engine."),
								   {WinX / 2.0f - 150.0f, WinY / 2.0f},
								   0.6f};
			menuCmd.pvm = hudPvm;
			renderer.renderText(menuCmd);
		}
		else if (gameOver)
		{
			std::ostringstream gameOverStream;
			gameOverStream << "GAME OVER\nScore: " << points << "\n Restart the engine.";
			TextCommand gameOverCmd = {gameOverStream.str(), {WinX / 2.0f - 150.0f, WinY / 2.0f}, 0.6f};

			gameOverCmd.pvm = hudPvm;
			renderer.renderText(gameOverCmd);
		}
		else if (paused)
		{
			TextCommand pausedCmd = {std::string("PAUSED"),
									 {WinX / 2.0f - 150.0f, WinY / 2.0f},
									 0.5f};
			pausedCmd.pvm = hudPvm;
			renderer.renderText(pausedCmd);
		}
		mu.popMatrix(gmu::PROJECTION);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
	}

	glutSwapBuffers();
}

// ------------------------------------------------------------
//
// Events from the Keyboard
//

void processKeys(unsigned char key, int xx, int yy)
{
	switch (key)
	{

	case 27:
		glutLeaveMainLoop();
		break;

	case 'w':
	case 'W':
		keyW = true;
		break;

	case 's':
	case 'S':
		keyS = true;
		break;

	case 'a':
	case 'A':
		keyA = true;
		break;

	case 'd':
	case 'D':
		keyD = true;
		break;

	case 'b':
	case 'B':
		env.toggleBoundingBoxes();
		toggleCarAABB();
		break;

	case 'p':
	case 'P':
		paused = !paused;
		printf(paused ? "Game paused\n" : "Game resumed\n");
		break;
	case 'r':
	case 'R':
		if (!gameStarted)
		{
			gameStarted = true;
		}
		else if (gameOver)
		{
			lives = MAX_LIVES;
			points = 0;
			lastScoreChange = 0;
			scoreChangeTimer = 0.0f;
			boostTimer = 0;
			boostCooldown = 0;
			smokeParticles.clear();
			env.gameOver();
			gameOver = false;
			resetCarToStart();
			env.resetPointOrbs();
		}
		break;
	case '1':
		orthoProj = true;
		perspProj = false;
		fixedCamera = true;
		trackingCamera = false;
		setCameraOver();
		updateProjection();
		printf("Fixed Camera - Orthogonal projection [%d]\n", orthoProj);
		break;
	case '2':
		orthoProj = false;
		perspProj = true;
		fixedCamera = true;
		trackingCamera = false;
		setCameraOver();
		updateProjection();
		printf("Fixed Camera - Perspective projection [%d]\n", perspProj);
		break;
	case '3':
		orthoProj = false;
		perspProj = true;
		trackingCamera = true;
		cameraOrientation = 0.0f;
		fixedCamera = false;
		r = FOLLOW_CAMERA_DISTANCE;
		setCameraBehind();
		updateProjection();
		printf("Movable Camera - Perspective projection [%d]\n", perspProj);
		break;
	case 'N':
	case 'n':
		globalIllumination = !globalIllumination;
		printf("Global Illumination [%d]\n", globalIllumination);
		break;
	case 'C':
	case 'c':
		candlesIllumination = !candlesIllumination;
		printf("Candles Illumination [%d]\n", candlesIllumination);
		break;
	case 'H':
	case 'h':
		carHeadlights = !carHeadlights;
		printf("Car Headlights [%d]\n", carHeadlights);
		break;

	case 'F':
	case 'f':
		togglefog();
		printf("Fog [%d]\n", fogEnabled);
		break;

	case 'M':
	case 'm':
		showRearMirror = !showRearMirror;
		printf("Rear View Mirror [%d]\n", showRearMirror);
		break;

	case 'O':
	case 'o':
		objVisible = !objVisible;
		updateCarBounds();
		updateCarAABB();

		printf("OBJ Visibility [%d]\n", objVisible);
		break;
	case ' ':
		startBoost();
		break;
	}
}

void processKeysUp(unsigned char key, int xx, int yy)
{
	switch (key)
	{

	case 'w':
	case 'W':
		keyW = false;
		break;

	case 's':
	case 'S':
		keyS = false;
		break;

	case 'a':
	case 'A':
		keyA = false;
		break;

	case 'd':
	case 'D':
		keyD = false;
		break;
	}
}

void handleGestures()
{
	GestureCmd c;
	while ((c = gestureInput::poll()) != GestureCmd::None)
	{
		switch (c)
		{
		case GestureCmd::CandlesOn:  
			candlesIllumination = true;  
			break;
		case GestureCmd::CandlesOff: 
			candlesIllumination = false;
			break;
		case GestureCmd::Pause:
			paused = true;
			break;
		case GestureCmd::Resume:
			paused = false;
			break;
		default:
			break;
		}
	}
}

// ------------------------------------------------------------
//
// Mouse Events
//

void processMouseButtons(int button, int state, int xx, int yy)
{
	// Block the camera movement by the mouse
	if (fixedCamera)
		return;

	// start tracking the mouse
	if (state == GLUT_DOWN)
	{
		startX = xx;
		startY = yy;
		if (button == GLUT_LEFT_BUTTON)
			tracking = 1;
		else if (button == GLUT_RIGHT_BUTTON)
			tracking = 2;
	}

	// stop tracking the mouse
	else if (state == GLUT_UP)
	{
		if (tracking == 1)
		{
			alpha -= (xx - startX);
			_beta += (yy - startY);
		}
		else if (tracking == 2)
		{
			r += (yy - startY) * 0.01f;
			if (r < 0.1f)
				r = 0.1f;
		}
		tracking = 0;
	}
}

// Track mouse motion while buttons are pressed

void processMouseMotion(int xx, int yy)
{

	// Block the camera movement by the mouse
	if (fixedCamera)
		return;

	int deltaX, deltaY;
	float alphaAux, betaAux;
	float rAux;

	deltaX = -xx + startX;
	deltaY = yy - startY;

	alphaAux = alpha;
	betaAux = _beta;

	// left mouse button: move camera
	if (tracking == 1)
	{

		if (trackingCamera)
		{
			cameraOrientation += deltaX * 0.1f;

			startX = xx;
			startY = yy;

			return;
		}

		alphaAux += deltaX;
		betaAux += deltaY;

		if (betaAux > 85.0f)
			betaAux = 85.0f;
		else if (betaAux < -85.0f)
			betaAux = -85.0f;
		rAux = r;
	}
	// right mouse button: zoom
	else if (tracking == 2)
	{

		alphaAux = alpha;
		betaAux = _beta;
		rAux = r + (deltaY * 0.01f);
		if (rAux < 0.1f)
			rAux = 0.1f;
	}

	camX = rAux * sin(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
	camZ = rAux * cos(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
	camY = rAux * sin(betaAux * 3.14f / 180.0f);

	//  uncomment this if not using an idle or refresh func
	//	glutPostRedisplay();
}

void mouseWheel(int wheel, int direction, int x, int y)
{

	r += direction * 0.1f;
	if (r < 0.1f)
		r = 0.1f;

	camX = r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camY = r * sin(_beta * 3.14f / 180.0f);

	//  uncomment this if not using an idle or refresh func
	//	glutPostRedisplay();
}

//
// Scene building with basic geometry
//

void buildScene()
{
	// Texture Object definition
	//  Texturas do template (0, 1, 2)
	renderer.TexObjArray.texture2D_Loader("assets/stone.tga");	   // 0
	renderer.TexObjArray.texture2D_Loader("assets/checker.png");   // 1
	renderer.TexObjArray.texture2D_Loader("assets/lightwood.tga"); // 2

	// Minhas texturas (3, 4, 5, 6)
	renderer.TexObjArray.texture2D_Loader("assets/table.jpg");		// 3
	renderer.TexObjArray.texture2D_Loader("assets/road.jpg");		// 4
	renderer.TexObjArray.texture2D_Loader("assets/finishline.jpg"); // 5
	renderer.TexObjArray.texture2D_Loader("assets/orange.jpg");		// 6
	renderer.TexObjArray.texture2D_Loader("assets/tire.png");		// 7
	renderer.TexObjArray.texture2D_Loader("assets/license.png");	// 8


	renderer.TexObjArray.texture2D_Loader("assets/engineoff.png");  // 9
	renderer.TexObjArray.texture2D_Loader("assets/engineon.png");   // 10

	// Scene geometry with triangle meshes

	MyMesh amesh;

	float amb[] = {0.2f, 0.15f, 0.1f, 1.0f};
	float diff[] = {0.8f, 0.6f, 0.4f, 1.0f};
	float spec[] = {0.8f, 0.8f, 0.8f, 1.0f};

	float amb1[] = {0.3f, 0.0f, 0.0f, 1.0f};
	float diff1[] = {0.8f, 0.1f, 0.1f, 1.0f};
	float spec1[] = {0.3f, 0.3f, 0.3f, 1.0f};

	float emissive[] = {0.0f, 0.0f, 0.0f, 1.0f};
	float shininess = 100.0f;
	int texcount = 0;

	// create geometry and VAO of the cube
	amesh = createCube();
	memcpy(amesh.mat.ambient, amb1, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff1, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec1, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the pawn
	amesh = createPawn();
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the sphere
	amesh = createSphere(1.0f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cylinder
	amesh = createCylinder(1.5f, 0.5f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cone
	amesh = createCone(2.5f, 1.2f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the torus
	amesh = createTorus(0.5f, 1.5f, 20, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// initialize the environment and position the car at the finish line
	env.init(renderer);

	// Definir a posição inicial do carro na meta
	CAR_START_X = carX = env.getFinishLinePosx() + 3.0f;
	CAR_START_Z = carZ = env.getFinishLinePosz();
	updateCarAABB();

	// Import .obj after all the original meshes
	std::string filepath = "assets/models/minicooper.obj";
	// import 3D file into Assimp scene graph
	if (!Import3DFromFile(filepath, importer, scene, scaleFactor))
		exit(0);
	// creation of Mymesh array with VAO Geometry and Material and array of Texture Objs for the model input by the user
	createMyMeshFromAssimp(scene, &renderer);

	// The truetypeInit creates a texture object in TexObjArray for storing the fontAtlasTexture

	fontLoaded = renderer.truetypeInit(fontPathFile);
	if (!fontLoaded)
		cerr << "Fonts not loaded\n";
	else
		cerr << "Fonts loaded\n";

	printf("\nNumber of Texture Objects is %d\n\n", renderer.TexObjArray.getNumTextureObjects());

	// set the camera position based on its spherical coordinates
	camX = r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camY = r * sin(_beta * 3.14f / 180.0f);
}

// ------------------------------------------------------------
//
// Main function
//

int main(int argc, char **argv)
{

	//  GLUT initialization
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_MULTISAMPLE);

	glutInitContextVersion(4, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);

	glutInitWindowPosition(100, 100);
	glutInitWindowSize(WinX, WinY);
	WindowHandle = glutCreateWindow(CAPTION);

	initArduinoSerial("\\\\.\\COM4");

	//  Callback Registration
	glutDisplayFunc(renderSim);
	glutReshapeFunc(changeSize);

	glutTimerFunc(0, timer, 0);
	// glutIdleFunc(renderSim);  // Use it for maximum performance
	glutTimerFunc(0, refresh, 0); // use it to to get 60 FPS whatever

	//	Mouse and Keyboard Callbacks
	glutKeyboardFunc(processKeys);
	glutKeyboardUpFunc(processKeysUp);
	glutMouseFunc(processMouseButtons);
	glutMotionFunc(processMouseMotion);
	glutMouseWheelFunc(mouseWheel);

	//	return from main loop
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	//	Init GLEW
	glewExperimental = GL_TRUE;
	glewInit();

	// some GL settings
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glClearColor(0.5f, 0.6f, 0.7f, 1.0f);
	glClearStencil(0x0);

	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n", glGetString(GL_VERSION));
	printf("GLSL: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	/* Initialization of DevIL */
	if (ilGetInteger(IL_VERSION_NUM) < IL_VERSION)
	{
		printf("wrong DevIL version \n");
		exit(0);
	}
	ilInit();
	gestureInput::init(); // initialize gesture input system
	buildScene();

	initAudio();

	if (!renderer.setRenderMeshesShaderProg("shaders/mesh.vert", "shaders/mesh.frag") ||
		!renderer.setRenderTextShaderProg("shaders/ttf.vert", "shaders/ttf.frag"))
		return (1);

	//  GLUT main loop
	glutMainLoop();
	gestureInput::shutdown(); // shutdown gesture input system

	cleanupAudio();

	return (0);
}
