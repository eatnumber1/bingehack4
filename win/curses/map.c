/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "nhcurses.h"

#define sgn(x) ((x) >= 0 ? 1 : -1)

struct color {
    short r, g, b;
};

static struct color oyellow, owhite, ohired, ohigreen, ohiyellow, ohiblue,
                    ohimagenta, ohicyan, ohiwhite;
static struct nh_dbuf_entry (*display_buffer)[COLNO] = NULL;
static const int xdir[8] = { -1,-1, 0, 1, 1, 1, 0,-1 };
static const int ydir[8] = {  0,-1,-1,-1, 0, 1, 1, 1 };

/*
 * Initialize curses colors to colors used by NetHack
 * (from Karl Garrison's curses UI for Nethack 3.4.3)
 */
void init_nhcolors(void)
{
    if (!has_colors())
	return;
    
    ui_flags.color = TRUE;
    
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLUE, -1); /* should be black, but COLOR_BLACK is invisible */
    init_pair(2, COLOR_RED, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_BLUE, -1);
    init_pair(6, COLOR_MAGENTA, -1);
    init_pair(7, COLOR_CYAN, -1);
    init_pair(8, -1, -1);

    if (COLORS >= 16) {
	init_pair(9, COLOR_WHITE, -1);
	init_pair(10, COLOR_RED + 8, -1);
	init_pair(11, COLOR_GREEN + 8, -1);
	init_pair(12, COLOR_YELLOW + 8, -1);
	init_pair(13, COLOR_BLUE + 8, -1);
	init_pair(14, COLOR_MAGENTA + 8, -1);
	init_pair(15, COLOR_CYAN + 8, -1);
	init_pair(16, COLOR_WHITE + 8, -1);
    }

    if (!can_change_color())
	return;
    
    /* Preserve initial terminal colors */
    color_content(COLOR_YELLOW, &oyellow.r, &oyellow.g, &oyellow.b);
    color_content(COLOR_WHITE, &owhite.r, &owhite.g, &owhite.b);
    
    /* Set colors to appear as NetHack expects */
    init_color(COLOR_YELLOW, 500, 300, 0);
    init_color(COLOR_WHITE, 600, 600, 600);
    
    if (COLORS >= 16) {
	/* Preserve initial terminal colors */
	color_content(COLOR_RED + 8, &ohired.r, &ohired.g, &ohired.b);
	color_content(COLOR_GREEN + 8, &ohigreen.r, &ohigreen.g, &ohigreen.b);
	color_content(COLOR_YELLOW + 8, &ohiyellow.r, &ohiyellow.g, &ohiyellow.b);
	color_content(COLOR_BLUE + 8, &ohiblue.r, &ohiblue.g, &ohiblue.b);
	color_content(COLOR_MAGENTA + 8, &ohimagenta.r, &ohimagenta.g, &ohimagenta.b);
	color_content(COLOR_CYAN + 8, &ohicyan.r, &ohicyan.g, &ohicyan.b);
	color_content(COLOR_WHITE + 8, &ohiwhite.r, &ohiwhite.g, &ohiwhite.b);
    
	/* Set colors to appear as NetHack expects */
	init_color(COLOR_RED + 8, 1000, 500, 0);
	init_color(COLOR_GREEN + 8, 0, 1000, 0);
	init_color(COLOR_YELLOW + 8, 1000, 1000, 0);
	init_color(COLOR_BLUE + 8, 0, 0, 1000);
	init_color(COLOR_MAGENTA + 8, 1000, 0, 1000);
	init_color(COLOR_CYAN + 8, 0, 1000, 1000);
	init_color(COLOR_WHITE + 8, 1000, 1000, 1000);
    }
}


void curses_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO])
{
    display_buffer = dbuf;
    draw_map(0);
}


