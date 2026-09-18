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

// include GLEW to access OpenGL 3.3 functions
#include <GL/glew.h>

// GLUT is the toolkit to interface with the OS
#include <GL/freeglut.h>

#include <IL/il.h>

#include "renderer.h"
#include "shader.h"
#include "mathUtility.h"
#include "model.h"
#include "texture.h"
#include "Environment.h"

using namespace std;

#define CAPTION "AVTM 2026 Welcome Demo"
int WindowHandle = 0;
int WinX = 640, WinY = 480;

unsigned int FrameCount = 0;

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
float r = 45.0f;

// Projections Booleans
bool orthoProj = false;
bool perspProj = true;

// Auxiliary variables
bool fixedCamera = false;
bool trackingCamera = false;
float cameraOrientation = 0.0f;

// Car Position and Rotation
float carPosX, carPosY, carPosZ;
float carRotX, carRotY, carRotZ;

// Mouse Tracking Variables
int startX, startY, tracking = 0;

// Frame counting and FPS computation
#define FPS 60
long myTime, timebase = 0, frame = 0;
char s[32];

// float lightPos[4] = {4.0f, 5.0f, 2.0f, 1.0f};
float lightPos[4] = {0.0f, 0.0f, 0.0f, 1.0f};

// Spotlight
bool spotlight_mode = false;
float coneDir[4] = {0.0f, -0.0f, -1.0f, 0.0f};

bool fontLoaded = false;

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
	if(h == 0)
		h = 1;
	// set the viewport to be the entire window
	glViewport(0, 0, w, h);
	// set the projection matrix
	ratio = (1.0f * w) / h;
	if (perspProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.perspective(53.13f, ratio, 0.1f, 1000.0f);
	}
	else if (orthoProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(-30.0f * ratio, 30.0f * ratio, -30.0f, 30.0f, -100.0f, 100.0f);
	}
}

// ------------------------------------------------------------
//
// Update Projection Function
// made by Gonçalo
//

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
		mu.perspective(53.13f, ratio, 0.1f, 1000.0f);
	}
	else if (orthoProj)
	{
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(-30.0f * ratio, 30.0f * ratio, -30.0f, 30.0f, -100.0f, 100.0f);
	}
	glutPostRedisplay(); // viewport refresh
}

// ------------------------------------------------------------
//
// Set Camera Over Function
// made by Gonçalo
//

void static setCameraOver()
{
	alpha = 0.0f;
	_beta = 90.0f; // Over Camera Spherical Coordinates
	r = 45.0f;
	camX = r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camY = r * sin(_beta * 3.14f / 180.0f);
}

// ------------------------------------------------------------
//
// Set Camera Behind the car Function
// made by Gonçalo
//

void static setCameraBehind()
{
	alpha = 270.0f + carRotY + cameraOrientation;
	_beta = 20.0f; // Behind Camera Spherical Coordinates - alpha horizontal rotation - beta vertical rotation
	r = 45.0f;
	camX = carPosX + r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camZ = carPosZ + r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
	camY = carPosY + r * sin(_beta * 3.14f / 180.0f);

	mu.lookAt(camX, camY, camZ, carPosX, carPosY, carPosZ, 0, 1, 0); // the sum of the car postion makes the camera follow it
}



// ------------------------------------------------------------
// Mesh IDs inside renderer.myMeshes
#define MESH_CUBE      0
#define MESH_PAWN      1
#define MESH_SPHERE    2
#define MESH_CYLINDER  3
#define MESH_CONE      4
#define MESH_TORUS     5

// ------------------------------------------------------------
// Car state
// For now the car does not move

float carX = 0.0f;
float carZ = 0.0f;
float carAngle = 0.0f;

// Top of the floor in the current scene
float carBaseY = env.getRoadSurfaceY();

// ------------------------------------------------------------
// Car keyboard state

bool keyW = false;
bool keyS = false;
bool keyA = false;
bool keyD = false;

// ------------------------------------------------------------
// Car movement

float carVelocity = 0.0f;
float carAcceleration = 0.0f;

// Direction of the car.
// For now the car points towards +Z.
float carDirection[3] = { 0.0f, 0.0f, 1.0f };

// Movement values
const float ACCELERATION = 3.0f;
const float DECELERATION = 2.0f;

const float TURN_SPEED = 75.0f;   // degrees per second

const float MAX_FORWARD_SPEED = 15.0f;
const float MAX_BACKWARD_SPEED = -10.0f;

