#include "sound.h"
#include <gba_sound.h>

// Play shooting sound (laser-like sweep on channel 1)
void playShootSound(void) {
    REG_SOUND1CNT_L = 0x0077; // Fast downward sweep
    REG_SOUND1CNT_H = 0xF140; // Full volume, very short decay
    REG_SOUND1CNT_X = 0x87C0; // High frequency, restart
}

// Play explosion sound (downward sweep on channel 1)
void playExplosionSound(void) {
    REG_SOUND1CNT_L = 0x0078; // Fast upward sweep (reversed)
    REG_SOUND1CNT_H = 0xF110; // Duty 25%, very quick decay
    REG_SOUND1CNT_X = 0x8600; // Higher starting frequency, restart
}

// Play menu selection sound (mid-tone beep on channel 1)
void playMenuSelectSound(void) {
    REG_SOUND1CNT_L = 0x0000; // No sweep
    REG_SOUND1CNT_H = 0x8300; // Duty 50%, short
    REG_SOUND1CNT_X = 0x8400; // Frequency ~800Hz, restart
}

// Play thruster sound (continuous low tone on channel 2)
void playThrusterSound(void) {
    REG_SOUND2CNT_L = 0x6800; // Duty 50%, initial volume 0x6 (medium), no envelope change
    REG_SOUND2CNT_H = 0x8100; // Frequency ~100Hz, restart
}

// Stop thruster sound
void stopThrusterSound(void) {
    // Stop channel 2 by setting volume to 0
    REG_SOUND2CNT_L = 0x0000;
    REG_SOUND2CNT_H = 0x0000;
}

// Play siren sound (rapid low-to-high sweep repeated 3 times on channel 1)
void playSirenSound(void) {
    REG_SOUND1CNT_L = 0x0068; // Medium speed upward sweep, repeated 3 times
    REG_SOUND1CNT_H = 0xF120; // Duty 25%, medium envelope decay
    REG_SOUND1CNT_X = 0x8300; // Mid-range starting frequency ~400Hz, restart
}

// Play player hit sound (high-pitched downward sweep repeated 3 times on channel 1)
void playPlayerHitSound(void) {
    // Stop thruster sound immediately when player gets hit
    stopThrusterSound();
    
    // Play the sound 3 times in rapid succession
    for (int i = 0; i < 3; i++) {
        REG_SOUND1CNT_L = 0x0077; // Fast downward sweep
        REG_SOUND1CNT_H = 0xF108; // Full volume, extremely quick decay
        REG_SOUND1CNT_X = 0x8780; // Very high starting frequency, restart
        
        // Longer delay between repetitions to make them distinct
        for (volatile int d = 0; d < 100000; d++);
    }
}

// Background Music System using Channel 2
static volatile int musicTick = 0;
static volatile int noteIndex = 0;
static volatile int frameCounter = 0;  // Track actual frames for consistent timing
static volatile int musicComplete = 0; // Flag to track if melody has finished playing

// Mario theme - using correct formula: 2048 - (131072 / Hz)
// CONFIRMED: Higher hex = higher pitch
// E4=0x0673, F4=0x0688, C5=0x0705, D5=0x0721, E5=0x0739, F5=0x074D, G4=0x06B2, G5=0x0759, A4=0x06D6, B4=0x06F7, C6=0x0778
static const u16 melodyNotes[] = {
    // Opening: E5 E5 _ E5 _ C5 E5 _ G5 _ _ _ G4 _ _ _
    0x0739, 0x0739, 0x0000, 0x0739, 0x0000, 0x0705, 0x0739, 0x0000,  // E E _ E _ C E _
    0x0759, 0x0000, 0x0000, 0x0000,                                  // G5 _ _ _
    0x06B2, 0x0000, 0x0000, 0x0000,                                  // G4 _ _ _
};
#define MELODY_LENGTH 16
#define NOTE_DURATION 2 // Frames per note/rest

void initProceduralMusic(void) {
    musicTick = 0;
    noteIndex = 0;
    frameCounter = 0;
    musicComplete = 0; // Reset the completion flag
    
    // Start first note on channel 2 with better tone
    if (melodyNotes[0] != 0x0000) {
        REG_SOUND2CNT_L = 0x3780; // Duty 25%, volume 0x3, no envelope
        REG_SOUND2CNT_H = 0x8000 | (melodyNotes[0] & 0x07FF); // Start with first note, mask to 11 bits
    }
}

void updateProceduralMusic(void) {
    // Don't update music if it's already complete
    if (musicComplete) {
        return;
    }
    
    frameCounter++;
    musicTick++;
    
    // Change note when duration expires
    if (musicTick >= NOTE_DURATION) {
        musicTick = 0;
        noteIndex++;
        
        // Check if we've reached the end of the melody
        if (noteIndex >= MELODY_LENGTH) {
            musicComplete = 1; // Mark as complete
            REG_SOUND2CNT_L = 0x0000;
            REG_SOUND2CNT_H = 0x0000;
            return;
        }
        
        u16 currentNote = melodyNotes[noteIndex];
        
        // Handle rests (silence) vs actual notes
        if (currentNote == 0x0000) {
            // Rest - turn off channel 2 briefly
            REG_SOUND2CNT_H = 0x0000;
        } else {
            // Play the note on channel 2
            REG_SOUND2CNT_L = 0x3780; // Duty 25%, volume 0x3, no envelope
            REG_SOUND2CNT_H = 0x8000 | (currentNote & 0x07FF); // Restart with new note, mask to 11 bits, ensure continuous mode
        }
    }
}

void stopProceduralMusic(void) {
    REG_SOUND2CNT_L = 0x0000;
    REG_SOUND2CNT_H = 0x0000;
    musicTick = 0;
    noteIndex = 0;
    frameCounter = 0;
    musicComplete = 0;
}