void draw_map(int frame)
{
    int x, y, symcount, attr;
    struct curses_symdef syms[4];
    
    if (!display_buffer)
	return;
       
    for (y = 0; y < ROWNO; y++) {
	for (x = 1; x < COLNO; x++) {
	    /* set the position for each character to prevent incorrect
	     * positioning due to charset issues (IBM chars on a unicode term
	     * or vice versa) */
	    wmove(mapwin, y, x-1);
	    
	    symcount = mapglyph(&display_buffer[y][x], syms);
	    attr = A_NORMAL;
	    if (((display_buffer[y][x].monflags & MON_TAME) && settings.hilite_pet) ||
		((display_buffer[y][x].monflags & MON_DETECTED) && settings.use_inverse))
		attr |= A_REVERSE;

	    print_sym(mapwin, &syms[frame % symcount], attr);
	}
    }

    wrefresh(mapwin);
}


void curses_clear_map(void)
{
    werase(mapwin);
    wrefresh(mapwin);
}


int curses_getpos(int *x, int *y, boolean force, const char *goal)
{
    int result = 0;
    int cx, cy;
    int key, dx, dy;
    int sidx;
    static const char pick_chars[] = ".,;:";
    const char *cp;
    char printbuf[BUFSZ];
    char *matching = NULL;
    enum nh_direction dir;

    curs_set(1);
    
    cx = *x >= 1 ? *x : 1;
    cy = *y >= 0 ? *y : 0;
    wmove(mapwin, cy, cx-1);
    
    while (1) {
	dx = dy = 0;
	key = nh_wgetch(mapwin);
	if (key == KEY_ESC) {
	    cx = cy = -10;
	    result = -1;
	    break;
	}
	
	if ((cp = strchr(pick_chars, (char)key)) != 0) {
	    /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
	    result = cp - pick_chars;
	    break;
	}

	dir = key_to_dir(key);
	if (dir != DIR_NONE) {
	    dx = xdir[dir];
	    dy = ydir[dir];
	} else if ( (dir = key_to_dir(tolower((char)key))) != DIR_NONE ) {
	    /* a shifted movement letter */
	    dx = xdir[dir] * 8;
	    dy = ydir[dir] * 8;
	}
	
	if (dx || dy) {
	    /* truncate at map edge */
	    if (cx + dx < 1 || cx + dx > COLNO-1)
		dx = 0;
	    if (cy + dy < 0 || cy + dy > ROWNO-1)
		dy = 0;
	    cx += dx;
	    cy += dy;
	    goto nxtc;
	}

	    
	if (!strchr(quitchars, key)) {
	    matching = malloc(default_drawing->num_bgelements);
	    int k = 0, tx, ty;
	    int pass, lo_x, lo_y, hi_x, hi_y;
	    memset(matching, 0, default_drawing->num_bgelements);
	    for (sidx = 1; sidx < default_drawing->num_bgelements; sidx++)
		if (key == default_drawing->bgelements[sidx].ch)
		    matching[sidx] = (char) ++k;
	    if (k) {
		for (pass = 0; pass <= 1; pass++) {
		    /* pass 0: just past current pos to lower right;
			pass 1: upper left corner to current pos */
		    lo_y = (pass == 0) ? cy : 0;
		    hi_y = (pass == 0) ? ROWNO - 1 : cy;
		    for (ty = lo_y; ty <= hi_y; ty++) {
			lo_x = (pass == 0 && ty == lo_y) ? cx + 1 : 1;
			hi_x = (pass == 1 && ty == hi_y) ? cx : COLNO - 1;
			for (tx = lo_x; tx <= hi_x; tx++) {
			    k = display_buffer[ty][tx].bg;
			    if (k && matching[k]) {
				cx = tx;
				cy = ty;
				goto nxtc;
			    }
			}	/* column */
		    }	/* row */
		}		/* pass */
		sprintf(printbuf, "Can't find dungeon feature '%c'.", (char)key);
		curses_msgwin(printbuf);
		goto nxtc;
	    } else {
		sprintf(printbuf, "Unknown direction%s.", 
			!force ? " (aborted)" : "");
		curses_msgwin(printbuf);
	    }
	} /* !quitchars */
	if (force) goto nxtc;
	cx = -1;
	cy = 0;
	result = 0;	/* not -1 */
	break;
	    
nxtc:
	wmove(mapwin, cy, cx-1);
	wrefresh(mapwin);
    }
    
    curs_set(0);
    *x = cx;
    *y = cy;
    if (matching)
	free(matching);
    return result;
}