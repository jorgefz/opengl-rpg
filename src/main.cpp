
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


bool IsValidMove(Engine::Tilemap &tmap, Engine::Shape &player, float &velx, float &vely){

	bool xmove=true, ymove=true;
	float vx=velx, vy=vely;

	// x movement	
	player.Move(velx, 0);	
	for(int i=0; i!=tmap.height*tmap.width; ++i){
		if(tmap.logic_grid[i] != Engine::TILE_WALL) continue;
		// FIX THIS - GetTileVert method and shape.Collides accepts float array
		std::vector<float> tile_vert(&tmap.shape.vertices[16*i], &tmap.shape.vertices[16*i]+16);
		if(player.Collides(tile_vert)){
			velx=0;
			xmove = false;
			break;
		}
	}
	player.Move(-vx, 0);

	// y movement
	player.Move(0, vely);
	for(int i=0; i!=tmap.height*tmap.width; ++i){
		if(tmap.logic_grid[i] != Engine::TILE_WALL) continue;
		// FIX THIS - GetTileVert method and shape.Collides accepts float array
		std::vector<float> tile_vert(&tmap.shape.vertices[16*i], &tmap.shape.vertices[16*i]+16);
		if(player.Collides(tile_vert)){
			vely=0;
			ymove = false;
			break;
		}
	}
	player.Move(0, -vy);

	return (xmove and ymove);
}


int main()
{

	GLFWwindow *window = Engine::GLBegin(1280, 720);
	
	std::srand( std::time(nullptr) );
	
	int side = 55;
    	float velx=0, vely=0;
	std::string tm = "res/test3.tm";
	//std::string ts = "res/tile_test2.png";
	std::string ts = "res/tileset.png";
	std::string vshader("res/vertex.shader");
	std::string fshader("res/fragment.shader");
	std::string player_tex("res/player.jpg");

	Engine::Tilemap tilemap;
	Engine::Shader shader;
	Engine::Shape player;

	tilemap.Init(tm, ts, side);
	shader.Init(vshader, fshader);
	player.Init(player_tex, 50, 50);

	shader.Bind();

	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		// Key states for player movement	
		if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS) vely = 0.01f;
		else if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS) vely = -0.01f;
		else vely = 0;
	
		if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS) velx = 0.01f;
		else if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS) velx = -0.01f;
		else velx = 0;
	
		//Aspect ratio correction
		velx *= 0.77;
		// Diagonal movement
		if(velx != 0 && vely != 0){
			velx *= sin(45); vely *= sin(45);
		}

		if(glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS){
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		IsValidMove(tilemap, player, velx, vely);

		// Drawing tilemap
		tilemap.Move(-velx, -vely);
		tilemap.Draw();
	
		// Drawing Player
		player.Draw();
		
		Engine::UpdateKeyStates(window);
		Engine::glOnWindowResize(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}
