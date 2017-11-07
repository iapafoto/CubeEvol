/*

https://stackoverflow.com/questions/26005744/how-to-display-pixels-on-screen-directly-from-a-raw-array-of-rgb-values-faster-t

Line strip
https://stackoverflow.com/questions/13184033/opengl-orthographic-sketching-with-gl-line-strip
https://en.wikibooks.org/wiki/OpenGL_Programming/Scientific_OpenGL_Tutorial_01

 */


#include <omp.h>  
#include "platform.hpp"
#include "include/Voxelyze.h"
#include "MeshRender.h"
#include "Creature.h"
#include <windows.h>

// third-party libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// standard C++ libraries
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <unordered_map>

// tdogl classes
#include "tdogl/Program.h"
//#include "tdogl/Texture.h"
#include "tdogl/Camera.h"


#define WITH_DISPLAY 
//#define WITH_SIMULATION
//#define DISPLAY_ANIM

int NB_CREATURE = 100;
int NB_SAVED = 6;
int NB_GENERATION = 1;
double SIMULATION_DURATION = 15.;

int creatureID = 73; // 98;
int WIDTH = 800;
int HEIGHT = 450;

char numstr[4]; 
char name[20];
int iGeneration = 0;

std::vector<Creature*> animCreature;

std::vector<std::string> get_all_files_names_within_folder(std::string folder)
{
	std::vector<std::string> names;
	std::string search_path = folder + "\\*.*";
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				names.push_back(fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return names;
}


// constants
const glm::vec2 SCREEN_SIZE(WIDTH, HEIGHT);

// globals
GLFWwindow* gWindow = NULL;
double gScrollY = 0.0;
//tdogl::Texture* gTexture = NULL;
tdogl::Program* gProgram = NULL;
tdogl::Program* gProgram2D = NULL;
tdogl::Camera gCamera;

GLfloat gDegreesRotated = 0.0f;
//CVoxelyze voxelEngine(.005);// .005  // 5 mm
Creature* pCreatureDisplay = NULL;

std::unordered_map<std::string, Creature*> mapCreature;


// loads the vertex shader and fragment shader, and links them to make the global gProgram
static void LoadShaders() {
    std::vector<tdogl::Shader> shaders;
    shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("vertex-shader.txt"), GL_VERTEX_SHADER));
    shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("fragment-shader.txt"), GL_FRAGMENT_SHADER));
    gProgram = new tdogl::Program(shaders);
}

static void LoadShaders2D() {
	std::vector<tdogl::Shader> shaders;
	shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("vertex-shader-2d.txt"), GL_VERTEX_SHADER));
	shaders.push_back(tdogl::Shader::shaderFromFile(ResourcePath("fragment-shader-2d.txt"), GL_FRAGMENT_SHADER));
	gProgram2D = new tdogl::Program(shaders);
}


////////////////////////////////////////////////////////////////////////////////

GLuint gVAO = 0;
GLuint gVBO = 0;

void addPoint(std::vector<float>* array, float x, float y, float u, float v) {
	array->push_back(x);
	array->push_back(y);
	array->push_back(0);
	array->push_back(u);
	array->push_back(v);
}


