#define ENABLE_VGA
/*
 * Phani: 09/17/2015
 *
 * Code reuse:
 * from: http://www.osdever.net/bkerndev/index.php 
 * from: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#kernel_002ec
 *
 */

#include <chal.h>
#include "kernel.h"
#include "string.h"
#include "io.h"
#include "isr.h"

#define STRLEN_MAX (81)

/* The number of columns. */
#define COLUMNS                 80
/* The number of lines. */
#define LINES                   25
/* The attribute of an character. */
#define ATTRIBUTE               0x0F
/* The video memory address. */
#define VIDEO                   0xB8000

#define SPACE			0x20

#define MAX_VGA_BUF			(2 * ((COLUMNS * LINES) + COLUMNS))

#define VGA_CTL_REG  0x3D4
#define VGA_DATA_REG 0x3D5

#define KEY_DEVICE    0x60
#define KEY_PENDING   0x64

/* Variables. */
/* Save the X position. */
static int csr_x;
/* Save the Y position. */
static int csr_y;
/* Point to the video memory. */
static volatile unsigned char *video;
/* Attribute of a character */
int attrib = ATTRIBUTE;

/* Forward declarations. */
static unsigned short *memsetw(unsigned short *dest, unsigned short val, size_t count);
static void puts(unsigned char *str);
static void move_csr(void);
static void cls(void);
static void cll(void);
static void putchar (int c);

/* Utility: set 16bit memory with 16bit value */
static unsigned short *memsetw(unsigned short *dest, unsigned short val, size_t count)
{
    /* Same as memset, but this time, we're working with a 16-bit
    *  'val' and dest pointer. Your code can be an exact copy of
    *  the memset, provided that your local variables if any, are
    *  unsigned short */
    unsigned short *temp = (unsigned short *)dest;
    for( ; count != 0; count--) *temp++ = val;
    return dest;
}

/* Updates the hardware cursor: the little blinking line
 *  on the screen under the last character pressed! */
static void move_csr(void)
{
	unsigned pos;

	/* The equation for finding the index in a linear
	 *  chunk of memory can be represented by:
	 *  Index = [(y * width) + x] */
	pos = csr_y * COLUMNS + csr_x;

	/* This sends a command to indicies 14 and 15 in the
	 *  CRT Control Register of the VGA controller. These
	 *  are the high and low bytes of the index that show
	 *  where the hardware cursor is to be 'blinking'. To
	 *  learn more, you should look up some VGA specific
	 *  programming documents. A great start to graphics:
	 *  http://www.brackeen.com/home/vga */
	outb(VGA_CTL_REG, 14);
	outb(VGA_DATA_REG, pos >> 8);
	outb(VGA_CTL_REG, 15);
	outb(VGA_DATA_REG, pos);
}

/* Clears the line from current y position */
static void cll(void)
{
	/* Again, we need the 'short' that will be used to
	 *  represent a space with color */
	unsigned blank = SPACE | (attrib << 8);
	/* Sets the entire screen to spaces in our current
	 *  color */
	memsetw ((unsigned short *)video + csr_y * COLUMNS + csr_x, blank, COLUMNS - csr_x);
}


/* Clears the screen */
static void cls(void)
{
	unsigned blank;
	int i;

	/* Again, we need the 'short' that will be used to
	 *  represent a space with color */
	blank = SPACE | (attrib << 8);

	/* Sets the entire screen to spaces in our current
	 *  color */
	for(i = 0; i < LINES; i++)
		memsetw ((unsigned short *)video + i * COLUMNS, blank, COLUMNS);

	/* Update out virtual cursor, and then move the
	 *  hardware cursor */
	csr_x = 0;
	csr_y = 0;
	move_csr();
}

/* Clear the screen and initialize VIDEO, XPOS and YPOS. */
void
vga_init (void)
{
	int i;

	video = (unsigned char *) VIDEO;

	csr_x = 0;
	csr_y = 0;
	cls();
	printk_register_handler(vga_puts);
}

/* VIDEO - High address. */
void
vga_high_init (void)
{
	video = chal_pa2va(VIDEO);
}

/* Put the character C on the screen. */
static void
putchar (int c)
{
	if (c == '\n' || c == '\r')
	{
newline:
		cll();
		csr_x = 0;
		csr_y++;
		if (csr_y >= LINES)
			//like rotation and not scrolling up!! 
			csr_y = 0;
		return;
	}

	*(video + (csr_x + csr_y * COLUMNS) * 2) = c & 0xFF;
	*(video + (csr_x + csr_y * COLUMNS) * 2 + 1) = attrib;

	csr_x++;
	if (csr_x >= COLUMNS)
		goto newline;
}

void
vga_puts(const char *s)
{
	puts((unsigned char *)s);
}

/* Uses the above routine to output a string... */
static void puts(unsigned char *text)
{
	size_t i;

	cll();
	for (i = 0; i < strnlen((const char*)text, STRLEN_MAX); i++)
	{
		putchar(text[i]);
	}
	move_csr();
}


void
keyboard_handler(struct pt_regs *regs)
{
	u16_t scancode;

	ack_irq(IRQ_KEYBOARD);

	while(inb(KEY_PENDING) & 2) {
		/* wait for keypress to be ready */
	}
	scancode = inb(KEY_DEVICE);
	printk("Keyboard press: %d\n", scancode);
}

