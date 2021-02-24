


/*

	2D Engine: [NAME HERE]

	TODO
- Improve error management: no preprocessor directives (GLCall, redefining assert...)i
- Debug messages with defines
- Custom screen sizes (no defines)
- Improve initialization functions

- Learning some pixel art
- Tilemap editor
- Viewport?
- Tilemap Portals


*/


#ifndef ENGINE_H
#define ENGINE_H

#define DEBUG

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

#ifdef __WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

#define assert(x) if(!(x)) exit(-1);

#define GLCall(x) do {\
		GLClearError();\
		x;\
		assert(GLCheckError(#x, __FILE__, __LINE__));\
	} while(0);


typedef unsigned char uchar;


namespace Engine {

void wait(float seconds);

void GLClearError();

bool GLCheckError(const char* func, const char* file, int line);

//================================================================

extern GLuint indices[6];

extern GLuint SCR_WIDTH;
extern GLuint SCR_HEIGHT;

// Vertex data layout
enum VertexArrayLayout{
    V1_X, V1_Y, V1_T, V1_S,
    V2_X, V2_Y, V2_T, V2_S,
    V3_X, V3_Y, V3_T, V3_S,
    V4_X, V4_Y, V4_T, V4_S,
};


struct texture_t {
    GLuint id;
    int width, height, channel_num;
    string filepath;
};

struct shader_t {
	string vertex_shader, fragment_shader, vertex_path, fragment_path;
	uint32_t program;
	GLuint uniform_loc;
	float color[4];
};

struct shape_t {
	GLuint vbo;		//Vertex buffer object
	GLuint vertex_num;	// Number of vertices
	GLuint elem_num;	// Number of elements in vertex data array
	GLuint sdims;	// Spatial dimensions (x,y, etc)
	GLuint tdims;	// Texture dimensions (s,t, etc)
	float *vertex_data;
	
	GLuint ibo;		// Index buffer object
	GLuint index_num;	// Number of indices in IBO
	GLuint *index_data;

	struct texture_t* texture;
	struct shader_t* shader;
};


enum TileMapIndices {
	TILE_EMPTY = 0, TILE_DIRT, TILE_SAND, TILE_PATH, TILE_WATER,
	
	TILE_RED=0, TILE_YELLOW, TILE_GREEN, TILE_CYAN,
	TILE_BLUE, TILE_PURPLE, TILE_PINK, TILE_ORANGE
};

enum TileMapLogicIndices {
	TILE_CLEAR = 0, TILE_BOUND, TILE_SPAWN
};


struct tilemap_t {
	int height, width, tilesize;
	int *tile_grid; // Which tile texture to place
	int *logic_grid; // Collisions, boundaries, portals, etc
	//int *layers[]; // Graphical layers on top
	struct shape_t map_shape;
	float *vertices;
	GLuint *map_indices;
	struct texture_t tileset; // Tileset
	GLuint tset_tilenum;
	GLuint tset_tilesize;
};



//=========================================================================


//Initialise OpenGL window
GLFWwindow* gl_begin(GLuint width, GLuint height, bool fullscreen=false);

/*
Converts pixel data from RGB to RGBA.
Return data must have at least (total bytes + pixels) memory spaces.
Fills new alpha channels with given value (default = 0)
*/
unsigned char* rgba(unsigned char *dest, unsigned char* data, int pixels, unsigned char alpha=0);

/*
Converts pixel data from RGBA to RGB.
Return data must have at least (total bytes - pixels) memory spaces.
*/
unsigned char* rgb(unsigned char* dest, unsigned char* data, int pixels);

//Saves lines in a file on a single string
string* file_read_lines(string& lines, const char* filepath);

//Copies texture coordinates into a set of vertices.
float* copy_texture_coords(float *vertices, float *texcoords);

//Copies position coordinates into a set of vertices
float* copy_position_coords(float *vertices, float *texcoords);

//Moves a set of vertices by a desired dx and dy
void translate(float *vertices, float velx, float vely);

//Converts pixel coordinates and size of a square to vertex data.
float* generate_square_coords(float *data, int x, int y, int side);


// ====================== TEXTURE FUNCTIONS =========================

// Mark for deletion and inclusion onto the other texture_init
struct texture_t* texture_init(struct texture_t *tex, unsigned char* img_data, int height, int width, int channel_num=4);

struct texture_t* texture_init(struct texture_t* tex, const char* _filepath);

void texture_bind(GLuint texture_id);

void texture_unbind();

void texture_delete(struct texture_t &texture);


// ====================== SHADER FUNCTIONS =========================

GLuint shader_compile(GLenum type, string& source);

struct shader_t* shader_init(struct shader_t& shader, const char* _vertex_path, const char* _fragment_path);

void shader_bind(struct shader_t* shader);

void shader_unbind();

void shader_delete(struct shader_t& shader);


// ====================== SHAPE FUNCTIONS =========================

// Make shape_init more parameter friendly
// Include texture initialization in shape
struct shape_t* shape_init(struct shape_t& shape, float* _vertex_data, GLuint* _index_data, GLuint _vnum=4, GLuint _inum=6, GLuint _sdims=2, GLuint _tdims=2);

struct shape_t* shape_update(struct shape_t& shape, float *new_vertex_data = nullptr);

struct shape_t* shape_set_texture(struct shape_t& shape, const char* texpath);

//struct shape_t* shape_set_texture(struct shape_t& shape, GLuint texture_id);

struct shape_t* shape_set_shader(struct shape_t& shape, const char* _vertex_shader_path, const char* _fragment_shader_path);

void shape_bind(struct shape_t& shape);

void shape_unbind();

void shape_free(struct shape_t &shape);

void shape_draw(GLuint indexnum=6);

// Moves shape by dersired delta. Identical to 'translate'
void shape_move(float *vertices, float delta_x, float delta_y);


// ==================== TILEMAP FUNCTIONS =======================

// Reads tilemao from file
void tilemap_fwrite(struct tilemap_t &tilemap, string &filename);

// Writes tilemap to file
struct tilemap_t* tilemap_fread(struct tilemap_t &tilemap, string &filename);

void tilemap_translate(struct tilemap_t &tilemap, float dx, float dy);

//Centers tilemap for player spawning.
void tilemap_center_spawn(struct tilemap_t &tilemap);

//Generates the tile texture coordinates based on tileset dimensions
void tilemap_gen_texture_coords(struct tilemap_t &tilemap, int tileset_height=2, int tileset_width=4, int tile_size=8);

//Initialises tilemap by reading tileset and tilemap from file
struct tilemap_t* tilemap_init(struct tilemap_t &tm, string &tilemap_fname, string &tileset_fname, int tilesize = 50);

// DELETE - too verbose, initialization above is better
void tilemap_init(struct tilemap_t &tilemap, int tilenum, int tilesize=50, int xsize=10, int ysize=10);

void tilemap_draw(struct tilemap_t &tilemap);

void tilemap_free(struct tilemap_t &tilemap);

// ========================= OTHER FUNCTIONS =================================


void shape_get_center(float *vertices, float &cx, float &cy);

/*
Returns true if two squares overlap,
by checking if any of the four vertices of a square is within the other square.
*/
bool shapes_collide(float *shape1, float *shape2);

// True if a given point (x,y) is within a shape
bool point_in_shape(float *shape, float x, float y);

// Returns index of the tile player is on.
int get_current_tile(struct tilemap_t &tilemap, float x, float y);

// Checks if next move will place player in a forbidden tile
bool is_valid_move(struct tilemap_t &tilemap, struct shape_t &player, float &velx, float &vely);

//True if a shape is visible on screen
bool is_on_screen(float *vertices);

//Extracts tile data from atlas tile map, stores it in heap, and returns a pointer to it.
// DELETE
unsigned char* load_tileset(const char *atlas_filename, int *_tile_bytes, int *_tile_num, int tile_side=8);


}


#endif // ENGINE_H
