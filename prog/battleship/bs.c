/*
 * bs.c - original author: Bruce Holloway
 *		salvo option by: Chuck A DeGaul
 * with improved user interface, autoconfiguration and code cleanup
 *
 * SPDX-FileCopyrightText: (C) Eric S. Raymond <esr@thyrsus.com>
 * SPDX-License-Identifier: BSD-2-Clause
 */
#link ncurses
#include <curses.h>
#import opt
#import time

// options
bool salvo = false;
bool blitz = false;
bool closepack = false;
bool PENGUIN = true;
bool DEBUG = false;

typedef {
	const char *name;  /* name of the ship type */
	int hits;          /* how many times has this ship been hit? */
	char symbol;       /* symbol for game purposes */
	int length;        /* length of ship */
	char x, y;         /* coordinates of ship start point */
	uint8_t dir; /* direction of `bow' */
	bool placed;       /* has it been placed on the board? */
} ship_t;

enum {
	S_MISS = 0,
	S_HIT = 1,
	S_SUNK = -1,
};

enum {
	RANDOM_FIRE = 0,
	RANDOM_HIT = 1,
	HUNT_DIRECT = 2,
	FIRST_PASS = 3,
	REVERSE_JUMP = 4,
	SECOND_PASS = 5,
};

// /* direction constants */
enum {
	E = 0,
	// SE = 1,
	S = 2,
	// SW = 3,
	W = 4,
	// NW = 5,
	N = 6,
	// NE = 7,
};

#define BWIDTH 10
#define BDEPTH 10

// state
int turn = 0;                   /* 0=player, 1=computer */
int plywon = 0;
int cpuwon = 0; /* How many games has each won? */
int next = RANDOM_FIRE;
bool used[4] = {};
ship_t ts = {};
int curx = (BWIDTH / 2); // /* current ship position and direction */
int cury = (BDEPTH / 2);
char hits[2][BWIDTH][BDEPTH] = {};
char board[2][BWIDTH][BDEPTH] = {};

const char carrier[] = "Aircraft Carrier";
const char battle[] = "Battleship";
const char sub[] = "Submarine";
const char destroy[] = "Destroyer";
const char ptboat[] = "PT Boat";

ship_t plyship[] = {
    {carrier, 0, 'A', 5,  0, 0, 0, false},
	{battle, 0, 'B', 4,  0, 0, 0, false},
	{destroy, 0, 'D', 3,  0, 0, 0, false},
    {sub, 0, 'S', 3,  0, 0, 0, false},
	{ptboat, 0, 'P', 2,  0, 0, 0, false},
};

ship_t cpuship[] = {
    {carrier, 0, 'A', 5,  0, 0, 0, false},
	{battle, 0, 'B', 4,  0, 0, 0, false},
	{destroy, 0, 'D', 3,  0, 0, 0, false},
    {sub, 0, 'S', 3,  0, 0, 0, false},
	{ptboat, 0, 'P', 2,  0, 0, 0, false},
};

const int SHIPTYPES = 5;

int main(int argc, char *argv[]) {
	opt.flag("b", "play a blitz game", &blitz);
	opt.flag("s", "play a salvo game", &salvo);
	opt.flag("c", "ships may be adjacent", &closepack);
	opt.parse(argc, argv);
	if (blitz && salvo) {
		fprintf(stderr, "-b and -s are mutually exclusive\n");
		exit(1);
	}
	intro();
	while (true) {
		play();
		if (!playagain()) break;
	}
	closegame(0);
	return 0;
}

void intro() {
	srand(time(NULL));

	signal(SIGINT, closegame);
	signal(SIGINT, closegame);
	signal(SIGABRT, closegame); /* for assert(3) */
	if (signal(OS.SIGQUIT, SIG_IGN) != SIG_IGN) {
		signal(OS.SIGQUIT, closegame);
	}

	OS.initscr();
	if (OS.KEY_MIN) OS.keypad(OS.stdscr, true);
	OS.saveterm();
	OS.nonl();
	OS.cbreak();
	OS.noecho();

	if (PENGUIN) {
		OS.clear();
		OS.mvaddstr(4, 29, "Welcome to Battleship!");
		// clang-format off
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
		// clang-format on
		OS.mvaddstr(22, 27, "Hit any key to continue...");
		OS.refresh();
		OS.getch();
	}

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

// #ifdef NCURSES_MOUSE_VERSION
	// OS.mousemask(OS.BUTTON1_CLICKED, NULL);
// #endif /* NCURSES_MOUSE_VERSION*/
}

