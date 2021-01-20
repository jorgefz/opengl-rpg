
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

 - Learn quads
 - Batch rendering

 - Scale squares to be square with any window height/width
 - Draw tile grid

*/


#define assert(x) if(!(x)) exit(-1);

#define GLCall(x) do {\
		GLClearError();\
		x;\
		assert(GLCheckError(#x, __FILE__, __LINE__));\
	} while(0);


#define SCR_WIDTH 1280
#define SCR_HEIGHT 720

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


/*
Converts pixel data from RGB to RGBA.
Return data must have at least (total bytes + pixels) memory spaces.
*/
unsigned char* rgba(unsigned char *dest, unsigned char* data, int pixels, unsigned char alpha=255){
	unsigned char* ptr = data;
	unsigned char* cpy = dest;
	for(int p=0; p!=pixels; ++p){
		copy(ptr, ptr+3, cpy); // copy rgb
		ptr += 3;
		cpy += 3;
		*cpy = (unsigned char)alpha; // add alpha 0xFF
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


struct texture_t* texture_init(struct texture_t *tex, unsigned char* img_data, int height, int width, int channel_num=4){

	unsigned char rgba_data[height * width * 4];
	if(channel_num == 3){
		cout << "Warning: texture will be converted to RGBA" << endl;
		channel_num++;
		rgba(rgba_data, img_data, height*width);
    } else{
		copy(img_data, img_data + height*width*4, rgba_data);
    }

	tex->width = width;
	tex->height = height;
	tex->channel_num = channel_num;

	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->width, tex->height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data));
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
    //// TESTING

	unsigned char rgba_data[tex->height * tex->width * 4];
    if(tex->channel_num == 3){
		cout << "Warning: texture '" << _filepath << "' will be converted to RGBA" << endl;
		tex->channel_num++;
		rgba(rgba_data, img_data, tex->height * tex->width);
    } else{
		copy(img_data, img_data + tex->height * tex->width * 4, rgba_data);
    }

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

	window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT, "Test Window", NULL, NULL);
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


// Moves shape by dersired delta.
void shape_move(float *vertices, float delta_x, float delta_y){
	vertices[V1_X] += delta_x; vertices[V2_X] += delta_x; vertices[V3_X] += delta_x; vertices[V4_X] += delta_x;
	vertices[V1_Y] += delta_y; vertices[V2_Y] += delta_y; vertices[V3_Y] += delta_y; vertices[V4_Y] += delta_y;
}


void Move(float *vertices, float velx, float vely, bool bound_checking=true){
	// Move x positions
	vertices[V1_X] += velx; vertices[V2_X] += velx; vertices[V3_X] += velx; vertices[V4_X] += velx;
	// Move y positions
	vertices[V1_Y] += vely; vertices[V2_Y] += vely; vertices[V3_Y] += vely; vertices[V4_Y] += vely;

	if(!bound_checking) return;
	//Left wall
	if(vertices[V1_X]<= -1.0f) shape_move(vertices,-(vertices[V1_X]+1.0f), 0);
	//Right wall
	else if(vertices[V2_X] >= 1.0f) shape_move(vertices, -(vertices[V2_X]-1.0f), 0);

	//Lower wall
	if(vertices[V1_Y]<= -1.0f) shape_move(vertices, 0, -(vertices[V1_Y]+1.0f));
	//Upper wall
	else if(vertices[V4_Y] >= 1.0f) shape_move(vertices, 0, -(vertices[V4_Y]-1.0f));
}

/*
Converts pixel coordinates sand size of a square
to openGL formatted vertex data.
*/
float* generate_square_coords(float *data, int x, int y, int side){
	// Origin is in lower-left corner
	int pixel_coords[] = {
		x,		y,
		x+side, y,
		x+side, y+side,
		x,		y+side
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
        // positions    					 // texture coords
		vertex_coords[0], vertex_coords[1],   0.0f, 1.0f, //1.0f, 1.0f,
		vertex_coords[2], vertex_coords[3],   1.0f, 1.0f, //0.0f, 1.0f,
		vertex_coords[4], vertex_coords[5],   1.0f, 0.0f, //0.0f, 0.0f,
		vertex_coords[6], vertex_coords[7],   0.0f, 0.0f, //1.0f, 0.0f
	};
	copy(vertices, vertices+16, data);
	return data;
}



enum TileMapIndices {
	TILE_EMPTY = 0, TILE_DIRT, TILE_SAND, TILE_PATH, TILE_WATER
};


struct tilemap_t {
	int tile_size;	// Tile square side in pixels
	vector<texture_t> *imageatlas;
	int map_height, map_width;
	int *tile_grid;	// Which tile texture to place
	int *logic_grid;	// Collissions, boundaries, etc
};

// Read image atlas, retrieve tile textures, and return tile texture vector.
int read_tile_map(const char *atlas_filename,  unsigned char *tiles, int tile_side=8){
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
	int atlas_wtiles = width/tile_side;
	// Height of the atlas in bytes
	int atlas_hbytes = atlas_htiles * tile_bytes;
	// Width of the atlas in bytes
	int atlas_wbytes = atlas_wtiles * tile_bytes;

	// Copy to structured tiles
	unsigned char tile_data[atlas_wtiles*atlas_htiles][tile_bytes];
	int stride = width*channel_num; //between tile pixel lines
	int lsize = tile_side*channel_num; //size of tile pixel line in bytes

	unsigned char *ptr = img_data;
	unsigned char one_tile[tile_bytes];

	for(int tile=0; tile!=tile_num; ++tile){
		for(int line=0; line!=tile_side; ++line){
			copy(ptr, ptr+lsize, one_tile + line*lsize);
			ptr += stride; // jump pointer to next tile line
		}
		copy(one_tile, one_tile+tile_bytes, tile_data[tile]);
		if(tile%atlas_htiles){
			ptr = img_data + int(lsize * ceil(float(tile)/2.0f));
		}
	}
	stbi_image_free(img_data);
	//stbi_write_jpg("res/output_tile.jpg", tile_side, tile_side, channel_num, tile_data[2], 100);
	copy(tile_data[0], tile_data[atlas_wtiles*atlas_htiles], tiles);

	return tile_num;
}


int main(int argc, char **argv)
{

	GLFWwindow *window;
	if( !(window = gl_begin()) ){
		return (-1);
	}
	srand(time(nullptr));
	GLuint indices[] = { 0, 1, 2,   // Triangle 1
						 2, 3, 0 };	// Triangle 2
	//


	int tm_height=10, tm_width=10;
	int tile_map[tm_height][tm_width];
	for(int h=0; h!=tm_height; ++h){
		for(int w=0; w!=tm_width; ++w){
			tile_map[h][w] = rand() % 8;
			cout << " " << tile_map[h][w];
		}
		cout << endl;
	}

	unsigned char tiles[8][8*8*3];
	int tile_num = read_tile_map("res/tile_test2.jpg", &tiles[0][0]);
	cout << " Tile Num: " << tile_num << endl;

	struct texture_t tiles_tex[tile_num];
	for(int i=0; i!=tile_num; ++i){
		texture_init(&tiles_tex[i], &tiles[i][0], 8, 8, 3);
	}

	int side = 50;
	float vertices[tm_height][tm_width][16];
	for(int h=0; h!=tm_height; ++h){
		for(int w=0; w!=tm_width; ++w){
			generate_square_coords(vertices[h][w], side*w, side*h, side);
			}
	}
	struct shape_t shapes[tm_height][tm_width];
	for(int h=0; h!=tm_height; ++h){
		for(int w=0; w!=tm_width; ++w){
			shape_init(shapes[h][w], vertices[h][w], indices);
		}
	}

    struct shader_t shader;
	shader_init(shader, "res/vertex.shader", "res/fragment.shader");
	shader_bind(&shader);

	// Player
	struct texture_t ch_texture;
    texture_init(&ch_texture, &tiles[0][0], 8, 8, 3);
    texture_bind(ch_texture.id);

    float ch_vex[16];
    float ch_velx=0, ch_vely=0;
    generate_square_coords(ch_vex, SCR_WIDTH/2-25, SCR_HEIGHT/2-25, 50);
	struct shape_t character;
	shape_init(character, ch_vex, indices);


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

		for(int h=0; h!=tm_height; ++h){
			for(int w=0; w!=tm_width; ++w){
				Move(vertices[h][w], -ch_velx, -ch_vely, false);
				shape_bind(shapes[h][w]);
				shape_update(shapes[h][w], vertices[h][w]);
				texture_bind(tiles_tex[int(tile_map[h][w])].id);
				GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));
			}
		}

		shape_bind(character);
		texture_bind(ch_texture.id);
		shape_update(character, ch_vex);
		GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//Deleting everything
	shape_unbind();
	texture_unbind();
	shader_unbind();

    shader_delete(shader);
	for(int h=0; h!=tm_height; ++h){
		for(int w=0; w!=tm_width; ++w){
			shape_free(shapes[h][w]);
		}
	}
	for(int i=0; i!=tile_num; ++i){
		glDeleteTextures(1, &tiles_tex[i].id);
	}

	glDeleteTextures(1, &ch_texture.id);
	shape_free(character);

	glfwTerminate();

	return 0;
}
