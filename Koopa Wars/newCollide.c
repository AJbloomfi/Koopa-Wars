/*
 * KOOPA WARS
 * Created by: 
 * Aaron Bloomfield
 * Jorge Contreras Mendez
 *
 * CPSC 305
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160



#include "text.h"
#include "music.h"


/* include the background image we are using */
#include "background.h"

/* include the sprite image we are using */
#include "koopa.h"

/* include the tile map we are using */
#include "map.h"

/* the tile mode flags needed for display control register */
#define MODE0 0x00
#define MODE1 0x01
#define MODE2 0x02
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800
#define BG0_ENABLE 0x100


/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000


/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* the address of the color palettes used for backgrounds and sprites */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}


/* define the timer control registers */
volatile unsigned short* timer0_data = (volatile unsigned short*) 0x4000100;
volatile unsigned short* timer0_control = (volatile unsigned short*) 0x4000102;

/* make defines for the bit positions of the control register */
#define TIMER_FREQ_1 0x0
#define TIMER_FREQ_64 0x2
#define TIMER_FREQ_256 0x3
#define TIMER_FREQ_1024 0x4
#define TIMER_ENABLE 0x80

/* the GBA clock speed is fixed at this rate */
#define CLOCK 16777216 
#define CYCLES_PER_BLANK 280806


/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* this causes the DMA destination to be the same each time rather than increment */
#define DMA_DEST_FIXED 0x400000

/* this causes the DMA to repeat the transfer automatically on some interval */
#define DMA_REPEAT 0x2000000

/* this causes the DMA repeat interval to be synced with timer 0 */
#define DMA_SYNC_TO_TIMER 0x30000000

/* pointers to the DMA source/dest locations and control registers */
volatile unsigned int* dma1_source = (volatile unsigned int*) 0x40000BC;
volatile unsigned int* dma1_destination = (volatile unsigned int*) 0x40000C0;
volatile unsigned int* dma1_control = (volatile unsigned int*) 0x40000C4;

volatile unsigned int* dma2_source = (volatile unsigned int*) 0x40000C8;
volatile unsigned int* dma2_destination = (volatile unsigned int*) 0x40000CC;
volatile unsigned int* dma2_control = (volatile unsigned int*) 0x40000D0;

volatile unsigned int* dma3_source = (volatile unsigned int*) 0x40000D4;
volatile unsigned int* dma3_destination = (volatile unsigned int*) 0x40000D8;
volatile unsigned int* dma3_control = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA channel 3 (normal memory transfers) */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma3_source = (unsigned int) source;
    *dma3_destination = (unsigned int) dest;
    *dma3_control = DMA_ENABLE | DMA_16 | amount;
}

/* the global interrupt enable register */
volatile unsigned short* interrupt_enable = (unsigned short*) 0x4000208;

/* this register stores the individual interrupts we want */
volatile unsigned short* interrupt_selection = (unsigned short*) 0x4000200;

/* this registers stores which interrupts if any occured */
volatile unsigned short* interrupt_state = (unsigned short*) 0x4000202;

/* the address of the function to call when an interrupt occurs */
volatile unsigned int* interrupt_callback = (unsigned int*) 0x3007FFC;

/* this register needs a bit set to tell the hardware to send the vblank interrupt */
volatile unsigned short* display_interrupts = (unsigned short*) 0x4000004;

/* the interrupts are identified by number, we only care about this one */
#define INTERRUPT_VBLANK 0x1

/* allows turning on and off sound for the GBA altogether */
volatile unsigned short* master_sound = (volatile unsigned short*) 0x4000084;
#define SOUND_MASTER_ENABLE 0x80

/* has various bits for controlling the direct sound channels */
volatile unsigned short* sound_control = (volatile unsigned short*) 0x4000082;

