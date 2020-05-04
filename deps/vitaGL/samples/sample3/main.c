// Drawing a colored quad with glBegin/glEnd

#include <vitaGL.h>

int main(){
	
	// Initializing graphics device
	vglInit(0x800000);
	
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 960, 544, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	for (;;){
		vglStartRendering();
		glClear(GL_COLOR_BUFFER_BIT);
		glBegin(GL_QUADS);
		glColor3f(1.0, 0.0, 0.0);
		glVertex3f(400, 0, 0);
		glColor3f(1.0, 1.0, 0.0);
		glVertex3f(800, 0, 0);
		glColor3f(0.0, 1.0, 0.0);
		glVertex3f(800, 400, 0);
		glColor3f(1.0, 0.0, 1.0);
		glVertex3f(400, 400, 0);
		glEnd();
		vglStopRendering();
		glLoadIdentity();
	}
	
	vglEnd();
	
}
