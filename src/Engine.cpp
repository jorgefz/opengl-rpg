
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <chrono>

// OpenGL
#include <GL/glew.h>
#include <GL/glxew.h>
#include <GLFW/glfw3.h>

// Reading / writing images
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Engine.h"




namespace Engine {


	
// ============== Global Namespace Variables 

const GLuint INDICES[6] = {0, 1, 2, 2, 3, 0};
const GLuint TSET_PIX = 8; //Tile side in pixels
const GLuint TILE_SZ = 55; //Pixel side of a tile

GLuint SCR_WIDTH = 0;
GLuint SCR_HEIGHT = 0;

int KEYSTATES[] =  { 0 };

GLFWwindow* WINDOW = nullptr;

// ============== TEXTURE METHODS
//Constructor
Texture::Texture(): filepath() {
	id = 0; width = 0; height = 0; channel_num = 0;
}

void Texture::Init(std::string& fpath, int mode){
	unsigned char *image_data;
	this->filepath = fpath;	
	image_data = stbi_load(fpath.c_str(), &this->width, &this->height, &this->channel_num, 0);
	if(!image_data){
		std::cerr << "Error: failed to load image '" << fpath << "'" << std::endl;
		exit(-1);
	}
   	
	unsigned char rgba_data[this->height * this->width * 4];
	if(this->channel_num == 3 and mode == GL_RGBA){
		std::cout << "Warning: texture '" << fpath << "' will be converted to RGBA" << std::endl;
		this->channel_num++;
		rgba(rgba_data, image_data, this->height * this->width);
	} else std::copy(image_data, image_data + this->height * this->width * 4, rgba_data);
	
	glGenTextures(1, &this->id);
	glBindTexture(GL_TEXTURE_2D, this->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);	
	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data));
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	stbi_image_free(image_data);
}

void Texture::Bind(){
	glBindTexture(GL_TEXTURE_2D, this->id);
}

void Texture::Unbind(){
	glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture(){
	Texture::Unbind();
	glDeleteTextures(1, &this->id);
}	



// ============== SHADER METHODS ================

//Constructor
Shader::Shader(): vpath(), fpath() {
	program = 0;
}

void Shader::Init(std::string& vpath, std::string& fpath){
	std::string vertex_source, fragment_source;
	this->vpath = vpath;
	this->fpath = fpath;

	//Read shader scripts
	FileReadLines(vertex_source, this->vpath.c_str());
	FileReadLines(fragment_source, this->fpath.c_str());

	this->program = glCreateProgram();
	GLuint vs = Shader::Compile(GL_VERTEX_SHADER, vertex_source);
	GLuint fs = Shader::Compile(GL_FRAGMENT_SHADER, fragment_source);

	glAttachShader(this->program, vs);
	glAttachShader(this->program, fs);
	glLinkProgram(this->program);
	glValidateProgram(this->program);

	//Shader data has been copied to program and is no longer necessary.
	glDeleteShader(vs);
	glDeleteShader(fs);
}

GLuint Shader::Compile(GLenum type, std::string& source){	
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
		std::cerr << "[GL] Error: failed to compile shader!" << std::endl;
		std::cerr << message << std::endl;
		glDeleteShader(id);
		exit(-1);
	}

	return id;
}

void Shader::Bind(){
	GLCall(glUseProgram(this->program));
}

void Shader::Unbind(){
	GLCall(glUseProgram(0));
}

Shader::~Shader(){
	Shader::Unbind();
	glDeleteProgram(this->program);
}

// ======================== SHAPE METHODS =======================

// Constructor
Shape::Shape(): vertices(), indices(), texture() {
	vbo=0; ibo=0; vertex_num=0; sdims=0; tdims=0;
	//sx = 0; sy = 0; wx = 0; wy = 0;
	//tilenum = 0; current_tile = -1;
}