/* bit patterns for the sound control register */
#define SOUND_A_RIGHT_CHANNEL 0x100
#define SOUND_A_LEFT_CHANNEL 0x200
#define SOUND_A_FIFO_RESET 0x800
#define SOUND_B_RIGHT_CHANNEL 0x1000
#define SOUND_B_LEFT_CHANNEL 0x2000
#define SOUND_B_FIFO_RESET 0x8000

/* the location of where sound samples are placed for each channel */
volatile unsigned char* fifo_buffer_a  = (volatile unsigned char*) 0x40000A0;
volatile unsigned char* fifo_buffer_b  = (volatile unsigned char*) 0x40000A4;

/* global variables to keep track of how much longer the sounds are to play */
unsigned int channel_a_vblanks_remaining = 0;
unsigned int channel_a_total_vblanks = 0;
unsigned int channel_b_vblanks_remaining = 0;


/* play a sound with a number of samples, and sample rate on one channel 'A' or 'B' */
void play_sound(const signed char* sound, int total_samples, int sample_rate, char channel) {
    /* start by disabling the timer and dma controller (to reset a previous sound) */
    *timer0_control = 0;
    if (channel == 'A') {
        *dma1_control = 0;
    } else if (channel == 'B') {
        *dma2_control = 0;
    }

    /* output to both sides and reset the FIFO */
    if (channel == 'A') {
        *sound_control |= SOUND_A_RIGHT_CHANNEL | SOUND_A_LEFT_CHANNEL | SOUND_A_FIFO_RESET;
    } else if (channel == 'B') {
        *sound_control |= SOUND_B_RIGHT_CHANNEL | SOUND_B_LEFT_CHANNEL | SOUND_B_FIFO_RESET;
    }

    /* enable all sound */
    *master_sound = SOUND_MASTER_ENABLE;

 /* set the dma channel to transfer from the sound array to the sound buffer */
    if (channel == 'A') {
        *dma1_source = (unsigned int) sound;
        *dma1_destination = (unsigned int) fifo_buffer_a;
        *dma1_control = DMA_DEST_FIXED | DMA_REPEAT | DMA_32 | DMA_SYNC_TO_TIMER | DMA_ENABLE;
    } else if (channel == 'B') {
        *dma2_source = (unsigned int) sound;
        *dma2_destination = (unsigned int) fifo_buffer_b;
        *dma2_control = DMA_DEST_FIXED | DMA_REPEAT | DMA_32 | DMA_SYNC_TO_TIMER | DMA_ENABLE;
    }

    /* set the timer so that it increments once each time a sample is due
     * we divide the clock (ticks/second) by the sample rate (samples/second)
     * to get the number of ticks/samples */
    unsigned short ticks_per_sample = CLOCK / sample_rate;

    /* the timers all count up to 65536 and overflow at that point, so we count up to that
     * now the timer will trigger each time we need a sample, and cause DMA to give it one! */
    *timer0_data = 65536 - ticks_per_sample;

    /* determine length of playback in vblanks
     * this is the total number of samples, times the number of clock ticks per sample,
     * divided by the number of machine cycles per vblank (a constant) */
    if (channel == 'A') {
        channel_a_vblanks_remaining = total_samples * ticks_per_sample * (1.0 / CYCLES_PER_BLANK);
        channel_a_total_vblanks = channel_a_vblanks_remaining;
    } else if (channel == 'B') {
        channel_b_vblanks_remaining = total_samples * ticks_per_sample * (1.0 / CYCLES_PER_BLANK);
    }

    /* enable the timer */
    *timer0_control = TIMER_ENABLE | TIMER_FREQ_1;
}


