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

// ============== Novo ==============
#include "Environment.h"
// ==================================

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
bool orthoProj = true;
bool perspProj = false;

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
	if (h == 0)
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
//
// Render stufff
//

void renderSim(void)
{

	FrameCount++;

	// ============== Novo ==============

	// update the environment
	env.update(FPS / 1000.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer.activateRenderMeshesShaderProg(); // use the required GLSL program to draw the meshes with illumination

	// Associar os Texture Units aos Objects Texture
	// stone.tga loaded in TU0; checker.tga loaded in TU1;  lightwood.tga loaded in TU2
	// renderer.setTexUnit(0, 0);
	// renderer.setTexUnit(1, 1);
	// renderer.setTexUnit(2, 2);

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

	dataMesh data;

	// ============== REMOVIDO (comentado) ==============
	// Chão original do template - substituído pela mesa do Environment.
	/*
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.0f, -1.15f, 0.0f);
	mu.scale(gmu::MODEL, 30.0f, 0.1f, 30.0f);
	mu.translate(gmu::MODEL, -0.5f, -0.5f, -0.5f); //centrar o cubo na origem

	mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
	mu.computeNormalMatrix3x3();

	data.meshID = 0;
	data.texMode = 1; //modulate diffuse color with texel color
	data.vm = mu.get(gmu::VIEW_MODEL),
	data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
	data.normal = mu.getNormalMatrix();
	renderer.renderMesh(data);
	mu.popMatrix(gmu::MODEL);
	*/
	// =====================================================

	// Desenha mesa / estrada / linha de meta / cheerios / manteigas / laranjas
	env.render(renderer, mu);

	// O Environment troca o bind das TU0/TU2 para as suas próprias texturas
	// (mesa, estrada, linha de meta, laranjas); como qualquer objeto de
	// demonstração desenhado a seguir também usa TU0/1/2, é preciso repor
	// os bindings originais do template antes de os desenhar.
	renderer.setTexUnit(0, 0);
	renderer.setTexUnit(1, 1);
	renderer.setTexUnit(2, 2);
	// =========================================

	// ============== REMOVIDO (comentado) ==============
	// Draw the other objects (OBJETOS DE DEMONSTRAÇÃO DO TEMPLATE)
	/*
	int objId = 0; //id of the current object mesh - to be used as index of the array Mymeshes in the renderer object
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			mu.pushMatrix(gmu::MODEL);
			mu.translate(gmu::MODEL, (float)i * 3.7f, 0.0f, (float)j * 3.7f);

			mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
			mu.computeNormalMatrix3x3();

			data.meshID = objId;
			data.texMode = i;   //0:no texturing; 1:modulate diffuse color with texel color; 2:diffuse color is replaced by texel color; 3: multitexturing
			data.vm = mu.get(gmu::VIEW_MODEL),
				data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
			data.normal = mu.getNormalMatrix();
			renderer.renderMesh(data);

			mu.popMatrix(gmu::MODEL);
			objId = (objId + 1) % 6;
		}
	}
	*/
	// =====================================================

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
		setCameraBehind();
		updateProjection();
		printf("Movable Camera - Perspective projection [%d]\n", perspProj);
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
	renderer.TexObjArray.texture2D_Loader("assets/table.jpg");		// 3
	renderer.TexObjArray.texture2D_Loader("assets/road.jpg");		// 4
	renderer.TexObjArray.texture2D_Loader("assets/finishline.jpg"); // 5
	renderer.TexObjArray.texture2D_Loader("assets/orange.jpg");		// 6

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

	// create geometry and VAO of the cube  -> fica em renderer.myMeshes[0]
	amesh = createCube();
	memcpy(amesh.mat.ambient, amb1, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff1, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec1, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the pawn  -> fica em renderer.myMeshes[1]
	amesh = createPawn();
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the sphere  -> fica em renderer.myMeshes[2]
	amesh = createSphere(1.0f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cylinder  -> fica em renderer.myMeshes[3]
	amesh = createCylinder(1.5f, 0.5f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cone  -> fica em renderer.myMeshes[4]
	amesh = createCone(2.5f, 1.2f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the torus  -> fica em renderer.myMeshes[5]
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
	glutIdleFunc(renderSim); // Use it for maximum performance
	// glutTimerFunc(0, refresh, 0);    //use it to to get 60 FPS whatever

	//	Mouse and Keyboard Callbacks
	glutKeyboardFunc(processKeys);
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
