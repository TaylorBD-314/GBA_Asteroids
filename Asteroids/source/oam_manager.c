#include <gba_types.h>
#include <gba_base.h>
#include "game_objects.h"

// GBA Hardware Definitions for OAM
typedef struct {
    u16 attr0;
    u16 attr1;
    u16 attr2;
    s16 fill; // Padding
} OAMEntry;

#define OAM_SIZE 128
#define OAM ((OAMEntry*)0x07000000)
#define MEM_OAM_TILE (u16*)0x06010000 // OAM VRAM for tile data (Mode 0)

// OAM Attributes: Simplified placeholders for GBA library constants
// You will need to define these based on your GBA toolchain (e.g., libgba, devkitPro)
#define ATTR0_Y_MASK        0x00FF
#define ATTR0_MODE_NORMAL   0x0000
#define ATTR0_8BPP          0x2000
#define ATTR0_HIDE          0x0200 // Bit 9: hide sprite
#define ATTR1_X_MASK        0x01FF
#define ATTR1_SIZE_8        0x0000 // 8x8 size
#define ATTR1_SIZE_16       0x4000 // 16x16 size
#define ATTR2_TILE_MASK     0x03FF
#define ATTR2_PRIO(p)       ((p) << 10) // Priority 0 is highest

// Tile indices for different sprite types
#define TILE_ASTEROID_L     0   // 16x16 asteroid (large)
#define TILE_ASTEROID_M     1   // 16x16 asteroid (medium)
#define TILE_ASTEROID_S     2   // 8x8 asteroid (small)
#define TILE_BULLET         3   // 8x8 bullet

// Global OAM cache to minimize hardware writes
static OAMEntry oam_copy[OAM_SIZE];
static u8 oam_free[OAM_SIZE]; // 0 = allocated, 1 = free

/**
 * Allocates a free OAM sprite index. Returns -1 if no sprites available.
 */
int allocateOAMSprite() {
    for (int i = 0; i < OAM_SIZE; i++) {
        if (oam_free[i]) {
            oam_free[i] = 0; // Mark as allocated
            return i;
        }
    }
    return -1; // No free sprites
}

/**
 * Deallocates an OAM sprite index (marks as free and hides it).
 */
void deallocateOAMSprite(int oam_index) {
    if (oam_index < 0 || oam_index >= OAM_SIZE) return;
    oam_free[oam_index] = 1; // Mark as free
    oam_copy[oam_index].attr0 |= ATTR0_HIDE; // Hide sprite
}

/**
 * Fills an 8x8 tile with a single palette color.
 * Each tile is 32 bytes (64 pixels = 8x8, 4 bits per pixel).
 */
static void fillTileWithColor(int tile_index, u8 color) {
    u32 *tile_ptr = (u32*)(MEM_OAM_TILE + tile_index * 32); // 32 bytes per tile
    u32 color_pair = (color << 4) | color; // Two pixels of same color per byte
    u32 color_word = (color_pair << 24) | (color_pair << 16) | (color_pair << 8) | color_pair;
    
    for (int i = 0; i < 8; i++) {
        tile_ptr[i] = color_word;
    }
}

/**
 * Creates simple sprite tiles: asteroids (red, magenta, yellow) and bullets (cyan).
 * Each tile is 8x8 pixels, 4 bits per pixel (16 colors).
 * Tile data is stored in OAM VRAM at tile indices 0-3.
 * 
 * Palette usage:
 * - Palette colors for asteroids/bullets use indices 1-4 (color 0 is transparent)
 * - Red (asteroid large/medium): color 1
 * - Magenta (bullet): color 2
 * - Yellow (asteroid small): color 3
 * - Cyan (sprite outline): color 4
 */
