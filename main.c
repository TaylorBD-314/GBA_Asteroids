#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <gba_sound.h>
#include <stdio.h>
#include <stdlib.h> // For rand() and srand()
#include <time.h>   // For time(NULL) seed
#include <string.h>
#include "graphics.h"
#include "game_objects.h"
#include "fixed_trig.h"
#include "save.h"
#include "sound.h"

// --- Constants ---
#define MENU_MODE        0
#define MATCH_MODE       1
#define RESET_MODE       2
#define PAUSE_MODE       3
#define CREDITS_MODE     4 
#define SETTINGS_MODE    5 

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define DAMAGE_DELAY 15
#define DEATH_DELAY 35
int DELAY = 0;
// Radius (in pixels) around the player spawn center to clear asteroids on respawn
#define RESPAWN_CLEAR_RADIUS 24

// Rainbow colors for the respawn circle animation
static const u16 rainbow_colors[] = {
    CLR_RED,    // Red
    CLR_YELLOW, // Yellow (R+G)
    CLR_LIME,   // Green
    CLR_CYAN,   // Cyan (G+B)
    CLR_BLUE,   // Blue
    CLR_MAG,    // Magenta (R+B)
};
#define RAINBOW_COLOR_COUNT 6

// Bouncing circles for game-over screen animation
typedef struct {
    int x, y;        // Position (in pixels)
    int vx, vy;      // Velocity (fixed-point or integer)
    int radius;      // Radius in pixels
    int colorIdx;    // Color cycle index
} BouncingCircle;

#define NUM_BOUNCING_CIRCLES 4
static BouncingCircle bouncing_circles[NUM_BOUNCING_CIRCLES];

// --- Menu floating outlined shapes (color-cycling, no gravity, bounce off walls) ---
#define NUM_MENU_SHAPES 5
typedef struct {
    int x, y;        // Position (pixels)
    int vx, vy;      // Velocity (pixels/frame)
    int radius;      // Size in pixels
    int sides;       // Number of polygon sides (3-6)
    int colorIdx;    // Fixed color index
    // Precomputed vertex offsets (relative to center) to avoid per-frame trig
    int vertexX[6];  // max 6 sides
    int vertexY[6];
} FloatingShape;

static FloatingShape menu_shapes[NUM_MENU_SHAPES];

// --- Main menu cursor animation state ---
static int menuCursorY = 0;
static int menuCursorTargetY = 0;
static int menuCursorAnim = 0;
static int menuCursorNeedsInit = 1;
static int menuPrevSelection = -1;
static int menuCursorStep = 5; // default pixels per frame

// Initialize menu shapes (called once when entering menu)
void initMenuShapes() {
    for (int i = 0; i < NUM_MENU_SHAPES; i++) {
        // Sizes and sides first (used for clamping positions)
        menu_shapes[i].radius = 6 + (i * 5) % 13; // between 6 and ~18
        menu_shapes[i].sides = 3 + (i % 4); // 3..6
        // Assign a fixed color index for each shape (unique across shapes)
        menu_shapes[i].colorIdx = i % RAINBOW_COLOR_COUNT;
        // Precompute vertex offsets for a non-rotating regular polygon
        for (int v = 0; v < menu_shapes[i].sides; v++) {
            int deg = (v * 360 / menu_shapes[i].sides) % 360;
            int cos_fp = cos_fp_deg(deg);
            int sin_fp = sin_fp_deg(deg);
            menu_shapes[i].vertexX[v] = (cos_fp * menu_shapes[i].radius) >> FP_SHIFT;
            menu_shapes[i].vertexY[v] = (sin_fp * menu_shapes[i].radius) >> FP_SHIFT;
        }

        // Spread shapes evenly across the screen horizontally with a small jitter
        int spacingX = SCREEN_WIDTH / (NUM_MENU_SHAPES + 1);
        int jitterX = ((i % 3) - 1) * 3; // -3,0,3
        menu_shapes[i].x = spacingX * (i + 1) + jitterX;
        // Vertical start: spread down the screen evenly with a small jitter
        int spacingY = SCREEN_HEIGHT / (NUM_MENU_SHAPES + 1);
        int jitterY = (i % 2) ? 3 : -3;
        menu_shapes[i].y = spacingY * (i + 1) + jitterY;

        // Clamp initial positions to keep shapes fully on-screen
        if (menu_shapes[i].x < menu_shapes[i].radius) menu_shapes[i].x = menu_shapes[i].radius + 1;
        if (menu_shapes[i].x > SCREEN_WIDTH - menu_shapes[i].radius - 1) menu_shapes[i].x = SCREEN_WIDTH - menu_shapes[i].radius - 1;
        if (menu_shapes[i].y < menu_shapes[i].radius) menu_shapes[i].y = menu_shapes[i].radius + 1;
        if (menu_shapes[i].y > SCREEN_HEIGHT - menu_shapes[i].radius - 1) menu_shapes[i].y = SCREEN_HEIGHT - menu_shapes[i].radius - 1;

        // Faster velocities (larger magnitude than before) with some variation
        int vx = ((i % 3) - 1) * 2; // -2,0,2
        if (vx == 0) vx = (i % 2) ? 2 : -2; // avoid zero, use +/-2
        menu_shapes[i].vx = vx;
        int vy = ((i + 1) % 2) ? 2 : -2; // alternating up/down, magnitude 2
        menu_shapes[i].vy = vy;
    }
}

