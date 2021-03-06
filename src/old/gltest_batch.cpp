
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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef __WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;


/*
TODO
 - Implement Tileset as texture coordinates - DONE
 - Batch rendering - DONE
 - Learning some pixel art
 - Tilemap editor
 - Viewport?
 - Portals
*/

// Triangle 1: 0,1,2
// Traingle 2: 2,3,0
GLuint indices[] = { 0, 1, 2, 2, 3, 0 };


#define assert(x) if(!(x)) exit(-1);

#define GLCall(x) do {\
		GLClearError();\
		x;\
		assert(GLCheckError(#x, __FILE__, __LINE__));\
	} while(0);


#define SCR_WIDTH 1280
#define SCR_HEIGHT 720

#define uchar unsigned char

void wait(float seconds){
#ifdef __WIN32
	Sleep( uint64_t(seconds * 1000) );
	sleep
#else
	usleep( uint64_t(seconds * 1000 * 1000) );
#endif
}

static void GLClearError(){
	while(glGetError() != GL_NO_ERROR);
}

static bool GLCheckError(const char* func, const char* file, int line){
	GLenum err;
	do {
		err = glGetError();
		if(err != 0) {
			cout << "[GL] Error " << err << " (" << "0x" << hex << err << ")" <<
			" -> " << func << " " << file << ":" << line << endl;
		}
	} while(err != 0);
	return true;
}

//====================================================================


//==============================================================================

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
};



//=======================================================================================

GLFWwindow *gl_begin(){
	GLFWwindow *window;

	if(!glfwInit()) {
		cerr << "[GLFW] Fatal error: failed to load!" << endl;
		return NULL;
	}

	window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT, "Test Window", NULL, NULL);
	if(!window){
		cerr << "[GLFW] Fatal error: failed to create window" << endl;
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	//vsync
	glfwSwapInterval(1);

	if(glewInit() != GLEW_OK){
		cerr << "[GLEW] Fatal error: failed to load!" << endl;
		return NULL;
	}
	cout << "[GLEW] Using version: " << glewGetString(GLEW_VERSION) << endl;
	return window;
}


/*
Converts pixel data from RGB to RGBA.
Return data must have at least (total bytes + pixels) memory spaces.
Fills new alpha channels with given value (default = 255
*/
unsigned char* rgba(unsigned char *dest, unsigned char* data, int pixels, unsigned char alpha=255){
	unsigned char* ptr = data;
	unsigned char* cpy = dest;
	for(int p=0; p!=pixels; ++p){
		copy(ptr, ptr+3, cpy); // copy rgb
		ptr += 3;
		cpy += 3;
		*cpy = (unsigned char)alpha; // add alpha, default 0xFF
		cpy++;
	}
	return dest;
}

/*
Converts pixel data from RGBA to RGB.
Return data must have at least (total bytes - pixels) memory spaces.
*/
unsigned char* rgb(unsigned char* dest, unsigned char* data, int pixels){
	unsigned char* ptr = data;
	unsigned char* cpy = dest;
	for(int p=0; p!=pixels; ++p){
		copy(ptr, ptr+3, cpy); // copy rgb
		ptr += 4;
		cpy += 3;
	}
	return dest;
}


string* file_read_lines(string& lines, const char* filepath){
	ifstream stream(filepath);
	string line;
	stringstream ss;
	if(stream.fail()){
		cout << "Error: shader file '" << filepath << "' not found" << endl;
		exit(-1);
	}
	while(getline(stream, line)) ss << line << '\n';
	lines = ss.str();
	return &lines;
}


/*
Copies texture coordinates into a set of vertices.
*/
float* copy_texture_coords(float *vertices, float *texcoords){
	GLuint ind[] = {V1_T, V1_S, V2_T, V2_S, V3_T, V3_S, V4_T, V4_S};
	for(int i=0; i!=8; ++i) vertices[ind[i]] = texcoords[i];
	return vertices;
}

/*
Copies position coordinates into a set of vertices
*/
float* copy_position_coords(float *vertices, float *texcoords){
	GLuint ind[] = {V1_X, V1_Y, V2_X, V2_Y, V3_X, V3_Y, V4_X, V4_Y};
	for(int i=0; i!=8; ++i) vertices[ind[i]] = texcoords[i];
	return vertices;
}


