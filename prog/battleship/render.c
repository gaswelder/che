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
const int PROMPTLINE = 14;
const int COMPUTER_FIELD_X = 48;
const int CYBASE = 3;
const int PLAYER_FIELD_X = 3;

// Upper y-coordinate of the boards.
const int BOARD_TOP = 3;

int COLWIDTH = 80;
const char *numbers = "   0  1  2  3  4  5  6  7  8  9";
const char *name = "stranger";

void cgoto(int y, x) {
	OS.move(CYBASE + y, COMPUTER_FIELD_X + x*3);
}

void pgoto(int y, x) {
	OS.move(BOARD_TOP + y, PLAYER_FIELD_X + x*3);
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
}

void draw_board(const char *title, int x0) {
	OS.mvaddstr(BOARD_TOP - 2, x0 + 5, title);
	OS.mvaddstr(BOARD_TOP - 1, x0 - 3, numbers);
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(BOARD_TOP + i, x0 - 3, (i + 'A'));
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
	OS.mvaddstr(BOARD_TOP + BDEPTH, x0 - 3, numbers);
}

void draw_empty_boards() {
	draw_board("Player", PLAYER_FIELD_X);
	OS.mvaddstr(CYBASE - 2, COMPUTER_FIELD_X + 7, "Computer");
	OS.mvaddstr(CYBASE - 1, COMPUTER_FIELD_X - 3, numbers);
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(CYBASE + i, COMPUTER_FIELD_X - 3, (i + 'A'));
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
	OS.mvaddstr(CYBASE + BDEPTH, COMPUTER_FIELD_X - 3, numbers);
}

void clear_field() {
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(BOARD_TOP + i, PLAYER_FIELD_X - 3, (i + 'A'));
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
}

pub void draw_placed_ships(game.state_t *g) {
	clear_field();
	for (int i = 0; i < SHIPTYPES; i++) {
		game.ship_t *ss = &g->players[PLAYER].ships[i];
		if (!ss->placed) continue;
		for (int l = 0; l < ss->length; ++l) {
			game.xy_t xy = game.shipxy(ss->x, ss->y, ss->dir, l);
			pgoto(xy.y, xy.x);
			OS.addch(ss->symbol);
		}
	}
}

pub void render_cursor(game.state_t *g) {
	game.xy_t xy = g->render_cursor;
	int atcpu = g->render_cursor_at;
	if (atcpu) {
		cgoto(xy.y, xy.x);
		OS.mvprintw(CYBASE + BDEPTH + 1, COMPUTER_FIELD_X + 11, "(%d, %c)", xy.x, 'A' + xy.y);
		cgoto(xy.y, xy.x);
	} else {
		pgoto(xy.y, xy.x);
		OS.mvprintw(BOARD_TOP + BDEPTH + 1, PLAYER_FIELD_X + 11, "(%d, %c)", xy.x, 'A' + xy.y);
		pgoto(xy.y, xy.x);
	}
	OS.refresh();
}

pub void render_clear_coords() {
	OS.mvaddstr(CYBASE + BDEPTH + 1, COMPUTER_FIELD_X + 11,"      ");
	OS.mvaddstr(BOARD_TOP + BDEPTH + 1, PLAYER_FIELD_X + 11,"      ");
}

pub void draw_status(game.state_t *g) {
	OS.move(PROMPTLINE + 1, 0);
	OS.clrtoeol();
	OS.printw(" *** %s", g->status);
	OS.refresh();
}



const char *errstr_hanging[] = {
	"Ship is hanging from the edge of the world",
	"Try fitting it on the board",
	"Figure I won't find it if you put it there?",
};
const char *errstr_collision[] = {
	"There's already a ship there",
	"Collision alert!  Aaaaaagh!",
	"Er, Admiral, what about the other ship?",
};

pub void draw_error(game.state_t *g) {
	const char *s;
	switch (g->error) {
		case 0: { s = NULL; }
		case game.ERR_HANGING: { s = errstr_hanging[rnd.intn(3)]; }
		case game.ERR_COLLISION: { s = errstr_collision[rnd.intn(3)]; }
		default: { s = "unknown error"; }
	}
	OS.move(PROMPTLINE + 2, 0);
	OS.clrtoeol();
	if (s) {
		OS.printw("%s", s);
	}
	OS.refresh();
}


pub void render_player_shot(game.state_t *g) {
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
		sprintf(g->status, "You scored a hit.");
	} else {
		sprintf(g->status, "You missed.");
	}
	draw_status(g);
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
	sprintf(g->status, "Play again? [yn]");
	draw_status(g);
}

pub void render_turn(game.state_t *g, ai.state_t *ai) {
	if (g->turn == COMPUTER) {
		render_ai_turn(g, ai);
	} else {
		render_player_shot(g);
	}
}

pub void render_ai_turn(game.state_t *g, ai.state_t *ai) {
	int x = ai->ai_last_x;
	int y = ai->ai_last_y;
	OS.clrtoeol();
	pgoto(y, x);
	if (g->players[COMPUTER].shots[x][y] == game.MARK_HIT) {
		if (OS.has_colors()) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
		}
		OS.addch(SHOWHIT);
	} else {
		if (OS.has_colors()) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
		}
		OS.addch(SHOWSPLASH);
	}
	OS.attrset(0);
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

pub void refresh(game.state_t *g) {
	switch (g->state) {
		case game.ST_PLACING: {
			draw_status(g);
			draw_placed_ships(g);
			draw_error(g);
			render_cursor(g);
		}
		case game.ST_PLAYING: {
			draw_status(g);
			draw_log(g);
		}
		case game.ST_FINISHED: {
			render_cpu_ships(g);
			render_winner(g);
			render_playagain_prompt(g);
		}
	}
}

void draw_log(game.state_t *g) {
	for (int i = 0; i < g->logsize; i++) {
		OS.move(17 + i, 0);
		OS.clrtoeol();
		OS.printw("%s", g->log[i]);
		OS.refresh();
	}
}