void on_vblank() {
    /* disable interrupts for now and save current state of interrupt */
    *interrupt_enable = 0;
    unsigned short temp = *interrupt_state;

    /* display frames of sound remaining */
    char a_remaining[32], b_remaining[32];
   // sprintf(a_remaining, "A = %d    ", channel_a_vblanks_remaining);
   // sprintf(b_remaining, "B = %d    ", channel_b_vblanks_remaining);

    /* look for vertical refresh */
    if ((*interrupt_state & INTERRUPT_VBLANK) == INTERRUPT_VBLANK) {

        /* update channel A */
        if (channel_a_vblanks_remaining == 0) {
            /* restart the sound again when it runs out */
            channel_a_vblanks_remaining = channel_a_total_vblanks;
            *dma1_control = 0;
            *dma1_source = (unsigned int) music;
            *dma1_control = DMA_DEST_FIXED | DMA_REPEAT | DMA_32 |
                DMA_SYNC_TO_TIMER | DMA_ENABLE;
        } else {
            channel_a_vblanks_remaining--;
        }

        /* update channel B */
        if (channel_b_vblanks_remaining == 0) {
            /* disable the sound and DMA transfer on channel B */
            *sound_control &= ~(SOUND_B_RIGHT_CHANNEL | SOUND_B_LEFT_CHANNEL | SOUND_B_FIFO_RESET);
            *dma2_control = 0;
        }
        else {
            channel_b_vblanks_remaining--;
        }
    }

    /* restore/enable interrupts */
    *interrupt_state = temp;
    *interrupt_enable = 1;
}








/* function to setup background 0 for this program */
void setup_background() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) background_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) background_data,
            (background_width * background_height) / 2);
    
    memcpy16_dma((unsigned short*) char_block(1), (unsigned short*) text_data,
            (text_width * text_height) / 2);

    /* set all control the bits in this register */
    *bg0_control = 0 |    /* priority, 0 is highest, 3 is lowest */
        (1 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (24 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */
    
    /* clear the text map to be all blanks */
    volatile unsigned short* ptr = screen_block(24);
    for (int i = 0; i < 32 * 32; i++) {
        ptr[i] = 0;
    }


    /* set all control the bits in this register */
    *bg1_control = 1 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (17 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    /* load the tile data into screen block 16 */
    memcpy16_dma((unsigned short*) screen_block(17), (unsigned short*) map, map_width * map_height);
}

/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

/* a sprite is a moveable image on the screen */
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};


/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];


int next_sprite_index = 0;
int next_sprite_index1 = 0;
/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y |             /* y coordinate */
        (0 << 8) |          /* rendering mode */
        (0 << 10) |         /* gfx mode */
        (0 << 12) |         /* mosaic */
        (1 << 13) |         /* color mode, 0:16, 1:256 */
        (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x |             /* x coordinate */
        (0 << 9) |          /* affine flag */
        (h << 12) |         /* horizontal flip flag */
        (v << 13) |         /* vertical flip flag */
        (size_bits << 14);  /* size */

    /* setup the second attribute */
    sprites[index].attribute2 = tile_index |   // tile index */
        (priority << 10) | // priority */
        (0 << 12);         // palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}


/* update all of the spries on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/*update all the sprites on the screen*/
void sprite_update_all1() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/*update all the sprites on the screen*/
void sprite_update_all2() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);

}
/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = SCREEN_HEIGHT;
        sprites[i].attribute1 = SCREEN_WIDTH;
    }
}


/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}



/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}


/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;

    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) koopa_palette, PALETTE_SIZE);

    /* load the image into sprite image memory */
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) koopa_data, (koopa_width * koopa_height) / 2);
}

/* a struct for the koopa's logic and behavior */
struct Koopa {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion in pixels */
    int x, y;

    /* the koopa's y velocity in 1/256 pixels/second */
    int yvel;

    /* the koopa's y acceleration in 1/256 pixels/second^2 */
    int gravity; 

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the koopa is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the koopa stays */
    int border;

    /* if the koopa is currently falling */
    int falling;
    
    /*the number of counts the koopa has moved along the x-axis*/
    int movement;