// Update menu shapes positions and bounce off walls
void updateMenuShapes() {
    for (int i = 0; i < NUM_MENU_SHAPES; i++) {
        FloatingShape *s = &menu_shapes[i];
        s->x += s->vx;
        s->y += s->vy;

        // Bounce on X
        if (s->x - s->radius <= 0) { s->x = s->radius; s->vx = -s->vx; }
        if (s->x + s->radius >= SCREEN_WIDTH) { s->x = SCREEN_WIDTH - s->radius; s->vx = -s->vx; }

        // Bounce on Y across the full screen height
        int bottom = SCREEN_HEIGHT - 1; // last visible scanline
        if (s->y - s->radius <= 0) { s->y = s->radius; s->vy = -s->vy; }
        if (s->y + s->radius >= bottom) { s->y = bottom - s->radius; s->vy = -s->vy; }

        // No rotation: nothing to update per-frame beyond position
    }
}

// Draw regular polygon outline for each shape
void drawMenuShapes() {
    for (int i = 0; i < NUM_MENU_SHAPES; i++) {
        FloatingShape *s = &menu_shapes[i];
        int sides = s->sides;
        int prev_x = 0, prev_y = 0, first_x = 0, first_y = 0;
        for (int v = 0; v < sides; v++) {
            int px = s->x + s->vertexX[v];
            int py = s->y + s->vertexY[v];
            if (v == 0) {
                first_x = px; first_y = py;
            } else {
                // draw line from prev to current
                drawLine(prev_x, prev_y, px, py, rainbow_colors[s->colorIdx % RAINBOW_COLOR_COUNT]);
            }
            prev_x = px; prev_y = py;
        }
        // close polygon
        drawLine(prev_x, prev_y, first_x, first_y, rainbow_colors[s->colorIdx % RAINBOW_COLOR_COUNT]);
    }
}

// --- Save notification helper ---
// Shorten generic notification to ~1/4 of previous duration (120 -> 30)
#define SAVE_NOTIFY_DELAY 15
// Make GAME OVER notification linger a bit longer for visibility
#define SAVE_NOTIFY_GO_DELAY 45
static int save_notify_counter = 0;
static int save_notify_ok = 0;
static int save_notify_gameover = 0; // when set, change text for game-over save
static int save_notify_deleted = 0;   // when set, show "DELETED!" on success
// Track the high score value at the start of each match to know if it was increased.
static int initialHighScore = 0;

void setSaveNotification(int ok) {
    save_notify_ok = ok;
    save_notify_counter = SAVE_NOTIFY_DELAY;
    save_notify_gameover = 0;
    save_notify_deleted = 0;
}

// Special-case notification used when saving at GAME OVER so the message
// can be different (e.g. "Hi Score Saved!").
void setSaveNotificationGameOver(int ok) {
    save_notify_ok = ok;
    save_notify_counter = SAVE_NOTIFY_GO_DELAY;
    save_notify_gameover = 1;
    save_notify_deleted = 0;
}

// Special-case notification used when deleting high score in settings
void setSaveNotificationDeleted(int ok) {
    save_notify_ok = ok;
    save_notify_counter = SAVE_NOTIFY_DELAY;
    save_notify_deleted = 1;
    save_notify_gameover = 0;
}

void maybeDrawSaveNotification(void) {
    if (save_notify_counter > 0) {
        if (save_notify_ok) {
            if (save_notify_gameover) {
                const char *text = "HIGH SCORE SAVED!";
                int text_width = strlen(text) * CHAR_PIX_SIZE;
                int centered_x = (SCREEN_WIDTH - text_width) / 2;
                displayText(text, centered_x, END_TEXT_Y + (4 * LINE_HEIGHT) + 16);
            } else if (save_notify_deleted) {
                const char *text = "DELETED!";
                int text_width = strlen(text) * CHAR_PIX_SIZE;
                int centered_x = (SCREEN_WIDTH - text_width) / 2;
                displayText(text, centered_x, END_TEXT_Y + (4 * LINE_HEIGHT) + 16);
            } else {
                // Center "SAVE SUCCESSFUL" horizontally
                const char *text = "SAVE SUCCESSFUL";
                int text_width = strlen(text) * CHAR_PIX_SIZE;
                int centered_x = (SCREEN_WIDTH - text_width) / 2;
                displayText(text, centered_x, END_TEXT_Y + (4 * LINE_HEIGHT) + 16);
            }
        } else {
            // Check if this is a "no save data" notification (indicated by all flags being 0)
            if (!save_notify_gameover && !save_notify_deleted) {
                const char *text = "...NO SAVE DATA...";
                int text_width = strlen(text) * CHAR_PIX_SIZE;
                int centered_x = (SCREEN_WIDTH - text_width) / 2;
                displayText(text, centered_x, END_TEXT_Y + (4 * LINE_HEIGHT) + 16);
            } else {
                // Center "SAVE FAILED" horizontally
                const char *text = "SAVE FAILED";
                int text_width = strlen(text) * CHAR_PIX_SIZE;
                int centered_x = (SCREEN_WIDTH - text_width) / 2;
                displayText(text, centered_x, END_TEXT_Y + (4 * LINE_HEIGHT) + 16);
            }
        }
        save_notify_counter--;
        if (save_notify_counter == 0) { save_notify_gameover = 0; save_notify_deleted = 0; }
    }
}

