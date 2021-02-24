
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




int main()
{

	GLFWwindow *window = Engine::gl_begin(1280, 720);
	
	std::srand( std::time(nullptr) );
	
	int side = 55;
	struct Engine::tilemap_t tilemap;
	string tm = "test.tm";
	string ts = "res/tile_test2.png";
	Engine::tilemap_init(tilemap, tm, ts, side);

   	struct Engine::shader_t shader;
	Engine::shader_init(shader, "res/vertex.shader", "res/fragment.shader");
	Engine::shader_bind(&shader);

	// Player	
	struct Engine::texture_t ch_texture;
    	//Engine::texture_init(&ch_texture, &tile_data, tile_size, tile_size, channel_num);
    	Engine::texture_init(&ch_texture, "res/output_tile.jpg");
	Engine::texture_bind(ch_texture.id);

	float ch_vex[16];
    	float ch_velx=0, ch_vely=0;
    	Engine::generate_square_coords(ch_vex, Engine::SCR_WIDTH/2-25, Engine::SCR_HEIGHT/2-25, 50);
	struct Engine::shape_t character;
	Engine::shape_init(character, ch_vex, Engine::indices);

	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		// Key states for player movement
		int w_state = glfwGetKey(window, GLFW_KEY_W);
		int s_state = glfwGetKey(window, GLFW_KEY_S);
		int a_state = glfwGetKey(window, GLFW_KEY_A);
		int d_state = glfwGetKey(window, GLFW_KEY_D);
		if(w_state == GLFW_PRESS) ch_vely = 0.01f;
		else if(s_state == GLFW_PRESS) ch_vely = -0.01f;
		else ch_vely = 0;
		if(d_state == GLFW_PRESS) ch_velx = 0.01f;
		else if(a_state == GLFW_PRESS) ch_velx = -0.01f;
		else ch_velx = 0;

		if(glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS){
			break;
		}
		// Diagonal movement
		if(ch_velx != 0 && ch_vely != 0){
			ch_velx *= sin(45); ch_vely *= sin(45);
		}

		Engine::is_valid_move(tilemap, character, ch_velx, ch_vely);
		
		// Drawing tilemap
		for(int i=0; i!=tilemap.height*tilemap.width; ++i){
			Engine::translate(&tilemap.vertices[16*i], -ch_velx, -ch_vely);
		}
		Engine::tilemap_draw(tilemap);		
		
		// Drawing Player
		Engine::shape_bind(character);
		Engine::texture_bind(ch_texture.id);
		Engine::shape_update(character, ch_vex);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Deleting everything
	Engine::shape_unbind();
	Engine::texture_unbind();
	Engine::shader_unbind();

    	Engine::shader_delete(shader);
	Engine::tilemap_free(tilemap);
	Engine::texture_delete(ch_texture);
	Engine::shape_free(character);	

	glfwTerminate();

	return 0;
}