//Rectangle Initialization
void Shape::Init(std::string& texture_path, GLuint sidex, GLuint sidey) {
	// Param init
	this->sdims = 2; // Spatial dimensions
       	this->tdims = 2; // Texture dimensions
	this->vertices = std::vector<float>(16);
	this->indices = std::vector<GLuint>(Engine::INDICES, Engine::INDICES+6);
	this->texture.Init(texture_path);
	this->sidex = sidex; this->sidey = sidey;
	this->tilenum = texture.width * texture.height / TSET_PIX / TSET_PIX;
	
	//GenerateRectangleCoords(&this->vertices[0], SCR_WIDTH/2-sidex/2, SCR_HEIGHT/2-sidex/2, sidex, sidey);

	GenerateRectangleCoords(&this->vertices[0], 0, 0, sidex, sidey);

	// OpenGL create and fill buffer objects
	glGenBuffers(1, &this->vbo);
	glGenBuffers(1, &this->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo);

	glBufferData(GL_ARRAY_BUFFER, this->vertices.size()*sizeof(float), &this->vertices[0], GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size()*sizeof(float), &this->indices[0], GL_DYNAMIC_DRAW);

	// Position Coordinates
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, this->sdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(0));
	
	// Texture Coordinates
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, this->tdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(this->sdims*sizeof(float)) );
}

// Generic shape initializer
void Shape::Init(std::vector<float>& verts, std::vector<GLuint>& inds, int sdims, int tdims) {

	this->sdims = sdims;
	this->tdims = tdims;
	this->vertices.assign(&verts[0], &verts[0] + verts.size());
	this->indices.assign(&inds[0], &inds[0]+inds.size());
	this->vertex_num = vertices.size()/(sdims+tdims);

	// opengl stuff
	glGenBuffers(1, &this->vbo);
	glGenBuffers(1, &this->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo);

	glBufferData(GL_ARRAY_BUFFER, this->vertices.size()*sizeof(float), &this->vertices[0], GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size()*sizeof(float), &this->indices[0], GL_DYNAMIC_DRAW);

	// Screen position data
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, this->sdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(0));
	
	// Texture position data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, this->tdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(this->sdims*sizeof(float)) );
}

void Shape::Update(float* new_vertices){
	if(new_vertices) vertices = std::vector<float>(new_vertices, new_vertices+vertices.size());
	
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBufferData(GL_ARRAY_BUFFER, this->vertices.size()*sizeof(float), &this->vertices[0], GL_DYNAMIC_DRAW);
	
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size()*sizeof(float), &this->indices[0], GL_DYNAMIC_DRAW);

	// Screen position data
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, this->sdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(0));
	
	// Texture position data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, this->tdims, GL_FLOAT, GL_FALSE, (this->sdims+this->tdims)*sizeof(float), (void*)(this->sdims*sizeof(float)) );
}

void Shape::SetTexture(std::string& tpath){
	if(this->texture.id != 0) return; //Texture already set
	this->texture.Init(tpath);
}

void Shape::SetTexture(Texture& new_texture){
	if(this->texture.id != 0) return; //Texture already set
	texture.id = new_texture.id;
	texture.filepath = new_texture.filepath;
	texture.width = new_texture.width;
	texture.height = new_texture.height;
	texture.channel_num = new_texture.channel_num;
}


void Shape::SetPosition(float x, float y){
	this->sx = x; this->sy = y;
	int px, py;
	ScreenToPixel(x, y, px, py);
	GenerateRectangleCoords(&this->vertices[0], px, py, sidex, sidey);
}


void Shape::SetPosition(int x, int y){
	//this->wx = x, this->wy = y;
	GenerateRectangleCoords(&this->vertices[0], x, y, sidex, sidey);
}