    /*the number of lives a koopa has*/
    int lives;
};
/* initialize the koopa */
void koopa_init(struct Koopa* koopa) {
    koopa->x = 100;
    koopa->y = 113;
    koopa->yvel = 0;
    koopa->gravity = 50;
    koopa->border = 120;
    koopa->frame = 0;
    koopa->move = 0;
    koopa->counter = 0;
    koopa->falling = 0;
    koopa->animation_delay = 8;
    koopa->sprite = sprite_init(koopa->x, koopa->y, SIZE_16_32, 0, 0, koopa->frame, 0);
    koopa->movement = 0;
    koopa->lives = 3;
}
/*initialize the koopa*/
void koopa_init1(struct Koopa* koopa1) {
    koopa1->x = 240;
    koopa1->y = 113;
    koopa1->yvel = 0;
    koopa1->gravity = 50;
    koopa1->border = 40;
    koopa1->frame = 0;
    koopa1->move = 0;
    koopa1->counter = 0;
    koopa1->falling = 0;
    koopa1->animation_delay = 8;
    koopa1->sprite = sprite_init(koopa1->x, koopa1->y, SIZE_16_32, 0, 0, koopa1->frame, 0);
    koopa1->movement = 0;
    koopa1->lives=0;
}

/*initialize the koopa*/
void koopa_init2(struct Koopa* koopa1) {
    koopa1->x = 300;
    koopa1->y = 113;
    koopa1->yvel = 0;
    koopa1->gravity = 50;
    koopa1->border = 40;
    koopa1->frame = 0;
    koopa1->move = 0;
    koopa1->counter = 0;
    koopa1->falling = 0;
    koopa1->animation_delay = 8;
    koopa1->sprite = sprite_init(koopa1->x, koopa1->y, SIZE_16_32, 0, 0, koopa1->frame, 0);
    koopa1->movement = 0;
    koopa1->lives = 0;
    }


/* move the koopa left or right returns if it is at edge of the screen */
int koopa_left(struct Koopa* koopa) {
    /* face left */
    sprite_set_horizontal_flip(koopa->sprite, 1);
    koopa->move = 1;

    /* if we are at the left end, just scroll the screen */
    if (koopa->x < koopa->border) {
        return 1;
    } else {
        /* else move left */
        koopa->x--;
        return 0;
    }
}

int koopa_left1(struct Koopa* koopa1) {
    /* face left */
    sprite_set_horizontal_flip(koopa1->sprite, 1);
    koopa1->move = 1;

    /* if we are at the left end, just scroll the screen */
    if (koopa1->x < koopa1->border) {
        return 1;
    } else {
        /* else move left */
        koopa1->x--;
        return 0;
    }
}

int koopa_left2(struct Koopa* koopa1) {
         /* face left */
            sprite_set_horizontal_flip(koopa1->sprite, 1);
         koopa1->move = 1;
     
             /* if we are at the left end, just scroll the screen */
             if (koopa1->x < koopa1->border) {
                     return 1;
                 } else {
                         /* else move left */
                             koopa1->x--;
                         return 0;
                     }
     }


int koopa_right(struct Koopa* koopa) {
    /* face right */
    sprite_set_horizontal_flip(koopa->sprite, 0);
    koopa->move = 1;

    /* if we are at the right end, just scroll the screen */
    if (koopa->x > (SCREEN_WIDTH - 16 - koopa->border)) {
        return 1;
    } else {
        /* else move right */
        koopa->x++;
        return 0;
    }
}

int koopa_right1(struct Koopa* koopa1) {
    /* face right */
    sprite_set_horizontal_flip(koopa1->sprite, 0);
    koopa1->move = 1;

    /* if we are at the right end, just scroll the screen */
    if (koopa1->x > (SCREEN_WIDTH - 16 - koopa1->border)) {
        return 1;
    } else {
        /* else move right */
        koopa1->x++;
        return 0;
    }
}


/* stop the koopa from walking left/right */
void koopa_stop(struct Koopa* koopa) {
    koopa->move = 0;
    koopa->frame = 0;
    koopa->counter = 7;
    sprite_set_offset(koopa->sprite, koopa->frame);
}
/*stop the second koopa from walking left/right*/
void koopa_stop1(struct Koopa* koopa1) {
    koopa1->move = 0;
    koopa1->frame = 0;
    koopa1->counter = 7;
    sprite_set_offset(koopa1->sprite, koopa1->frame);
}



