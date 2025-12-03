#include "characters.h"
#include "game_objects.h" // Needed to define prototypes that use these structs

#define MEM_VRAM        0x06000000
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   160

// OAM Tile indices for asteroids and bullets
#define TILE_ASTEROID_L     0   // 16x16 asteroid (large)
#define TILE_ASTEROID_M     1   // 16x16 asteroid (medium)
#define TILE_ASTEROID_S     2   // 8x8 asteroid (small)
#define TILE_BULLET         3   // 8x8 bullet

// OAM Size bits
#define ATTR1_SIZE_8        0x0000 // 8x8 size
#define ATTR1_SIZE_16       0x4000 // 16x16 size

// Color definitions
#define CLR_BLACK       0x0000
#define CLR_RED         0x001F
#define CLR_LIME        0x03E0
#define CLR_YELLOW      0x03FF
#define CLR_BLUE        0x7C00
#define CLR_MAG         0x7C1F
#define CLR_CYAN        0x7FE0
#define CLR_WHITE       0x7FFF

#define CHAR_PIX_SIZE   8
#define LINE_HEIGHT     12
#define NUM_CHARS_LINE  10

// Menu text positions
#define MENU_TEXT_X     ((SCREEN_WIDTH/2)-(CHAR_PIX_SIZE*NUM_CHARS_LINE/2))
#define MENU_TEXT_Y     (SCREEN_HEIGHT/2)-32
#define MENU_ITEM_1     MENU_TEXT_Y+(2*LINE_HEIGHT)
#define MENU_ITEM_2     MENU_ITEM_1+LINE_HEIGHT
// Add a third menu item position for extended menus
#define MENU_ITEM_3     MENU_ITEM_2+LINE_HEIGHT
#define MENU_ITEM_4     MENU_ITEM_3+LINE_HEIGHT

// Game Over/Pause text positions
#define END_TEXT_X      ((SCREEN_WIDTH/2)-(CHAR_PIX_SIZE*NUM_CHARS_LINE/2))
#define END_TEXT_Y      (SCREEN_HEIGHT/2)-16
#define NUM_LIVES_Y     (END_TEXT_Y+(2*LINE_HEIGHT))

// Scoreboard positions
#define SCORE_Y         10
#define PLAYER_SYM_Y    (SCREEN_HEIGHT-LINE_HEIGHT-SCORE_Y)

typedef u16             M3LINE[SCREEN_WIDTH];

// --- Double Buffering Declarations ---
// Declare the back buffer in RAM. Defined in graphics.c
extern M3LINE back_buffer[SCREEN_HEIGHT]; 

// Redefine m3_vram to point to the back buffer for all drawing operations.
// This is the simplest way to direct all existing drawing functions to RAM.
#define m3_vram         ((M3LINE*)back_buffer) 

// Menu structure definition
struct MenuScreen {
    int selection;
};

// --- Graphics Drawing Functions ---
void clearScreen();
void clearRegion(int x, int y, int w, int h);
void setPixel(int x, int y, u16 color); // Prototype added
// Draw a line between two points
void drawLine(int x0, int y0, int x1, int y1, u16 color);
void displayText(const char* text, int x, int y); 
void printChar(const bool char_map[64], int x, int y);
void clearMenu();
void flipBuffer(); // NEW: Function to copy back buffer to VRAM

// Game Object Drawing
void drawPlayerShip(GameObject *ship);
void drawAsteroid(Asteroid *asteroid);
void drawBullet(GameObject *bullet);
void drawScoreboard(int score, int lives, int highScore);
// Draw a circle perimeter (useful for showing respawn clear radius)
void drawCircle(int cx, int cy, int radius, u16 color);

// Debug visualization of collision circles
void drawCollisionCircles(GameObject *ship, Asteroid asteroids[], GameObject bullets[]);