void Shape::SetTile(int tile){
	if(tile >= this->tilenum) return;
	this->current_tile = tile;

	//Regenerate texture coordinates
	float tx = float(TSET_PIX)/float(this->texture.width);
	float ty = float(TSET_PIX)/float(this->texture.height);
	
	//int tile = this->logic_grid[which];
	int x = tile % int(1.0f/tx);
	int y = tile / int(1.0f/tx);

	float texcoords[] = {
		tx*float(x),     ty*(float(y)+1),
		tx*(float(x)+1), ty*(float(y)+1),
		tx*(float(x)+1), ty*float(y),
		tx*float(x),     ty*float(y)
	};
	CopyTextureCoords(&this->vertices[0], texcoords);
}


void Shape::Bind(){
	glBindBuffer(GL_ARRAY_BUFFER, vbo );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo );
}

void Shape::Unbind(){
	glBindBuffer(GL_ARRAY_BUFFER, 0 );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0 );
}

void Shape::Draw(){
	Shape::Bind();
	if(this->texture.id != 0) this->texture.Bind();
	Shape::Update();
	GLCall(glDrawElements(GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, nullptr));
}

void Shape::Move(float dx, float dy){
	// Move x positions
	vertices[V1_X] += dx; vertices[V2_X] += dx; vertices[V3_X] += dx; vertices[V4_X] += dx;
	// Move y positions
	vertices[V1_Y] += dy; vertices[V2_Y] += dy; vertices[V3_Y] += dy; vertices[V4_Y] += dy;	
}

void Shape::GetCenter(float& cx, float& cy){
	cx = (vertices[V2_X] + vertices[V1_X])/2.0;
	cy = (vertices[V4_Y] + vertices[V1_Y])/2.0;
}

bool Shape::Collides(Shape& obstacle){
	if(this->Collides(obstacle.vertices)) return true;
	return false;
}

bool Shape::Collides(std::vector<float>& obstacle){
	float x,y;
	for(int i=0; i!=4; i++){
		x = this->vertices[4*i];
		y = this->vertices[4*i+1];
		if(x>obstacle[V1_X] && x<obstacle[V2_X] && y>obstacle[V1_Y] && y<obstacle[V4_Y]){
			return true;
		}
	}
	
	for(int i=0; i!=4; i++){
		x = obstacle[4*i];
		y = obstacle[4*i+1];
		if(x>vertices[V1_X] && x<vertices[V2_X] && y>vertices[V1_Y] && y<vertices[V4_Y]){
			return true;
		}
	}
	return false;
}

bool Shape::EnclosesPoint(float x, float y){
	if(x>vertices[V1_X] && x<vertices[V2_X] && y>vertices[V1_Y] && y<vertices[V4_Y]) return true;
	return false;
}

bool Shape::OnScreen(){
	std::vector<float> screen_pos = {
		-1.0f, -1.0f, 0, 0,
		 1.0f, -1.0f, 0, 0,
		 1.0f,  1.0f, 0, 0,
		-1.0f,  1.0f, 0, 0
	};

	// Smaller screen for testing purposes
	//float screen[] = {
	//	-0.9f, -0.9f, 0, 0,
	//	0.9f, -0.9f, 0, 0,
	//	0.9f, 0.9f, 0, 0,
	//	-0.9f, 0.9f, 0, 0
	//};
	
	return this->Collides(screen_pos);
}

Shape::~Shape(){
	Shape::Unbind();
	//Texture destructor called here
}


// ======================== TILEMAP METHODS =======================

Tilemap::Tilemap(): shape(), tileset(), ftmap() {
	height=0; width=0; tilesize=0, tset_tilenum=0;
	tile_grid=nullptr; logic_grid=nullptr;
	//layers = nullptr;
}