void translate(float *vertices, float velx, float vely){
	// Move x positions
	vertices[V1_X] += velx; vertices[V2_X] += velx; vertices[V3_X] += velx; vertices[V4_X] += velx;
	// Move y positions
	vertices[V1_Y] += vely; vertices[V2_Y] += vely; vertices[V3_Y] += vely; vertices[V4_Y] += vely;	
}

/*
Converts pixel coordinates and size of a square
to vertex data.
*/
float* generate_square_coords(float *data, int x, int y, int side){
	// Origin is in lower-left corner
	int pixel_coords[] = {
		x,	y,
		x+side, y,
		x+side, y+side,
		x,	y+side
	};
	// Normalizing to window dimensions and recentering.
	float vertex_coords[8];
	int norm;
	for(int i=0; i!=8; ++i){
		(i%2) ? norm = SCR_HEIGHT : norm = SCR_WIDTH;
		vertex_coords[i] = float(pixel_coords[i])/float(norm);
		vertex_coords[i] = vertex_coords[i]*2 - 1.0f;
	}
	float vertices[] = {
		vertex_coords[0], vertex_coords[1],   0.0f, 0.5f,
		vertex_coords[2], vertex_coords[3],   0.25f, 0.5f,
		vertex_coords[4], vertex_coords[5],   0.25f, 0.0f,
		vertex_coords[6], vertex_coords[7],   0.0f, 0.0f
	};

	copy(vertices, vertices+16, data);
	return data;
}

// =======================================================================================

// ====================== TEXTURE FUNCTIONS =========================

struct texture_t* texture_init(struct texture_t *tex, unsigned char* img_data, int height, int width, int channel_num=4){

	unsigned char rgba_data[height * width * 4];
	if(channel_num == 3){
		cout << "Warning: texture will be converted to RGBA" << endl;
		channel_num++;
		rgba(rgba_data, img_data, height*width);
	} else copy(img_data, img_data + height*width*4, rgba_data);

	tex->width = width;
	tex->height = height;
	tex->channel_num = channel_num;

	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->width, tex->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data));
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

struct texture_t* texture_init(struct texture_t* tex, const char* _filepath){
	unsigned char *img_data;
	img_data = stbi_load(_filepath, &tex->width, &tex->height, &tex->channel_num, 0);
	if(!img_data){
		cout << "Error: failed to load image '" << _filepath << "'" << endl;
		exit(-1);
	}
   	
	unsigned char rgba_data[tex->height * tex->width * 4];
	if(tex->channel_num == 3){
		cout << "Warning: texture '" << _filepath << "' will be converted to RGBA" << endl;
		tex->channel_num++;
		rgba(rgba_data, img_data, tex->height * tex->width);
	} else copy(img_data, img_data + tex->height * tex->width * 4, rgba_data);

	tex->filepath = _filepath;
	tex = texture_init(tex, rgba_data, tex->height, tex->width);
	stbi_image_free(img_data);
	return tex;
}

void texture_bind(GLuint texture_id){
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);
}

