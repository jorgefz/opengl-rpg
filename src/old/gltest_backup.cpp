


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



class Shader {
public:
	string vertexShader, fragmentShader, filepath;
	uint32_t program;
	GLuint uniform_loc;
	float color[4];

	void load(const string &infilepath){
		filepath = infilepath;
		Shader::parse();
		Shader::create();
	}

	void parse() {
		ifstream stream(filepath);
		string line;
		stringstream ss[2];

		enum class ShaderType{
			NONE = -1, VERTEX = 0, FRAGMENT = 1
		};

		ShaderType type = ShaderType::NONE;

		while(getline(stream, line)){

			if(line.find("#shader") != string::npos){

				if(line.find("vertex") != string::npos){
					type = ShaderType::VERTEX;
				}
				else if(line.find("fragment") != string::npos){
					type = ShaderType::FRAGMENT;
				}
			}
			else if (type != ShaderType::NONE){
				ss[int(type)] << line << '\n';
			}
		}

		vertexShader = ss[0].str();
		fragmentShader = ss[1].str();
	}

	uint32_t compile(uint32_t type){
		uint32_t id = glCreateShader(type);

		if(type == GL_VERTEX_SHADER){
			const char *src = vertexShader.c_str();
			glShaderSource(id, 1, &src, nullptr);
		} else {
			const char *src = fragmentShader.c_str();
			glShaderSource(id, 1, &src, nullptr);
		}

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
			return 0;
		}

		return id;
	}

	bool create() {
		program = glCreateProgram();
		uint32_t vs = Shader::compile(GL_VERTEX_SHADER);
		uint32_t fs = Shader::compile(GL_FRAGMENT_SHADER);

		if(vs == 0 || fs == 0){
			return false;
		}

		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);
		glValidateProgram(program);

		//Shader data has been copied to program and is no longer necessary.
		glDeleteShader(vs);
		glDeleteShader(fs);

		return true;
	}

	void use(){
		GLCall(glUseProgram(program));
	}

	void del(){
		glDeleteProgram(program);
	}

	void loadUniform(const char *varUniformName){
        uniform_loc = glGetUniformLocation(program, varUniformName);
        if(uniform_loc == -1){
            cout << "Shader: failed to load uniform '" << varUniformName << "'" << endl;
            exit(-1);
        }
    }

    void setColor(GLfloat r=1.0f, GLfloat g=1.0f, GLfloat b=1.0f, GLfloat a=1.0f){
        color[0]=r; color[1]=g; color[2]=b; color[3]=a;
    }

};



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

struct shape_t {
    GLuint vbo;		//Vertex buffer object
    GLuint vnum;	// Number of vertices
    GLuint sdims;	// Spatial dimensions (x,y, etc)
    GLuint tdims;	// Texture dimensions (s,t, etc)
    float *vertex_data;

    GLuint ibo;		// Index buffer object
	GLuint inum;	// Number of elements in VBO
    GLuint *index_data;

    struct texture_t *tex;
    Shader shader;
};


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



struct shape_t* shape_init(struct shape_t &shape, float* _vertex_data, GLuint* _index_data, GLuint _vnum=16, GLuint _inum=6, GLuint _sdims=2, GLuint _tdims=2) {

	shape.vertex_data = new float[_vnum];
	copy(_vertex_data, _vertex_data+_vnum, shape.vertex_data);

	shape.index_data = new GLuint[_inum];
	copy(_index_data, _index_data+_vnum, shape.index_data);

	shape.vnum = _vnum;
	shape.inum = _inum;
	shape.sdims = _sdims;
	shape.tdims = _tdims;

    glGenBuffers(1, &shape.vbo);
    glGenBuffers(1, &shape.ibo);

    glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);

    glBufferData(GL_ARRAY_BUFFER, shape.vnum*sizeof(float), &shape.vertex_data[0], GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.inum*sizeof(GLuint), &shape.index_data[0], GL_DYNAMIC_DRAW);

    // Telling OpenGL how to interpret data in buffer
    // Screen position data
    glEnableVertexAttribArray(0);
    glVertexAttribPointer( 0, shape.sdims, GL_FLOAT, GL_FALSE, (_sdims+_tdims)*sizeof(float), (void*)(0));
    // Texture position data
    glEnableVertexAttribArray(1);
    glVertexAttribPointer( 1, shape.tdims, GL_FLOAT, GL_FALSE, (_sdims+_tdims)*sizeof(float), (void*)(_sdims*sizeof(float)) );

    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return &shape;
}


