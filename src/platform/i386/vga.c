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

static u16_t *video_mem = (u16_t *)VIDEO_MEM;
static u8_t cursor_x;
static u8_t cursor_y;

static inline u8_t 
gen_color(u8_t forground, u8_t background)
{
    return (background << 4) | (forground & 0x0F);
}

static void
update_cursor(u8_t row, u8_t col)
{
    u16_t pos = row * COLUMNS + col;
    
    outb(VGA_CTL_REG, 0x0E);
    outb(VGA_DATA_REG, pos >> 8);
    outb(VGA_CTL_REG, 0x0F);
    outb(VGA_DATA_REG, pos);
}

static void
scroll(void)
{
    u16_t blank = ((u8_t)' ') | gen_color(WHITE, BLACK);
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
    u8_t color = gen_color(LIGHT_GREY, BLACK);
    u16_t attribute = color << 8;
    u16_t *location;

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
    u8_t color = gen_color(WHITE, BLACK);
    u16_t blank = ((u8_t)' ') | color << 8;
    wmemset(video_mem, blank, COLUMNS*LINES);
}

