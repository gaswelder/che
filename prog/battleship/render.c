#link ncurses
#include <curses.h>
#import game.c
#import rnd
#import ai.c

#define BWIDTH 10
#define BDEPTH 10
enum {
	PLAYER = 0,
	COMPUTER = 1,
};
const int SHIPTYPES = 5;

#define SHOWSPLASH ' '
#define SHOWHIT '*'
const int PROMPTLINE = 21;
const int CXBASE = 48;
const int CYBASE = 3;
const int HXBASE = 0;
const int HYBASE = 3 + BDEPTH + 3 - 1;
// const int MXBASE = 64;
// const int MYBASE = 3 + BDEPTH + 3 - 1;
const int PXBASE = 3;
const int PYBASE = 3;
// const int SXBASE = 63;
// const int SYBASE = 3 + BDEPTH + 3;
int COLWIDTH = 80;
const char *numbers = "   0  1  2  3  4  5  6  7  8  9";
const char *name = "stranger";

void cgoto(int y, x) {
	OS.move(CYBASE + y, CXBASE + x*3);
}

void pgoto(int y, x) {
	OS.move(PYBASE + y, PXBASE + x*3);
}

pub void init() {
	OS.initscr();
	if (OS.KEY_MIN) OS.keypad(OS.stdscr, true);
	OS.saveterm();
	OS.nonl();
	OS.cbreak();
	OS.noecho();
	if (OS.A_COLOR) {
		OS.start_color();
		OS.init_pair(OS.COLOR_BLACK, OS.COLOR_BLACK, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_GREEN, OS.COLOR_GREEN, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_RED, OS.COLOR_RED, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_CYAN, OS.COLOR_CYAN, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_WHITE, OS.COLOR_WHITE, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_MAGENTA, OS.COLOR_MAGENTA, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_BLUE, OS.COLOR_BLUE, OS.COLOR_BLACK);
		OS.init_pair(OS.COLOR_YELLOW, OS.COLOR_YELLOW, OS.COLOR_BLACK);
	}
}

pub void end() {
	OS.clear();
	OS.refresh();
	OS.resetterm();
	OS.echo();
	OS.endwin();
}

pub void reset() {
	OS.clear();
	OS.mvaddstr(0, 35, "BATTLESHIPS");
	OS.move(PROMPTLINE + 2, 0);
	draw_empty_boards();
	draw_manual();
}

void draw_empty_boards() {
	OS.mvaddstr(PYBASE - 2, PXBASE + 5, "Main Board");
	OS.mvaddstr(PYBASE - 1, PXBASE - 3, numbers);
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(PYBASE + i, PXBASE - 3, (i + 'A'));
		if (OS.has_colors()) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_BLUE));
		}
		OS.addch(' ');
		for (int j = 0; j < BWIDTH; j++) {
			OS.addstr(" . ");
		}
		OS.attrset(0);
		OS.addch(' ');
		OS.addch((i + 'A'));
	}
	OS.mvaddstr(PYBASE + BDEPTH, PXBASE - 3, numbers);
	OS.mvaddstr(CYBASE - 2, CXBASE + 7, "Hit/Miss Board");
	OS.mvaddstr(CYBASE - 1, CXBASE - 3, numbers);
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(CYBASE + i, CXBASE - 3, (i + 'A'));
		if (OS.has_colors()) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_BLUE));
		}
		OS.addch(' ');
		for (int j = 0; j < BWIDTH; j++) {
			OS.addstr(" . ");
		}
		OS.attrset(0);
		OS.addch(' ');
		OS.addch(i + 'A');
	}
	OS.mvaddstr(CYBASE + BDEPTH, CXBASE - 3, numbers);
}

void draw_manual() {
	OS.mvprintw(HYBASE, HXBASE, "To position your ships: move the cursor to a spot, then");
	OS.mvprintw(HYBASE + 1, HXBASE, "type the first letter of a ship type to select it, then");
	OS.mvprintw(HYBASE + 2, HXBASE, "type a direction ([hjkl] or [4862]), indicating how the");
	OS.mvprintw(HYBASE + 3, HXBASE, "ship should be pointed. You may also type a ship letter");
	OS.mvprintw(HYBASE + 4, HXBASE, "followed by `r' to position it randomly, or type `R' to");
	OS.mvprintw(HYBASE + 5, HXBASE, "place all remaining ships randomly.");
}



pub void draw_placed_ship(game.ship_t *ss) {
	for (int l = 0; l < ss->length; ++l) {
		game.xy_t xy = game.shipxy(ss->x, ss->y, ss->dir, l);
		pgoto(xy.y, xy.x);
		OS.addch(ss->symbol);
	}
}