/* start the koopa jumping, unless already fgalling */
void koopa_jump(struct Koopa* koopa) {
    if (!koopa->falling) {
        koopa->yvel = -1350;
        koopa->falling = 1;
    }
}

/* finds which tile a screen coordinate maps to, taking scroll into acco  unt */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

    /* adjust for the scroll */
    x += xscroll;
    y += yscroll;

    /* convert from screen coordinates to tile coordinates */
    x >>= 3;
    y >>= 3;

    /* account for wraparound */
    while (x >= tilemap_w) {
        x -= tilemap_w;
    }
    while (y >= tilemap_h) {
        y -= tilemap_h;
    }
    while (x < 0) {
        x += tilemap_w;
    }
    while (y < 0) {
        y += tilemap_h;
    }

    /* the larger screen maps (bigger than 32x32) are made of multiple stitched
       together - the offset is used for finding which screen block we are in
       for these cases */
    int offset = 0;

    /* if the width is 64, add 0x400 offset to get to tile maps on right   */
    if (tilemap_w == 64 && x >= 32) {
        x -= 32;
        offset += 0x400;
    }

    /* if height is 64 and were down there */
    if (tilemap_h == 64 && y >= 32) {
        y -= 32;

        /* if width is also 64 add 0x800, else just 0x400 */
        if (tilemap_w == 64) {
            offset += 0x800;
        } else {
            offset += 0x400;
        }
    }

    /* find the index in this tile map */
    int index = y * 32 + x;

    /* return the tile */
    return tilemap[index + offset];
}

/* update the koopa */
void koopa_update(struct Koopa* koopa, int xscroll) {
    /* update y position and speed if falling */
    if (koopa->falling) {
        koopa->y += (koopa->yvel >> 8);
        koopa->yvel += koopa->gravity;
    }

    /* check which tile the koopa's feet are over */
    unsigned short tile = tile_lookup(koopa->x + 8, koopa->y + 32, xscroll, 0, map,
            map_width, map_height);

    /* if it's block tile
     * these numbers refer to the tile indices of the blocks the koopa can walk on */
    if ((tile >= 1 && tile <= 6) || 
            (tile >= 12 && tile <= 17)) {
        /* stop the fall! */
        koopa->falling = 0;
        koopa->yvel = 0;

        /* make him line up with the top of a block works by clearing out the lower bits to 0 */
        koopa->y &= ~0x3;

        /* move him down one because there is a one pixel gap in the image */
        koopa->y++;

    } else {
        /* he is falling now */
        koopa->falling = 1;
    }


    /* update animation if moving */
    if (koopa->move) {
        koopa->counter++;
        if (koopa->counter >= koopa->animation_delay) {
            koopa->frame = koopa->frame + 16;
            if (koopa->frame > 16) {
                koopa->frame = 0;
            }
            sprite_set_offset(koopa->sprite, koopa->frame);
            koopa->counter = 0;
        }
    }

    /* set on screen position */
    sprite_position(koopa->sprite, koopa->x, koopa->y);
}