void Tilemap::Init(std::string &tilemap_file, std::string &tileset_file, int tilesize) {
		
	this->Read(tilemap_file);
	this->tileset.Init(tileset_file); //Rely on internal shape texture instead

	#ifdef DEBUG
	std::cout << "[DEBUG] Logic Grid" << std::endl;
	for(int i=0; i!=this->width*this->height; ++i){
		std::cout << this->logic_grid[i] << " ";
		if((i+1) % this->width == 0) std::cout << std::endl;
	}
	std::cout << std::endl;
	std::cout << "[DEBUG] Tile Grid" << std::endl;
	for(int i=0; i!=this->width*this->height; ++i){
		std::cout << this->tile_grid[i] << " ";
		if((i+1) % this->width == 0) std::cout << std::endl;
	}
	std::cout << std::endl;
	#endif //DEBUG

	// Initialize params
	this->tilesize = tilesize;
	this->tset_tilenum = this->tileset.width * this->tileset.height / TSET_PIX / TSET_PIX;

	std::vector<float> vertices(width*height*16);
	std::vector<GLuint> indices(width*height*6);

	// IMPROVE THIS
	for(int h=0; h!=this->height; ++h){
		for(int w=0; w!=this->width; ++w){
			GenerateRectangleCoords(&vertices[16*(w + h*this->width)], tilesize*w, tilesize*h, tilesize, tilesize);	
			for(int i=0; i!=6; ++i) indices[6*(w+h*this->width)+i] = INDICES[i] + 4*(w+h*this->width);
		}
	}
	
	/*
	for(int tile=0; tile!=height*width; ++tile){
		int w = tile % this->width;
		int h = tile / this->width;
		GenerateRectangleCoords(&vertices[16*(w + h*this->width)], tilesize*w, tilesize*h, tilesize, tilesize);	
		for(int i=0; i!=6; ++i)
			indices[6*(w+h*this->width)+i] = INDICES[i] + 4*(w+h*this->width);
	}
	*/
	
	this->shape.Init(vertices, indices);
	this->GenTextureCoords();
	this->CenterSpawn();
}

void Tilemap::Write(std::string &filename){
}

void Tilemap::Read(std::string &filename){
	
	std::fstream file(filename.c_str(), std::ios::binary|std::ios::in );
	if(!file.is_open()){
		std::cerr << "Error opening file " << filename << std::endl;
		exit(-1);
	}
	this->ftmap = filename;
	char ch;

	// Reading width and height
	file.get(ch);
	this->width = int(ch);
	file.get(ch);
	this->height = int(ch);
	
	this->tile_grid = new int[this->width*this->height];
	this->logic_grid = new int[this->width*this->height];

	// Reading tile grid
	for(int i=0; i!=this->width*this->height; ++i){
		if(!file.good()){
			std::cout << "Could not read tilemap" << std::endl;
			exit(-1);
		}
		file.get(ch);
		this->tile_grid[i] = int(ch);
	}
	// Reading tile grid
	for(int i=0; i!=this->width*this->height; ++i){
		if(!file.good()){
			std::cout << "Could not read tilemap" << std::endl;
			exit(-1);
		}
		file.get(ch);
		this->logic_grid[i] = int(ch);
	}
	file.close();
}

void Tilemap::Move(float dx, float dy){
	for(int j=0; j!=height*width; ++j){
		VerticesTranslate(&shape.vertices[0]+16*j, dx, dy);
	}
}

void Tilemap::CenterSpawn(){
	// Seek spawn coords in tilemap	and translate so player spawns on spawn tile
	// whilst also being at the center of the screen
	int spawn_ind = 0;
	for(int i=0; i!=this->height*this->width; ++i){
		// REPLACE WITH TILE_SPAWN VALUE
		if(this->logic_grid[i] == L_SPAWN){
			spawn_ind = i;
			break;
		}
	}

	float *spawn_vertices = &this->shape.vertices[0] + 16*spawn_ind;
	float spawn_cx = (spawn_vertices[V2_X] + spawn_vertices[V1_X])/2.0;
	float spawn_cy = (spawn_vertices[V4_Y] + spawn_vertices[V1_Y])/2.0;
	this->Move(-spawn_cx, -spawn_cy);
}