pub void render_placement_prompt(game.state_t *g) {
	render_prompt(1, "Type one of [%s] to pick a ship.", g->docked + 1);
}

void render_ai_shot(game.state_t *g, ai.state_t *ai) {
	int x = ai->ai_last_x;
	int y = ai->ai_last_y;
	if (g->players[COMPUTER].shots[x][y] == game.MARK_HIT) {
		render_ai_hit(g, ai);
	} else {
		render_ai_miss(ai);
	}
}

pub void render_penguin() {
	OS.clear();
	OS.mvaddstr(4, 29, "Welcome to Battleship!");
	OS.move(8,0);
	OS.addstr("                                                  \\\n");
	OS.addstr("                           \\                     \\ \\\n");
	OS.addstr("                          \\ \\                   \\ \\ \\_____________\n");
	OS.addstr("                         \\ \\ \\_____________      \\ \\/            |\n");
	OS.addstr("                          \\ \\/             \\      \\/             |\n");
	OS.addstr("                           \\/               \\_____/              |__\n");
	OS.addstr("           ________________/                                       |\n");
	OS.addstr("           \\  S.S. Penguin                                         |\n");
	OS.addstr("            \\                                                     /\n");
	OS.addstr("             \\___________________________________________________/\n");
	OS.mvaddstr(22, 27, "Hit any key to continue...");
	OS.refresh();
}

pub void render_current_coords(game.xy_t xy, int atcpu) {
	if (atcpu) {
		OS.mvprintw(CYBASE + BDEPTH + 1, CXBASE + 11, "(%d, %c)", xy.x, 'A' + xy.y);
		cgoto(xy.y, xy.x);
	} else {
		OS.mvprintw(PYBASE + BDEPTH + 1, PXBASE + 11, "(%d, %c)", xy.x, 'A' + xy.y);
		pgoto(xy.y, xy.x);
	}
}

pub void render_cursor(game.xy_t xy, int atcpu) {
	if (atcpu) {
		cgoto(xy.y, xy.x);
	} else {
		pgoto(xy.y, xy.x);
	}
	OS.refresh();
}

pub void render_manual2() {
	OS.mvprintw(HYBASE, HXBASE, "To fire, move the cursor to your chosen aiming point   ");
	OS.mvprintw(HYBASE + 1, HXBASE, "and strike any key other than a motion key.            ");
	OS.mvprintw(HYBASE + 2, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 3, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 4, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 5, HXBASE, "                                                       ");
	render_prompt(0, "Press any key to start...", "");
}

pub void render_placement_error(int err) {
	if (err == game.ERR_HANGING) {
		switch (rnd.intn(3)) {
			case 0: { render_error("Ship is hanging from the edge of the world"); }
			case 1: { render_error("Try fitting it on the board"); }
			case 2: { render_error("Figure I won't find it if you put it there?"); }
		}
	}
	if (err == game.ERR_COLLISION) {
		switch (rnd.intn(3)) {
			case 0: { render_error("There's already a ship there"); }
			case 1: { render_error("Collision alert!  Aaaaaagh!"); }
			case 2: { render_error("Er, Admiral, what about the other ship?"); }
		}
	}
}

pub void render_prompt(int n, const char *f, const char *s) {
	OS.move(PROMPTLINE + n, 0);
	OS.clrtoeol();
	OS.printw(f, s);
	OS.refresh();
}

pub void render_error(const char *s) {
	OS.move(PROMPTLINE + 2, 0);
	OS.clrtoeol();
	if (s) {
		OS.addstr(s);
		OS.beep();
	}
}

void render_player_shot(game.state_t *g) {
	int cury = g->cury;
	int curx = g->curx;
	bool hit = g->player_hit;
	game.ship_t *sunk = g->sunk_by_player;
	cgoto(cury, curx);
	if (OS.has_colors()) {
		if (hit) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
		} else {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
		}
	}
	OS.addch(g->players[PLAYER].shots[curx][cury]);
	OS.attrset(0);
	if (hit) {
		render_prompt(1, "You scored a hit.", "");
	} else {
		render_prompt(1, "You missed.", "");
	}
	if (sunk) {
		char *m;
		switch (rnd.intn(5)) {
			case 0: { m = " You sank my %s!"; }
			case 1: { m = " I have this sinking feeling about my %s...."; }
			case 2: { m = " My %s has gone to Davy Jones's locker!"; }
			case 3: { m = " Glub, glub -- my %s is headed for the bottom!"; }
			case 4: { m = " You'll pick up survivors from my %s, I hope...!"; }
		}
		OS.printw(m, game.shipname(sunk->kind));
		OS.beep();
	}
}

