#link ncurses
#include <curses.h>

#import game.c
#import rnd

#define BWIDTH 10
#define BDEPTH 10
enum {
	PLAYER = 0,
	COMPUTER = 1,
}
const int SHIPTYPES = 5;

const int STATUS_Y = 14;

// Upper y-coordinate of the fields.
const int BOARD_Y = 3;

// x-coordinates of the two fields
const int PLAYER_FIELD_X = 3;
const int COMPUTER_FIELD_X = 48;

// pgoto (player) moves the cursor to the (x, y) of the left field.
// cgoto (computer) moves the cursor to the (x,y) of the right field.
void pgoto(int y, x) { OS.move(BOARD_Y + y, PLAYER_FIELD_X + x*3); }
void cgoto(int y, x) { OS.move(BOARD_Y + y, COMPUTER_FIELD_X + x*3); }

int COLWIDTH = 80;

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

const char *msg_you_sunk[] = {
	"You sank my %s!",
	"I have this sinking feeling about my %s....",
	"My %s has gone to Davy Jones's locker!",
	"Glub, glub -- my %s is headed for the bottom!",
	"You'll pick up survivors from my %s, I hope...!",
};

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
}

pub void render_cursor(game.state_t *g) {
	game.xy_t xy = g->render_cursor;
	int atcpu = g->render_cursor_at;
	if (atcpu) {
		cgoto(xy.y, xy.x);
	} else {
		pgoto(xy.y, xy.x);
	}
	OS.refresh();
}

pub void refresh(game.state_t *g) {
	switch (g->state) {
		case game.ST_PLACING: {
			draw_field("Player", PLAYER_FIELD_X);
			draw_status(g);
			draw_ships(g, PLAYER);
			draw_error(g);
			render_cursor(g);
		}
		case game.ST_PLAYING: {
			draw_field("Player", PLAYER_FIELD_X);
			draw_field("Computer", COMPUTER_FIELD_X);
			draw_field_contents(g, COMPUTER);
			draw_ships(g, PLAYER);
			draw_field_contents(g, PLAYER);
			if (g->turn == PLAYER) {
				bool hit = g->player_hit;
				game.ship_t *sunk = g->sunk_by_player;
				if (sunk) {
					sprintf(g->status, msg_you_sunk[rnd.intn(5)], game.shipname(sunk->kind));
				} else if (hit) {
					sprintf(g->status, "You scored a hit.");
				} else {
					sprintf(g->status, "You missed.");
				}
			}
			draw_status(g);
			draw_log(g);
		}
		case game.ST_FINISHED: {
			draw_ships(g, COMPUTER);
			draw_score(g);
			sprintf(g->status, "Play again? [yn]");
			draw_status(g);
		}
	}
}

void draw_score(game.state_t *g) {
	char *name = "Player";
	int j = 18 + strlen(name);
	int pwins = g->players[PLAYER].wins;
	int cwins = g->players[COMPUTER].wins;
	if (pwins >= 10) ++j;
	if (cwins >= 10) ++j;
	OS.mvprintw(1, (COLWIDTH - j) / 2, "%s: %d     Computer: %d", name, pwins, cwins);
}

const char *LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void draw_field(const char *title, int x0) {
	const char *numbers = "   0  1  2  3  4  5  6  7  8  9";
	OS.mvaddstr(BOARD_Y - 2, x0 + 5, title);
	OS.mvaddstr(BOARD_Y - 1, x0 - 3, numbers);
	for (int i = 0; i < BDEPTH; ++i) {
		OS.mvaddch(BOARD_Y + i, x0 - 3, LETTERS[i]);
		if (OS.has_colors()) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_BLUE));
		}
		OS.addch(' ');
		for (int j = 0; j < BWIDTH; j++) {
			OS.addstr("   ");
		}
		OS.attrset(0);
		OS.addch(' ');
		OS.addch(LETTERS[i]);
	}
	OS.mvaddstr(BOARD_Y + BDEPTH, x0 - 3, numbers);
}

void draw_field_contents(game.state_t *g, int player) {
	for (int x = 0; x < 10; x++) {
		for (int y = 0; y < 10; y++) {
			if (player == COMPUTER) {
				cgoto(y, x);
			} else {
				pgoto(y, x);
			}
			int shot = g->players[1-player].shots[x][y];
			// Haven't shot here -> show nothing.
			if (!shot) {
				continue;
			}
			// Shot and missed -> show a miss.
			if (shot == game.MARK_MISS) {
				if (OS.has_colors()) {
					OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
				}
				OS.addch(game.MARK_MISS);
				OS.attrset(0);
				continue;
			}
			// Shot and sunk -> show the ship.
			game.ship_t *ss = game.getshipat(g, player, x, y);
			if (!ss) panic("where's the ship?");
			if (!game.ship_afloat(ss)) {
				if (OS.has_colors()) {
					OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
				}
				OS.addch(ss->symbol);
				OS.attrset(0);
				continue;
			}
			// Shot and hit -> show a hit.
			if (OS.has_colors()) {
				OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
			}
			OS.addch(game.MARK_HIT);
			OS.attrset(0);
		}
	}
}

void draw_ships(game.state_t *g, int player) {
	game.ship_t *ships = g->players[player].ships;
	for (int i = 0; i < SHIPTYPES; i++) {
		game.ship_t *ss = &ships[i];
		if (!ss->placed) continue;
		for (int j = 0; j < ss->length; j++) {
			game.xy_t xy = game.shipxy(ss->x, ss->y, ss->dir, j);
			if (player == COMPUTER) {
				cgoto(xy.y, xy.x);
			} else {
				pgoto(xy.y, xy.x);
			}
			OS.addch(ss->symbol);
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

void draw_status(game.state_t *g) {
	OS.move(STATUS_Y + 1, 0);
	OS.clrtoeol();
	OS.printw(" *** %s", g->status);
	OS.refresh();
}

void draw_error(game.state_t *g) {
	const char *s;
	switch (g->error) {
		case 0: { s = NULL; }
		case game.ERR_HANGING: { s = errstr_hanging[rnd.intn(3)]; }
		case game.ERR_COLLISION: { s = errstr_collision[rnd.intn(3)]; }
		default: { s = "unknown error"; }
	}
	OS.move(STATUS_Y + 2, 0);
	OS.clrtoeol();
	if (s) {
		OS.printw("%s", s);
	}
	OS.refresh();
}