void Tilemap::GenTileTextureCoords(int which){
	float tx = float(TSET_PIX)/float(this->tileset.width);
	float ty = float(TSET_PIX)/float(this->tileset.height);
	
	int tile = this->logic_grid[which];
	int x = tile % int(1.0f/tx);
	int y = tile / int(1.0f/tx);

	float texcoords[] = {
		tx*float(x),     ty*(float(y)+1),
		tx*(float(x)+1), ty*(float(y)+1),
		tx*(float(x)+1), ty*float(y),
		tx*float(x),     ty*float(y)
	};
	CopyTextureCoords(&this->shape.vertices[0]+which*16, texcoords);
}

void Tilemap::GenTextureCoords(){
	for(int i=0; i!=this->height*this->width; ++i){
		this->GenTileTextureCoords(i);
	}
}

void Tilemap::Draw(){
	tileset.Bind();
	shape.Draw();
}

GLuint Tilemap::GetTile(float x, float y){
	for(int i=0; i!=height*width; ++i){
		if(VerticesEnclose(&this->shape.vertices[16*i],x,y)) return i;
	}
	return height*width;
}

float* Tilemap::GetTileVertices(int tile){
	return &shape.vertices[16*tile];
}

Tilemap::~Tilemap(){
	delete[] this->logic_grid;
	delete[] this->tile_grid;
}



// ======================== OTHER FUNCTIONS =======================


void wait(float seconds){
#ifdef __WIN32
	Sleep( uint64_t(seconds * 1000) );
	sleep
#else
	usleep( uint64_t(seconds * 1000 * 1000) );
#endif
}

void GLClearError(){
	while(glGetError() != GL_NO_ERROR);
}

bool GLCheckError(const char* func, const char* file, int line){
	GLenum err;
	do {
		err = glGetError();
		if(err != 0) {
			std::cout << "[GL] Error " << err << " (" << "0x" << std::hex << err << ")" <<
			" -> " << func << " " << file << ":" << line << std::endl;
		}
	} while(err != 0);
	return true;
}

GLFWwindow *GLBegin(GLuint width, GLuint height, bool fullscreen){
	GLFWwindow *window;
	GLFWmonitor *monitor;	
	SCR_WIDTH = width;
	SCR_HEIGHT = height;

	if(!glfwInit()) {
		std::cerr << "[GLFW] Fatal error: failed to load!" << std::endl;
		exit(-1);
	}
	if( (width%16 != 0) or (height%9 != 0) ){
		std::cout << " Error: screen resolution must have 16:9 aspect ratio " << std::endl;
		exit(-1);
	}
	fullscreen ? monitor = glfwGetPrimaryMonitor() : monitor = NULL;
	window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT, "OpenGL 2D RPG", monitor, NULL);
	if(!window){
		std::cerr << "[GLFW] Fatal error: failed to create window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	WINDOW = window;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // activate VSYNC, problems with NVIDIA cards
	glViewport(0, 0, width, height);
	if(glewInit() != GLEW_OK){
		std::cerr << "[GLEW] Fatal error: failed to load!" << std::endl;
		exit(-1);
	}
	int glfw_major, glfw_minor, glfw_rev;
	glfwGetVersion(&glfw_major, &glfw_minor, &glfw_rev);
	std::cout << "[GLEW] Version " << glewGetString(GLEW_VERSION) << std::endl;
	std::cout << "[GLFW] Version "<<glfw_major<<'.'<<glfw_minor<<'.'<<glfw_rev<<std::endl;
	return window;
}

std::string* FileReadLines(std::string& lines, const char* filepath){
	std::ifstream stream(filepath);
	std::string line;
	std::stringstream ss;
	if(stream.fail()){
		std::cout << "Error: shader file '" << filepath << "' not found" << std::endl;
		exit(-1);
	}
	while(std::getline(stream, line)) ss << line << '\n';
	lines = ss.str();
	return &lines;
}

// Same as Shape::Encloses but with input vertices
bool VerticesEnclose(float* vex, float x, float y){
	if(x>vex[V1_X] && x<vex[V2_X] && y>vex[V1_Y] && y<vex[V4_Y]) return true;
	return false;
}