/*update the second koopa*/
void koopa_update1(struct Koopa* koopa1, int xscroll) {
    /* update y position and speed if falling */
    if (koopa1->falling) {
        koopa1->y += (koopa1->yvel >> 8);
        koopa1->yvel += koopa1->gravity;
    }

    /* check which tile the koopa's feet are over */
    unsigned short tile = tile_lookup(koopa1->x + 8, koopa1->y + 32, xscroll, 0, map,
            map_width, map_height);

    /* if it's block tile
     * these numbers refer to the tile indices of the blocks the koopa can walk on */
    if ((tile >= 1 && tile <= 6) ||
            (tile >= 12 && tile <= 17)) {
        /* stop the fall! */
        koopa1->falling = 0;
        koopa1->yvel = 0;

        /* make him line up with the top of a block works by clearing out the lower bits to 0 */
        koopa1->y &= ~0x3;

        /* move him down one because there is a one pixel gap in the image */
        koopa1->y++;

    } else {
        /* he is falling now */
        koopa1->falling = 1;
    }


    /* update animation if moving */
    if (koopa1->move) {
        koopa1->counter++;
        if (koopa1->counter >= koopa1->animation_delay) {
            koopa1->frame = koopa1->frame + 16;
            if (koopa1->frame > 16) {
                koopa1->frame = 0;
            }
            sprite_set_offset(koopa1->sprite, koopa1->frame);
            koopa1->counter = 0;
        }
    }

    /* set on screen position */
    sprite_position(koopa1->sprite, koopa1->x, koopa1->y);
}

/*update the third koopa*/
void koopa_update2(struct Koopa* koopa1, int xscroll) {
         /* update y position and speed if falling */
             if (koopa1->falling) {
                     koopa1->y += (koopa1->yvel >> 8);
                     koopa1->yvel += koopa1->gravity;
                 }
    
             /* check which tile the koopa's feet are over */
             unsigned short tile = tile_lookup(koopa1->x + 8, koopa1->y + 32, xscroll, 0, map,
                             map_width, map_height);
     
             /* if it's block tile
                   639      * these numbers refer to the tile indices of the blocks the koopa can walk on */
             if ((tile >= 1 && tile <= 6) ||
                             (tile >= 12 && tile <= 17)) {
                    /* stop the fall! */
                         koopa1->falling = 0;
                    koopa1->yvel = 0;
             
                         /* make him line up with the top of a block works by clearing out the lower bits to 0 */
                         koopa1->y &= ~0x3;
            
                         /* move him down one because there is a one pixel gap in the image */
                         koopa1->y++;
             } else {
                          /* he is falling now */
                              koopa1->falling = 1;
                      }
     
         
             /* update animation if moving */
             if (koopa1->move) {
                     koopa1->counter++;
                     if (koopa1->counter >= koopa1->animation_delay) {
                             koopa1->frame = koopa1->frame + 16;
                             if (koopa1->frame > 16) {
                                     koopa1->frame = 0;
                                 }
                             sprite_set_offset(koopa1->sprite, koopa1->frame);
                             koopa1->counter = 0;
                         }
                 }
     
             /* set on screen position */
             sprite_position(koopa1->sprite, koopa1->x, koopa1->y);
     }

/* function to set text on the screen at a given location */
 void set_text(char* str, int row, int col) {
              /* find the index in the texmap to draw to */
          int index = row * 32 + col; 
                      
                              /* the first 32 characters are missing from the map (controls etc.) */                   
           int missing = 32;
                                   
                                           /* pointer to text map */
           volatile unsigned short* ptr = screen_block(24);
                                                
                                                        /* for each character */
           while (*str) {
           /* place this character in the map */
           ptr[index] = *str - missing;
           /* move onto the next character */
           index++;
           str++;    
           } 
 }   

/*function to move a sprite and set an x-coordinate*/
void implement_AI(struct Koopa* koopa, struct Koopa* koopa1){
    if(button_pressed(0)||button_pressed(1)){
        koopa_left(koopa1);
        koopa1->x--;
        if(koopa1->x < 0){
            koopa1->x=240;
        }
        if(koopa->lives==0 || koopa->movement>1000){
            koopa1->x=240;
            
        }
    }
}    

/*function to move a sprite and set x-coordinates*/
void implement_AI1(struct Koopa* koopa, struct Koopa* koopa1){
         if(button_pressed(0)||button_pressed(1)){
                 koopa_left(koopa1);
                 koopa1->x--;
                 if(koopa1->x < 0){
                         koopa1->x=240;
                     }
                 if(koopa->lives==0 || koopa->movement>1000){
                    koopa1->x=240;
                 }
             }
        
     }
