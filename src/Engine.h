


/*

	2D Engine: [NAME HERE]


- Improve error management: no preprocessor directives (GLCall, redefining assert...)i
- Debug messages with defines
- Learning some pixel art
- Tilemap Portals

Development
- Find better place to put to-do.
- Versioning system
- Choose a name for the Engine

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
- Naming
	Change struct member naming to "m_name" or "name_"
- Debug
	Improve Debug messages: more of them, basically.
	Place DEBUG define on editor source file - doesn't work?
- Key Input SyStem (KISS) - DONE
	Repeat for mouse buttons

VERSIONS
*/


#ifndef ENGINE_H
#define ENGINE_H

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


#define assert(x) if(!(x)) exit(-1);
#define GLCall(x) do {\
		GLClearError();\
		x;\
		assert(GLCheckError(#x, __FILE__, __LINE__))\
	} while(0);


typedef unsigned char uchar;
typedef unsigned int uint;


namespace Engine {

extern const GLuint INDICES[6];
extern const GLuint TSET_PIX; // Side in pixels of tiles in the tileset
extern const GLuint TILE_SZ; //Side of a tile in world coordinates

extern GLuint SCR_WIDTH;
extern GLuint SCR_HEIGHT;

#define KEYNUM (GLFW_KEY_LAST + 1)
extern int KEYSTATES[ KEYNUM ];

extern GLFWwindow* WINDOW;

// Colors

#define COLOR_RED {255,0,0}
#define COLOR_LGREEN {60,166,58}
#define COLOR_PURPLE {92,51,196}


// Vertex data layout
enum VertexArrayLayout{
    V1_X, V1_Y, V1_T, V1_S,
    V2_X, V2_Y, V2_T, V2_S,
    V3_X, V3_Y, V3_T, V3_S,
    V4_X, V4_Y, V4_T, V4_S,
};

enum TileMapIndices {
	TILE_GRASS = 0, TILE_WALL

};

enum TileMapLogicIndices {
	L_CLEAR = 0, L_OBSTACLE, L_PORTAL, L_SPAWN=15
};


struct Texture {
	GLuint id;
	int width, height, channel_num;
	std::string filepath;
	
	Texture(); //Constructor
	//Texture(string& fpath, int mode=GL_RGBA); //DELETE
	void Init(std::string& fpath, int mode=GL_RGBA);
	void Bind();
	void Unbind();
	~Texture(); //Destructor
};

struct Shader {
	std::string vpath, fpath; //filepaths
	GLuint program;
	GLuint uniform;
	double color[4];
	
	Shader(); //Constructor
	void Init(std::string& vpath, std::string& fpath);
	GLuint Compile(GLenum type, std::string& source);
	void Bind();
	void Unbind();
	~Shader(); //Destructor
};


struct Shape {
	GLuint vbo, ibo; //Vertex and Index Buffer Object
	GLuint vertex_num; // Number of vertices
	GLuint sdims, tdims; // Spatial and texture dimensions
	std::vector<float> vertices;
	std::vector<GLuint> indices;
	Texture texture;
	int tilenum, current_tile;
	int sidex, sidey; //Pixel side size
	int wx, wy; //World coords
	float sx, sy; //Screen coords

	// Methods
	Shape(); //Constructor	
	void Init(std::string& texture_path, GLuint sidex, GLuint sidey); // Simple square/rectangle
	void Init(std::vector<float>& vertices, std::vector<GLuint>& indices, int sdims=2, int tdims=2); //Generic shape, no texture
	void Update(float* new_vertices = nullptr);
	void SetTexture(std::string& path);
	void SetTexture(Texture& newTexture);
	void SetPosition(int x, int y); //Position in pixel coordinates
	void SetPosition(float x, float y); //Position in screen coordinates
	void SetTile(int tile); //Choose tile texture for shape from tileset
	void Bind();
	void Unbind();
	void Draw();
	void Move(float dx, float dy);
	void GetCenter(float& cx, float& cy);
	bool Collides(Shape&);
	bool Collides(std::vector<float>&);
	bool EnclosesPoint(float x, float y);
	bool OnScreen();
	~Shape(); //Destructor
};

struct Tilemap {
	int height, width, tilesize;
	int *tile_grid; // Which tile texture to place
	int *logic_grid; // Collisions, boundaries, portals, etc
	//int *layers[]; // Graphical layers on top	
	Shape shape; //includes map vertices and indices
	Texture tileset;
	GLuint tset_tilenum;
	std::string ftmap; //Filename

	Tilemap(); //Constructor
	void Init(std::string &tilemap_file, std::string &tileset_file, int tilesize = 50);
	void Write(std::string &filename); //Saves tilemap on file
	void Read(std::string &filename); //Reads tilemap from file
	void Move(float dx, float dy);
	void CenterSpawn(); //Centers screen/player on spawn
	void GenTileTextureCoords(int which);
	void GenTextureCoords();
	void Draw();
	bool TileEncloses(GLuint tile, float x, float y);
	GLuint GetTile(float x, float y);
	float* GetTileVertices(int tile);
	~Tilemap(); //Destructor
};


// ======================== FUNCTIONS =======================

void wait(float seconds);
void GLClearError();
bool GLCheckError(const char* func, const char* file, int line);

GLFWwindow* GLBegin(GLuint width, GLuint height, bool fullscreen=false);

//Saves lines in a file on a single string
std::string* FileReadLines(std::string& lines, const char* filepath);

// True if input point (x,y) is enclosed by square vertices
bool VerticesEnclose(float* vex, float x, float y);

void GenerateRectangleCoords(float* vertices, int x, int y, int side_x, int side_y);

float* CopyTextureCoords(float *vertices, float *texcoords);

float* CopyPositionCoords(float *vertices, float *poscoords);

void VerticesTranslate(float *vertices, float velx, float vely);

// Resizes viewport if window dimensions are changed
void glOnWindowResize(GLFWwindow* window);

void UpdateKeyStates(GLFWwindow *window);

// Coordinate transform from GLFW screen (-1 to 1), to pixels
void ScreenToPixel(float sx, float sy, int &px, int &py);

//Transform from screen pixels to screen.
void PixelToScreen(int px, int py, float &sx, float &sy);

unsigned char* rgba(unsigned char *dest, unsigned char* data, int pixels, unsigned char alpha=255);

unsigned char* rgb(unsigned char* dest, unsigned char* data, int pixels);

} // namespace Engine


#endif // ENGINE_H