void texture_unbind(){
	glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_delete(struct texture_t &texture){
	glDeleteTextures(1, &texture.id);
}

//========================================================================================================================

// ====================== SHADER FUNCTIONS =========================

GLuint shader_compile(GLenum type, string& source){
	GLuint id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	GLCall(glCompileShader(id));

	// Error Handling
	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if(result == GL_FALSE){
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		// Stack allocator
		char *message = static_cast<char*>(alloca(length * sizeof(char)));
		glGetShaderInfoLog(id, length, &length, message);
		cerr << "[GL] Error: failed to compile shader!" << endl;
		cerr << message << endl;
		glDeleteShader(id);
		exit(-1);
	}

	return id;
}


struct shader_t* shader_init(struct shader_t& shader, const char* _vertex_path, const char* _fragment_path){
	shader.vertex_path = _vertex_path;
	shader.fragment_path = _fragment_path;

	//Read shader scripts
	file_read_lines(shader.vertex_shader, shader.vertex_path.c_str());
	file_read_lines(shader.fragment_shader, shader.fragment_path.c_str());

	shader.program = glCreateProgram();
	GLuint vs = shader_compile(GL_VERTEX_SHADER, shader.vertex_shader);
	GLuint fs = shader_compile(GL_FRAGMENT_SHADER, shader.fragment_shader);

	if(!vs || !fs) return NULL;

	glAttachShader(shader.program, vs);
	glAttachShader(shader.program, fs);
	glLinkProgram(shader.program);
	glValidateProgram(shader.program);

	//Shader data has been copied to program and is no longer necessary.
	glDeleteShader(vs);
	glDeleteShader(fs);

	return &shader;
}


void shader_bind(struct shader_t* shader){
	GLCall(glUseProgram(shader->program));
}

void shader_unbind(){
	GLCall(glUseProgram(0));
}

void shader_delete(struct shader_t& shader){
	glDeleteProgram(shader.program);
}


//========================================================================================================================

// ====================== SHAPE FUNCTIONS =========================

struct shape_t* shape_init(struct shape_t& shape, float* _vertex_data, GLuint* _index_data, GLuint _vnum=4, GLuint _inum=6, GLuint _sdims=2, GLuint _tdims=2) {

	shape.vertex_num = _vnum;
	shape.index_num = _inum;
	shape.sdims = _sdims;
	shape.tdims = _tdims;
	shape.elem_num = _vnum*(_sdims + _tdims);
	shape.texture = nullptr;
	shape.shader = nullptr;

	shape.vertex_data = new float[shape.elem_num];
	copy(_vertex_data, _vertex_data+shape.elem_num, shape.vertex_data);

	shape.index_data = new GLuint[shape.index_num];
	copy(_index_data, _index_data+_inum, shape.index_data);
	
	glGenBuffers(1, &shape.vbo);
	glGenBuffers(1, &shape.ibo);

	glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);

	glBufferData(GL_ARRAY_BUFFER, shape.elem_num*sizeof(float), &shape.vertex_data[0], GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.index_num*sizeof(GLuint), &shape.index_data[0], GL_DYNAMIC_DRAW);

	// Telling OpenGL how to interpret data in buffer
	// Screen position data
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (_sdims+_tdims)*sizeof(float), (void*)(0));
	// Texture position data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (_sdims+_tdims)*sizeof(float), (void*)(_sdims*sizeof(float)) );

    return &shape;
}

struct shape_t* shape_update(struct shape_t& shape, float *new_vertex_data = nullptr){
	if(new_vertex_data){
		copy(new_vertex_data, new_vertex_data+shape.elem_num, shape.vertex_data);
	}
	glBufferData(GL_ARRAY_BUFFER, shape.elem_num*sizeof(float), shape.vertex_data, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(shape.sdims*sizeof(float)) );
	return &shape;
}


struct shape_t* shape_set_texture(struct shape_t& shape, const char* texpath){
	shape.texture = new struct texture_t;
	texture_init(shape.texture, texpath);
	return &shape;
}


struct shape_t* shape_set_shader(struct shape_t& shape, const char* _vertex_shader_path, const char* _fragment_shader_path){
	shape.shader = new struct shader_t;
	shader_init(*shape.shader, _vertex_shader_path, _fragment_shader_path);
	shader_bind(shape.shader);
	return &shape;
}

void shape_bind(struct shape_t& shape){
	glBindBuffer(GL_ARRAY_BUFFER, shape.vbo );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo );
}

