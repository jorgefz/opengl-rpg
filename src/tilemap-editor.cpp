
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <chrono>

#include <GL/glew.h>
#include <GL/glxew.h>
#include <GLFW/glfw3.h>

#include "Engine.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


/*
	TODO

Editor
- Key to switch between logic and tile grids
- Key to save
- New command actually creates new file
- UI element that displays current chosen tile
- Scrolling

Development
- Find better place to put to-do.
- Versioning system

Graphical
- Testing: make (or find) a good tileset texture

Engine
- Coordinate Systems
	Lay out world-to-screen coordinate conversions properly
- Tilemap: normalize values for logic grid.
	Hence fix TILE_SPAWN value
- UI System
	1) Static shapes with uniform color
	2) Make them display information (current selected tile to draw?)

*/


GLuint cooldown = 15;
GLuint chosen_tile = 0;

/*
struct Engine::tilemap_t* tilemap_fcreate(struct Engine::tilemap_t& tilemap, string &ftilemap, string &ftileset, GLuint width, GLuint height){
	//Create tilemap file and fill it with template/zeroes and a spawn
	// Write to file
	// Close tilemap
}
*/

bool ProcessInput(GLFWwindow* window, Engine::Tilemap &tmap, float &dx, float &dy){
	// Key states for player movement
	int w_state = glfwGetKey(window, GLFW_KEY_W);
	int s_state = glfwGetKey(window, GLFW_KEY_S);
	int a_state = glfwGetKey(window, GLFW_KEY_A);
	int d_state = glfwGetKey(window, GLFW_KEY_D);
	if(w_state == GLFW_PRESS) dy = 0.01f;
	else if(s_state == GLFW_PRESS) dy = -0.01f;
	else dy = 0;
	if(d_state == GLFW_PRESS) dx = 0.01f;
	else if(a_state == GLFW_PRESS) dx = -0.01f;
	else dx = 0;	
	// Correct for aspect ratio deformations
	dx *= 0.77;
	// Diagonal movement
	if(dx != 0 && dy != 0){
		dx *= sin(45);
		dy *= sin(45);
	}

	// Exit option
	if(glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS){
		glfwSetWindowShouldClose(window, true);
	}
	
	// Change tile to draw
	if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS and cooldown == 15){
		if(chosen_tile == 7) chosen_tile = 0;
		else chosen_tile++;
	}

	// Change tile option, mouse cursor
	if( glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ){
		double mouse_x, mouse_y;
		int sz_x, sz_y; // from upper left corner
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		glfwGetFramebufferSize(window, &sz_x, &sz_y);
			
		// Mouse on screen
		if(mouse_x>=0 and mouse_x<sz_x and mouse_y>=0 and mouse_y<sz_y){
			//Get fractional positions
			float cx = 2*(float(mouse_x)/float(sz_x) - 0.5f);
			float cy = 2*(float(sz_y - mouse_y)/float(sz_y) - 0.5f);
			int tile_id = tmap.GetTile(cx, cy);
			if(tile_id != tmap.width*tmap.height and tmap.logic_grid[tile_id] != chosen_tile){
				tmap.logic_grid[tile_id] = chosen_tile;
				tmap.GenTileTextureCoords(tile_id);
			}
		}
		
	}

	if(cooldown > 0) cooldown--;
	else cooldown = 15;

	return true;
}


void ParseArgs(int argc, char** argv, string& ftilemap, string& ftileset){
	
	if( argc < 4 ){
		cout << "Not enough arguments" << endl;
		exit(-1);
	}
	string mode = argv[1];
	ftilemap  = argv[2];
	ftileset = argv[3];

	if(mode == "new"){
		if(argc < 6){
			cout << "Missing new tilemap dimensions" << endl;
			exit(-1);
		} else{
			string swidth = argv[4];
			string sheight = argv[5];
			int width = std::stoi(swidth);
			int height = std::stoi(sheight);
			cout << width << " " << height << endl;
		}
	} else if(mode != "load"){
		cout << " Unknown mode " << mode << endl;
		exit(-1);
	}	
	cout << "Loading tilemap " << ftilemap << " and tileset " << ftileset <<"..."<< endl;
}



int main(int argc, char** argv)
{
	string ftilemap, ftileset;

	ParseArgs(argc, argv, ftilemap, ftileset);

    	float dx=0, dy=0;
	int tileside = 50;

	GLFWwindow *window;
	window = Engine::gl_begin(1280, 720);

	// Shader
	string vshader = "res/vertex.shader";
	string fshader = "res/fragment.shader";
	Engine::Shader shader;
	shader.Init(vshader, fshader);
	shader.Bind();

	// Tilemap
	Engine::Tilemap tmap;
	tmap.Init(ftilemap, ftileset, tileside);

	// Cursor
	string texcursor_path = "res/cursor.png";
	Engine::Shape shape;
	shape.Init(texcursor_path, 50, 50);

	
	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		ProcessInput(window, tmap, dx, dy);	
		
		tmap.Move(-dx, -dy);
		tmap.Draw();

		shape.Draw();
		
		Engine::glOnWindowResize(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}