// Used to calculate the time between frames
int lastUpdateTime = 0;


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
	{ 0.02f, 0.05f, 0.15f, 1.0f },
	{ 0.02f, 0.30f, 0.80f, 1.0f },
	{ 0.60f, 0.70f, 0.90f, 1.0f },
	70.0f
};


// Wheels - matte black
CarMaterial wheelMaterial = {
	{ 0.05f, 0.05f, 0.05f, 1.0f },   
	{ 0.12f, 0.12f, 0.13f, 1.0f },  
	{ 0.10f, 0.10f, 0.10f, 1.0f },   
	8.0f
};


// Headlights - white/blue
CarMaterial headlightMaterial = {
	{ 0.12f, 0.14f, 0.18f, 1.0f },
	{ 0.75f, 0.85f, 1.00f, 1.0f },
	{ 1.00f, 1.00f, 1.00f, 1.0f },
	100.0f
};

// RearLights - red
CarMaterial rearLightMaterial = {
	{ 0.20f, 0.00f, 0.00f, 1.0f },
	{ 0.90f, 0.05f, 0.05f, 1.0f },
	{ 0.80f, 0.30f, 0.30f, 1.0f },
	50.0f
};


// Exhaust - metallic grey
CarMaterial exhaustMaterial = {
	{ 0.10f, 0.10f, 0.10f, 1.0f },
	{ 0.42f, 0.44f, 0.48f, 1.0f },
	{ 0.85f, 0.85f, 0.88f, 1.0f },
	80.0f
};


// Windshield and windows
CarMaterial darkGlassMaterial = {
	{ 0.02f, 0.04f, 0.06f, 1.0f },
	{ 0.08f, 0.16f, 0.22f, 1.0f },
	{ 0.70f, 0.80f, 0.90f, 1.0f },
	90.0f
};


