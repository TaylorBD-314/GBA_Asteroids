MUSIC FOLDER
============

Place your music files here (.mod, .xm, .s3m, or .it format).

Maxmod will automatically convert these files into a soundbank during compilation.

To play a music track in your game:
1. Place your .mod or .xm file in this folder
2. Run make clean && make to rebuild
3. The soundbank.h file will contain definitions like MOD_YOURFILENAME
4. Use mmStart(MOD_YOURFILENAME, MM_PLAY_LOOP) to play the music

Example music file naming:
- menu_theme.mod
- gameplay.xm
- gameover.s3m

Note: Keep music files small (under 100KB recommended) to avoid memory issues.
