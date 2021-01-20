


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef __WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;


/*
TODO

Better error handling
Separate fragment and vertex shader files.


*/


#define assert(x) if(!(x)) exit(-1);

#define GLCall(x) do {\
		GLClearError();\
		x;\
		assert(GLCheckError(#x, __FILE__, __LINE__));\
	} while(0);


void wait(float seconds){
#ifdef __WIN32
	Sleep( uint64_t(seconds * 1000) );
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

//========================================================================================================================

struct texture_t* texture_init(struct texture_t* tex, const char* _filepath){

    unsigned char *img_data;

    img_data = stbi_load(_filepath, &tex->width, &tex->height, &tex->channel_num, 0);
    if(!img_data){
        cout << "Error: failed to load image '" << _filepath << "'" << endl;
        exit(-1);
    }
    tex->filepath = _filepath;

	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->width, tex->height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data));
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

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

//========================================================================================================================

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


GLFWwindow *gl_begin(){
	GLFWwindow *window;

	if(!glfwInit()) {
		cerr << "[GLFW] Fatal error: failed to load!" << endl;
		return NULL;
	}

	window = glfwCreateWindow(800, 800, "Test Window", NULL, NULL);
	if(!window){
		cerr << "[GLFW] Fatal error: failed to create window" << endl;
		glfwTerminate();
		return NULL;
	}
	glfwMakeContextCurrent(window);

	//Sync with refresh rate
	glfwSwapInterval(1);

	if(glewInit() != GLEW_OK){
		cerr << "[GLEW] Fatal error: failed to load!" << endl;
		return NULL;
	}
	cout << "[GLEW] Using version: " << glewGetString(GLEW_VERSION) << endl;
	return window;
}


void Move(float *vertices, float &velx, float &vely){

	// Move x positions
	vertices[V1_X] += velx; vertices[V2_X] += velx; vertices[V3_X] += velx; vertices[V4_X] += velx;
	if(vertices[V1_X] <= -1.0f || vertices[V2_X] <= -1.0f || vertices[V3_X] <= -1.0f || vertices[V4_X] <= -1.0f)   velx = fabs(velx);
	else if(vertices[V1_X] >= 1.0f || vertices[V2_X] >= 1.0f || vertices[V3_X] >= 1.0f || vertices[V4_X] >= 1.0f) velx = -fabs(velx);

	// Move y positions
	vertices[V1_Y] += vely; vertices[V2_Y] += vely; vertices[V3_Y] += vely; vertices[V4_Y] += vely;
	if(vertices[V1_Y] <= -1.0f || vertices[V2_Y] <= -1.0f || vertices[V3_Y] <= -1.0f || vertices[V4_Y] <= -1.0f)   vely = fabs(vely);
	else if(vertices[V1_Y] >= 1.0f || vertices[V2_Y] >= 1.0f || vertices[V3_Y] >= 1.0f || vertices[V4_Y] >= 1.0f) vely = -fabs(vely);
}



int main(int argc, char **argv)
{

	GLFWwindow *window;
	if( !(window = gl_begin()) ){
		return (-1);
	}

	#define NUM 100

	srand(time(nullptr));

	GLuint indices[] = { 0, 1, 2,   // Triangle 1
						2, 3, 0 };	// Triangle 2

	float vertices[] = {
        // positions    // texture coords
		-0.1f, -0.1f,   1.0f, 1.0f,
		 0.1f, -0.1f,   0.0f, 1.0f,
		 0.1f,  0.1f,   0.0f, 0.0f,
		-0.1f,  0.1f,   1.0f, 0.0f
	};

	float vdata[NUM][16];
	float velx[NUM], vely[NUM];

	for(int i=0; i!=NUM; ++i){
		copy(vertices, vertices+16, vdata[i]);
		velx[i] = float((rand()%700)-300)/100.0f/100.0f;
		vely[i] = float((rand()%700)-300)/100.0f/100.0f;
	}

	struct texture_t texture;
    texture_init(&texture, "res/test.jpg");
    texture_bind(texture.id);

    struct shader_t shader;
	shader_init(shader, "res/vertex.shader", "res/fragment.shader");
	shader_bind(&shader);


	struct shape_t shapes[NUM];
	for(int i=0; i!=NUM; ++i){
		shape_init(shapes[i], vdata[i], indices);
	}

	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		for(int i=0; i!=NUM; ++i){
			Move(vdata[i], velx[i], vely[i]);
			shape_bind(shapes[i]);
			shape_update(shapes[i], vdata[i]);
			GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Deleting everything
	shape_unbind();
	texture_unbind();
	shader_unbind();

    shader_delete(shader);
    glDeleteTextures(1, &texture.id);

	for(int i=0; i!=NUM; ++i) shape_free(shapes[i]);

	glfwTerminate();

	return 0;
}
