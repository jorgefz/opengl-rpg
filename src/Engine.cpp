
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


using namespace std;




namespace Engine {

// ============== Global Namespace Variables ==================
GLuint indices[6] = {0, 1, 2, 2, 3, 0};

const GLuint INDICES[6] = {0, 1, 2, 2, 3, 0};
const GLuint TILE_SZ = 50;
const GLuint TSET_PIX = 8;

GLuint SCR_WIDTH = 0;
GLuint SCR_HEIGHT = 0;


// ============== TEXTURE METHODS ================
//Constructor
Texture::Texture(): filepath() {
	id = 0; width = 0; height = 0; channel_num = 0;
}

void Texture::Init(string& fpath, int mode){
	unsigned char *image_data;
	this->filepath = fpath;	
	image_data = stbi_load(fpath.c_str(), &this->width, &this->height, &this->channel_num, 0);
	if(!image_data){
		cerr << "Error: failed to load image '" << fpath << "'" << endl;
		exit(-1);
	}
   	
	unsigned char rgba_data[this->height * this->width * 4];
	if(this->channel_num == 3 and mode == GL_RGBA){
		cout << "Warning: texture '" << fpath << "' will be converted to RGBA" << endl;
		this->channel_num++;
		rgba(rgba_data, image_data, this->height * this->width);
	} else copy(image_data, image_data + this->height * this->width * 4, rgba_data);
	
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

void Shader::Init(string& vpath, string& fpath){
	string vertex_source, fragment_source;
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

GLuint Shader::Compile(GLenum type, string& source){	
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
}


//Rectangle Initialization
void Shape::Init(string& texture_path, GLuint sidex, GLuint sidey) {
	// Param init
	this->sdims = 2; // Spatial dimensions
       	this->tdims = 2; // Texture dimensions
	this->vertices = vector<float>(16);
	this->indices = vector<GLuint>(Engine::INDICES, Engine::INDICES+6);
	this->texture.Init(texture_path);

	GenerateRectangleCoords(&this->vertices[0], SCR_WIDTH/2-sidex/2, SCR_HEIGHT/2-sidey/2, sidex, sidey);

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
void Shape::Init(vector<float>& verts, vector<GLuint>& inds, int sdims, int tdims) {

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
	if(new_vertices) vertices = vector<float>(new_vertices, new_vertices+vertices.size());
	
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

void Shape::SetTexture(string& tpath){
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

bool Shape::Collides(vector<float>& obstacle){
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
	vector<float> screen_pos = {
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

Tilemap::Tilemap(): shape(), tileset() {
	height=0; width=0; tilesize=0, tset_tilenum=0;
	tile_grid=nullptr; logic_grid=nullptr;
	//layers = nullptr;
}


void Tilemap::Init(string &tilemap_file, string &tileset_file, int tilesize) {
		
	Read(tilemap_file);
	this->tileset.Init(tileset_file); //Rely on internal shape texture instead

	#ifdef DEBUG
	cout << "[DEBUG] Logic Grid" << endl; 
	for(int i=0; i!=this->width*this->height; ++i){
		cout << this->logic_grid[i] << " ";
		if((i+1) % this->width == 0) cout << endl;
	}
	cout << endl;
	cout << "[DEBUG] Tile Grid" << endl;
	for(int i=0; i!=this->width*this->height; ++i){
		cout << this->tile_grid[i] << " ";
		if((i+1) % this->width == 0) cout << endl;
	}
	cout << endl;
	#endif //DEBUG

	// Initialize params
	this->tilesize = tilesize;
	this->tset_tilenum = this->tileset.width * this->tileset.height / TSET_PIX / TSET_PIX;

	vector<float> vertices(width*height*16);
	vector<GLuint> indices(width*height*6);

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

void Tilemap::Write(string &filename){
}

void Tilemap::Read(string &filename){
	
	fstream file(filename.c_str(), ios::binary|ios::in );
	if(!file.is_open()){
		cout << "Error opening file " << filename << endl;
		exit(-1);
	}
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
			cout << "Could not read tilemap" << endl;
			exit(-1);
		}
		file.get(ch);
		this->tile_grid[i] = int(ch);
	}
	// Reading tile grid
	for(int i=0; i!=this->width*this->height; ++i){
		if(!file.good()){
			cout << "Could not read tilemap" << endl;
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
		if(this->logic_grid[i] == 7){
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
	//shape.Bind();
	//shape.Update();
	//GLCall(glDrawElements(GL_TRIANGLES, 6*tilemap.height*tilemap.width, GL_UNSIGNED_INT, nullptr));
	shape.Draw();
}

GLuint Tilemap::GetTile(float x, float y){
	for(int i=0; i!=height*width; ++i){
		if(VerticesEnclose(&this->shape.vertices[16*i],x,y)) return i;
	}
	return height*width;
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
			cout << "[GL] Error " << err << " (" << "0x" << hex << err << ")" <<
			" -> " << func << " " << file << ":" << line << endl;
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
		cerr << "[GLFW] Fatal error: failed to load!" << endl;
		exit(-1);
	}
	if( (width%16 != 0) or (height%9 != 0) ){
		cout << " Error: screen resolution must have 16:9 aspect ratio " << endl;
		exit(-1);
	}
	fullscreen ? monitor = glfwGetPrimaryMonitor() : monitor = NULL;
	window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT, "OpenGL 2D RPG", monitor, NULL);
	if(!window){
		cerr << "[GLFW] Fatal error: failed to create window" << endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // activate VSYNC, problems with NVIDIA cards
	glViewport(0, 0, width, height);
	if(glewInit() != GLEW_OK){
		cerr << "[GLEW] Fatal error: failed to load!" << endl;
		exit(-1);
	}
	cout << "[GLEW] Version " << glewGetString(GLEW_VERSION) << endl;
	return window;
}

string* FileReadLines(string& lines, const char* filepath){
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
	vector<float> vert = {
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

/*
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
*/



//=======================================================================================

GLFWwindow *gl_begin(GLuint width, GLuint height, bool fullscreen){
	
	GLFWwindow *window;
	GLFWmonitor *monitor;	

	if(!glfwInit()) {
		cerr << "[GLFW] Fatal error: failed to load!" << endl;
		exit(-1);
	}
	
	fullscreen ? monitor = glfwGetPrimaryMonitor() : monitor = NULL;

	if( (width%16 != 0) or (height%9 != 0) ){
		cout << " Error: screen resolution must have 16:9 aspect ratio " << endl;
		exit(-1);
	}

	SCR_WIDTH = width;
	SCR_HEIGHT = height;

	//glfwWindowHint(GLFW_VERSION_MAJOR, 3);
	//glfwWindowHint(GLFW_VERSION_MINOR, 3);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_RESIZABLE, 1);
	//glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);	

	window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT, "OpenGL 2D RPG", monitor, NULL);
	if(!window){
		cerr << "[GLFW] Fatal error: failed to create window" << endl;
		glfwTerminate();
		exit(-1);
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // activate VSYNC, problems with NVIDIA cards

	glViewport(0, 0, width, height);

	if(glewInit() != GLEW_OK){
		cerr << "[GLEW] Fatal error: failed to load!" << endl;
		exit(-1);
	}
	cout << "[GLEW] Version " << glewGetString(GLEW_VERSION) << endl;
	return window;
}

// Resizes viewport if window dimensions are changed
void glOnWindowResize(GLFWwindow* window){	
	int screen_height, screen_width;
	glfwGetFramebufferSize(window, &screen_height, &screen_width);		
	glViewport(0, 0, screen_height, screen_width);
	SCR_WIDTH = screen_width;
	SCR_HEIGHT = screen_height;
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



//Copies texture coordinates into a set of vertices.
float* copy_texture_coords(float *vertices, float *texcoords){
	GLuint ind[] = {V1_T, V1_S, V2_T, V2_S, V3_T, V3_S, V4_T, V4_S};
	for(int i=0; i!=8; ++i) vertices[ind[i]] = texcoords[i];
	return vertices;
}


//Copies position coordinates into a set of vertices
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


//Converts pixel coordinates and size of a square to vertex data.
float* generate_square_coords(float *data, int x, int y, int side_x, int side_y){
	if(side_y == 0) side_y = side_x;
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
	float vertices[] = {
		vertex_coords[0], vertex_coords[1],   0.0f, 1.0f, //dummy texture coords
		vertex_coords[2], vertex_coords[3],   1.0f, 1.0f,
		vertex_coords[4], vertex_coords[5],   1.0f, 0.0f,
		vertex_coords[6], vertex_coords[7],   0.0f, 0.0f
	};

	copy(vertices, vertices+16, data);
	return data;
}


// ====================== TEXTURE FUNCTIONS =========================

struct texture_t* texture_init(struct texture_t *tex, unsigned char* img_data, int height, int width, int channel_num){

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
	tex = texture_init(tex, rgba_data, tex->height, tex->width, 4);
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


// ====================== SHAPE FUNCTIONS =========================


// Initialises a single square
struct shape_t* shape_init(struct shape_t& shape, string& texture_path, GLuint side){	
	
	// Param init
	shape.sdims = 2; // Spatial dimensions
       	shape.tdims = 2; // Texture dimensions
	shape.vertex_num = 4;
	shape.index_num = 6;
	shape.elem_num = shape.vertex_num * (shape.sdims + shape.tdims);
	shape.vertex_data = new float[shape.elem_num];
	shape.index_data = new GLuint[shape.index_num];
	shape.shader = nullptr;

	copy(indices, indices+6, shape.index_data);		
	generate_square_coords(shape.vertex_data, SCR_WIDTH/2-side/2, SCR_HEIGHT/2-side/2, side);
	texture_init(&shape.texture, texture_path.c_str());

	// opengl stuff
	glGenBuffers(1, &shape.vbo);
	glGenBuffers(1, &shape.ibo);

	glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);

	glBufferData(GL_ARRAY_BUFFER, shape.elem_num*sizeof(float), &shape.vertex_data[0], GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.index_num*sizeof(GLuint), &shape.index_data[0], GL_DYNAMIC_DRAW);

	// Screen position data
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(0));
	
	// Texture position data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(shape.sdims*sizeof(float)) );

	return &shape;
}


struct shape_t* shape_init(struct shape_t& shape, string& texpath, vector<float>& vertices, vector<GLuint>& indices){

	shape.sdims = 2;
	shape.tdims = 2;
	shape.vertex_num = vertices.size()/4;
	shape.index_num = indices.size();
	shape.elem_num = shape.vertex_num * (shape.sdims+shape.tdims);

	copy(&vertices[0], &vertices[0]+shape.vertex_num, shape.vertex_data);
	copy(&indices[0], &indices[0] + shape.index_num, shape.index_data);
	texture_init(&shape.texture, texpath.c_str());

	glGenBuffers(1, &shape.vbo);
	glGenBuffers(1, &shape.ibo);

	glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);

	glBufferData(GL_ARRAY_BUFFER, shape.elem_num*sizeof(float), &shape.vertex_data[0], GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.index_num*sizeof(GLuint), &shape.index_data[0], GL_DYNAMIC_DRAW);

	// Screen position data
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(0));
	
	// Texture position data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(shape.sdims*sizeof(float)) );


	return &shape;
}

struct shape_t* shape_init(struct shape_t& shape, float* _vertex_data, GLuint* _index_data, GLuint _vnum, GLuint _inum, GLuint _sdims, GLuint _tdims) {

	shape.vertex_num = _vnum;
	shape.index_num = _inum;
	shape.sdims = _sdims;
	shape.tdims = _tdims;
	shape.elem_num = _vnum*(_sdims + _tdims);
	shape.texture.id = 0;
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

struct shape_t* shape_update(struct shape_t& shape, float *new_vertices){
	if(new_vertices){
		copy(new_vertices, new_vertices+shape.elem_num, shape.vertex_data);
	}
	glBufferData(GL_ARRAY_BUFFER, shape.elem_num*sizeof(float), shape.vertex_data, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (shape.sdims+shape.tdims)*sizeof(float), (void*)(shape.sdims*sizeof(float)) );
	return &shape;
}

/*
struct shape_t* shape_set_texture(struct shape_t& shape, const char* texpath){
	shape.texture = new struct texture_t;
	texture_init(shape.texture, texpath);
	return &shape;
}
*/

/*
void shape_set_texture(struct shape_t& shape, struct texture_t& texture){
	shape.texture.id = texture.id;
}
*/
/*
struct shape_t* shape_set_shader(struct shape_t& shape, const char* _vertex_shader_path, const char* _fragment_shader_path){
	shape.shader = new struct shader_t;
	shader_init(*shape.shader, _vertex_shader_path, _fragment_shader_path);
	shader_bind(shape.shader);
	return &shape;
}
*/

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
	if(shape.texture.id > 0) texture_delete(shape.texture);
	if(shape.shader) delete shape.shader;
}

void shape_draw(struct shape_t& shape){
	shape_bind(shape);
	texture_bind(shape.texture.id);
	shape_update(shape);
	GLCall(glDrawElements(GL_TRIANGLES, shape.index_num, GL_UNSIGNED_INT, nullptr));
}

// Moves shape by dersired delta.
void shape_move(float *vertices, float delta_x, float delta_y){
	vertices[V1_X] += delta_x; vertices[V2_X] += delta_x; vertices[V3_X] += delta_x; vertices[V4_X] += delta_x;
	vertices[V1_Y] += delta_y; vertices[V2_Y] += delta_y; vertices[V3_Y] += delta_y; vertices[V4_Y] += delta_y;
}


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


//Centers tilemap for player spawning.
void tilemap_center_spawn(struct tilemap_t &tilemap){

	// Seek spawn coords in tilemap	and translate so player spawns on spawn tile
	// whilst also being at the center of the screen
	// This is why a viewport might be useful...
	int spawn_ind = 0;
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		// REPLACE WITH TILE_SPAWN VALUE
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


void tilemap_update_tile_texcoords(struct tilemap_t& tilemap, int which){
	
	float tx = float(TSET_PIX)/float(tilemap.tileset.width);
	float ty = float(TSET_PIX)/float(tilemap.tileset.height);
	
	int tile = tilemap.logic_grid[which];
	int x = tile % int(1.0f/tx);
	int y = tile / int(1.0f/tx);

	float texcoords[] = {
		tx*float(x),     ty*(float(y)+1),
		tx*(float(x)+1), ty*(float(y)+1),
		tx*(float(x)+1), ty*float(y),
		tx*float(x),     ty*float(y)
	};
	copy_texture_coords(tilemap.vertices+which*16, texcoords);
	return;
}


//Generates the tileset texture coordinates for the tiles of a tilemap
void tilemap_gen_texture_coords(struct tilemap_t &tilemap){
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		tilemap_update_tile_texcoords(tilemap, i);
	}
}


//Initialises tilemap by reading tileset and tilemap from file
struct tilemap_t* tilemap_init(struct tilemap_t &tm, string &tilemap_fname, string &tileset_fname, int tilesize){
	// Initialize tileset
	texture_init(&tm.tileset, tileset_fname.c_str());

	// Open and read tilemap file
	if(!tilemap_fread(tm, tilemap_fname)) exit(-1);

	
	#ifdef DEBUG
	cout << "[DEBUG] Logic Grid" << endl; 
	for(int i=0; i!=tm.width*tm.height; ++i){
		cout << tm.logic_grid[i] << " ";
		if((i+1) % tm.width == 0) cout << endl;
	}
	cout << endl;
	cout << "[DEBUG] Tile Grid" << endl;
	for(int i=0; i!=tm.width*tm.height; ++i){
		cout << tm.tile_grid[i] << " ";
		if((i+1) % tm.width == 0) cout << endl;
	}
	cout << endl;
	#endif //DEBUG

	// Initialize params
	tm.tilesize = tilesize;
	tm.tset_tilenum = tm.tileset.width * tm.tileset.height / TSET_PIX / TSET_PIX;
	
	// Initialize map shape and indices	
	for(int h=0; h!=tm.height; ++h){
		for(int w=0; w!=tm.width; ++w){
			generate_square_coords(&tm.vertices[16*(w + h*tm.width)], tilesize*w, tilesize*h, tilesize);	
			for(int i=0; i!=6; ++i) tm.map_indices[6*(w+h*tm.width)+i] = indices[i] + 4*(w+h*tm.width);
			//copy(indices, indices+4*(w+h*tm.width), tm.map_indices + 6*(w+h*tm.width));
		}
	}

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


void shape_get_center(float *vertices, float &cx, float &cy){
	cx = (vertices[V2_X] + vertices[V1_X])/2.0;
	cy = (vertices[V4_Y] + vertices[V1_Y])/2.0;
}


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

// Checks if a given position is within a shape
bool point_in_shape(float *shape, float x, float y){
	if(x>shape[V1_X] && x<shape[V2_X] && y>shape[V1_Y] && y<shape[V4_Y]) return true;
	return false;
}


int get_current_tile(struct tilemap_t &tilemap, float x, float y){
	for(int i=0; i!=tilemap.height*tilemap.width; ++i){
		if(point_in_shape(&tilemap.vertices[16*i],x,y)) return i;
	}
	return tilemap.height*tilemap.width;
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


//True if a shape is visible on screen
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



//Extracts tile data from atlas tile map, stores it in heap, and returns a pointer to it.
unsigned char* load_tileset(const char *atlas_filename, int *_tile_bytes, int *_tile_num, int tile_side){
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


} // namespace Engine