GameObject playerShip;
Asteroid asteroids[MAX_ASTEROIDS];
GameObject bullets[MAX_BULLETS];

// --- EXTERNAL GAME LOGIC DECLARATIONS (Defined in game_logic.c) ---
extern void setupMatch(GameObject *ship, Asteroid asteroids[], GameObject bullets[],int *score, int *lives);
extern void updatePlayer(GameObject *ship, u16 keys);
extern void spawnBullet(GameObject bullets[], GameObject *ship);
extern void updateBullets(GameObject bullets[]);
extern void updateAsteroids(Asteroid asteroids[]);
extern void manageAsteroidSpawning(GameObject *ship, Asteroid asteroids[]);
extern void handleCollisions(GameObject *ship, Asteroid asteroids[], GameObject bullets[], int *lives, int *score);

// --- Function Prototypes ---
void creditsMode(bool *menuVisible, int *gameMode);

// Initialize bouncing circles for game-over animation
void initBouncingCircles() {
    int radii[] = { 8, 12, 6, 10 };
    int startX[] = { 40, 140, 200, 100 };
    for (int i = 0; i < NUM_BOUNCING_CIRCLES; i++) {
        bouncing_circles[i].x = startX[i];
        bouncing_circles[i].y = 20 + (i * 10);
        bouncing_circles[i].vx = 1 + (i % 2) * 2; // 1 or 3 px/frame
        bouncing_circles[i].vy = 0;
        bouncing_circles[i].radius = radii[i];
        bouncing_circles[i].colorIdx = i; // Stagger color cycles
    }
}

// Update bouncing circles physics (gravity, bounce, wrap)
void updateBouncingCircles() {
    for (int i = 0; i < NUM_BOUNCING_CIRCLES; i++) {
        BouncingCircle *c = &bouncing_circles[i];
        
        // Apply gravity
        c->vy += 1;
        
        // Update position
        c->x += c->vx;
        c->y += c->vy;
        
        // Bounce off bottom
        if (c->y + c->radius >= SCREEN_HEIGHT) {
            c->y = SCREEN_HEIGHT - c->radius;
            c->vy = -c->vy; // Reverse and dampen
            c->vy = (c->vy * 9) / 10; // 90% bounce
        }
        
        // Bounce off top
        if (c->y - c->radius <= 0) {
            c->y = c->radius;
            c->vy = -c->vy;
        }
        
        // Bounce off right side
        if (c->x + c->radius >= SCREEN_WIDTH) {
            c->x = SCREEN_WIDTH - c->radius;
            c->vx = -c->vx;
        }
        
        // Bounce off left side
        if (c->x - c->radius <= 0) {
            c->x = c->radius;
            c->vx = -c->vx;
        }
        
        // Cycle color
        c->colorIdx++;
    }
}

// Draw bouncing circles
void drawBouncingCircles() {
    for (int i = 0; i < NUM_BOUNCING_CIRCLES; i++) {
        BouncingCircle *c = &bouncing_circles[i];
        u16 color = rainbow_colors[c->colorIdx % RAINBOW_COLOR_COUNT];
        drawCircle(c->x, c->y, c->radius, color);
    }
}


// --- Menu Functions ---

/**
 * Sets the menu cursor position based on the current selection.
 * Uses the selector arrow from the graphics/characters data.
 */
void setMenuCursor(int selection) {
    // Compute centered title X and position cursor relative to it
    int titleX = (SCREEN_WIDTH - (strlen("GBA ASTEROIDS") * CHAR_PIX_SIZE)) / 2;
    int cursorX = titleX + CHAR_PIX_SIZE;
    int selY = MENU_ITEM_1 + (selection * LINE_HEIGHT);

    // Initialize on first entry to menu
    if (menuCursorNeedsInit || menuCursorY == 0) {
        menuCursorY = selY;
        menuCursorTargetY = selY;
        menuCursorAnim = 0;
        menuPrevSelection = selection;
        menuCursorNeedsInit = 0;
        menuCursorStep = 5; // reset to default speed on entry
    }

    // Start animation when selection changes
    if (selection != menuPrevSelection) {
        // Use faster speed when wrapping between first and last items
        int wrapMove = ((menuPrevSelection == 3 && selection == 0) ||
                        (menuPrevSelection == 0 && selection == 3));
        menuCursorStep = wrapMove ? 10 : 5;
        menuCursorTargetY = selY;
        menuCursorAnim = 1;
        menuPrevSelection = selection;
    }

    // Step animation if active
    if (menuCursorAnim) {
        const int step = menuCursorStep; // pixels per frame
        if (menuCursorY < menuCursorTargetY) {
            menuCursorY += step;
            if (menuCursorY > menuCursorTargetY) menuCursorY = menuCursorTargetY;
        } else if (menuCursorY > menuCursorTargetY) {
            menuCursorY -= step;
            if (menuCursorY < menuCursorTargetY) menuCursorY = menuCursorTargetY;
        }
        if (menuCursorY == menuCursorTargetY) menuCursorAnim = 0;
    }

    int drawY = menuCursorAnim ? menuCursorY : selY;
    printChar(selector[0], cursorX, drawY);
}

/**
 * Handles the main menu state and user interaction.
 */
