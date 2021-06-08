/**************************************************************

   grid.cpp - Simple test grid

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#define SDL_MAIN_HANDLED
#define NUM_GRIDS 2

#include <SDL2/SDL.h>

typedef struct grid_display
{
	int index;
	int width;
	int height;

	SDL_Window *window;
	SDL_Renderer *renderer;
} GRID_DISPLAY;

//============================================================
//  draw_grid
//============================================================

void draw_grid(int num_grid, int width, int height, SDL_Renderer *renderer)
{
	// Clean the surface
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_Rect rect {0, 0, width, height};

	switch (num_grid)
	{
		case 0:
			// 16 x 12 squares
			{
				// Fill the screen with red
				rect = {0, 0, width, height};
				SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
				SDL_RenderFillRect(renderer, &rect);

				// Draw white rectangle
				rect = {width / 32, height / 24 , width - width / 16, height - height / 12};
				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
				SDL_RenderFillRect(renderer, &rect);

				// Draw grid using black rectangles
				SDL_Rect rects[16 * 12];

				// Set the thickness of horizontal and vertical lines based on the screen resolution
				int line_w = round(float(width) / 320.0);
				int line_h = round(float(height) / 240.0);
				if ( line_w < 1 ) line_w = 1;
				if ( line_h < 1 ) line_h = 1;

				float rect_w = (width - line_w * 17) / 16.0;
				float rect_h = (height - line_h * 13) / 12.0;

				for (int i = 0; i < 16; i++)
				{
					int x_pos1 = ceil(i * rect_w);
					int x_pos2 = ceil((i+1) * rect_w);
					for (int j = 0; j < 12; j++)
					{
						int y_pos1 = ceil(j * rect_h);
						int y_pos2 = ceil((j+1) * rect_h);
						rects[i + j * 16] = {x_pos1 + (i+1) * line_w , y_pos1 + (j+1) * line_h, x_pos2 - x_pos1, y_pos2 - y_pos1};
					}
				}

				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
				SDL_RenderFillRects(renderer, rects, 16 * 12);
			}
			break;

		case 1:
			// cps2 grid

			// Draw outer rectangle
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
			SDL_RenderDrawRect(renderer, &rect);

			for (int i = 0;  i < width / 16; i++)
			{
				for (int j = 0; j < height / 16; j++)
				{
					if (i == 0 || j == 0 || i == (width / 16) - 1 || j == (height / 16) - 1)
						SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
					else
						SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

					rect = {i * 16, j * 16, 16, 16};
					SDL_RenderDrawRect(renderer, &rect);

					rect = {i * 16 + 7, j * 16 + 7, 2, 2};
					SDL_RenderDrawRect(renderer, &rect);
				}
			}
			break;
	}

	SDL_RenderPresent(renderer);
}

//============================================================
//  main
//============================================================

int main(int argc, char **argv)
{
	SDL_Window* win_array[10] = {};
	GRID_DISPLAY display_array[10] = {};
	int display_total = 0;

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		printf("error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}

	// Get target displays
	if (argc > 1)
	{
		// Parse command line for display indexes
		int display_index = 0;
		int num_displays = SDL_GetNumVideoDisplays();

		for (int arg = 1; arg < argc; arg++)
		{
			sscanf(argv[arg], "%d", &display_index);

			if (display_index < 0 || display_index > num_displays - 1)
			{
				printf("error, bad display_index: %d\n", display_index);
				return 1;
			}

			display_array[display_total].index = display_index;
			display_total++;
		}
	}
	else
	{
		// No display specified, use default
		display_array[0].index = 0;
		display_total = 1;
	}

	// Create windows
	for (int disp = 0; disp < display_total; disp++)
	{
		// Get target display size
		SDL_DisplayMode dm;
		SDL_GetCurrentDisplayMode(display_array[disp].index, &dm);

		SDL_ShowCursor(SDL_DISABLE);

		display_array[disp].width = dm.w;
		display_array[disp].height = dm.h;

		// Create window
		display_array[disp].window = SDL_CreateWindow("Switchres test grid", SDL_WINDOWPOS_CENTERED_DISPLAY(display_array[disp].index), SDL_WINDOWPOS_CENTERED, dm.w, dm.h, SDL_WINDOW_FULLSCREEN_DESKTOP);

		// Required by Window multi-monitor
		SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

		// Create renderer
		display_array[disp].renderer = SDL_CreateRenderer(display_array[disp].window, -1, SDL_RENDERER_ACCELERATED);

		// Draw grid
		draw_grid(0, display_array[disp].width, display_array[disp].height, display_array[disp].renderer);
	}

	// Wait for escape key
	bool close = false;
	int  num_grid = 0;

	while (!close)
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					close = true;
					break;

				case SDL_KEYDOWN:
					switch (event.key.keysym.scancode)
					{
						case SDL_SCANCODE_ESCAPE:
							close = true;
							break;

						case SDL_SCANCODE_TAB:
							num_grid ++;
							for (int disp = 0; disp < display_total; disp++)
								draw_grid(num_grid % NUM_GRIDS, display_array[disp].width, display_array[disp].height, display_array[disp].renderer);
							break;

						default:
							break;
					}
			}
		}
	}

	// Destroy all windows
	for (int disp = 0; disp < display_total; disp++)
		SDL_DestroyWindow(display_array[disp].window);

	SDL_Quit();

	return 0;
}