void play() {
	initgame();
	while (awinna() == -1) {
		if (!blitz) {
			if (!salvo) {
				if (turn) {
					cputurn();
				} else {
					plyturn();
				}
			} else {
				int i = scount(turn);
				while (i--) {
					if (turn) {
						if (cputurn() &&
							awinna() != -1) {
							i = 0;
						}
					} else {
						if (plyturn() &&
							awinna() != -1) {
							i = 0;
						}
					}
				}
			}
		} else {
			while (true) {
				bool ok;
				if (turn) ok = cputurn();
				else ok = plyturn();
				if (!ok) {
					break;
				}
			}
		}
		turn = 1-turn;
	}
}

bool playagain() {
	int j;
	ship_t *ss;

	for (ss = cpuship; ss < cpuship + SHIPTYPES; ss++) {
		for (j = 0; j < ss->length; j++) {
			cgoto(ss->y + j * yincr[ss->dir], ss->x + j * xincr[ss->dir]);
			OS.addch(ss->symbol);
		}
	}

	if (awinna()) {
		++cpuwon;
	} else {
		++plywon;
	}
	j = 18 + strlen(name);
	if (plywon >= 10) {
		++j;
	}
	if (cpuwon >= 10) {
		++j;
	}
	OS.mvprintw(1, (COLWIDTH - j) / 2, "%s: %d     Computer: %d", name,
	               plywon, cpuwon);

	if (awinna()) {
		prompt(2, "Want to be humiliated again [yn]?", "");
	} else {
		prompt(2, "Going to give me a chance for revenge [yn]?", "");
	}
	return sgetc("YN") == 'Y';
}

void cgoto(int y, x) {
	OS.move(CY(y), CX(x));
}


// /*
//  * Constants for tuning the random-fire algorithm. It prefers moves that
//  * diagonal-stripe the board with a stripe separation of srchstep. If
//  * no such preferred moves are found, srchstep is decremented.
//  */
#define BEGINSTEP 3 /* initial value of srchstep */

// /* miscellaneous constants */
enum {
	PLAYER = 0,
	COMPUTER = 1,
	MARK_HIT = 'H',
	MARK_MISS = 'o',
};

#define CTRLC '\003' /* used as terminate command */
#define FF '\014'    /* used as redraw command */


// /* display symbols */
#define SHOWHIT '*'
#define SHOWSPLASH ' '
bool IS_SHIP(int c) {
	return OS.isupper(c);
}

// /* how to position us on player board */
#define PYBASE 3
#define PXBASE 3

int PY(int y) { return (PYBASE + (y)); }
int PX(int x) { return (PXBASE + (x)*3); }
void pgoto(int y, x) {
	OS.move(PY(y), PX(x));
}

// /* how to position us on cpu board */
int CYBASE = 3;
int CXBASE = 48;
int CY(int y) { return CYBASE + y; }
int CX(int x) { return CXBASE + x*3; }
// #define CYINV(y) ((y)-CYBASE)
// #define CXINV(x) (((x)-CXBASE) / 3)

bool ONBOARD(int x, y) {
	return (x >= 0 && x < BWIDTH && y >= 0 && y < BDEPTH);
}

// /* other board locations */
int COLWIDTH = 80;
int PROMPTLINE = 21;
#define SYBASE CYBASE + BDEPTH + 3 /* move key diagram */
#define SXBASE 63
#define MYBASE SYBASE - 1 /* diagram caption */
#define MXBASE 64
#define HYBASE SYBASE - 1 /* help area */
#define HXBASE 0