void setMeshMaterial(int meshID, const CarMaterial& material)
{
	memcpy(
		renderer.myMeshes[meshID].mat.ambient,
		material.ambient,
		4 * sizeof(float)
	);

	memcpy(
		renderer.myMeshes[meshID].mat.diffuse,
		material.diffuse,
		4 * sizeof(float)
	);

	memcpy(
		renderer.myMeshes[meshID].mat.specular,
		material.specular,
		4 * sizeof(float)
	);

	renderer.myMeshes[meshID].mat.shininess =
		material.shininess;
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


void updateCar(float dt)
{
	// ------------------------------------------------------------
	// ACCELERATION

	if (keyW && !keyS)
	{
		// Accelerate forward
		carAcceleration = ACCELERATION;
	}
	else if (keyS && !keyW)
	{
		// Accelerate backwards
		carAcceleration = -ACCELERATION;
	}
	else
	{
		// Neither W nor S is pressed: gradually reduce the velocity.

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


	// When no acceleration key is pressed, stop at exactly zero instead of crossing it.
	if (!keyW && !keyS)
	{
		if ((oldVelocity > 0.0f && carVelocity < 0.0f) ||
			(oldVelocity < 0.0f && carVelocity > 0.0f))
		{
			carVelocity = 0.0f;
			carAcceleration = 0.0f;
		}
	}


	// ------------------------------------------------------------
	// LIMIT MAXIMUM SPEED

	if (carVelocity > MAX_FORWARD_SPEED)
		carVelocity = MAX_FORWARD_SPEED;

	if (carVelocity < MAX_BACKWARD_SPEED)
		carVelocity = MAX_BACKWARD_SPEED;

	// ------------------------------------------------------------
	// STEERING

	// Only turn if the car is moving
	if (fabs(carVelocity) > 0.01f)
	{
		// When reversing, steering direction is reversed
		float directionSign = 1.0f;

		if (carVelocity < 0.0f)
			directionSign = -1.0f;


		if (keyA)
			carAngle += TURN_SPEED * directionSign * dt;

		if (keyD)
			carAngle -= TURN_SPEED * directionSign * dt;
	}

	// ------------------------------------------------------------
	// CAR DIRECTION

	float angleRad = carAngle * 3.14159265f / 180.0f;

	carDirection[0] = sin(angleRad);
	carDirection[1] = 0.0f;
	carDirection[2] = cos(angleRad);

	
	// ------------------------------------------------------------
	// UPDATE POSITION

	carX += carDirection[0] * carVelocity * dt;
	carZ += carDirection[2] * carVelocity * dt;
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
	setMeshMaterial(MESH_TORUS, wheelMaterial);
	drawMesh(MESH_TORUS, 2);

	mu.popMatrix(gmu::MODEL);

	// Rim
	mu.pushMatrix(gmu::MODEL);

	mu.scale(gmu::MODEL, 0.4f, 0.12f, 0.4f);
	setMeshMaterial(MESH_CYLINDER, wheelMaterial);
	drawMesh(MESH_CYLINDER);

	mu.popMatrix(gmu::MODEL);

	mu.popMatrix(gmu::MODEL);
}

void drawSideWindow(float x, float z)
{
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, x, 1.5f, z);
	mu.scale(gmu::MODEL, 0.02f, 0.4f, 0.9f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, darkGlassMaterial);
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

	setMeshMaterial(MESH_CUBE, darkGlassMaterial);
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

	mu.pushMatrix(gmu::MODEL);

	// Slightly towards the rear of the car
	mu.translate(gmu::MODEL, 0.0f, 1.5f, -0.4f);
	mu.scale(gmu::MODEL, 2.4f, 0.7f, 2.4f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, bodyMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

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
	// FRONT WINDSHIELD

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.5f, 0.81f);
	mu.scale(gmu::MODEL, 1.9f, 0.5f, 0.02f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, darkGlassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);


	// ------------------------------------------------------------
	// REAR WINDOW

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 1.5f, -1.61f);
	mu.scale(gmu::MODEL, 1.7f, 0.4f, 0.02f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);

	setMeshMaterial(MESH_CUBE, darkGlassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);

	// ------------------------------------------------------------
	// SIDE WINDOWS

	// right windows
	drawSideWindow(-1.21f, 0.2f);
	drawSideWindow(-1.21f, -0.9f);

	// left windows
	drawSideWindow(1.21f, 0.2f);
	drawSideWindow(1.21f, -0.9f);


	// ------------------------------------------------------------
	// MIRRORS

	drawMirror(-1.0f);  // Right mirror
	drawMirror(1.0f);   // Left mirror


	// ------------------------------------------------------------
	// WHEELS

	drawWheel(-1.65f, 0.5f, 1.9f);   // Front right
	drawWheel(1.65f, 0.5f, 1.9f);    // Front left

	drawWheel(-1.65f, 0.5f, -1.9f);  // Rear right
	drawWheel(1.65f, 0.5f, -1.9f);   // Rear left

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

	setMeshMaterial(MESH_CUBE, darkGlassMaterial);
	drawMesh(MESH_CUBE);

	mu.popMatrix(gmu::MODEL);


	// ------------------------------------------------------------
	// FRONT LICENSE PLATE

	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.0f, 0.75f, 2.70f);
	mu.scale(gmu::MODEL, 0.8f, 0.25f, 0.02f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f);
	renderer.setTexUnit(0, 9);
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
	
	renderer.setTexUnit(2, 8);

	// Left Exhaust
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, 0.90f, 0.50f, -2.9f);
	mu.rotate(gmu::MODEL, 90.0f, 1.0f, 0.0f, 0.0f);  // Rotate it so its axis points towards the rear.
	mu.scale(gmu::MODEL, 0.20f, 0.20f, 0.20f);

	setMeshMaterial(MESH_CYLINDER, exhaustMaterial);
	drawMesh(MESH_CYLINDER, 1);

	mu.popMatrix(gmu::MODEL);

	// Right Exhaust
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -0.90f, 0.50f, -2.9f);
	mu.rotate(gmu::MODEL, 90.0f, 1.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 0.20f, 0.20f, 0.20f);

	setMeshMaterial(MESH_CYLINDER, exhaustMaterial);
	drawMesh(MESH_CYLINDER, 1);

	mu.popMatrix(gmu::MODEL);


	// ------------------------------------------------------------
	// REAR LIGHTS
	
	// Right outer light
	mu.pushMatrix(gmu::MODEL);

	mu.translate(gmu::MODEL, -1.3f, 0.9f, -2.7f);
	mu.scale(gmu::MODEL, 0.15f, 0.15f, 0.1f);;
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
	mu.scale(gmu::MODEL, 0.15f, 0.15f, 0.1f);;
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
	renderer.setTexUnit(0, 9);
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


	// ------------------------------------------------------------
	// END OF CAR

	mu.popMatrix(gmu::MODEL);
}

// ------------------------------------------------------------
//
// Render stufff
//