void render_ai_hit(game.state_t *g, ai.state_t *ai) {
	int x = ai->ai_last_x;
	int y = ai->ai_last_y;
	OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "hit");
	if (g->sunk_by_ai) {
		OS.printw(" I've sunk your %s", game.shipname(g->sunk_by_ai->kind));
	}
	OS.clrtoeol();
	pgoto(y, x);
	if (OS.has_colors()) {
		OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
	}
	OS.addch(SHOWHIT);
	OS.attrset(0);
}

void render_ai_miss(ai.state_t *ai) {
	int x = ai->ai_last_x;
	int y = ai->ai_last_y;
	OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "miss");
	OS.clrtoeol();
	pgoto(y, x);
	if (OS.has_colors()) {
		OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
	}
	OS.addch(SHOWSPLASH);
	OS.attrset(0);
}

pub void render_cpu_ships(game.state_t *g) {
	int j;
	game.ship_t *ss;
	for (ss = g->players[COMPUTER].ships; ss < g->players[COMPUTER].ships + SHIPTYPES; ss++) {
		for (j = 0; j < ss->length; j++) {
			game.xy_t xy = game.shipxy(ss->x, ss->y, ss->dir, j);
			cgoto(xy.y, xy.x);
			OS.addch(ss->symbol);
		}
	}
}

pub void render_winner(game.state_t *g) {
	int j = 18 + strlen(name);
	int pwins = g->players[PLAYER].wins;
	int cwins = g->players[COMPUTER].wins;
	if (pwins >= 10) ++j;
	if (cwins >= 10) ++j;
	OS.mvprintw(1, (COLWIDTH - j) / 2, "%s: %d     Computer: %d", name, pwins, cwins);
}

pub void render_playagain_prompt(game.state_t *g) {
	if (g->winner == COMPUTER) {
		render_prompt(2, "Want to be humiliated again [yn]?", "");
	} else {
		render_prompt(2, "Going to give me a chance for revenge [yn]?", "");
	}
}

pub void render_turn(game.state_t *g, ai.state_t *ai) {
	if (g->turn == COMPUTER) {
		render_ai_shot(g, ai);
	} else {
		render_player_shot(g);
	}
}

void draw_revealed_ship(game.state_t *g, int player, game.ship_t *ss) {
	for (int i = 0; i < ss->length; ++i) {
		game.xy_t xy = game.shipxy(ss->x, ss->y, ss->dir, i);
		int x1 = xy.x;
		int y1 = xy.y;

		g->players[player].shots[x1][y1] = ss->symbol;
		if (player == PLAYER) {
			cgoto(y1, x1);
			OS.addch(ss->symbol);
		} else {
			pgoto(y1, x1);
			if (OS.has_colors()) {
				OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
			}
			OS.addch(SHOWHIT);
			OS.attrset(0);
		}
	}
}

void draw_empty_water(game.state_t *g, game.ship_t *ss) {
	if (g->closepack) return;
	for (int j = -1; j <= 1; j++) {
		game.xy_t xy = game.shipxy(ss->x, ss->y, (ss->dir + 2) % 8, j);
		int bx = xy.x;
		int by = xy.y;
		for (int i = -1; i <= ss->length; ++i) {
			game.xy_t xy1 = game.shipxy(bx, by, ss->dir, i);
			int x1 = xy1.x;
			int y1 = xy1.y;
			if (game.coords_valid(x1, y1)) {
				if (g->turn % 2 == PLAYER) {
					cgoto(y1, x1);
					if (OS.has_colors()) {
						OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
					}
					OS.addch(game.MARK_MISS);
					OS.attrset(0);
				} else {
					pgoto(y1, x1);
					OS.addch(SHOWSPLASH);
				}
			}
		}
	}
}

pub void render_hit_ship(game.state_t *g, game.ship_t *ss) {
	int oldx;
	int oldy;
	OS.getyx(OS.stdscr, oldy, oldx);
	draw_empty_water(g, ss);
	draw_revealed_ship(g, g->turn, ss);
	OS.move(oldy, oldx);
}

pub void render_clear_coords(int atcpu) {
	if (atcpu) {
		OS.mvaddstr(CYBASE + BDEPTH + 1, CXBASE + 11,"      ");
	} else {
		OS.mvaddstr(PYBASE + BDEPTH + 1, PXBASE + 11,"      ");
	}
}