// /* this will need to be changed if BWIDTH changes */
const char *numbers = "   0  1  2  3  4  5  6  7  8  9";
const char *name = "stranger";

const int xincr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
const int yincr[8] = {0, 1, 1, 1, 0, -1, -1, -1};



/* end the game, either normally or due to signal */
void closegame(int sig) {
	(void) sig;
	OS.clear();
	OS.refresh();
	OS.resetterm();
	OS.echo();
	OS.endwin();
	exit(1);
}

/* announce which game options are enabled */
void announceopts() {
	if (salvo || blitz || closepack) {
		OS.printw("Playing optional game (");
		if (salvo) {
			OS.printw("salvo, ");
		} else {
			OS.printw("nosalvo, ");
		}
		if (blitz) {
			OS.printw("blitz ");
		} else {
			OS.printw("noblitz, ");
		}
		if (closepack) {
			OS.printw("closepack)");
		} else {
			OS.printw("noclosepack)");
		}
	} else {
		OS.printw("Playing standard game (noblitz, nosalvo, noclosepack)");
	}
}

/* print a message at the prompt line */
void prompt(int n, const char *f, const char *s) {
	OS.move(PROMPTLINE + n, 0);
	OS.clrtoeol();
	OS.printw(f, s);
	OS.refresh();
}

void error(const char *s) {
	OS.move(PROMPTLINE + 2, 0);
	OS.clrtoeol();
	if (s) {
		OS.addstr(s);
		OS.beep();
	}
}

void placeship(int b, ship_t *ss, int vis) {
	for (int l = 0; l < ss->length; ++l) {
		int newx = ss->x + l * xincr[ss->dir];
		int newy = ss->y + l * yincr[ss->dir];
		board[b][newx][newy] = ss->symbol;
		if (vis) {
			pgoto(newy, newx);
			OS.addch(ss->symbol);
		}
	}
	ss->hits = 0;
}

int rnd(int n) { return (((rand() & 0x7FFF) % n)); }

/* generate a valid random ship placement into px,py */
void randomplace(int b, ship_t *ss) {
	while (true) {
		if (rnd(2)) {
			ss->dir = E;
			ss->x = rnd(BWIDTH - ss->length);
			ss->y = rnd(BDEPTH - 0);
		} else {
			ss->dir = S;
			ss->x = rnd(BWIDTH - 0);
			ss->y = rnd(BDEPTH - ss->length);
		}
		if (!checkplace(b, ss, false)) continue;
		else break;
	}
}