void renderSim(void)
{
	FrameCount++;

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

	// update the environment and car
	env.update(dt);
	updateCar(dt);


	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
	else
		mu.lookAt(camX, camY, camZ, 0, 0, 0, 0, 1, 0);

	// send the light position in eye coordinates
	renderer.setLightPos(lightPos); // efeito capacete do mineiro, ou seja lighPos foi definido em eye coord

	// float lposAux[4];
	// mu.multMatrixPoint(gmu::VIEW, lightPos, lposAux);   //lightPos definido em World Coord so is converted to eye space
	// renderer.setLightPos(lposAux);

	// Spotlight settings
	renderer.setSpotLightMode(spotlight_mode);
	renderer.setSpotParam(coneDir, 0.93);

	// Desenha mesa / estrada / linha de meta / cheerios / manteigas / laranjas
	env.render(renderer, mu);

	// Draw the car
	drawCar();

	// Render text (bitmap fonts) in screen coordinates. So use ortoghonal projection with viewport coordinates.
	// Each glyph quad texture needs just one byte color channel: 0 in background and 1 for the actual character pixels. Use it for alpha blending
	// text to be rendered in last place to be in front of everything

	if (fontLoaded)
	{
		glDisable(GL_DEPTH_TEST);
		TextCommand textCmd = {"AVTM 2026 Welcome:\nGood Luck!", {100, 100}, 0.5};
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
		textCmd.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		renderer.renderText(textCmd);
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

		case 'c': 
			printf("Camera Spherical Coordinates (%f, %f, %f)\n", alpha, _beta, r);
			break;

		case 'l': // toggle spotlight mode
			if (!spotlight_mode)
		{
				spotlight_mode = true;
				printf("Point light disabled. Spot light enabled\n");
			}
			else
		{
				spotlight_mode = false;
				printf("Spot light disabled. Point light enabled\n");
			}
			break;

		case 'r': // reset
			alpha = 57.0f;
			_beta = 18.0f; // Camera Spherical Coordinates
			r = 45.0f;
			camX = r * sin(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
			camZ = r * cos(alpha * 3.14f / 180.0f) * cos(_beta * 3.14f / 180.0f);
			camY = r * sin(_beta * 3.14f / 180.0f);
			break;

	case 'm':
		glEnable(GL_MULTISAMPLE);
		break;
	case 'n':
		glDisable(GL_MULTISAMPLE);
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
		setCameraBehind();
		updateProjection();
		printf("Movable Camera - Perspective projection [%d]\n", perspProj);
		break;
	}
}

void processKeysUp(unsigned char key, int xx, int yy)
{
	switch (key) {

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


// ------------------------------------------------------------
//
// Mouse Events
//

void processMouseButtons(int button, int state, int xx, int yy)
{
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

	// left mouse button: move camera
	if (tracking == 1)
	{

		alphaAux = alpha + deltaX;
		betaAux = _beta + deltaY;
		cameraOrientation += deltaX * 0.1f;

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
	renderer.TexObjArray.texture2D_Loader("assets/table.jpg");       // 3
	renderer.TexObjArray.texture2D_Loader("assets/road.jpg");        // 4
	renderer.TexObjArray.texture2D_Loader("assets/finishline.jpg");  // 5
	renderer.TexObjArray.texture2D_Loader("assets/orange.jpg");      // 6
	renderer.TexObjArray.texture2D_Loader("assets/tire.png");        // 7
	renderer.TexObjArray.texture2D_Loader("assets/exhaust_metal.png"); // 8
	renderer.TexObjArray.texture2D_Loader("assets/license.png");     // 9

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

	// initialize the environment
	env.init(renderer);

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
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA | GLUT_MULTISAMPLE);

	glutInitContextVersion(4, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);

	glutInitWindowPosition(100, 100);
	glutInitWindowSize(WinX, WinY);
	WindowHandle = glutCreateWindow(CAPTION);

	//  Callback Registration
	glutDisplayFunc(renderSim);
	glutReshapeFunc(changeSize);

	glutTimerFunc(0, timer, 0);
	//glutIdleFunc(renderSim);  // Use it for maximum performance
	glutTimerFunc(0, refresh, 0);    //use it to to get 60 FPS whatever

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
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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

	buildScene();

	if (!renderer.setRenderMeshesShaderProg("shaders/mesh.vert", "shaders/mesh.frag") ||
		!renderer.setRenderTextShaderProg("shaders/ttf.vert", "shaders/ttf.frag"))
		return (1);

	//  GLUT main loop
	glutMainLoop();

	return (0);
}