void shape_unbind(){
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void shape_free(struct shape_t &shape){
	delete[] shape.vertex_data;
	delete[] shape.index_data;
	delete shape.texture;
	delete shape.shader;
}

void shape_draw(GLuint indexnum=6){
	GLCall(glDrawElements(GL_TRIANGLES, indexnum, GL_UNSIGNED_INT, nullptr));
}

// Moves shape by dersired delta.
void shape_move(float *vertices, float delta_x, float delta_y){
	vertices[V1_X] += delta_x; vertices[V2_X] += delta_x; vertices[V3_X] += delta_x; vertices[V4_X] += delta_x;
	vertices[V1_Y] += delta_y; vertices[V2_Y] += delta_y; vertices[V3_Y] += delta_y; vertices[V4_Y] += delta_y;
}



//=========================================================================

// ==================== TILEMAP FUNCTIONS =======================

void tilemap_fwrite(struct tilemap_t &tilemap, string &filename){
	
	fstream file(filename.c_str(), ios::binary|ios::out|ios::trunc);
	if(!file.is_open()){
		cout << "Error opening file " << filename << endl;
		exit(-1);
	}
	// Writing dimensions of tilemap: 1 byte width, 1 byte height
	file.put((uchar)tilemap.width);
	file.put((uchar)tilemap.height);
	
	// Write tile grid
	for(int i=0; i!=tilemap.width*tilemap.height; ++i){
		file.put((uchar)tilemap.tile_grid[i]);
	}
	// Write logic grid
	for(int i=0; i!=tilemap.width*tilemap.height; ++i){
		file.put((uchar)tilemap.logic_grid[i]);
	}

	file.close();
}

struct tilemap_t* tilemap_fread(struct tilemap_t &tilemap, string &filename){
	
	fstream file(filename.c_str(), ios::binary|ios::in);
	if(!file.is_open()){
		cout << "Error reopening file " << filename << endl;
		exit(-1);
	}
	char ch;

	// Reading width and height
	file.get(ch);
	tilemap.width = int(ch);
	file.get(ch);
	tilemap.height = int(ch);
	
	tilemap.tile_grid = new int[tilemap.width*tilemap.height];
	tilemap.logic_grid = new int[tilemap.width*tilemap.height];
	tilemap.vertices = new float[16*tilemap.width*tilemap.height];
	tilemap.map_indices = new GLuint[6*tilemap.width*tilemap.height];	

	// Reading tile grid
	for(int i=0; i!=tilemap.width*tilemap.height; ++i){
		if(!file.good()){
			cout << "Could not read tilemap" << endl;
			return nullptr;
		}
		file.get(ch);
		tilemap.tile_grid[i] = int(ch);
	}
	// Reading tile grid
	for(int i=0; i!=tilemap.width*tilemap.height; ++i){
		if(!file.good()){
			cout << "Could not read tilemap" << endl;
			return nullptr;
		}
		file.get(ch);
		tilemap.logic_grid[i] = int(ch);
	}
	file.close();
	return &tilemap;
}


void tilemap_translate(struct tilemap_t &tilemap, float dx, float dy){
	for(int j=0; j!=tilemap.height*tilemap.width; ++j){
		translate(tilemap.vertices+16*j, dx, dy);
	}
}

/*
Centers tilemap for player spawning.
*/
void tilemap_center_spawn(struct tilemap_t &tilemap){

	// Seek spawn coords in tilemap	and translate so player spawns on spawn tile
	// whilst also being at the center of the screen
	// This is why a viewport might be useful...
	int spawn_ind = 0;
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		if(tilemap.logic_grid[i] == 7){
			spawn_ind = i;
			break;
		}
	}	

	float *spawn_vertices = (tilemap.vertices + 16*spawn_ind);
	float spawn_cx = (spawn_vertices[V2_X] + spawn_vertices[V1_X])/2.0;
	float spawn_cy = (spawn_vertices[V4_Y] + spawn_vertices[V1_Y])/2.0;
	tilemap_translate(tilemap, -spawn_cx, -spawn_cy);
}


/*
Generates the tileset texture coordinates for the tiles of a tilemap
*/
void tilemap_gen_texture_coords(struct tilemap_t &tilemap, int tileset_height=2, int tileset_width=4, int tile_size=8){

	float tx = 1.0f/tileset_width; //Fractional sides of tile in tileset
	float ty = 1.0f/tileset_height;
	/*
	Counting starts at lower left.
	From left to right, then bottom to top.
	E.g.:
	5 6 7 8
	1 2 3 4
	*/

	// Generate set of texture coordinates for each tile in the tileset	
	float tex_coords[tileset_height*tileset_width*8];
	for(int y=0; y!=tileset_height; ++y){
		for(int x=0; x!=tileset_width; ++x){
			float _tc[] = {
			tx*float(x),     ty*(float(y)+1),
			tx*(float(x)+1), ty*(float(y)+1),
			tx*(float(x)+1), ty*float(y),
			tx*float(x),     ty*float(y)
			};
			copy(_tc, _tc+8, tex_coords + (y*tileset_width+x)*8 );
		}
	}
	
	// Copy-in set of texture coords for each tile in tilemap
	for(int t=0; t!=(tilemap.height*tilemap.width); ++t){
		int ind = tilemap.logic_grid[t]; //which tile
		float *vertices = tilemap.vertices + t*16; //where to copy-in	
		copy_texture_coords(vertices, tex_coords+ind*8);
	}
}


