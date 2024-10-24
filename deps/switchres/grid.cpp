/**************************************************************

   grid.cpp - Simple test grid

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2022 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse,
                         Radek Dutkiewicz

 **************************************************************/

// To add additional text lines:
// export GRID_TEXT="$(echo -e "\nArrows - screen position, Page Up/Down - H size\n\nH size: 1.0\nH position: 0\nV position: 0")"

// To remove additional lines:
// unset GRID_TEXT


#define SDL_MAIN_HANDLED
#define NUM_GRIDS 2

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "font.h"
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <math.h>

#define FONT_SIZE 11

enum GRID_ADJUST
{
	LEFT = 64,
	RIGHT,
	UP,
	DOWN,
	H_SIZE_INC,
	H_SIZE_DEC
};

typedef struct grid_display
{
	int index;
	int width;
	int height;

	SDL_Window *window;
	SDL_Renderer *renderer;
	std::vector<SDL_Texture*> textures;
} GRID_DISPLAY;

SDL_Surface *surface;
TTF_Font *font;
std::vector<std::string> grid_texts;


//============================================================
//  draw_grid
//============================================================

void draw_grid(int num_grid, int width, int height, SDL_Renderer *renderer, std::vector<SDL_Texture*> textures)
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

	// Compute text scaling factors
	int text_scale_w = std::max(1, (int)floor(width / 320 + 0.5));
	int text_scale_h = std::max(1, (int)floor(height / 240 + 0.5));

	SDL_Rect text_frame = {0, 0, 0, 0};

	// Compute text frame size
	for (SDL_Texture *t : textures)
	{
		int texture_width = 0;
		int texture_height = 0;

		SDL_QueryTexture(t, NULL, NULL, &texture_width, &texture_height);

		text_frame.w = std::max(text_frame.w, texture_width);
		text_frame.h += texture_height;
	}

	text_frame.w *= text_scale_w;
	text_frame.h *= text_scale_h;

	text_frame.w += FONT_SIZE * text_scale_w; // margin x
	text_frame.h += FONT_SIZE * text_scale_h; // margin y

	text_frame.x = ceil((width - text_frame.w ) / 2);
	text_frame.y = ceil((height - text_frame.h ) / 2);

	// Draw Text background
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(renderer, &text_frame);

	// Draw Text frame
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderDrawRect(renderer, &text_frame);

	// Draw text lines
	SDL_Rect text_rect = {0, text_frame.y + FONT_SIZE * text_scale_h / 2, 0, 0};

	for (SDL_Texture *t : textures)
	{
		int texture_width = 0;
		int texture_height = 0;

		SDL_QueryTexture(t, NULL, NULL, &texture_width, &texture_height);

		text_rect.w = texture_width *= text_scale_w;
		text_rect.h = texture_height *= text_scale_h;
		text_rect.x = ceil((width - text_rect.w ) / 2);

		SDL_RenderCopy(renderer, t, NULL, &text_rect);

		text_rect.y += text_rect.h;
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

	// Initialize text
	TTF_Init();
	SDL_RWops* font_resource = SDL_RWFromConstMem(ATTRACTPLUS_TTF, (sizeof(ATTRACTPLUS_TTF)) / (sizeof(ATTRACTPLUS_TTF[0])));
	font = TTF_OpenFontRW(font_resource, 1, FONT_SIZE);

	grid_texts.push_back(" "); // empty line for screen resolution

	char *grid_text = getenv("GRID_TEXT");

	if ( grid_text != NULL )
	{
		char* p = strtok(grid_text, "\n");
		while(p)
		{
			grid_texts.push_back(p);
			p = strtok(NULL, "\n");
		}
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
		SDL_RenderPresent(display_array[disp].renderer);

		// Render first text
		grid_texts[0] = "Mode: " + std::to_string(dm.w) + " x " + std::to_string(dm.h) + " @ " + std::to_string(dm.refresh_rate);

		for ( int i = 0; i < grid_texts.size(); i++ )
		{
			surface = TTF_RenderText_Solid(font, grid_texts[i].c_str(), {255, 255, 255});
			display_array[disp].textures.push_back(SDL_CreateTextureFromSurface(display_array[disp].renderer, surface));
		}

		// Draw grid
		draw_grid(0, display_array[disp].width, display_array[disp].height, display_array[disp].renderer, display_array[disp].textures);
	}




	// Wait for escape key
	bool close = false;
	int  num_grid = 0;
	int return_code = 0;
	int CTRL_modifier = 0;

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
					if (event.key.keysym.mod & KMOD_LCTRL || event.key.keysym.mod & KMOD_RCTRL)
						CTRL_modifier = 1<<7;

					switch (event.key.keysym.scancode)
					{
						case SDL_SCANCODE_ESCAPE:
						case SDL_SCANCODE_Q:
							close = true;
							return_code = 1;
							break;

						case SDL_SCANCODE_BACKSPACE:
						case SDL_SCANCODE_DELETE:
							close = true;
							return_code = 2;
							break;

						case SDL_SCANCODE_R:
							close = true;
							return_code = 3;
							break;

						case SDL_SCANCODE_RETURN:
						case SDL_SCANCODE_KP_ENTER:
							close = true;
							break;

						case SDL_SCANCODE_TAB:
							num_grid ++;
							for (int disp = 0; disp < display_total; disp++)
								draw_grid(num_grid % NUM_GRIDS, display_array[disp].width, display_array[disp].height, display_array[disp].renderer, display_array[disp].textures);
							break;

						case SDL_SCANCODE_LEFT:
							close = true;
							return_code = GRID_ADJUST::LEFT;
							break;

						case SDL_SCANCODE_RIGHT:
							close = true;
							return_code = GRID_ADJUST::RIGHT;
							break;

						case SDL_SCANCODE_UP:
							close = true;
							return_code = GRID_ADJUST::UP;
							break;

						case SDL_SCANCODE_DOWN:
							close = true;
							return_code = GRID_ADJUST::DOWN;
							break;

						case SDL_SCANCODE_PAGEUP:
							close = true;
							return_code = GRID_ADJUST::H_SIZE_INC;
							break;

						case SDL_SCANCODE_PAGEDOWN:
							close = true;
							return_code = GRID_ADJUST::H_SIZE_DEC;
							break;

						default:
							break;
					}

			}
		}
	}

	// Destroy font
	SDL_FreeSurface(surface);
	TTF_CloseFont(font);
	TTF_Quit();

	// Destroy all windows
	for (int disp = 0; disp < display_total; disp++)
	{
		for (SDL_Texture *t : display_array[disp].textures)
			SDL_DestroyTexture(t);
		SDL_DestroyWindow(display_array[disp].window);
	}

	SDL_Quit();

	return return_code | CTRL_modifier;
}