// loads a triangle into the VAO global
static void LoadTriangle() {
	// make and bind the VAO and VBO
	glGenVertexArrays(1, &gVAO);
	glGenBuffers(1, &gVBO);

	glBindVertexArray(gVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gVBO);

	std::vector<float> data;
		// Put the three triangle verticies into the VBO
			//  X     Y     Z
	addPoint(&data, -1, -1,  0, 0);
	addPoint(&data, -1, 1, 0, 1); 
	addPoint(&data, 1, 1,  1, 1);

	addPoint(&data, -1, -1, 0, 0);
	addPoint(&data, 1, 1,  1, 1);
	addPoint(&data, 1,  -1,  1, 0);

	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);

	// connect the xyz to the "vert" attribute of the vertex shader
	glEnableVertexAttribArray(gProgram2D->attrib("vert"));
	glVertexAttribPointer(gProgram2D->attrib("vert"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), NULL);

	// connect the uv coords to the "vertTexCoord" attribute of the vertex shader
	glEnableVertexAttribArray(gProgram2D->attrib("vertTexCoord"));
	glVertexAttribPointer(gProgram2D->attrib("vertTexCoord"), 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

	// unbind the VBO and VAO
//	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}


// update the scene based on the time elapsed since last update
void Update(float secondsElapsed) {

	if (glfwGetKey(gWindow, 'A') || glfwGetKey(gWindow, 'a')) {
		pCreatureDisplay->resetTime();// .offsetPosition(secondsElapsed * moveSpeed * -gCamera.right());
	}
	if (glfwGetKey(gWindow, 'S') || glfwGetKey(gWindow, 's')) {
		pCreatureDisplay->saveJSON("C:\\Users\\durands\\Desktop\\creature.json");// .offsetPosition(secondsElapsed * moveSpeed * -gCamera.right());
	}
}

// records how far the y axis has been scrolled
void OnScroll(GLFWwindow* window, double deltaX, double deltaY) {
    gScrollY += deltaY;
}

void OnError(int errorCode, const char* msg) {
    throw std::runtime_error(msg);
}


std::string createName(int gen, int id) {
	sprintf_s(name, (NB_CREATURE <= 100)?"%d-%02d": "%d-%03d", gen, id);
	std::string sname;
	sname.append(name);
	return sname;
}


void createMutation(Creature* creature, int nbCreature, std::vector<Creature*>* nextGeneration) {

	for (int i = 0; i < nbCreature; i++) {

		// On determine le nombre de mutation a effectuer
		int nbMutation = creature->voxelEngine->voxelCount() / 10;
		if (nbMutation < 2) nbMutation = 2;
//		nbMutation += (rand() % 30 > 28 ? 1 : 0) + (rand() % 100 > 98 ? 1 : 0);

		// On applique les mutation
		Creature* newCreature = creature->mutate(nbMutation);

		// On test si pas deja la meme creature dans la liste pour eviter de calculer pour rien
		bool isSameInList = false;
		for (int j = 0; j < nextGeneration->size(); j++) {
			if (newCreature->isSame((*nextGeneration)[j])) {
				isSameInList = true;
				break;
			}
		}
		// Si pas deja presente, on ajoute cette creature là
		if (!isSameInList) {
			nextGeneration->push_back(newCreature);
		}
		else {
			// la creature va etre oublie, on libere l'espace memoire associe
			delete newCreature;
		}
		//newCreature->setName(createName(iGeneration, nextGeneration->size()));
	}
}


bool simulateAll(std::vector<Creature*>* nextGeneration, double simulationTime) {

	bool exit = false;
	std::vector<Creature*> toEvaluate;
	Creature* creature, oldCreature;
	// on remplis les score deja connus
	for (int i = 0; i < nextGeneration->size(); i++) {
		creature = (*nextGeneration)[i];
		if (creature->getScore() < 0) {
			std::string key = creature->getKey();
			Creature* oldCreature = mapCreature[key];
			if (oldCreature != NULL) {
				creature->setScore(oldCreature->getScore());
			} 
		}
		if (creature->getScore() < 0) {
			toEvaluate.push_back(creature);
		}
	}

	std::cout << toEvaluate.size() << "/" << nextGeneration->size() << " New creatures to evaluate\n";

	if (toEvaluate.size() > 0) {
	//	omp_set_num_threads(10);
#pragma omp parallel
		{
#pragma omp for
			for (int i = 0; i < toEvaluate.size(); i++) {
				toEvaluate[i]->simulate(i, simulationTime);
				if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
					exit = true;
				}
			}
		}

		for (int i = 0; i < toEvaluate.size(); i++) {
			mapCreature[toEvaluate[i]->getKey()] = toEvaluate[i];
		}
	}

	std::cout << "\n\n";

	std::sort(nextGeneration->begin(), nextGeneration->end(), Creature::sorter);

	float score;
	Creature *parent;

	for (int i = 0; i < nextGeneration->size(); i++) {
		creature = (*nextGeneration)[i];
		parent = creature->getParent();
		score = creature->getScore();

		if (creature->getName() == "") {
			creature->setName(createName(iGeneration, i));
		}

		if (parent == NULL) {
			std::cout << "    ";
		}
		else {
			std::cout << (score > parent->getScore() ? "+++ " :
				          score < parent->getScore() ? "--- " : "=== ");
		}

		std::cout << creature->getGenealogie() << " :  ";

		if (parent == NULL) {
			std::cout << score << "m\n";
		}
		else {
			std::cout << parent->getScore() << " => " << score << "m\n";
		}
	}

	return exit;
}


void initOpenGL() {

	// initialise GLFW
	glfwSetErrorCallback(OnError);
	if (!glfwInit())
		throw std::runtime_error("glfwInit failed");

	// open a window with GLFW
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_SAMPLES, 4); // Antialiasing global

	gWindow = glfwCreateWindow((int)SCREEN_SIZE.x, (int)SCREEN_SIZE.y, "Go Darwin, go!", NULL, NULL);
	if (!gWindow)
		throw std::runtime_error("glfwCreateWindow failed. Can your hardware handle OpenGL 3.2?");

	// GLFW settings