void menuMode(bool *menuVisible, struct MenuScreen *mainMenu, int *gameMode,
    GameObject *ship, Asteroid asteroids[], GameObject bullets[], int *score, int *lives) {
    
    // Define a new, shifted X-position for the menu items
    #define MENU_ITEM_X_SHIFTED (MENU_TEXT_X + (2 * CHAR_PIX_SIZE))

    // Initialize shapes the first time we enter the menu
    if (!*menuVisible) {
        initMenuShapes();
        *menuVisible = true;
        // Reset cursor animation state on first frame entering the menu
        menuCursorNeedsInit = 1;
        // Start background music when entering menu
        initProceduralMusic();
    }

    // Clear menu area using optimized direct buffer writes (same as gameplay)
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            m3_vram[y][x] = CLR_BLACK;
        }
    }
    updateMenuShapes();
    drawMenuShapes();

    // Draw the static menu text (drawn each frame so it appears above shapes)
    int titleX = (SCREEN_WIDTH - (strlen("GBA ASTEROIDS") * CHAR_PIX_SIZE)) / 2;
    int itemX = titleX + (2 * CHAR_PIX_SIZE);
    displayText("GBA ASTEROIDS", titleX,   MENU_TEXT_Y);
    displayText(" NEW GAME ", itemX, MENU_ITEM_1);
    displayText(" CONTINUE ", itemX, MENU_ITEM_2);
    displayText(" SETTINGS ", itemX, MENU_ITEM_3);
    displayText(" CREDITS ", itemX, MENU_ITEM_4);
    setMenuCursor(mainMenu->selection);

    u16 keys_down = keysDown();
    bool selectionMade    = (keys_down & KEY_START) || (keys_down & KEY_A );
    bool changedSelection = (keys_down & KEY_DOWN)  || (keys_down & KEY_UP);

    if (selectionMade) {
        clearMenu(); // Clear menu text and cursor
        if (mainMenu->selection == 0) {
            // NEW GAME: Stop music first, then setup
            stopProceduralMusic(); // Stop menu music before starting game
            // Capture the high score at match start to detect increases by game over time
            initialHighScore = getHighScore();
            setupMatch(ship, asteroids, bullets, score, lives); // Initialize game objects
            *gameMode = MATCH_MODE; // Start the game
        }
        else if (mainMenu->selection == 1) { // CONTINUE
            if (hasSavedGame()) {
                stopProceduralMusic(); // Stop menu music before resuming game
                if (loadGameState(score, lives, ship, asteroids, bullets)) {
                    initialHighScore = getHighScore();
                    *gameMode = MATCH_MODE; // Resume saved game
                } else {
                    // Show error notification
                    setSaveNotification(0);
                }
            } else {
                // No save data - show notification using the notification system
                save_notify_counter = SAVE_NOTIFY_DELAY;
                save_notify_ok = 0;
                save_notify_gameover = 0;
                save_notify_deleted = 0;
            }
        }
        else if (mainMenu->selection == 2) { // SETTINGS
            *gameMode = SETTINGS_MODE;
        }
        else if (mainMenu->selection == 3) { // CREDITS
            *gameMode = CREDITS_MODE;
        }
    } else if (changedSelection) {
        // Change selection up/down across four items
        if (keys_down & KEY_DOWN) {
            mainMenu->selection = (mainMenu->selection + 1) % 4;
            playMenuSelectSound();
        } else if (keys_down & KEY_UP) {
            mainMenu->selection = (mainMenu->selection + 3) % 4; // -1 mod 4
            playMenuSelectSound();
        }
        setMenuCursor(mainMenu->selection);
    }

    // Draw any transient notifications (e.g., save result)
    maybeDrawSaveNotification();

    // Copy the final frame from the back buffer to the visible VRAM
    flipBuffer();
}

/**
 * Handles the credits screen.
 */
void creditsMode(bool *menuVisible, int *gameMode) {
    // Clear the screen and redraw the text every frame to ensure it is visible.
    clearScreen();

    // Center the credits title text
    const char *creditsTitle = "MADE BY TAYLOR BOESE";
    int creditsTitleX = (SCREEN_WIDTH - (strlen(creditsTitle) * CHAR_PIX_SIZE)) / 2;
    displayText(creditsTitle, creditsTitleX, END_TEXT_Y - LINE_HEIGHT);

    // Center the MAIN MENU option along with its cursor as a pair
    const char *menuText = " MAIN MENU ";
    int menuTextWidth = strlen(menuText) * CHAR_PIX_SIZE;
    int pairWidth = CHAR_PIX_SIZE + menuTextWidth; // cursor + text
    int pairLeftX = (SCREEN_WIDTH - pairWidth) / 2;
    int menuTextX = pairLeftX + CHAR_PIX_SIZE;
    int menuY = END_TEXT_Y + (2 * LINE_HEIGHT);
    displayText(menuText, menuTextX, menuY);
    printChar(selector[0], pairLeftX, menuY);

    // Always mark as visible since we are in this mode
    *menuVisible = true; 

    u16 keys_down = keysDown();

    // Check for START or A button to return to main menu
    if (keys_down & KEY_START || keys_down & KEY_A) {
        clearScreen();
        *menuVisible = false;
        *gameMode = MENU_MODE;
    }
    
    // Copy the final frame from the back buffer to the visible VRAM
    flipBuffer();
}

/**
 * Handles the settings screen.
 * Shows a single option: DELETE HIGH SCORE, with the standard selector cursor.
 * A: Delete (set high score to 0 and save), START/B: Return to Main Menu.
 */
