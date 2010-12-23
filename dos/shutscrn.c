/* Show a shutdown screen, just for fun. */

#include <allegro.h>

int main(void)
{
   PALETTE palette;
   int c;
   BITMAP* dispImg;

   allegro_init();
   install_timer();
   install_keyboard();
   if (set_gfx_mode(GFX_AUTODETECT, 320, 400, 0, 0) != 0) {
      if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
	 allegro_message("Error setting graphics mode\n%s\n", allegro_error);
	 return 1;
      }
   }

   /* Load our first image */
   dispImg = load_bmp("C:\\EXT\\logow.sys", palette);

   /* Set the palette */
   set_palette(palette);

   /* Copy our image onto the screen */
   acquire_screen();
   blit(dispImg, screen, 0, 0, 0, 0, 320, 400);
   release_screen();

   /* Wait */
   rest(1000);

   /* Load our second image */
   dispImg = load_bmp("C:\\EXT\\logos.sys", palette);

   /* Set the palette */
   set_palette(palette);

   /* Copy our image onto the screen */
   acquire_screen();
   blit(dispImg, screen, 0, 0, 0, 0, 320, 400);
   release_screen();

   /* Wait until the user presses a key */
   c = readkey();

   return 0;
}

END_OF_MAIN();
