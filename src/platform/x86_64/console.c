#define ENABLE_CONSOLE

#include "io.h"
#include "string.h"
#include "isr.h"
#include "kernel.h"

#define VIDEO_MEM 0xb8000

#define VGA_CTL_REG 0x3D4
#define VGA_DATA_REG 0x3D5

#define KEY_DEVICE 0x60
#define KEY_PENDING 0x64

#define COLUMNS 80
#define LINES 25

/* FIXME these should go somewhere else */
#define BACKSPACE 0x08
#define TAB 0x09

enum vga_colors
{
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
static u8_t   cursor_x;
static u8_t   cursor_y;

static void
wmemset(void *dst, int c, size_t count)
{
	unsigned short *tmp = (unsigned short *)dst;

	for (; count != 0; count--) *tmp++ = c;
}

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
	u16_t    blank = ((u8_t)' ') | gen_color(WHITE, BLACK);
	unsigned i;

	if (cursor_y < LINES) return;

	for (i = 0; i < (LINES - 1) * COLUMNS; i++) video_mem[i] = video_mem[i + COLUMNS];

	wmemset(video_mem + ((LINES - 1) * COLUMNS), blank, COLUMNS);
	cursor_y = LINES - 1;
}

static void
vga_putch(char c)
{
	u8_t   color     = gen_color(LIGHT_GREY, BLACK);
	u16_t  attribute = color << 8;
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
		location  = video_mem + (cursor_y * COLUMNS + cursor_x);
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
vga_puts(const char *s)
{
	for (; *s != '\0'; s++) vga_putch(*s);
}

void
vga_clear(void)
{
	u8_t  color = gen_color(WHITE, BLACK);
	u16_t blank = ((u8_t)' ') | color << 8;
	wmemset(video_mem, blank, COLUMNS * LINES);
}

int
keyboard_handler(struct pt_regs *regs)
{
	u16_t scancode;
	int   preempt = 1;

	ack_irq(IRQ_KEYBOARD);

	while (inb(KEY_PENDING) & 2) {
		/* wait for keypress to be ready */
	}
	scancode = inb(KEY_DEVICE);
	printk("Keyboard press: %d\n", scancode);
	return preempt;
}

void
console_init(void)
{
	vga_clear();
	printk_register_handler(vga_puts);
}