void settingsMode(bool *menuVisible, int *gameMode) {
    static int confirmMode = 0;      // 0 = normal list, 1 = confirming delete
    static int confirmSelection = 0; // 0 = NO, 1 = YES
    static int settingsSelection = 0; // 0 = DELETE HIGH SCORE, 1 = MAIN MENU
    // Cursor animation state for normal settings view
    static int cursorYSettings = 0;         // current Y position for cursor animation
    static int cursorTargetYSettings = 0;   // target Y position for cursor animation
    static int animSettingsActive = 0;      // 1 while animating
    // Cursor animation state for confirmation view
    static int cursorYConfirm = 0;
    static int cursorTargetYConfirm = 0;
    static int animConfirmActive = 0;
    // Clear screen using optimized direct buffer writes (same as gameplay)
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            m3_vram[y][x] = CLR_BLACK;
        }
    }

    // Title position (centered)
    int titleX = (SCREEN_WIDTH - (strlen("SETTINGS") * CHAR_PIX_SIZE)) / 2;

    displayText("SETTINGS", titleX, MENU_TEXT_Y);

    u16 keys_down = keysDown();

    if (!confirmMode) {
        // Normal view: two options, each centered with cursor as a pair
        // 1) DELETE HIGH SCORE
        const char *delTxt = " DELETE HIGH SCORE ";
        int delTextWidth = strlen(delTxt) * CHAR_PIX_SIZE;
        int delPairWidth = CHAR_PIX_SIZE + delTextWidth; // cursor + text
        int delLeftX = (SCREEN_WIDTH - delPairWidth) / 2;
        int delTextX = delLeftX + CHAR_PIX_SIZE;
        displayText(delTxt, delTextX, MENU_ITEM_2);

        // 2) MAIN MENU
        const char *backTxt = " MAIN MENU ";
        int backTextWidth = strlen(backTxt) * CHAR_PIX_SIZE;
        int backPairWidth = CHAR_PIX_SIZE + backTextWidth;
        int backLeftX = (SCREEN_WIDTH - backPairWidth) / 2;
        int backTextX = backLeftX + CHAR_PIX_SIZE;
        displayText(backTxt, backTextX, MENU_ITEM_3);

        // Initialize cursor positions on first entry
        if (cursorYSettings == 0) {
            cursorYSettings = (settingsSelection == 0) ? MENU_ITEM_2 : MENU_ITEM_3;
            cursorTargetYSettings = cursorYSettings;
            animSettingsActive = 0;
        }

        // Draw cursor with simple vertical wrap animation
        int cursorDrawY = animSettingsActive ? cursorYSettings
                           : ((settingsSelection == 0) ? MENU_ITEM_2 : MENU_ITEM_3);
        int cursorDrawX = (cursorDrawY <= (MENU_ITEM_2 + MENU_ITEM_3) / 2) ? delLeftX : backLeftX;
        printChar(selector[0], cursorDrawX, cursorDrawY);

        // Navigation in normal view
        if ((keys_down & KEY_UP) || (keys_down & KEY_DOWN) || (keys_down & KEY_LEFT) || (keys_down & KEY_RIGHT)) {
            int prevY = (settingsSelection == 0) ? MENU_ITEM_2 : MENU_ITEM_3;
            settingsSelection ^= 1; // toggle between 0 and 1
            int newY = (settingsSelection == 0) ? MENU_ITEM_2 : MENU_ITEM_3;
            // Start animation from previous to new position
            if (!animSettingsActive) cursorYSettings = prevY;
            cursorTargetYSettings = newY;
            animSettingsActive = 1;
        }

        // Step animation if active
        if (animSettingsActive) {
            const int step = 8; // pixels per frame (even faster)
            if (cursorYSettings < cursorTargetYSettings) {
                cursorYSettings += step;
                if (cursorYSettings > cursorTargetYSettings) cursorYSettings = cursorTargetYSettings;
            } else if (cursorYSettings > cursorTargetYSettings) {
                cursorYSettings -= step;
                if (cursorYSettings < cursorTargetYSettings) cursorYSettings = cursorTargetYSettings;
            }
            if (cursorYSettings == cursorTargetYSettings) animSettingsActive = 0;
        }

        // Confirm selection
        if (keys_down & KEY_A) {
            if (settingsSelection == 0) {
                // DELETE HIGH SCORE -> enter confirmation
                confirmMode = 1;
                confirmSelection = 0; // default to NO
                // Initialize confirm cursor at NO line
                cursorYConfirm = MENU_ITEM_3;
                cursorTargetYConfirm = MENU_ITEM_3;
                animConfirmActive = 0;
            } else {
                // MAIN MENU -> return immediately
                *gameMode = MENU_MODE;
            }
        }

        // START/B still returns to main menu
        if ((keys_down & KEY_START) || (keys_down & KEY_B)) {
            *gameMode = MENU_MODE;
        }
    } else {
        // Confirmation view
        const char *q = "CONFIRM DELETE?";
        int qx = (SCREEN_WIDTH - (strlen(q) * CHAR_PIX_SIZE)) / 2;
        displayText(q, qx, MENU_ITEM_1);

        // Center YES and NO lines with their cursors as pairs
        const char *yesTxt = " YES ";
        int yesTextWidth = strlen(yesTxt) * CHAR_PIX_SIZE;
        int yesPairWidth = CHAR_PIX_SIZE + yesTextWidth; // cursor + text
        int yesLeftX = (SCREEN_WIDTH - yesPairWidth) / 2;
        int yesTextX = yesLeftX + CHAR_PIX_SIZE;
        displayText(yesTxt, yesTextX, MENU_ITEM_2);

        const char *noTxt = " NO ";
        int noTextWidth = strlen(noTxt) * CHAR_PIX_SIZE;
        int noPairWidth = CHAR_PIX_SIZE + noTextWidth;
        int noLeftX = (SCREEN_WIDTH - noPairWidth) / 2;
        int noTextX = noLeftX + CHAR_PIX_SIZE;
        displayText(noTxt, noTextX, MENU_ITEM_3);

        // Initialize confirm cursor position (if not set)
        if (cursorYConfirm == 0) {
            cursorYConfirm = (confirmSelection == 1) ? MENU_ITEM_2 : MENU_ITEM_3;
            cursorTargetYConfirm = cursorYConfirm;
            animConfirmActive = 0;
        }

        // Draw animated cursor for confirmation
        int cDrawY = animConfirmActive ? cursorYConfirm
                        : ((confirmSelection == 1) ? MENU_ITEM_2 : MENU_ITEM_3);
        int cDrawX = (cDrawY <= (MENU_ITEM_2 + MENU_ITEM_3) / 2) ? yesLeftX : noLeftX;
        printChar(selector[0], cDrawX, cDrawY);

        // Navigation: UP/DOWN toggles selection; LEFT/RIGHT also supported
        if ((keys_down & KEY_UP) || (keys_down & KEY_DOWN) || (keys_down & KEY_LEFT) || (keys_down & KEY_RIGHT)) {
            int prevY = (confirmSelection == 1) ? MENU_ITEM_2 : MENU_ITEM_3;
            confirmSelection ^= 1; // toggle between 0 and 1
            int newY = (confirmSelection == 1) ? MENU_ITEM_2 : MENU_ITEM_3;
            if (!animConfirmActive) cursorYConfirm = prevY;
            cursorTargetYConfirm = newY;
            animConfirmActive = 1;
        }

        // Step confirmation animation
        if (animConfirmActive) {
            const int step = 8; // pixels per frame (even faster)
            if (cursorYConfirm < cursorTargetYConfirm) {
                cursorYConfirm += step;
                if (cursorYConfirm > cursorTargetYConfirm) cursorYConfirm = cursorTargetYConfirm;
            } else if (cursorYConfirm > cursorTargetYConfirm) {
                cursorYConfirm -= step;
                if (cursorYConfirm < cursorTargetYConfirm) cursorYConfirm = cursorTargetYConfirm;
            }
            if (cursorYConfirm == cursorTargetYConfirm) animConfirmActive = 0;
        }
        // Confirm/cancel
        if (keys_down & KEY_A) {
            if (confirmSelection == 1) {
                // YES: delete, save, notify, then exit confirmation
                setHighScore(0);
                saveHighScore();
                setSaveNotificationDeleted(wasLastSaveOK());
            }
            // In both cases, leave confirm mode back to settings screen
            confirmMode = 0;
        }
        if (keys_down & KEY_B || keys_down & KEY_START) {
            // Cancel confirmation, return to settings screen
            confirmMode = 0;
        }
    }

    // Draw transient save notification (if any) and present
    maybeDrawSaveNotification();
    flipBuffer();
}