/*
Initialises tilemap by reading tileset and tilemap from file
*/
struct tilemap_t* tilemap_init(struct tilemap_t &tm, string &tilemap_fname, string &tileset_fname, int tilesize = 50){
	// Initialize tileset
	texture_init(&tm.tileset, tileset_fname.c_str());
	cout << tm.tileset.width << endl;

	// Open and read tilemap file
	if(!tilemap_fread(tm, tilemap_fname)) exit(-1);

	cout << tm.height << tm.width << endl;

	for(int i=0; i!=tm.width*tm.height; ++i){
		cout << tm.logic_grid[i] << " ";
		if(i % tm.width == 0) cout << endl;
	}
	cout << endl;


	for(int i=0; i!=tm.width*tm.height; ++i){
		cout << tm.tile_grid[i] << " ";
		if(i % tm.width == 0 and i > 1) cout << endl;
	}
	cout << endl;

	// Initialize params
	tm.tilesize = tilesize;	
	
	// Initialize map shape and indices	
	for(int h=0; h!=tm.height; ++h){
		for(int w=0; w!=tm.width; ++w){
			generate_square_coords(&tm.vertices[16*(w + h*tm.width)], tilesize*w, tilesize*h, tilesize);	
			for(int i=0; i!=6; ++i) tm.map_indices[6*(w+h*tm.width)+i] = indices[i] + 4*(w+h*tm.width);
			//copy(indices, indices+4*(w+h*tm.width), tm.map_indices + 6*(w+h*tm.width));
		}
	}
	cout << "square coords generated" << endl;	

	shape_init(tm.map_shape, tm.vertices, tm.map_indices, 4*tm.height*tm.width, 6*tm.height*tm.width);
	
	tilemap_gen_texture_coords(tm);
	tilemap_center_spawn(tm);

	return &tm; 
}

/*
void tilemap_init(struct tilemap_t &tilemap, int tilenum, int tilesize=50, int xsize=10, int ysize=10){
	tilemap.height = ysize;
	tilemap.width = xsize;
	tilemap.tile_grid = new int[xsize*ysize];
	tilemap.logic_grid = new int[xsize*ysize];
	tilemap.vertices = new float[xsize*ysize*16];
	tilemap.map_indices = new GLuint[6*xsize*ysize];

	int logic_map[] = {
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,2,3,4,0,0,0,0,0,1,
		1,0,0,0,0,0,6,7,5,0,0,0,0,0,1,
		1,0,0,0,0,0,4,3,2,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
	};

	copy(logic_map, logic_map + xsize*ysize, tilemap.logic_grid);
	copy(logic_map, logic_map + xsize*ysize, tilemap.tile_grid);

	for(int h=0; h!=ysize; ++h){
		for(int w=0; w!=xsize; ++w){
			cout << " " << tilemap.logic_grid[w + h*xsize];
			generate_square_coords(&tilemap.vertices[16*(w + h*xsize)], tilesize*w, tilesize*h, tilesize);	
			for(int i=0; i!=6; ++i) tilemap.map_indices[6*(w+h*xsize)+i] = indices[i] + 4*(w+h*xsize);
		}
		cout << endl;
	}		
	shape_init(tilemap.map_shape, tilemap.vertices, tilemap.map_indices, 4*xsize*ysize, 6*xsize*ysize); 
}
*/

void tilemap_draw(struct tilemap_t &tilemap){
	texture_bind(tilemap.tileset.id);
	shape_bind(tilemap.map_shape);
	shape_update(tilemap.map_shape, tilemap.vertices);	
	GLCall(glDrawElements(GL_TRIANGLES, 6*tilemap.height*tilemap.width, GL_UNSIGNED_INT, nullptr));
}

void tilemap_free(struct tilemap_t &tilemap){
	delete[] tilemap.tile_grid;
	delete[] tilemap.logic_grid;
	delete[] tilemap.vertices;
	shape_free(tilemap.map_shape);
	delete[] tilemap.map_indices;
	texture_delete(tilemap.tileset);
}

