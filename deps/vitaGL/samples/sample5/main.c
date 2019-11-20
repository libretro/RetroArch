// Drawing a rotating cube

#include <vitaGL.h>
#include <math.h>

float colors[] = {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0}; // Colors for a face

float vertices_front[] = {-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f}; // Front Face
float vertices_back[] = {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // Back Face
float vertices_left[] = {-0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f}; // Left Face
float vertices_right[] = {0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // Right Face
float vertices_top[] = {-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f}; // Top Face
float vertices_bottom[] = {-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // Bottom Face

uint16_t indices[] = {
	 0, 1, 2, 1, 2, 3, // Front
	 4, 5, 6, 5, 6, 7, // Back
	 8, 9,10, 9,10,11, // Left
	12,13,14,13,14,15, // Right
	16,17,18,17,18,19, // Top
	20,21,22,21,22,23  // Bottom
};

void init_perspective(float fov, float aspect, float near, float far){
	float half_height = near * tanf(((fov * M_PI) / 180.0f) * 0.5f);
	float half_width = half_height * aspect;
	
	glFrustum(-half_width, half_width, -half_height, half_height, near, far);
}

int main(){
	
	// Initializing graphics device
	vglInit(0x800000);
	vglWaitVblankStart(GL_TRUE);
	
	// Creating colors array
	float color_array[12*6];
	int i;
	for (i=0;i<12*6;i++){
		color_array[i] = colors[i % 12];
	}
	
	// Creating vertices array
	float vertex_array[12*6];
	memcpy(&vertex_array[12*0], &vertices_front[0], sizeof(float) * 12);
	memcpy(&vertex_array[12*1], &vertices_back[0], sizeof(float) * 12);
	memcpy(&vertex_array[12*2], &vertices_left[0], sizeof(float) * 12);
	memcpy(&vertex_array[12*3], &vertices_right[0], sizeof(float) * 12);
	memcpy(&vertex_array[12*4], &vertices_top[0], sizeof(float) * 12);
	memcpy(&vertex_array[12*5], &vertices_bottom[0], sizeof(float) * 12);
	
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	init_perspective(90.0f, 960.f/544.0f, 0.01f, 100.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -3.0f); // Centering the cube
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	for (;;){
		vglStartRendering();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vertex_array);
		glColorPointer(3, GL_FLOAT, 0, color_array);
		glRotatef(1.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(0.5f, 0.0f, 1.0f, 0.0f);
		glDrawElements(GL_TRIANGLES, 6*6, GL_UNSIGNED_SHORT, indices);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		vglStopRendering();
	}
	
	vglEnd();
	
}