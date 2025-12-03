#include "save.h"
#include "game_objects.h"
#include <stdint.h>
#include <string.h>

// Use byte-wise access to SRAM for compatibility with emulators that
// expose 8- or 16-bit windows. Address is the standard GBA SRAM base.
#define SRAM_BASE ((volatile uint8_t*)0x0E000000)

// Offsets (in bytes)
#define OFF_MAGIC   0
#define OFF_HIGHSCO 4
#define OFF_GAMESTATE_MAGIC 8
#define OFF_GAMESTATE_DATA  12

// Magic values
#define SAVE_MAGIC 0xA5A5A5A5u
#define GAMESTATE_MAGIC 0x47414D45u  // "GAME" in hex

static int highScore = 0;
static volatile int last_save_ok = 0;
static volatile int highscore_dirty = 0;

// Helper: write a 32-bit value little-endian to SRAM at byte offset
static void sram_write_u32(uint32_t offset, uint32_t value) {
    volatile uint8_t *p = SRAM_BASE + offset;
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

// Helper: read a 32-bit little-endian value from SRAM at byte offset
static uint32_t sram_read_u32(uint32_t offset) {
    volatile uint8_t *p = SRAM_BASE + offset;
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

void loadHighScore(void) {
    uint32_t magic = sram_read_u32(OFF_MAGIC);
    if (magic == SAVE_MAGIC) {
        int loaded = (int)sram_read_u32(OFF_HIGHSCO);
        if (loaded < 0 || loaded > 1000000) {
            highScore = 0;
        } else {
            highScore = loaded;
        }
    } else {
        highScore = 0;
    }
}

void saveHighScore(void) {
    // Write magic and high score as byte sequences to improve compatibility
    sram_write_u32(OFF_MAGIC, SAVE_MAGIC);
    sram_write_u32(OFF_HIGHSCO, (uint32_t)highScore);

    // Slightly longer delay for slower emulators/hardware to settle
    for (volatile int i = 0; i < 10000; i++) { __asm__("nop"); }

    // Read back and verify
    uint32_t magic = sram_read_u32(OFF_MAGIC);
    uint32_t hs = sram_read_u32(OFF_HIGHSCO);

    if (magic != SAVE_MAGIC || (int)hs != highScore) {
        // Clear area to avoid later confusion
        sram_write_u32(OFF_MAGIC, 0);
        sram_write_u32(OFF_HIGHSCO, 0);
        last_save_ok = 0;
        // leave dirty flag so we can try again later
    } else {
        last_save_ok = 1;
        highscore_dirty = 0;
    }
}

int getHighScore(void) {
    return highScore;
}

void setHighScore(int v) {
    highScore = v;
    highscore_dirty = 1;
}

// Returns non-zero if the runtime high score differs from persisted storage
int isHighScoreDirty(void) {
    return highscore_dirty;
}

int wasLastSaveOK(void) {
    return last_save_ok;
}

// Check if a saved game exists
int hasSavedGame(void) {
    uint32_t magic = sram_read_u32(OFF_GAMESTATE_MAGIC);
    return (magic == GAMESTATE_MAGIC) ? 1 : 0;
}

// Save complete game state
void saveGameState(int score, int lives, GameObject *ship, Asteroid asteroids[], GameObject bullets[]) {
    uint32_t offset = OFF_GAMESTATE_DATA;
    
    // Write game state magic
    sram_write_u32(OFF_GAMESTATE_MAGIC, GAMESTATE_MAGIC);
    
    // Write score and lives
    sram_write_u32(offset, (uint32_t)score);
    offset += 4;
    sram_write_u32(offset, (uint32_t)lives);
    offset += 4;
    
    // Write player ship data
    sram_write_u32(offset, (uint32_t)ship->x);
    offset += 4;
    sram_write_u32(offset, (uint32_t)ship->y);
    offset += 4;
    sram_write_u32(offset, (uint32_t)ship->velocityX);
    offset += 4;
    sram_write_u32(offset, (uint32_t)ship->velocityY);
    offset += 4;
    sram_write_u32(offset, (uint32_t)ship->angle);
    offset += 4;
    sram_write_u32(offset, (uint32_t)ship->isAlive);
    offset += 4;
    
    // Write asteroids (all MAX_ASTEROIDS slots)
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        sram_write_u32(offset, (uint32_t)asteroids[i].obj.isAlive);
        offset += 4;
        if (asteroids[i].obj.isAlive) {
            sram_write_u32(offset, (uint32_t)asteroids[i].obj.x);
            offset += 4;
            sram_write_u32(offset, (uint32_t)asteroids[i].obj.y);
            offset += 4;
            sram_write_u32(offset, (uint32_t)asteroids[i].obj.velocityX);
            offset += 4;
            sram_write_u32(offset, (uint32_t)asteroids[i].obj.velocityY);
            offset += 4;
            sram_write_u32(offset, (uint32_t)asteroids[i].sizeType);
            offset += 4;
        }
    }
    
    // Write bullets (all MAX_BULLETS slots)
    for (int i = 0; i < MAX_BULLETS; i++) {
        sram_write_u32(offset, (uint32_t)bullets[i].isAlive);
        offset += 4;
        if (bullets[i].isAlive) {
            sram_write_u32(offset, (uint32_t)bullets[i].x);
            offset += 4;
            sram_write_u32(offset, (uint32_t)bullets[i].y);
            offset += 4;
            sram_write_u32(offset, (uint32_t)bullets[i].velocityX);
            offset += 4;
            sram_write_u32(offset, (uint32_t)bullets[i].velocityY);
            offset += 4;
        }
    }
    
    // Delay for write to settle
    for (volatile int i = 0; i < 10000; i++) { __asm__("nop"); }
    
    // Verify the magic was written
    uint32_t verify = sram_read_u32(OFF_GAMESTATE_MAGIC);
    last_save_ok = (verify == GAMESTATE_MAGIC) ? 1 : 0;
}

// Load complete game state, returns 1 if successful, 0 if no save data
int loadGameState(int *score, int *lives, GameObject *ship, Asteroid asteroids[], GameObject bullets[]) {
    uint32_t magic = sram_read_u32(OFF_GAMESTATE_MAGIC);
    if (magic != GAMESTATE_MAGIC) {
        return 0;  // No save data
    }
    
    uint32_t offset = OFF_GAMESTATE_DATA;
    
    // Read score and lives
    *score = (int)sram_read_u32(offset);
    offset += 4;
    *lives = (int)sram_read_u32(offset);
    offset += 4;
    
    // Read player ship data
    ship->x = (int)sram_read_u32(offset);
    offset += 4;
    ship->y = (int)sram_read_u32(offset);
    offset += 4;
    ship->velocityX = (int)sram_read_u32(offset);
    offset += 4;
    ship->velocityY = (int)sram_read_u32(offset);
    offset += 4;
    ship->angle = (int)sram_read_u32(offset);
    offset += 4;
    ship->isAlive = (int)sram_read_u32(offset);
    offset += 4;
    
    // Initialize ship fields that aren't saved
    ship->width = 8;  // PLAYER_SIZE
    ship->height = 8;
    ship->prevX = FP_TO_INT(ship->x);
    ship->prevY = FP_TO_INT(ship->y);
    ship->colorIdx = 0;
    
    // Read asteroids
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].obj.isAlive = (int)sram_read_u32(offset);
        offset += 4;
        if (asteroids[i].obj.isAlive) {
            asteroids[i].obj.x = (int)sram_read_u32(offset);
            offset += 4;
            asteroids[i].obj.y = (int)sram_read_u32(offset);
            offset += 4;
            asteroids[i].obj.velocityX = (int)sram_read_u32(offset);
            offset += 4;
            asteroids[i].obj.velocityY = (int)sram_read_u32(offset);
            offset += 4;
            asteroids[i].sizeType = (int)sram_read_u32(offset);
            offset += 4;
            
            // Initialize asteroid fields that aren't saved
            asteroids[i].obj.width = asteroids[i].sizeType;
            asteroids[i].obj.height = asteroids[i].sizeType;
            asteroids[i].obj.prevX = FP_TO_INT(asteroids[i].obj.x);
            asteroids[i].obj.prevY = FP_TO_INT(asteroids[i].obj.y);
            asteroids[i].obj.angle = 0;
            asteroids[i].obj.colorIdx = i & 0xFF;
        }
    }
    
    // Read bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].isAlive = (int)sram_read_u32(offset);
        offset += 4;
        if (bullets[i].isAlive) {
            bullets[i].x = (int)sram_read_u32(offset);
            offset += 4;
            bullets[i].y = (int)sram_read_u32(offset);
            offset += 4;
            bullets[i].velocityX = (int)sram_read_u32(offset);
            offset += 4;
            bullets[i].velocityY = (int)sram_read_u32(offset);
            offset += 4;
            
            // Initialize bullet fields that aren't saved
            bullets[i].width = 2;  // BULLET_SIZE
            bullets[i].height = 2;
            bullets[i].prevX = FP_TO_INT(bullets[i].x);
            bullets[i].prevY = FP_TO_INT(bullets[i].y);
            bullets[i].angle = 0;
            bullets[i].colorIdx = i & 0xFF;
        }
    }
    
    return 1;  // Success
}