void initgame() {
	OS.clear();
	OS.mvaddstr(0, 35, "BATTLESHIPS");
	OS.move(PROMPTLINE + 2, 0);
	announceopts();

	memset(board, 0, sizeof(char) * BWIDTH * BDEPTH * 2);
	memset(hits, 0, sizeof(char) * BWIDTH * BDEPTH * 2);

	for (int i = 0; i < SHIPTYPES; i++) {
		ship_t *ss = cpuship + i;
		ss->x = ss->y = ss->dir = ss->hits = 0;
		ss->placed = false;
		ss = plyship + i;
		ss->x = ss->y = ss->dir = ss->hits = 0;
		ss->placed = false;
	}

	/* draw empty boards */
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
	OS.mvprintw(HYBASE, HXBASE, "To position your ships: move the cursor to a spot, then");
	OS.mvprintw(HYBASE + 1, HXBASE, "type the first letter of a ship type to select it, then");
	OS.mvprintw(HYBASE + 2, HXBASE, "type a direction ([hjkl] or [4862]), indicating how the");
	OS.mvprintw(HYBASE + 3, HXBASE, "ship should be pointed. You may also type a ship letter");
	OS.mvprintw(HYBASE + 4, HXBASE, "followed by `r' to position it randomly, or type `R' to");
	OS.mvprintw(HYBASE + 5, HXBASE, "place all remaining ships randomly.");

	OS.mvaddstr(MYBASE, MXBASE, "Aiming keys:");
	OS.mvaddstr(SYBASE, SXBASE, "y k u    7 8 9");
	OS.mvaddstr(SYBASE + 1, SXBASE, " \\|/      \\|/ ");
	OS.mvaddstr(SYBASE + 2, SXBASE, "h-+-l    4-+-6");
	OS.mvaddstr(SYBASE + 3, SXBASE, " /|\\      /|\\ ");
	OS.mvaddstr(SYBASE + 4, SXBASE, "b j n    1 2 3");

	/* have the computer place ships */
	ship_t *ss = NULL;
	for (ss = cpuship; ss < cpuship + SHIPTYPES; ss++) {
		randomplace(COMPUTER, ss);
		placeship(COMPUTER, ss, false);
	}

	int unplaced;
	ss = NULL;
	while (true) {
		char c;
		char docked[SHIPTYPES + 2];
		char *cp = docked;

		/* figure which ships still wait to be placed */
		*cp++ = 'R';
		for (int i = 0; i < SHIPTYPES; i++) {
			if (!plyship[i].placed) {
				*cp++ = plyship[i].symbol;
			}
		}
		*cp = '\0';

		/* get a command letter */
		prompt(1, "Type one of [%s] to pick a ship.", docked + 1);
		while (true) {
			c = toupper(getcoord(PLAYER));
			if (!strchr(docked, c)) continue;
			else break;
		}

		if (c == 'R') {
			OS.ungetch('R');
		} else {
			/* map that into the corresponding symbol */
			for (ss = plyship; ss < plyship + SHIPTYPES; ss++) {
				if (ss->symbol == c) {
					break;
				}
			}

			prompt(1, "Type one of [hjklrR] to place your %s.", ss->name);
			pgoto(cury, curx);
		}

		while (true) {
			c = OS.getch();
			if (!strchr("hjklrR", c) || c == FF) continue;
			break;
		}

		if (c == FF) {
			OS.clearok(OS.stdscr, true);
			OS.refresh();
		} else if (c == 'r') {
			prompt(1, "Random-placing your %s", ss->name);
			randomplace(PLAYER, ss);
			placeship(PLAYER, ss, true);
			error(NULL);
			ss->placed = true;
		} else if (c == 'R') {
			prompt(1, "Placing the rest of your fleet at random...", "");
			for (ss = plyship; ss < plyship + SHIPTYPES; ss++) {
				if (!ss->placed) {
					randomplace(PLAYER, ss);
					placeship(PLAYER, ss, true);
					ss->placed = true;
				}
			}
			error(NULL);
		} else if (strchr("hjkl8462", c)) {
			ss->x = curx;
			ss->y = cury;

			switch (c) {
				case 'k', '8': { ss->dir = N; }
				case 'j', '2': { ss->dir = S; }
				case 'h', '4': { ss->dir = W; }
				case 'l', '6': { ss->dir = E; }
			}

			if (checkplace(PLAYER, ss, true)) {
				placeship(PLAYER, ss, true);
				error(NULL);
				ss->placed = true;
			}
		}
		unplaced = 0;
		for (int i = 0; i < SHIPTYPES; i++) {
			unplaced += !plyship[i].placed;
		}
		if (unplaced) continue;
		else break;
	}

	turn = rnd(2);

	OS.mvprintw(HYBASE, HXBASE, "To fire, move the cursor to your chosen aiming point   ");
	OS.mvprintw(HYBASE + 1, HXBASE, "and strike any key other than a motion key.            ");
	OS.mvprintw(HYBASE + 2, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 3, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 4, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 5, HXBASE, "                                                       ");

	prompt(0, "Press any key to start...", "");
	OS.getch();
}