void shape_bind_texture(Shader shader, GLuint texture_id){
	// Binding texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    //glUniform1i(glGetUniformLocation(shader.program, "u_Texture"), 0);
}


void shape_free(struct shape_t &shape){
	delete[] shape.vertex_data;
	delete[] shape.index_data;
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


int main(int argc, char **argv)
{

    int img_width, img_height, channelnum;
    unsigned char *img_data = stbi_load("res/test.jpg", &img_width, &img_height, &channelnum, 0);
    if(!img_data){
        cout << "Error: failed to load image" << endl;
        exit(-1);
    }


	GLFWwindow *window;
	if( !(window = gl_begin()) ){
		return (-1);
	}

	// Passing texture to OpenGL


	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data));
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(img_data);


    //struct texture_t tex;
    //texture_init(&tex, "res/test.jpg");
    //GLuint texture_id = tex.id;

	float vertices[] = {
        // positions    // texture coords
		-0.2f, -0.2f,   1.0f, 1.0f,
		 0.2f, -0.2f,   0.0f, 1.0f,
		 0.2f,  0.2f,   0.0f, 0.0f,
		-0.2f,  0.2f,   1.0f, 0.0f
	};

	GLuint indices[] = { 0, 1, 2,   // Triangle 1
                         2, 3, 0 };	// Triangle 2


    GLuint id, ibo;
    glGenBuffers(1, &id);
    glGenBuffers(1, &ibo);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    glBufferData(GL_ARRAY_BUFFER, 16*sizeof(float), vertices, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(GLuint), indices, GL_DYNAMIC_DRAW);

    // Telling OpenGL how to interpret data in buffer
    // Screen position data
    glEnableVertexAttribArray(0);
    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);

    // Texture position data
    glEnableVertexAttribArray(1);
    glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)) );


	//struct shape_t shape;
	//shape_init(shape, vertices, indices);

	//shape.tex = &tex;

    Shader shader;
    shader.load("res/basic_texture.shader");
    shader.use();
    //shader.loadUniform("u_Color");
    //shader.loadUniform("color");
    //shader.setColor(0.2f, 0.6f, 0.8f, 1.0f);

	//shape_apply_texture(shader, tex.id);

    // Binding texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glUniform1i(glGetUniformLocation(shader.program, "u_Texture"), 0);


	int i = 0;
	float velx = 0.008f;
	float vely = 0.015f;
	float r = 0.5f, g = 0.3f, b = 1.0f;
	float rvel = 0.005f, gvel = 0.003f, bvel = 0.004f;

	while( !glfwWindowShouldClose(window) ){

		glClear(GL_COLOR_BUFFER_BIT);

		// Update
		//shader.setColor(r, g, b, 1.0f);

		if(r >= 1.0f) rvel = -fabs(rvel);
		else if(r <= 0.0f) rvel = fabs(rvel);
		r += rvel*0.9;

		if(g >= 1.0f) gvel = -fabs(gvel);
		else if(g <= 0.0f) gvel = fabs(gvel);
		g += gvel*0.9;

		if(b >= 1.0f) bvel = -fabs(bvel);
		else if(b <= 0.0f) bvel = fabs(bvel);
		b += bvel*0.9;

		// Move x positions
		vertices[V1_X] += velx; vertices[V2_X] += velx; vertices[V3_X] += velx; vertices[V4_X] += velx;

		if(vertices[V1_X] <= -1.0f || vertices[V2_X] <= -1.0f || vertices[V4_X] <= -1.0f)   velx = fabs(velx);

		else if(vertices[V1_X] >= 1.0f || vertices[V2_X] >= 1.0f || vertices[V4_X] >= 1.0f) velx = -fabs(velx);

		// Move y positions
		vertices[V1_Y] += vely;
		vertices[V2_Y] += vely;
		vertices[V3_Y] += vely;
		vertices[V4_Y] += vely;

		if(vertices[V1_Y] <= -1.0f || vertices[V2_Y] <= -1.0f || vertices[V3_Y] <= -1.0f) vely = fabs(vely);

		else if(vertices[V1_Y] >= 1.0f || vertices[V2_Y] >= 1.0f || vertices[V3_Y] >= 1.0f) vely = -fabs(vely);

		glBufferData(GL_ARRAY_BUFFER, 16*sizeof(float), vertices, GL_DYNAMIC_DRAW);
		//GLCall(glUniform4f(shader.uniform_loc, shader.color[0], shader.color[1], shader.color[2], shader.color[3]));

		GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));

        i++;
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

    shader.del();

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDeleteTextures(1, &texture_id);

    //shape_free(shape);

	glfwTerminate();

	return 0;
}
