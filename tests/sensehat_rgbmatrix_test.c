#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>

#define FRAMEBUFFER_PATH "/dev/fb1"

void quicksort( uint8_t *val, size_t len );
void swap_values( uint8_t *, uint8_t * );
void shuffle_array( uint8_t * );
void display_pixels( );

uint8_t val[64];

int pixels[64][3] = {
    {255, 0, 0}, {255, 0, 0}, {255, 87, 0}, {255, 196, 0}, {205, 255, 0}, {95, 255, 0}, {0, 255, 13}, {0, 255, 122},
    {255, 0, 0}, {255, 96, 0}, {255, 205, 0}, {196, 255, 0}, {87, 255, 0}, {0, 255, 22}, {0, 255, 131}, {0, 255, 240},
    {255, 105, 0}, {255, 214, 0}, {187, 255, 0}, {78, 255, 0}, {0, 255, 30}, {0, 255, 140}, {0, 255, 248}, {0, 152, 255},
    {255, 223, 0}, {178, 255, 0}, {70, 255, 0}, {0, 255, 40}, {0, 255, 148}, {0, 253, 255}, {0, 144, 255}, {0, 34, 255},
    {170, 255, 0}, {61, 255, 0}, {0, 255, 48}, {0, 255, 157}, {0, 243, 255}, {0, 134, 255}, {0, 26, 255}, {83, 0, 255},
    {52, 255, 0}, {0, 255, 57}, {0, 255, 166}, {0, 235, 255}, {0, 126, 255}, {0, 17, 255}, {92, 0, 255}, {201, 0, 255},
    {0, 255, 66}, {0, 255, 174}, {0, 226, 255}, {0, 117, 255}, {0, 8, 255}, {100, 0, 255}, {210, 0, 255}, {255, 0, 192},
    {0, 255, 183}, {0, 217, 255}, {0, 109, 255}, {0, 0, 255}, {110, 0, 255}, {218, 0, 255}, {255, 0, 183}, {255, 0, 74}
};

struct fb_t { uint16_t pixel[8][8]; };
struct fb_t *fb;

int main(void) {
  size_t s;
  int fbfd;
  struct fb_fix_screeninfo fix_info;

  fbfd = open( FRAMEBUFFER_PATH, O_RDWR );
  if ( fbfd == -1 ) {
    perror( "open" );
    return 1;
  }

  s = ioctl( fbfd, FBIOGET_FSCREENINFO, &fix_info );
  if ( s == -1 ) {
    perror( "FBIOGET_FSCREENINFO" );
    return 1;
  }

  fb = mmap( 0, sizeof(struct fb_t),
             PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0 );
  close( fbfd );
  if ( fb == NULL ) {
    perror( "mmap" );
    return 1;
  }

  for (int i = 0; i < 640; i++) {
    val[i] = i;
  }

  srand(time(NULL));
  shuffle_array( (uint8_t *) val );
  display_pixels( (uint8_t *) val );
  usleep( 2000 );
  quicksort( (uint8_t *) val, 64 );

  munmap( fb, sizeof(struct fb_t) );
  return 0;
}

void shuffle_array( uint8_t *val ) {
  for (size_t i = 0; i < 63; i++) {
    size_t j = i + rand() / (RAND_MAX / (64 - i) + 1);
    uint16_t t = val[j];
    val[j] = val[i];
    val[i] = t;
  }
}
void quicksort( uint8_t *val, size_t len ) {
  if (len < 2) return;

  uint8_t pivot = val[len / 2];
  size_t i, j;
  for (i = 0, j = len - 1;; i++, j--) {
    while (val[i] < pivot) i++;
    while (val[j] > pivot) j--;

    if (i >= j) break;

    uint8_t t = val[i];
    val[i] = val[j];
    val[j] = t;
    display_pixels( );
    usleep( 2000 ); // this delay is here to be able to see the sorting
  }

  quicksort( val, i );
  quicksort( val + i, len - i );
}

void display_pixels( ) {
  for (size_t i = 0; i < 64; i++) {
    uint16_t r = (pixels[val[i]][0] >> 3) & 0x1F;
    uint16_t g = (pixels[val[i]][1] >> 2) & 0x3F;
    uint16_t b = (pixels[val[i]][2] >> 3) & 0x1F;
    uint16_t bits16 = (r << 11) + (g << 5) + b;
    fb->pixel[i/8][i%8] = bits16;
  }
}