int getcoord(int atcpu) {
	if (atcpu) {
		cgoto(cury, curx);
	} else {
		pgoto(cury, curx);
	}
	OS.refresh();
	for (;;) {
		int ny;
		int nx;
		int c;
		if (atcpu) {
			OS.mvprintw(CYBASE + BDEPTH + 1, CXBASE + 11, "(%d, %c)", curx, 'A' + cury);
			cgoto(cury, curx);
		} else {
			OS.mvprintw(PYBASE + BDEPTH + 1, PXBASE + 11, "(%d, %c)", curx, 'A' + cury);
			pgoto(cury, curx);
		}

		switch (c = OS.getch()) {
			case 'k', '8', OS.KEY_UP: {
				ny = cury + BDEPTH - 1;
				nx = curx;
			}
			case 'j', '2', OS.KEY_DOWN: {
				ny = cury + 1;
				nx = curx;
			}
			case 'h', '4', OS.KEY_LEFT: {
				ny = cury;
				nx = curx + BWIDTH - 1;
			}
			case 'l', '6', OS.KEY_RIGHT: {
				ny = cury;
				nx = curx + 1;
			}
			case 'y', '7', OS.KEY_A1: {
				ny = cury + BDEPTH - 1;
				nx = curx + BWIDTH - 1;
			}
			case 'b', '1', OS.KEY_C1: {
				ny = cury + 1;
				nx = curx + BWIDTH - 1;
			}
			case 'u', '9', OS.KEY_A3: {
				ny = cury + BDEPTH - 1;
				nx = curx + 1;
			}
			case 'n', '3', OS.KEY_C3: {
				ny = cury + 1;
				nx = curx + 1;
			}
			case FF: {
				nx = curx;
				ny = cury;
				OS.clearok(OS.stdscr, true);
				OS.refresh();
			}
			// case OS.KEY_MOUSE: {
			// 	OS.MEVENT myevent;
			// 	OS.getmouse(&myevent);
			// 	if (atcpu && myevent.y >= CY(0) &&
			// 		myevent.y <= CY(BDEPTH) && myevent.x >= CX(0) &&
			// 		myevent.x <= CX(BDEPTH)) {
			// 		curx = CXINV(myevent.x);
			// 		cury = CYINV(myevent.y);
			// 		return (' ');
			// 	}
			// 	OS.beep();
			// 	continue;
			// }
			default: {
				if (atcpu) {
					OS.mvaddstr(CYBASE + BDEPTH + 1, CXBASE + 11,"      ");
				} else {
					OS.mvaddstr(PYBASE + BDEPTH + 1, PXBASE + 11,"      ");
				}
				return (c);
			}
		}

		curx = nx % BWIDTH;
		cury = ny % BDEPTH;
	}
	panic("!");
	return 123; // shouldn't happen
}

/* is this location on the selected zboard adjacent to a ship? */
bool collidecheck(int b, int y, int x) {
	bool collide;

	/* anything on the square */
	if ((collide = IS_SHIP(board[b][x][y])) != false) {
		return (collide);
	}

	/* anything on the neighbors */
	if (!closepack) {
		for (int i = 0; i < 8; i++) {
			int yend = y + yincr[i];
			int xend = x + xincr[i];
			if (ONBOARD(xend, yend) && IS_SHIP(board[b][xend][yend])) {
				collide = true;
				break;
			}
		}
	}
	return collide;
}

bool checkplace(int b, ship_t *ss, int vis) {
	/* first, check for board edges */
	int xend = ss->x + (ss->length - 1) * xincr[ss->dir];
	int yend = ss->y + (ss->length - 1) * yincr[ss->dir];
	if (!ONBOARD(xend, yend)) {
		if (vis) {
			switch (rnd(3)) {
				case 0: { error("Ship is hanging from the edge of the world"); }
				case 1: { error("Try fitting it on the board"); }
				case 2: { error("Figure I won't find it if you put it there?"); }
			}
		}
		return false;
	}

	for (int l = 0; l < ss->length; ++l) {
		if (collidecheck(b, ss->y + l * yincr[ss->dir], ss->x + l * xincr[ss->dir])) {
			if (vis) {
				switch (rnd(3)) {
					case 0: { error("There's already a ship there"); }
					case 1: { error("Collision alert!  Aaaaaagh!"); }
					case 2: { error("Er, Admiral, what about the other ship?"); }
				}
			}
			return false;
		}
	}
	return true;
}

