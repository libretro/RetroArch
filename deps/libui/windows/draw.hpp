// 5 may 2016

// draw.cpp
extern ID2D1Factory *d2dfactory;
struct uiDrawContext {
	ID2D1RenderTarget *rt;
	// TODO find out how this works
	std::vector<struct drawState> *states;
	ID2D1PathGeometry *currentClip;
};

// drawpath.cpp
extern ID2D1PathGeometry *pathGeometry(uiDrawPath *p);

// drawmatrix.cpp
extern void m2d(uiDrawMatrix *m, D2D1_MATRIX_3X2_F *d);