// ====================================================================


/*
Returns true if two squares overlap,
by checking if any of the four vertices of a square is within the other square.
*/
bool shapes_collide(float *shape1, float *shape2){
	float x,y;
	for(int i=0; i!=4; i++){
		x = shape1[4*i];
		y = shape1[4*i+1];
		if(x>shape2[V1_X] && x<shape2[V2_X] && y>shape2[V1_Y] && y<shape2[V4_Y]){
			return true;
		}
	}
	
	for(int i=0; i!=4; i++){
		x = shape2[4*i];
		y = shape2[4*i+1];
		if(x>shape1[V1_X] && x<shape1[V2_X] && y>shape1[V1_Y] && y<shape1[V4_Y]){
			return true;
		}
	}
	return false;
}

int get_current_tile(struct tilemap_t &tilemap, float *shape_vertices){
	int cur = tilemap.height*tilemap.width;
	// Not too accurate to use shapes_collide
	// Better to make a function that checks collision between a tile and a point (center of player tile)
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		if(shapes_collide(shape_vertices, &tilemap.vertices[16*i])) cur = i;
	}
	return cur;
}


// Checks if next move will place player in a forbidden tile
bool is_valid_move(struct tilemap_t &tilemap, struct shape_t &player, float &velx, float &vely){
	float new_pos_x[16], new_pos_y[16];
	copy(player.vertex_data, player.vertex_data+16, new_pos_x);
	copy(player.vertex_data, player.vertex_data+16, new_pos_y);
	translate(new_pos_x, velx, 0);
	translate(new_pos_y, 0, vely);

	// Iterate through tilemap tiles
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		if(tilemap.logic_grid[i] != TILE_BOUND) continue;
		if(shapes_collide(new_pos_x, &tilemap.vertices[16*i])) velx = 0;
		if(shapes_collide(new_pos_y, &tilemap.vertices[16*i])) vely = 0;
	}
	return true;
}

/*
True if a shape is visible on screen
*/
bool is_on_screen(float *vertices){

	float screen[] = {
		-1.0f, -1.0f, 0, 0,
		1.0f, -1.0f, 0, 0,
		1.0f, 1.0f, 0, 0,
		-1.0f, 1.0f, 0, 0
	};
	/*
	// Smaller screen for testing purposes
	float screen[] = {
		-0.9f, -0.9f, 0, 0,
		0.9f, -0.9f, 0, 0,
		0.9f, 0.9f, 0, 0,
		-0.9f, 0.9f, 0, 0
	};
	*/
	return shapes_collide(vertices, screen);
}


/*
Extracts tile data from atlas tile map, stores it in heap, and returns a pointer to it.
*/
unsigned char* load_tileset(const char *atlas_filename, int *_tile_bytes, int *_tile_num, int tile_side=8){
	// Read tile atlas
	int height, width, channel_num;
	unsigned char *img_data;
	img_data = stbi_load(atlas_filename, &width, &height, &channel_num, 0);
	if(!img_data){
		cerr << "Error loading atlas texture" << endl;
		exit(-1);
	}

	// Total pixels in a tile
	int tile_pix = tile_side * tile_side;
	// Total bytes in a tile
	int tile_bytes = tile_pix * channel_num;
	// Total bytes in the atlas tile map
	int total_bytes = width * height * channel_num;
	// Number of tiles in the atlas
	int tile_num = total_bytes / tile_bytes;

	// Height of the atlas in tiles
	int atlas_htiles = height/tile_side;
	// Width of the atlas in tiles
	//int atlas_wtiles = width/tile_side;

	*_tile_bytes = tile_bytes;
	*_tile_num = tile_num;
	cout << "Allocating " << tile_num << "*" << tile_bytes << " bytes" << endl;
	unsigned char *tile_data = new unsigned char[tile_num*tile_bytes];

	if(!tile_data){
		cerr << "Allocation failed!" << endl;
		exit(-1);
	}

	// Copy to structured tiles
	//unsigned char tile_data[atlas_wtiles*atlas_htiles][tile_bytes];
	int stride = width*channel_num; //between tile pixel lines
	int lsize = tile_side*channel_num; //size of tile pixel line in bytes

	unsigned char *ptr = img_data;
	unsigned char one_tile[tile_bytes];

	for(int tile=0; tile!=tile_num; ++tile){
		for(int line=0; line!=tile_side; ++line){
			copy(ptr, ptr+lsize, one_tile + line*lsize);
			ptr += stride; // jump pointer to next tile line
		}
		copy(one_tile, one_tile+tile_bytes, &tile_data[tile*tile_bytes]);
		if(tile%atlas_htiles){
			ptr = img_data + int(lsize * ceil(float(tile)/2.0f));
		}
	}
	stbi_image_free(img_data);
	//stbi_write_jpg("res/output_tile.jpg", tile_side, tile_side, channel_num, tile_data[2], 100);
	//copy(tile_data, tile_data[tile_num*tile_bytes], tiles);

	return tile_data;
}