int awinna() {
	int i;
	int j;

	for (i = 0; i < 2; ++i) {
		ship_t *ss;
		if (i) ss = cpuship;
		else ss = plyship;
		for (j = 0; j < SHIPTYPES; ) {
			if (ss->length > ss->hits) {
				break;
			}
			++j;
			++ss;
		}
		if (j == SHIPTYPES) {
			return 1-turn;
		}
	}
	return -1;
}

ship_t *hitship(int x, int y) {
	/* register a hit on the targeted ship */
	int oldx;
	int oldy;

	OS.getyx(OS.stdscr, oldy, oldx);

	ship_t *sb;
	ship_t *ss;
	if (turn) sb = plyship;
	else sb = cpuship;

	char sym = board[1-turn][x][y];
	if (sym == 0) {
		return NULL;
	}

	
	for (ss = sb; ss < sb + SHIPTYPES; ++ss) {
		if (ss->symbol == sym) {
			if (++ss->hits < ss->length) { /* still afloat? */
				return NULL;
			} else { /* sunk! */
				int i;

				if (!closepack) {
					int j;

					for (j = -1; j <= 1; j++) {
						int bx = ss->x + j * xincr[(ss->dir + 2) % 8];
						int by = ss->y + j * yincr[(ss->dir + 2) % 8];

						for (i = -1; i <= ss->length; ++i) {
							int x1 = bx + i * xincr[ss->dir];
							int y1 = by + i * yincr[ss->dir];
							if (ONBOARD(x1, y1)) {
								hits[turn][x1][y1] = MARK_MISS;
								if (turn % 2 == PLAYER) {
									cgoto(y1, x1);
									if (OS.has_colors()) {
										OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
									}
									OS.addch(MARK_MISS);
									OS.attrset(0);
								} else {
									pgoto(y1, x1);
									OS.addch(SHOWSPLASH);
								}
							}
						}
					}
				}

				for (i = 0; i < ss->length; ++i) {
					int x1 = ss->x + i * xincr[ss->dir];
					int y1 = ss->y + i * yincr[ss->dir];

					hits[turn][x1][y1] = ss->symbol;
					if (turn % 2 == PLAYER) {
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
				OS.move(oldy, oldx);
				return (ss);
			}
		}
	}
	OS.move(oldy, oldx);
	return NULL;
}

int plyturn() {
	ship_t *ss;
	bool hit;
	char *m = NULL;

	prompt(1, "Where do you want to shoot? ", "");
	for (;;) {
		getcoord(COMPUTER);
		if (hits[PLAYER][curx][cury]) {
			prompt(1, "You shelled this spot already! Try again.", "");
			OS.beep();
		} else {
			break;
		}
	}
	hit = IS_SHIP(board[COMPUTER][curx][cury]);
	if (hit) {
		hits[PLAYER][curx][cury] = MARK_HIT;
	} else {
		hits[PLAYER][curx][cury] = MARK_MISS;
	}
	
	cgoto(cury, curx);

	if (OS.has_colors()) {
		if (hit) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
		} else {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
		}
	}

	OS.addch(hits[PLAYER][curx][cury]);

	OS.attrset(0);


	if (hit) {
		prompt(1, "You scored a hit.", "");
	} else {
		prompt(1, "You missed.", "");
	}
	
	if (hit && (ss = hitship(curx, cury))) {
		switch (rnd(5)) {
			case 0: { m = " You sank my %s!"; }
			case 1: { m = " I have this sinking feeling about my %s...."; }
			case 2: { m = " My %s has gone to Davy Jones's locker!"; }
			case 3: { m = " Glub, glub -- my %s is headed for the bottom!"; }
			case 4: { m = " You'll pick up survivors from my %s, I hope...!"; }
		}
		OS.printw(m, ss->name);
		OS.beep();
		return (awinna() == -1);
	}
	return (hit);
}


int sgetc(const char *s) {
	const char *s1;

	OS.refresh();
	for (;;) {
		int ch = OS.getch();
		if (OS.islower(ch)) {
			ch = toupper(ch);
		}
		if (ch == CTRLC) {
			closegame(0);
		}
		for (s1 = s; *s1 && ch != *s1; ++s1) {
			continue;
		}
		if (*s1) {
			OS.addch(ch);
			OS.refresh();
			return (ch);
		}
	}
	panic("return");
}

// implements simple diagonal-striping strategy
int turncount = 0;
int srchstep = BEGINSTEP;
int huntoffs = 0; /* Offset on search strategy */
void randomfire(int *px, int *py) {
	int ypossible[BWIDTH * BDEPTH];
	int xpossible[BWIDTH * BDEPTH];
	int ypreferred[BWIDTH * BDEPTH];
	int xpreferred[BWIDTH * BDEPTH];

	if (turncount++ == 0) {
		huntoffs = rnd(srchstep);
	}

	/* first, list all possible moves */
	int nposs = 0;
	int npref = 0;
	for (int x = 0; x < BWIDTH; x++) {
		for (int y = 0; y < BDEPTH; y++) {
			if (!hits[COMPUTER][x][y]) {
				xpossible[nposs] = x;
				ypossible[nposs] = y;
				nposs++;
				if (((x + huntoffs) % srchstep) !=
				    (y % srchstep)) {
					xpreferred[npref] = x;
					ypreferred[npref] = y;
					npref++;
				}
			}
		}
	}

	if (npref) {
		int i = rnd(npref);
		*px = xpreferred[i];
		*py = ypreferred[i];
	} else if (nposs) {
		int i = rnd(nposs);
		*px = xpossible[i];
		*py = ypossible[i];
		if (srchstep > 1) {
			--srchstep;
		}
	} else {
		panic("No moves possible?? Help!");
	}
}


/* fire away at given location */
int cpufire(int x, int y) {
	bool hit;
	bool sunk;
	ship_t *ss = NULL;

	if (hit = (board[PLAYER][x][y])) {
		hits[COMPUTER][x][y] = MARK_HIT;
		OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "hit");
		ss = hitship(x, y);
		sunk = ss != NULL;
		if (sunk) {
			OS.printw(" I've sunk your %s", ss->name);
		}
	} else {
		hits[COMPUTER][x][y] = MARK_MISS;
		OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "miss");
	}

	OS.clrtoeol();
	pgoto(y, x);

	if (OS.has_colors()) {
		if (hit) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
		} else {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
		}
	}
	if (hit) {
		OS.addch(SHOWHIT);
		OS.attrset(0);
		if (sunk) {
			return S_SUNK;
		} else {
			return S_HIT;
		}
	} else {
		OS.addch(SHOWSPLASH);
		OS.attrset(0);
		return S_MISS;
	}
}



