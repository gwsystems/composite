#include "ports.h"
#include "string.h"
#include "vga.h"

#define VIDEO_MEM 0xb8000

#define VGA_CTL_REG  0x3D4
#define VGA_DATA_REG 0x3D5

#define COLUMNS 80
#define LINES 25

/* FIXME these should go somewhere else */
#define BACKSPACE 0x08
#define TAB 0x09

enum vga_colors {
    BLACK = 0x00,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHT_GREY,
    DARK_GREY,
    LIGHT_BLUE,
    LIGHT_GREEN,
    LIGHT_CYAN,
    LIGHT_RED,
    LIGHT_MAGENTA,
    LIGHT_BROWN,
    WHITE
};

static uint16_t *video_mem = (uint16_t *)VIDEO_MEM;
static uint8_t cursor_x;
static uint8_t cursor_y;

static inline uint8_t 
gen_color(uint8_t forground, uint8_t background)
{
    return (background << 4) | (forground & 0x0F);
}

static void
update_cursor(uint8_t row, uint8_t col)
{
    uint16_t pos = row * COLUMNS + col;
    
    outb(VGA_CTL_REG, 0x0E);
    outb(VGA_DATA_REG, pos >> 8);
    outb(VGA_CTL_REG, 0x0F);
    outb(VGA_DATA_REG, pos);
}

static void
scroll(void)
{
    uint16_t blank = ((uint8_t)' ') | gen_color(WHITE, BLACK);
    unsigned i;

    if (cursor_y < LINES)
        return;

    for (i = 0; i < (LINES-1)*COLUMNS; i++) 
        video_mem[i] = video_mem[i + COLUMNS];

    wmemset(video_mem + ((LINES - 1)*COLUMNS), blank, COLUMNS); 
    cursor_y = LINES - 1;
}

void
vga__putch(char c)
{
    uint8_t color = gen_color(LIGHT_GREY, BLACK);
    uint16_t attribute = color << 8;
    uint16_t *location;

    if (c == BACKSPACE && cursor_x)
        cursor_x--;
    else if (c == TAB)
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    else if (c == '\r')
        cursor_x = 0; 
    else if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c >= ' ') {
        location = video_mem + (cursor_y * COLUMNS + cursor_x);
        *location = c | attribute;
        cursor_x++;
    }

    if (cursor_x >= COLUMNS) {
        cursor_x = 0;
        cursor_y++;
    }

    scroll();
    update_cursor(cursor_y, cursor_x);
}

void
vga__puts(const char *s)
{
    for (; *s != '\0'; s++)
        vga__putch(*s);

}

void
vga__clear(void)
{
    uint8_t color = gen_color(WHITE, BLACK);
    uint16_t blank = ((uint8_t)' ') | color << 8;
    wmemset(video_mem, blank, COLUMNS*LINES);
}

