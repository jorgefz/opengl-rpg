
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
- Key to change current tile - DONE
- Key to switch between logic and tile grids
- Key to save
- New command actually creates new file
- Mouse pointer to draw - DONE
- UI element that displays current chosen tile

- Improve shape management
- Tilemap: make a function like tilemap_gen_texture_coords for single tile
- Tilemap: variable with number of tiles in tileset and other data.
	Hence reduce number of arguments in tilemap_gen_texture_coords.
	WHy does gen_texture_coords work with only one parameter??
- Tilemap: make a good tilemap
- Tilemap: normalize values for logic grid.
	Hence fix TILE_SPAWN value
*/


GLuint cooldown = 15;
GLuint chosen_tile = 0;

struct Engine::tilemap_t* tilemap_fcreate(struct Engine::tilemap_t& tilemap, string &ftilemap, string &ftileset, GLuint width, GLuint height){
	//Create tilemap file and fill it with template/zeroes and a spawn
	// Write to file
	// Close tilemap
}


bool process_input(GLFWwindow* window, struct Engine::shape_t cursor, struct Engine::tilemap_t &tmap, float &dx, float &dy){
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
		glfwGetCursorPos(window, &mouse_x, &mouse_y);

		// Get window position and size
		int sz_x, sz_y; // from upper left corner
		glfwGetFramebufferSize(window, &sz_x, &sz_y);
			
		// Mouse on screen
		if(mouse_x>=0 and mouse_x<sz_x and mouse_y>=0 and mouse_y<sz_y){
			//Get fractional positions
			float cx = 2*(float(mouse_x)/float(sz_x) - 0.5f);
			float cy = 2*(float(sz_y - mouse_y)/float(sz_y) - 0.5f);
			int tile_id = Engine::get_current_tile(tmap, cx, cy);
			if(tile_id != tmap.width*tmap*height){
				tmap.logic_grid[tile_id] = chosen_tile;
				Engine::tilemap_gen_texture_coords(tmap);
			}
		}
		
	}

	if(cooldown > 0) cooldown--;
	else cooldown = 15;

	return true;
}


int main(int argc, char** argv)
{
	string ftilemap, ftileset, mode;
	GLuint width, height;

	float vexcursor[16];
    	float dx=0, dy=0;
	int tileside = 50;
	
	GLFWwindow *window;
	struct Engine::tilemap_t tilemap;
   	struct Engine::shader_t shader;	
	struct Engine::shape_t cursor;
	struct Engine::texture_t texcursor;

	// Input validation
	if( argc < 4 ){
		cout << "Not enough arguments" << endl;
		return 1;
	}
	mode = argv[1];
	ftilemap  = argv[2];
	ftileset = argv[3];

	if(mode == "new"){
		if(argc < 6){
			cout << "Missing new tilemap dimensions" << endl;
			return 1;
		} else{
			string swidth = argv[4];
			string sheight = argv[5];
			width = std::stoi(swidth);
			height = std::stoi(sheight);
			cout << width << " " << height << endl;
		}
	} else if(mode != "load"){
		cout << " Unknown mode " << mode << endl;
		return 1;
	}	
	cout << "Loading tilemap " << ftilemap << " and tileset " << ftileset <<"..."<< endl;

	window = Engine::gl_begin(1280, 720);
	glViewport(0, 0, 1280, 720);
	std::srand( std::time(nullptr) );
	
	// Tilemap
	Engine::tilemap_init(tilemap, ftilemap, ftileset, tileside);
	Engine::shader_init(shader, "res/vertex.shader", "res/fragment.shader");
	Engine::shader_bind(&shader);

	// Cursor
	Engine::texture_init(&texcursor, "res/cursor.png");
	Engine::texture_bind(texcursor.id);

    	Engine::generate_square_coords(vexcursor, Engine::SCR_WIDTH/2-25, Engine::SCR_HEIGHT/2-25, 50);
	Engine::shape_init(cursor, vexcursor, Engine::indices);

	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		if( !process_input(window, cursor, tilemap, dx, dy) ) break;
		
		// Drawing tilemap
		Engine::tilemap_translate(tilemap, -dx, -dy);
		Engine::tilemap_draw(tilemap);
		
		// Drawing cursor
		Engine::shape_bind(cursor);
		Engine::texture_bind(texcursor.id);
		Engine::shape_update(cursor, vexcursor);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		//Resizing
		int screen_height, screen_width;
		glfwGetFramebufferSize(window, &screen_height, &screen_width);		
		glViewport(0, 0, screen_height, screen_width);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Deleting everything
	Engine::shape_unbind();
	Engine::texture_unbind();
	Engine::shader_unbind();

    	Engine::shader_delete(shader);
	Engine::tilemap_free(tilemap);
	Engine::texture_delete(texcursor);
	Engine::shape_free(cursor);

	glfwTerminate();

	return 0;
}