/**
 * Handles the main game logic loop (Match Mode).
 */
void matchMode(GameObject *ship, Asteroid asteroids[], GameObject bullets[],
    int *score, int *lives, int *gameMode) {
    
    // --- Speed Multiplier ---
    const int SPEED_MULTIPLIER = 1; 

    for (int i = 0; i < SPEED_MULTIPLIER; i++) {

        u16 keys_held = keysHeld();
        u16 keys_down = keysDown();
        
        // 1. INPUT & LOGIC UPDATES
        updatePlayer(ship, keys_held);

        if (keys_down & KEY_A) {
            spawnBullet(bullets, ship);
        }

        if (keys_down & KEY_START) {
            *gameMode = PAUSE_MODE;
            return; // Exit multiplier loop if paused
        }

        // 2. APPLY MOVEMENT (Ship wrapping is handled here for screen boundaries)
        ship->prevX = FP_TO_INT(ship->x);
        ship->prevY = FP_TO_INT(ship->y);

        // Apply Player Movement
        ship->x += ship->velocityX;
        ship->y += ship->velocityY;

        // Wrap Player around screen
        int x = FP_TO_INT(ship->x);
        int y = FP_TO_INT(ship->y);

        if (x < -ship->width) ship->x = INT_TO_FP(SCREEN_WIDTH);
        else if (x > SCREEN_WIDTH) ship->x = INT_TO_FP(-ship->width);

        if (y < -ship->height) ship->y = INT_TO_FP(SCREEN_HEIGHT);
        else if (y > SCREEN_HEIGHT) ship->y = INT_TO_FP(-ship->height);

        // Update Bullets and Asteroids
        updateBullets(bullets);
        updateAsteroids(asteroids);

        // This call must be added to spawn new asteroids over time.
        // Use the current match's ship pointer (passed into matchMode) instead of the global playerShip.
        manageAsteroidSpawning(ship, asteroids);

        // 3. COLLISION DETECTION
        handleCollisions(ship, asteroids, bullets, lives, score);

        // Update runtime high score immediately when beaten so the
        // in-game scoreboard shows the current session best.
        if (*score > getHighScore()) {
            setHighScore(*score);
        }

        // 4. DEATH/GAME OVER CHECK
        if (ship->isAlive == 0) { 
            *gameMode = RESET_MODE; // Transition to death delay/reset screen
            return; // Exit multiplier loop if ship is dead
        }
    } // End of SPEED_MULTIPLIER loop

    // 5. DRAWING
    // Clear the entire gameplay screen each frame (full height to catch objects at edges)
    // but use optimized row-by-row u32 writes instead of clearScreen()'s per-pixel approach
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            m3_vram[y][x] = CLR_BLACK;
        }
    }

    drawScoreboard(*score, *lives, getHighScore()); // Use the correct drawScoreboard function

    if (ship->isAlive) {
        drawPlayerShip(ship);
    }

    // Draw all active asteroids
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].obj.isAlive) {
            drawAsteroid(&asteroids[i]);
        }
    }

    // Draw all active bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isAlive) {
            drawBullet(&bullets[i]);
        }
    }
    
    // Draw collision circles for debugging
    //drawCollisionCircles(ship, asteroids, bullets);
    
    // Copy the final frame from the back buffer to the visible VRAM
    flipBuffer();
}