//	glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//	glfwSetCursorPos(gWindow, 0, 0);
//	glfwSetScrollCallback(gWindow, OnScroll);
	glfwMakeContextCurrent(gWindow);

	// initialise GLEW
	glewExperimental = GL_TRUE; //stops glew crashing on OSX :-/

	if (glewInit() != GLEW_OK)
		throw std::runtime_error("glewInit failed");

	// GLEW throws some errors, so discard all the errors so far
	while (glGetError() != GL_NO_ERROR) {}

	// print out some info about the graphics drivers
	std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

	// make sure OpenGL version 3.2 API is available
	if (!GLEW_VERSION_3_2)
		throw std::runtime_error("OpenGL 3.2 API is not available.");

	// OpenGL settings
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// load vertex and fragment shaders into opengl
	LoadShaders();
	LoadShaders2D();
	LoadTriangle();
}


void DisplayCreature(Creature* creature) {

	pCreatureDisplay->glInit();
	pCreatureDisplay->glUpdate(gProgram->attrib("vert"), gProgram->attrib("vertTexCoord"), gProgram->attrib("vertColor"));

	glm::vec3 pos = pCreatureDisplay->getPosition();

	float t = .1*glfwGetTime();
	float scale = 1.;
	float r = scale*(.6 + .2*cos(t*.1));
	gCamera.setPosition(glm::vec3(pos.x + r*cos(t), pos.y + r*sin(t), /*pos.z +*/ scale*.5));
	gCamera.lookAt(pos);
	gCamera.setViewportAspectRatio(SCREEN_SIZE.x / SCREEN_SIZE.y);
	gCamera.setFieldOfView(50.);

	// draw one frame
	glClearColor(1., .2, .21, 1); // black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// bind the program (the shaders)
	gProgram->use();

	// set the "camera" uniform
	gProgram->setUniform("camera", gCamera.matrix()); // , false);
	gProgram->setUniform("model", glm::mat4());

	pCreatureDisplay->glRender();
	gProgram->stopUsing();
}

void Display2DGUI()
{
	gProgram2D->use();
	glBindVertexArray(gVAO);
	glDrawArrays(GL_TRIANGLES, 0, 2*3);
	glBindVertexArray(0);
	gProgram2D->stopUsing();
}
int min(int i, int j) {
	return i < j ? i : j;
}

void DisplayCreatureGL() {
	double lastTime = glfwGetTime();
	double creatureTime = lastTime;

	while (!glfwWindowShouldClose(gWindow)) {
		// process pending events
		glfwPollEvents();

		// update the scene based on the time elapsed since last update
#ifdef DISPLAY_ANIM
		double thisTime = (double)timeGetTime() / 1000.;
#else
		double thisTime = glfwGetTime();
#endif
		float dt = (float)(thisTime - lastTime);
		lastTime = thisTime;

		Update(dt);

#ifdef WITH_SIMULATION
		#pragma omp critical(pCreatureDisplay)
#endif
		{
			int nbTry1 = 0, nbTry3 = 0;

#ifdef DISPLAY_ANIM
			if (thisTime - creatureTime > 2*(creatureID/5)+5) {
				creatureID++;
				if (creatureID < animCreature.size())
				{
					creatureTime = thisTime;
					pCreatureDisplay = animCreature[min(creatureID, animCreature.size() - 1)]->clone();
					pCreatureDisplay->animate(&nbTry1, &nbTry3);
					pCreatureDisplay->resetTime();
				}
			}
#endif
			double dtc = 0;
		
			for (int i = 0; i < 200; i++) {
				dtc += pCreatureDisplay->animate(&nbTry1, &nbTry3);
				if (dtc >= dt)
					break;
			}

			DisplayCreature(pCreatureDisplay);
		//	Display2DGUI();
		}

		// swap the display buffers (displays what was just drawn)
		glfwSwapBuffers(gWindow);

		// check for errors
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
			std::cerr << "OpenGL Error " << error << std::endl;

	//	if (glfwGetKey(gWindow, GLFW_KEY_SPACE))
	//		break;

		//exit program if escape key is pressed
		if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE))
			glfwSetWindowShouldClose(gWindow, GL_TRUE);
	}
}


