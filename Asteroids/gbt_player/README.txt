GBT Player Setup Instructions
==============================

GBT Player is not included in your devkitPro installation. To use it:

1. Download GBT Player from: https://github.com/AntonioND/gbt-player
   
2. Copy these files to gbt_player/ directory:
   - gbt_player.s
   - gbt_player.h
   - gbt_player_bank1.s
   
3. Download mod2gbt tool from the same repository

4. Convert your MOD file:
   mod2gbt your_music.mod output_name -c4
   
   This creates output_name.c and output_name.h

5. Add the output files to your source/ directory

6. I'll update the code to use GBT Player instead

NOTE: GBT Player only supports 4-channel MOD files (ProTracker modules)
It does NOT support XM, IT, or S3M formats.

Alternative: We can stick with the current sound effects setup which works perfectly.
