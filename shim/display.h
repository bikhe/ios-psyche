#pragma once

/* Make the ABL splash framebuffer reach the panel on its own (display.c). */
void display_start(void);
/* Print what display_start() found on the console. */
void display_report(void);