/*function to randomly generate an integer within a certain range*/
int random_int(int min, int max){
    return min + rand() % (max+1 - min);
}

/* assembly implementation of the strlen function */
int mystrlen(char* string);

/*assembly implementation of addition function*/
int add_asm(int a, int b);

/*assembly implementation of subtracition function*/
int sub_asm(int c, int d);

/*assembly implementation of multiplication function*/
int mul_asm(int e, int f);

/*function to increment the movement variable in a koopa*/
void increment_Movement(struct Koopa* koopa){
        koopa->movement++;
}

/*function to test whether the koopas collide with each other and adds
  the "You Win", "Lives", or "you lose" text to the screen*/
void collisions(struct Koopa* koopa, struct Koopa* koopa1, struct Koopa* koopa2){
    char text [32];
        
    if(koopa->movement>1000){
        sprintf(text, "You Win!!");
        set_text(text, 0, 0);
    }

    if((koopa->x >= koopa1->x-7 && koopa->x <= koopa1->x+7)&&
            koopa->y == koopa1->y){
        koopa->x=100;
        koopa1->x=random_int(220, 400);
        koopa2->x=random_int(220, 400);
        koopa->lives = sub_asm(koopa->lives, 1);
        sprintf(text, "Lives:  %d", koopa->lives);
        set_text(text, 0, 0);
        if(koopa->lives==0){
            
            sprintf(text, "Game Over");
            set_text(text, 0, 0);

        }
        

     }
    
    
}




/* the main function */
int main() {
    /* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE | BG1_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    /* create custom interrupt handler for vblank - whole point is to turn off sound at right time
    * we disable interrupts while changing them, to avoid breaking things */
    *interrupt_enable = 0;
    *interrupt_callback = (unsigned int) &on_vblank;
    *interrupt_selection |= INTERRUPT_VBLANK;
    *display_interrupts |= 0x08;
    *interrupt_enable = 1;

    /* clear the sound control initially */
    *sound_control = 0;

    /* set the music to play on channel A */
    play_sound(music, music_bytes,16000, 'A');
    
    /* setup the background 0 */
    setup_background();
   
    /* setup the sprite image data */
    setup_sprite_image();

    /* clear all the sprites on screen now */
    sprite_clear();

    /* create the koopa */
    struct Koopa koopa;
	struct Koopa koopa1;
    struct Koopa koopa2;
    koopa_init(&koopa);
	koopa_init1(&koopa1);
    koopa_init2(&koopa2);


    int result = 3;    
    /* print it */
    char text[32];
    sprintf(text, "Lives:  %d", result);
    set_text(text, 0, 0);


    /* set initial scroll to 0 */
    int xscroll = 0;
    /* loop forever */
    while (1) {





        /* update the koopa */
        koopa_update(&koopa, xscroll);
	    koopa_update1(&koopa1, xscroll);
        koopa_update2(&koopa2, xscroll);

        /* now the arrow keys move the koopa */
        if (button_pressed(BUTTON_RIGHT)) {
            /*this increments the movement variable in part of a koopa*/
            increment_Movement(&koopa);
            if (koopa_right(&koopa)) {
                xscroll++;
            }
        } else if (button_pressed(BUTTON_LEFT)) {
            if (koopa_left(&koopa)) {
                xscroll--;
            }
        } else {
            koopa_stop(&koopa);
	    	;
        }

        /* check for jumping */
        if (button_pressed(BUTTON_A)) {
            koopa_jump(&koopa);
        }
        /*this begins the artifical movement for both koopa sprites*/
    	implement_AI(&koopa,&koopa1);
        implement_AI(&koopa, &koopa2);

        /*this begins the detection of collisions between the sprites*/
	    collisions(&koopa, &koopa1, &koopa2);
        collisions(&koopa, &koopa2, &koopa1);

        /* wait for vblank before scrolling and moving sprites */
        wait_vblank();
        *bg1_x_scroll = xscroll;
        sprite_update_all();
        /* delay some */
        delay(300);
    }
}