void loadSpriteTiles() {
    // Tile 0: TILE_ASTEROID_L (red)
    fillTileWithColor(0, 1);
    
    // Tile 1: TILE_ASTEROID_M (magenta)
    fillTileWithColor(1, 2);
    
    // Tile 2: TILE_ASTEROID_S (yellow)
    fillTileWithColor(2, 3);
    
    // Tile 3: TILE_BULLET (cyan)
    fillTileWithColor(3, 4);
}

/**
 * Initializes the sprite palette and tile data.
 * Sprite palette is stored at 0x05000200 (OBJ palette RAM).
 */
void initSpritePalette() {
    // Sprite palette RAM address (Object palette starts at 0x05000200)
    u16 *spr_palette = (u16*)0x05000200;
    
    // Colors: 16-bit BGR format (bit 0-4: Blue, 5-9: Green, 10-14: Red)
    spr_palette[0] = 0x0000; // Color 0: Black (transparent)
    spr_palette[1] = 0x001F; // Color 1: Red
    spr_palette[2] = 0x7C1F; // Color 2: Magenta
    spr_palette[3] = 0x03E0; // Color 3: Yellow
    spr_palette[4] = 0x7FFF; // Color 4: Cyan
}

/**
 * Initializes the OAM system: sets the video mode and clears the OAM memory.
 */
void initOAM() {
    // Initialize sprite palette
    initSpritePalette();
    
    // Set REG_DISPCNT to enable Mode 0 (Tile/Text mode) and OAM (Sprites)
    // You may already have this in main.c, but ensure OAM is enabled.
    // REG_DISPCNT |= DCNT_OBJ | DCNT_OBJ_1D; 
    
    // Clear the OAM memory and mark all sprites as free
    for (int i = 0; i < OAM_SIZE; i++) {
        oam_copy[i].attr0 = ATTR0_HIDE; // Hide all sprites
        oam_copy[i].attr1 = 0;
        oam_copy[i].attr2 = 0;
        oam_free[i] = 1; // Mark as free
    }
    
    // Load sprite tile data
    loadSpriteTiles();
}

/**
 * Updates an OAM entry in the local cache. Supports variable sizes.
 * size_bits: 0=8x8, 0x4000=16x16, etc. (ATTR1_SIZE_*)
 */
void setOAMAttributes(int oam_index, int x, int y, int tile_index, u16 size_bits) {
    if (oam_index < 0 || oam_index >= OAM_SIZE) return;

    // Attribute 0: Y position (mask 0-255), Mode (Normal), Color Depth (4bpp)
    oam_copy[oam_index].attr0 = (y & ATTR0_Y_MASK) | ATTR0_MODE_NORMAL;

    // Attribute 1: X position (mask 0-511), Size
    oam_copy[oam_index].attr1 = (x & ATTR1_X_MASK) | size_bits;
    
    // Attribute 2: Tile Index, Priority
    oam_copy[oam_index].attr2 = (tile_index & ATTR2_TILE_MASK) | ATTR2_PRIO(1); // Priority 1 (below player)
}

/**
 * Hide an OAM sprite (for inactive objects).
 */
void hideOAMSprite(int oam_index) {
    if (oam_index < 0 || oam_index >= OAM_SIZE) return;
    oam_copy[oam_index].attr0 |= ATTR0_HIDE;
}

/**
 * Show an OAM sprite.
 */
void showOAMSprite(int oam_index) {
    if (oam_index < 0 || oam_index >= OAM_SIZE) return;
    oam_copy[oam_index].attr0 &= ~ATTR0_HIDE;
}

/**
 * Copies the local OAM cache to the hardware OAM memory.
 * This should be called once per frame (after VBlank).
 */
void updateOAM() {
    // Copy the local cache to the hardware OAM memory
    // This is the actual fast hardware write operation.
    // Use the compiler's intrinsic for fast memory copy if available (e.g., DmaCopy)
    // For simplicity, using a loop here, but a DMA copy is faster.
    for (int i = 0; i < OAM_SIZE; i++) {
        OAM[i] = oam_copy[i];
    }
}