//Converts pixel coordinates and size of a square to vertex data.
void GenerateRectangleCoords(float* vertices, int x, int y, int side_x, int side_y){	
	// Origin is in lower-left corner
	int pixel_coords[] = {
		x,	  y,
		x+side_x, y,
		x+side_x, y+side_y,
		x,	  y+side_y
	};
	// Normalizing to window dimensions and recentering.
	float vertex_coords[8];
	int norm;
	for(int i=0; i!=8; ++i){
		(i%2) ? norm = SCR_HEIGHT : norm = SCR_WIDTH;
		vertex_coords[i] = float(pixel_coords[i])/float(norm);
		vertex_coords[i] = vertex_coords[i]*2 - 1.0f;
	}
	std::vector<float> vert = {
		vertex_coords[0], vertex_coords[1],   0.0f, 1.0f, //dummy texture coords
		vertex_coords[2], vertex_coords[3],   1.0f, 1.0f,
		vertex_coords[4], vertex_coords[5],   1.0f, 0.0f,
		vertex_coords[6], vertex_coords[7],   0.0f, 0.0f
	};
	for(int i=0; i!=vert.size(); ++i) vertices[i] = vert[i];
}

//Copies texture coordinates into a set of vertices.
float* CopyTextureCoords(float *vertices, float *texcoords){
	GLuint ind[] = {V1_T, V1_S, V2_T, V2_S, V3_T, V3_S, V4_T, V4_S};
	for(int i=0; i!=8; ++i) vertices[ind[i]] = texcoords[i];
	return vertices;
}


//Copies position coordinates into a set of vertices
float* CopyPositionCoords(float *vertices, float *poscoords){
	GLuint ind[] = {V1_X, V1_Y, V2_X, V2_Y, V3_X, V3_Y, V4_X, V4_Y};
	for(int i=0; i!=8; ++i) vertices[ind[i]] = poscoords[i];
	return vertices;
}


void VerticesTranslate(float *vertices, float velx, float vely){
	// Move x positions
	vertices[V1_X] += velx; vertices[V2_X] += velx; vertices[V3_X] += velx; vertices[V4_X] += velx;
	// Move y positions
	vertices[V1_Y] += vely; vertices[V2_Y] += vely; vertices[V3_Y] += vely; vertices[V4_Y] += vely;	
}

// Resizes viewport if window dimensions are changed
void glOnWindowResize(GLFWwindow* window){	
	int screen_height, screen_width;
	glfwGetFramebufferSize(window, &screen_height, &screen_width);		
	glViewport(0, 0, screen_height, screen_width);
	SCR_WIDTH = screen_width;
	SCR_HEIGHT = screen_height;
}


void UpdateKeyStates(GLFWwindow* window){
	for(int i=0; i!=KEYNUM; ++i) Engine::KEYSTATES[i] = glfwGetKey(window, i);
}


void ScreenToPixel(float sx, float sy, int &px, int &py){
	px = SCR_WIDTH*int((sx+1)/2.0f) ;
	py = SCR_HEIGHT*int((sy+1)/2.0f) ;
}

void PixelToScreen(int px, int py, float &sx, float &sy){
	sx = 2.0f*float(px)/float(SCR_WIDTH) - 1;
	sy = 2.0f*float(py)/float(SCR_HEIGHT) - 1;
}

/*
Converts pixel data from RGB to RGBA.
Return data must have at least (total bytes + pixels) memory spaces.
Fills new alpha channels with given value (default = 255
*/
unsigned char* rgba(unsigned char *dest, unsigned char* data, int pixels, unsigned char alpha){
	unsigned char* ptr = data;
	unsigned char* cpy = dest;
	for(int p=0; p!=pixels; ++p){
		std::copy(ptr, ptr+3, cpy); // copy rgb
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
		std::copy(ptr, ptr+3, cpy); // copy rgb
		ptr += 4;
		cpy += 3;
	}
	return dest;
}

} // namespace Engine