bool POSSIBLE(int x, y) {
	return ONBOARD(x, y) && !hits[COMPUTER][x][y];
}

// /*
//  * This code implements a fairly irregular FSM, so please forgive the rampant
//  * unstructuredness below. The five labels are states which need to be held
//  * between computer turns.
//  */
typedef {
	int x, y, hit, d;
} turn_state_t;
bool cputurn() {
	turn_state_t st = {.hit = S_MISS};

	switch (next) {
		/* last shot was random and missed */
		case RANDOM_FIRE: {
			randomfire(&st.x, &st.y);
			if (!(st.hit = cpufire(st.x, st.y))) {
				next = RANDOM_FIRE;
			} else {
				ts.x = st.x;
				ts.y = st.y;
				ts.hits = 1;
				if (st.hit == S_SUNK) {
					next = RANDOM_FIRE;
				} else {
					next = RANDOM_HIT;
				}
			}
		}

		/* last shot was random and hit */
		case RANDOM_HIT: {
			used[E / 2] = used[S / 2] = used[W / 2] = used[N / 2] = false;
			/* FALLTHROUGH */
			do_hunt_direct(&st);
		}

		/* last shot hit, we're looking for ship's long axis */
		case HUNT_DIRECT: {
			do_hunt_direct(&st);
		}

		/* we have a start and a direction now */
		case FIRST_PASS: {
			st.x = ts.x + xincr[ts.dir];
			st.y = ts.y + yincr[ts.dir];
			if (POSSIBLE(st.x, st.y) && (st.hit = cpufire(st.x, st.y))) {
				ts.x = st.x;
				ts.y = st.y;
				ts.hits++;
				if (st.hit == S_SUNK) {
					next = RANDOM_FIRE;
				} else {
					next = FIRST_PASS;
				}
			} else {
				next = REVERSE_JUMP;
			}
		}


		/* nail down the ship's other end */
		case REVERSE_JUMP: {
			st.d = ts.dir + 4;
			st.x = ts.x + ts.hits * xincr[st.d];
			st.y = ts.y + ts.hits * yincr[st.d];
			if (POSSIBLE(st.x, st.y) && (st.hit = cpufire(st.x, st.y))) {
				ts.x = st.x;
				ts.y = st.y;
				ts.dir = st.d;
				ts.hits++;
				if (st.hit == S_SUNK) {
					next = RANDOM_FIRE;
				} else {
					next = SECOND_PASS;
				}
			} else {
				next = RANDOM_FIRE;
			}
		}

		/* kill squares not caught on first pass */
		case SECOND_PASS: {
			st.x = ts.x + xincr[ts.dir];
			st.y = ts.y + yincr[ts.dir];
			if (POSSIBLE(st.x, st.y) && (st.hit = cpufire(st.x, st.y))) {
				ts.x = st.x;
				ts.y = st.y;
				ts.hits++;
				if (st.hit == S_SUNK) {
					next = RANDOM_FIRE;
				} else {
					next = SECOND_PASS;
				}
			} else {
				next = RANDOM_FIRE;
			}
		}
	}

	/* check for continuation and/or winner */
	if (salvo) {
		OS.refresh();
		time.sleep(1);
	}
	if (awinna() != -1) {
		return false;
	}

	if (DEBUG) {
		OS.mvprintw(PROMPTLINE + 2, 0, "New state %d, x=%d, y=%d, d=%d", next, st.x, st.y, st.d);
	}
	return st.hit;
}