/**
 * The main game loop.
 */
int main(void) {

    // Interrupt handlers setup
    irqInit();
    irqEnable(IRQ_VBLANK);

    // Initialize sound system for sound effects
    REG_SOUNDCNT_X = 0x80; // Enable sound
    REG_SOUNDCNT_L = 0x7777; // Enable all channels left/right, full volume

    // --- Game Variables ---
    GameObject ship;
    Asteroid asteroids[MAX_ASTEROIDS];
    GameObject bullets[MAX_BULLETS];
    int score = 0, lives = 3; // Initialize default values
    // Load persisted high score from SRAM
    loadHighScore();

    // --- Menu Variables ---
    struct MenuScreen mainMenu;
    mainMenu.selection = 0;
    bool menuVisible = false;
    int gameMode = MENU_MODE;
    int resetCounter = 0;
    int colorCycleIndex = 0; // For animating the respawn circle color
    
    // --- Pause Menu Variables ---
    int pauseMenuSelection = 0; // 0 = SAVE, 1 = RESUME

    // Seed the random number generator
    srand(time(NULL));

    // Set GBA display to Mode 3 (240x160, 16-bit color, BG2 active)
    SetMode( MODE_3 | BG2_ON );

    // Main Game Loop
    while (1) {
        VBlankIntrWait(); // Synchronize screen updates
        scanKeys();
        
        if (gameMode == MENU_MODE) {
            // Update background music only in menu mode
            updateProceduralMusic();
            menuMode(&menuVisible, &mainMenu, &gameMode, &ship, asteroids, bullets, &score, &lives);

        } else if (gameMode == MATCH_MODE) {
            matchMode(&ship, asteroids, bullets, &score, &lives, &gameMode);

        } else if (gameMode == CREDITS_MODE) { // <--- CREDITS MODE HANDLER
            creditsMode(&menuVisible, &gameMode);

        } else if (gameMode == SETTINGS_MODE) { // <--- SETTINGS MODE HANDLER
            settingsMode(&menuVisible, &gameMode);

        } else if (gameMode == RESET_MODE) {
            // Death Delay / Game Over Screen
            resetCounter++;
            // Clear screen using optimized direct buffer writes (same as gameplay)
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    m3_vram[y][x] = CLR_BLACK;
                }
            }
            
            // Display "DANGER!" if lives remain, or "GAME OVER!" if not.
            if (lives > 0) {
                // Draw respawn-clear radius circle with animated rainbow color FIRST (behind text)
                int spawnCenterX = SCREEN_WIDTH / 2;
                int spawnCenterY = SCREEN_HEIGHT / 2;
                u16 circleColor = rainbow_colors[colorCycleIndex % RAINBOW_COLOR_COUNT];
                drawCircle(spawnCenterX, spawnCenterY, RESPAWN_CLEAR_RADIUS, circleColor);
                colorCycleIndex++; // Advance color for next frame

                // Center the DANGER! warning horizontally
                const char *dangerText = "DANGER!";
                int dangerWidth = strlen(dangerText) * CHAR_PIX_SIZE;
                int dangerX = (SCREEN_WIDTH - dangerWidth) / 2;
                displayText(dangerText, dangerX, END_TEXT_Y);

                // Display remaining lives (keep at original scoreboard area for clarity)
                char livesBuf[16];
                snprintf(livesBuf, sizeof(livesBuf), "LIVES LEFT: %d", lives);
                displayText(livesBuf, END_TEXT_X, NUM_LIVES_Y);

                DELAY = DAMAGE_DELAY;
            } else {
                displayText(" GAME OVER! ", END_TEXT_X, END_TEXT_Y);
                // Initialize bouncing circles and conditionally save on first frame of game-over
                if (resetCounter == 1) {
                    initBouncingCircles();
                    // Only persist and notify if high score increased during this match
                    if (getHighScore() > initialHighScore) {
                        saveHighScore();
                        setSaveNotificationGameOver(wasLastSaveOK());
                    }
                }
                // Update and draw bouncing circles
                updateBouncingCircles();
                drawBouncingCircles();
                DELAY = DEATH_DELAY;
            }

            // Draw any transient save notification (so it shows during GAME OVER)
            maybeDrawSaveNotification();
            // Copy the final frame from the back buffer to the visible VRAM
            flipBuffer();

            if (resetCounter > DELAY) {
                resetCounter = 0;
                clearScreen();

                // Re-spawn logic
                if (lives > 0) {
                        // Clear nearby asteroids around the default spawn center
                        int spawnCenterX = SCREEN_WIDTH / 2;
                        int spawnCenterY = SCREEN_HEIGHT / 2;
                        int r2 = RESPAWN_CLEAR_RADIUS * RESPAWN_CLEAR_RADIUS;
                        
                        for (int a = 0; a < MAX_ASTEROIDS; a++) {
                            if (asteroids[a].obj.isAlive) {
                                int ax = FP_TO_INT(asteroids[a].obj.x) + (asteroids[a].obj.width / 2);
                                int ay = FP_TO_INT(asteroids[a].obj.y) + (asteroids[a].obj.height / 2);
                                int dx = ax - spawnCenterX;
                                int dy = ay - spawnCenterY;
                                if (dx*dx + dy*dy <= r2) {
                                    asteroids[a].obj.isAlive = 0; // destroy asteroid near spawn
                                }
                            }
                        }
                        // SHIP REAPPEARS: Set isAlive and return to match
                        ship.isAlive = 1;
                        gameMode = MATCH_MODE;
                } else {
                    // FINAL GAME OVER: Reset and go to menu
                        menuVisible = false;
                        // High score already persisted when Game Over screen started
                        setupMatch(&ship, asteroids, bullets, &score, &lives);
                        gameMode = MENU_MODE;
                }
            }
          } else if (gameMode == PAUSE_MODE) {
                 u16 keys_down = keysDown();
                 
                 // Clear the pause menu area each frame to avoid cursor duplication
                 clearRegion(END_TEXT_X - CHAR_PIX_SIZE - 8, END_TEXT_Y, 14 * CHAR_PIX_SIZE, 5 * LINE_HEIGHT);
                 
                 // Handle selection navigation (UP/DOWN to move cursor, START or A to select)
                 if (keys_down & KEY_DOWN) {
                     pauseMenuSelection = (pauseMenuSelection + 1) % 3;
                 } else if (keys_down & KEY_UP) {
                     pauseMenuSelection = (pauseMenuSelection + 2) % 3; // -1 mod 3
                 }
                 
                 // Draw PAUSE text
                 displayText("  PAUSED!   ", END_TEXT_X, END_TEXT_Y);

                 // Draw menu options with cursor
                 int option1Y = END_TEXT_Y + (2 * LINE_HEIGHT);
                 int option2Y = option1Y + LINE_HEIGHT;
                 int option3Y = option2Y + LINE_HEIGHT;
                 
                 displayText(" RESUME ", END_TEXT_X, option1Y);
                 displayText(" SAVE GAME ", END_TEXT_X, option2Y);
                 displayText(" QUIT ", END_TEXT_X, option3Y);
                 
                 // Draw cursor at selected option
                 if (pauseMenuSelection == 0) {
                     printChar(selector[0], END_TEXT_X - CHAR_PIX_SIZE, option1Y);
                 } else if (pauseMenuSelection == 1) {
                     printChar(selector[0], END_TEXT_X - CHAR_PIX_SIZE, option2Y);
                 } else {
                     printChar(selector[0], END_TEXT_X - CHAR_PIX_SIZE, option3Y);
                 }
                 
                 // Handle selection confirmation
                 // START button always resumes, regardless of cursor position
                 if (keys_down & KEY_START) {
                     gameMode = MATCH_MODE;
                     clearRegion(END_TEXT_X - CHAR_PIX_SIZE - 8, END_TEXT_Y, 14 * CHAR_PIX_SIZE, 5 * LINE_HEIGHT);
                     clearRegion(END_TEXT_X, END_TEXT_Y + (4 * LINE_HEIGHT), 12 * CHAR_PIX_SIZE, LINE_HEIGHT); // Clear notification text
                     save_notify_counter = 0;
                 }
                 // A button selects the highlighted option
                 else if (keys_down & KEY_A) {
                     if (pauseMenuSelection == 0) {
                         // RESUME selected
                         gameMode = MATCH_MODE;
                         clearRegion(END_TEXT_X - CHAR_PIX_SIZE - 8, END_TEXT_Y, 14 * CHAR_PIX_SIZE, 5 * LINE_HEIGHT);
                         clearRegion(END_TEXT_X, END_TEXT_Y + (4 * LINE_HEIGHT), 12 * CHAR_PIX_SIZE, LINE_HEIGHT); // Clear notification text
                         save_notify_counter = 0;
                     } else if (pauseMenuSelection == 1) {
                         // SAVE GAME selected
                         if (save_notify_counter == 0) {
                             saveGameState(score, lives, &ship, asteroids, bullets);
                             setSaveNotification(wasLastSaveOK());
                         }
                     } else {
                         // QUIT selected - return to main menu
                         menuVisible = false;
                         gameMode = MENU_MODE;
                     }
                 }

                 // Draw any transient save notification and present frame
                 maybeDrawSaveNotification();
                 flipBuffer();
          }
        
    } // End of while(1)
} // End of main(void)