void DoEvolution(Creature* pCreature1) {
	bool exit = false;

	std::vector<Creature*> sortedGeneration;
	std::vector<Creature*> nextGeneration;

//	for (int i = 0; i < NB_CREATURE; i++) {		
	sortedGeneration.push_back(pCreature1);
//	}
	//createMutation(pCreature1, NB_CREATURE - 1, &sortedGeneration);


	while (!glfwWindowShouldClose(gWindow)) {

		// Prepare next generation
		nextGeneration.clear();

		for (int i = 0; i < NB_SAVED && i<sortedGeneration.size(); i++) {
			nextGeneration.push_back(sortedGeneration[i]);
//			createMutation(sortedGeneration[i], 6, &nextGeneration);
		}

		size_t remains = NB_CREATURE - nextGeneration.size();
		for (int j = 0; j < remains; j++) {
			Creature* creatureRandom = sortedGeneration[(rand() % (j + 1)) % sortedGeneration.size()];
			createMutation(creatureRandom, 1, &nextGeneration);
		}

		// On libere un peu de memoire
		for (int i = NB_SAVED; i < sortedGeneration.size(); i++) {
			sortedGeneration[i]->clearMemory();
		}
		sortedGeneration.clear();

		sortedGeneration.insert(sortedGeneration.end(), nextGeneration.begin(), nextGeneration.end());

		if (simulateAll(&sortedGeneration, SIMULATION_DURATION)) {
			/*	//exit = true;
				std::cout << "\n--------------";
				std::cout << "\n AFFINATE !!!";
				std::cout << "\n--------------";
				for (int i = 0; i < sortedGeneration.size(); i++) {
					sortedGeneration[i] = sortedGeneration[i]->divide();
				}
				*/
		}

		double moy = 0;
		for (int i = 0; i < sortedGeneration.size(); i++) {
			moy += sortedGeneration[i]->getScore();
		}
		moy /= (double)sortedGeneration.size();
	
		std::cout << "\n--------";
		std::cout << "\nGeneration : " << iGeneration;
		std::cout << "\n  Best: " << sortedGeneration[0]->getScore();
		std::cout << "\n  Average:" << moy;
		std::cout << "\n--------\n";

		iGeneration++;


		if (sortedGeneration[0]->getScore() > pCreatureDisplay->getScore())
#ifdef WITH_DISPLAY
			#pragma omp critical(pCreatureDisplay)
#endif
		{
			if (pCreatureDisplay != NULL) delete pCreatureDisplay;	
			pCreatureDisplay = sortedGeneration[0]->clone();
#ifdef WITH_DISPLAY
			int nbTry1 = 0, nbTry3 = 0;
			pCreatureDisplay->animate(&nbTry1, &nbTry3);
			pCreatureDisplay->resetTime();			
#endif 
			sprintf_s(numstr, "%03d", creatureID);
			std::string filePath = "C:\\Users\\durands\\Desktop\\Creature\\creatureSave";
			filePath += numstr;
			filePath += ".json";
			sortedGeneration[0]->saveJSON(filePath);
			creatureID++;
		}


		//DisplayCreature(pCreatureDisplay);
	}
}

// the program starts here
void AppMain(int argc, char *argv[]) {

	

	Creature* pCreature1;

#ifdef DISPLAY_ANIM
	
	for (int i = 0; i < 33; i++) {
		sprintf_s(numstr, "%03d", i);
		std::string filePath = "C:\\Users\\durands\\Desktop\\Creature\\creatureSave";
		filePath += numstr;
		filePath += ".json";
		animCreature.push_back(new Creature(filePath));
	}
	pCreature1 = animCreature[0];

#else

	if (argc == 2) {
		pCreature1 = new Creature(argv[1]);
		std::cout << "Open: " << argv[1];
	}
	else {
		
		sprintf_s(numstr, "%03d", creatureID);
		std::string filePath = "C:\\Users\\durands\\Desktop\\Creature\\creatureSave";
		filePath += numstr;
		filePath += ".json";
		creatureID++;
		
		pCreature1 = new Creature(filePath);
		
		//pCreature1 = new Creature();
		//pCreature1->setName(createName(iGeneration, 0));
		//iGeneration++;
	}
#endif

	int nbTry1=0, nbTry3=0;
// prepare Creature to display
	pCreatureDisplay = pCreature1->clone();
	pCreatureDisplay->animate(&nbTry1, &nbTry3);
	pCreatureDisplay->resetTime();

#if defined(WITH_DISPLAY) && defined(WITH_SIMULATION)
	omp_set_num_threads(2);
	omp_set_nested(1);

	#pragma omp parallel sections
	{
		#pragma omp section
		{
#endif
#ifdef WITH_DISPLAY
			initOpenGL();
			DisplayCreatureGL();
			// clean up and exit
			glfwTerminate();
#endif
#if defined(WITH_DISPLAY) && defined(WITH_SIMULATION)		
		}
		#pragma omp section
		{
#endif
#ifdef WITH_SIMULATION
			DoEvolution(pCreature1);
#endif
#if defined(WITH_DISPLAY) && defined(WITH_SIMULATION)
		}
	}
#endif

}


int main(int argc, char *argv[]) {
    try {
        AppMain(argc, argv);
    } catch (const std::exception& e){
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