// ====================================================================

int main()
{

	GLFWwindow *window;
	if( !(window = gl_begin()) ){
		return (-1);
	}
	srand(time(nullptr));

	int tile_bytes, tile_num, tile_size = 8;
	unsigned char* tile_data = load_tileset("res/tile_test2.png", &tile_bytes, &tile_num, tile_size);
	int channel_num = tile_bytes/tile_num/tile_num;
	cout << " tile num: " << tile_num << endl;
	cout << " tile bytes: " << tile_bytes << endl;
	cout << " channel num: " << channel_num << endl;
	
	/*
	struct texture_t tileset;
	int ts_width, ts_height, ts_channel_num;
	unsigned char *img_data = stbi_load("res/tile_test2.png", &ts_width, &ts_height, &ts_channel_num, 0);
	texture_init(&tileset, img_data, ts_height, ts_width);
	*/
	
	int side = 55;
	struct tilemap_t tilemap;
	string tm = "test.tm";
	string ts = "res/tile_test2.png";
	//tilemap_init(tilemap, tile_num, side, 15, 15);
	tilemap_init(tilemap, tm, ts, side);


	//tilemap_gen_texture_coords(tilemap);
	//tilemap_center_spawn(tilemap);

	/*
	string tmfile = "test.tm";
	tilemap_fwrite(tilemap, tmfile);
	tilemap_fread(tilemap, tmfile);
	*/

   	struct shader_t shader;
	shader_init(shader, "res/vertex.shader", "res/fragment.shader");
	shader_bind(&shader);

	// Player
	struct texture_t ch_texture;
    	texture_init(&ch_texture, &tile_data[TILE_GREEN*tile_bytes], tile_size, tile_size, channel_num);
    	texture_bind(ch_texture.id);

	float ch_vex[16];
    	float ch_velx=0, ch_vely=0;
    	generate_square_coords(ch_vex, SCR_WIDTH/2-25, SCR_HEIGHT/2-25, 50);
	struct shape_t character;
	shape_init(character, ch_vex, indices);

	vector<int> frame_time;

	while( !glfwWindowShouldClose(window) ){

		// Timing measure begin
		auto t1 = std::chrono::high_resolution_clock::now();

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

		is_valid_move(tilemap, character, ch_velx, ch_vely);
		
		// Drawing tilemap
		for(int i=0; i!=tilemap.height*tilemap.width; ++i){
			translate(&tilemap.vertices[16*i], -ch_velx, -ch_vely);
		}
		tilemap_draw(tilemap);		
		
		// Drawing Player
		shape_bind(character);
		texture_bind(ch_texture.id);
		shape_update(character, ch_vex);
		GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));

		// Timing measure end
		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
		frame_time.push_back(int(duration));

		

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	int acum = 0;
	for(int i=0; i!=int(frame_time.size()); ++i) acum += frame_time.at(i);
    	double average_frame_time =  double(acum)/double(frame_time.size())/1E6;
    	cout << " Average fps was " << 1.0f/average_frame_time << endl;

	//Deleting everything
	shape_unbind();
	texture_unbind();
	shader_unbind();

    	shader_delete(shader);

	tilemap_free(tilemap);
	
	texture_delete(ch_texture);
	
	shape_free(character);

	delete[] tile_data;

	glfwTerminate();

	return 0;
}
