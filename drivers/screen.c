#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/smp.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 50
#define SCREEN_LOC(x, y) ((y)*SCREEN_WIDTH + (x))

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = { 0 };
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = { 0 };

/* cursor position */
static void vt100_move_cursor(int x, int y)
{
	// \033[y;xH
	printv("%c[%d;%dH", 27, y + 1, x + 1);
}

/* clear screen */
static void vt100_clear()
{
	// \033[2J
	printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor()
{
	// \033[?25l
	printv("%c[?25l", 27);
}

/* show cursor */
static void vt100_show_cursor()
{
	//\033[?25h
	printv("%c[?25h", 27);
}

/* write a char */
static void screen_write_ch(char ch)
{
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	if (ch == '\n') {
		current_running->cursor_x = 0;
		current_running->cursor_y++;
	} else {
		new_screen[SCREEN_LOC(current_running->cursor_x,
				      current_running->cursor_y)] = ch;
		current_running->cursor_x++;
	}
}

void init_screen(void)
{
	vt100_hidden_cursor();
	vt100_clear();
	screen_clear();
}

void screen_clear(void)
{
	spin_lock_acquire(&print_lock);
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	int i, j;
	for (i = 0; i < SCREEN_HEIGHT; i++) {
		for (j = 0; j < SCREEN_WIDTH; j++) {
			new_screen[SCREEN_LOC(j, i)] = ' ';
		}
	}
	current_running->cursor_x = 0;
	current_running->cursor_y = 0;
	spin_lock_release(&print_lock);
	screen_reflush();
}

void screen_move_cursor(int x, int y)
{
	spin_lock_acquire(&print_lock);
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	current_running->cursor_x = x;
	current_running->cursor_y = y;
	vt100_move_cursor(x, y);
	spin_lock_release(&print_lock);
}

void screen_write(char *buff)
{
	spin_lock_acquire(&print_lock);
	int i = 0;
	int l = strlen(buff);

	for (i = 0; i < l; i++) {
		screen_write_ch(buff[i]);
	}
	spin_lock_release(&print_lock);
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void)
{
	spin_lock_acquire(&print_lock);
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	int i, j;

	/* here to reflush screen buffer to serial port */
	for (i = 0; i < SCREEN_HEIGHT; i++) {
		for (j = 0; j < SCREEN_WIDTH; j++) {
			/* We only print the data of the modified location. */
			if (new_screen[SCREEN_LOC(j, i)] !=
			    old_screen[SCREEN_LOC(j, i)]) {
				vt100_move_cursor(j, i);
				bios_putchar(new_screen[SCREEN_LOC(j, i)]);
				old_screen[SCREEN_LOC(j, i)] =
					new_screen[SCREEN_LOC(j, i)];
			}
		}
	}

	/* recover cursor position */
	vt100_move_cursor(current_running->cursor_x, current_running->cursor_y);
	spin_lock_release(&print_lock);
}

void screen_backspace(void)
{
	spin_lock_acquire(&print_lock);
	uint64_t cpuID = get_current_cpu_id();
	pcb_t *volatile current_running = cpu[cpuID].current_running;
	if (current_running->cursor_x <= 0)
		return;
	vt100_move_cursor(--current_running->cursor_x,
			  current_running->cursor_y);
	screen_write_ch(' ');
	spin_lock_release(&print_lock);
	screen_reflush();
	spin_lock_acquire(&print_lock);
	vt100_move_cursor(--current_running->cursor_x,
			  current_running->cursor_y);
	spin_lock_release(&print_lock);
}

void screen_hidden_cursor(void)
{
	vt100_hidden_cursor();
}

void screen_show_cursor(void)
{
	vt100_show_cursor();
}