void do_hunt_direct(turn_state_t *st) {
	int navail = 0;
	st->d = 0;
	while (st->d < 4) {
		st->x = ts.x + xincr[st->d * 2];
		st->y = ts.y + yincr[st->d * 2];
		if (!used[st->d] && POSSIBLE(st->x, st->y)) {
			navail++;
		} else {
			used[st->d] = true;
		}
		st->d++;
	}
	if (navail == 0) /* no valid places for shots adjacent... */ {
		randomfire(&st->x, &st->y);
		if (!(st->hit = cpufire(st->x, st->y))) {
			next = RANDOM_FIRE;
		} else {
			ts.x = st->x;
			ts.y = st->y;
			ts.hits = 1;
			if (st->hit == S_SUNK) {
				next = RANDOM_FIRE;
			} else {
				next = RANDOM_HIT;
			}
		}
	} else {
		st->d = 0;
		int n = rnd(navail) + 1;
		while (n) {
			while (used[st->d]) {
				st->d++;
			}
			n--;
		}

		assert(st->d <= 4);

		used[st->d] = false;
		st->x = ts.x + xincr[st->d * 2];
		st->y = ts.y + yincr[st->d * 2];

		assert(POSSIBLE(st->x, st->y));

		if (!(st->hit = cpufire(st->x, st->y))) {
			next = HUNT_DIRECT;
		} else {
			ts.x = st->x;
			ts.y = st->y;
			ts.dir = st->d * 2;
			ts.hits++;
			if (st->hit == S_SUNK) {
				next = RANDOM_FIRE;
			} else {
				next = FIRST_PASS;
			}
		}
	}
}

int scount(int who) {
	ship_t *sp;
	if (who) {
		sp = cpuship; /* count cpu shots */
	} else {
		sp = plyship; /* count player shots */
	}

	int shots = 0;
	for (int i = 0; i < SHIPTYPES; i++) {
		if (sp->hits >= sp->length) {
			sp++;
			continue; /* dead ship */
		} else {
			sp++;
			shots++;
		}
	}
	return (shots);
}

