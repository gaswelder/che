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
#import game.c

// lib
int rnd(int n) { return (((rand() & 0x7FFF) % n)); }

// options
bool salvo = false;
bool blitz = false;
bool closepack = false;
bool PENGUIN = true;
bool DEBUG = false;

// model
enum {
	S_MISS = 0,
	S_HIT = 1,
	S_SUNK = -1,
};

// ai_next values
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

game.state_t gamestate = {
	.curx = BWIDTH / 2,
	.cury = BDEPTH / 2,
	.plyship = {
		{game.SHIP_CARRIER, 0, 'A', 5,  0, 0, 0, false},
		{game.SHIP_BATTLESHIP, 0, 'B', 4,  0, 0, 0, false},
		{game.SHIP_DESTROYER, 0, 'D', 3,  0, 0, 0, false},
		{game.SHIP_SUBMARINE, 0, 'S', 3,  0, 0, 0, false},
		{game.SHIP_PTBOAT, 0, 'P', 2,  0, 0, 0, false},
	},
	.cpuship = {
		{game.SHIP_CARRIER, 0, 'A', 5,  0, 0, 0, false},
		{game.SHIP_BATTLESHIP, 0, 'B', 4,  0, 0, 0, false},
		{game.SHIP_DESTROYER, 0, 'D', 3,  0, 0, 0, false},
		{game.SHIP_SUBMARINE, 0, 'S', 3,  0, 0, 0, false},
		{game.SHIP_PTBOAT, 0, 'P', 2,  0, 0, 0, false},
	},
	.ai_srchstep = BEGINSTEP
};

const int SHIPTYPES = 5;

const int xincr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
const int yincr[8] = {0, 1, 1, 1, 0, -1, -1, -1};

int main(int argc, char *argv[]) {
	opt.flag("b", "play a blitz game", &blitz);
	opt.flag("s", "play a salvo game", &salvo);
	opt.flag("c", "ships may be adjacent", &closepack);
	opt.parse(argc, argv);
	if (blitz && salvo) {
		fprintf(stderr, "-b and -s are mutually exclusive\n");
		exit(1);
	}
	srand(time(NULL));

	signal(SIGINT, closegame);
	signal(SIGINT, closegame);
	signal(SIGABRT, closegame); /* for assert(3) */
	if (signal(OS.SIGQUIT, SIG_IGN) != SIG_IGN) {
		signal(OS.SIGQUIT, closegame);
	}

	render_init();
	if (PENGUIN) {
		render_penguin();
		input_any();
	}

// #ifdef NCURSES_MOUSE_VERSION
	// OS.mousemask(OS.BUTTON1_CLICKED, NULL);
// #endif /* NCURSES_MOUSE_VERSION*/

	while (true) {
		play();
		render_cpu_ships();
		render_winner();
		render_playgain_prompt();
		if (input_keychoice("YN") != 'Y') {
			break;
		}
	}
	closegame(0);
	return 0;
}

void play() {
	initgame();
	if (blitz) {
		while (game_winner() == -1) {
			while (true) {
				bool ok;
				if (gamestate.turn == COMPUTER) {
					ok = game_cputurn();
				} else {
					input_player_shot();
					ok = game_playerturn();
				}
				render_turn();
				if (!ok) {
					break;
				}
			}
			gamestate.turn = 1 - gamestate.turn;
		}
	} else if (salvo) {
		while (game_winner() == -1) {
			int i = game_shipscount(gamestate.turn);
			while (i--) {
				if (gamestate.turn == COMPUTER) {
					if (game_cputurn() && game_winner() != -1) {
						i = 0;
					}
				} else {
					input_player_shot();
					int result = game_playerturn();
					if (result && game_winner() != -1) {
						i = 0;
					}
				}
				render_turn();
			}
			gamestate.turn = 1 - gamestate.turn;
		}
	} else {
		while (game_winner() == -1) {
			if (gamestate.turn == COMPUTER) {
				game_cputurn();
			} else {
				input_player_shot();
				game_playerturn();
			}
			render_turn();
			gamestate.turn = 1 - gamestate.turn;
		}
	}
	
	if (game_winner() == COMPUTER) {
		gamestate.winner = COMPUTER;
		gamestate.cpuwon++;
	} else {
		gamestate.winner = PLAYER;
		gamestate.plywon++;
	}
}


// /*
//  * Constants for tuning the random-fire algorithm. It prefers moves that
//  * diagonal-stripe the board with a stripe separation of gamestate.ai_srchstep. If
//  * no such preferred moves are found, gamestate.ai_srchstep is decremented.
//  */
#define BEGINSTEP 3 /* initial value of gamestate.ai_srchstep */

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


#define PYBASE 3
#define PXBASE 3

int CYBASE = 3;
int CXBASE = 48;

void pgoto(int y, x) {
	OS.move(PYBASE + y, PXBASE + x*3);
}
void cgoto(int y, x) {
	OS.move(CYBASE + y, CXBASE + x*3);
}

#define SYBASE CYBASE + BDEPTH + 3 /* move key diagram */
#define SXBASE 63
#define MYBASE SYBASE - 1 /* diagram caption */
#define MXBASE 64
#define HYBASE SYBASE - 1 /* help area */
#define HXBASE 0

void closegame(int sig) {
	render_exit();
	exit(sig);
}

void initgame() {
	game_reset();
	render_reset();

	game.ship_t *ss = NULL;
	int unplaced;
	while (true) {
		/* figure which ships still wait to be placed */
		char *cp = gamestate.docked;
		*cp++ = 'R';
		for (int i = 0; i < SHIPTYPES; i++) {
			if (!gamestate.plyship[i].placed) {
				*cp++ = gamestate.plyship[i].symbol;
			}
		}
		*cp = '\0';


		render_placement_prompt();

		char c;
		while (true) {
			c = toupper(getcoord(PLAYER));
			if (!strchr(gamestate.docked, c)) continue;
			else break;
		}

		if (c == 'R') {
			OS.ungetch('R');
		} else {
			/* map that into the corresponding symbol */
			for (ss = gamestate.plyship; ss < gamestate.plyship + SHIPTYPES; ss++) {
				if (ss->symbol == c) {
					break;
				}
			}

			render_prompt(1, "Type one of [hjklrR] to place your %s.", shipname(ss->kind));
			pgoto(gamestate.cury, gamestate.curx);
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
			render_prompt(1, "Random-placing your %s", shipname(ss->kind));

			game_randomplace(PLAYER, ss);
			game_placeship(PLAYER, ss);
			ss->placed = true;

			render_placed_ship(ss);
			render_error(NULL);
			
		} else if (c == 'R') {
			render_prompt(1, "Placing the rest of your fleet at random...", "");
			for (ss = gamestate.plyship; ss < gamestate.plyship + SHIPTYPES; ss++) {
				if (!ss->placed) {
					game_randomplace(PLAYER, ss);
					game_placeship(PLAYER, ss);
					ss->placed = true;

					render_placed_ship(ss);					
				}
			}
			render_error(NULL);
		} else if (strchr("hjkl8462", c)) {
			ss->x = gamestate.curx;
			ss->y = gamestate.cury;
			switch (c) {
				case 'k', '8': { ss->dir = N; }
				case 'j', '2': { ss->dir = S; }
				case 'h', '4': { ss->dir = W; }
				case 'l', '6': { ss->dir = E; }
			}

			int err = game_checkplacement(PLAYER, ss);
			if (err) {
				render_placement_error(err);
			} else {
				game_placeship(PLAYER, ss);
				ss->placed = true;
				
				render_placed_ship(ss);
				render_error(NULL);
			}
		}
		unplaced = 0;
		for (int i = 0; i < SHIPTYPES; i++) {
			unplaced += !gamestate.plyship[i].placed;
		}
		if (unplaced) continue;
		else break;
	}

	if (rnd(2)) {
		gamestate.turn = COMPUTER;
	} else {
		gamestate.turn = PLAYER;
	}

	render_manual2();
	input_any();
}





int getcoord(int atcpu) {
	if (atcpu) {
		cgoto(gamestate.cury, gamestate.curx);
	} else {
		pgoto(gamestate.cury, gamestate.curx);
	}
	OS.refresh();
	int ny;
	int nx;
	for (;;) {
		render_current_coords(atcpu);
		int c = input_key();
		
		switch (c) {
			case 'k', '8', OS.KEY_UP: {
				ny = gamestate.cury + BDEPTH - 1;
				nx = gamestate.curx;
			}
			case 'j', '2', OS.KEY_DOWN: {
				ny = gamestate.cury + 1;
				nx = gamestate.curx;
			}
			case 'h', '4', OS.KEY_LEFT: {
				ny = gamestate.cury;
				nx = gamestate.curx + BWIDTH - 1;
			}
			case 'l', '6', OS.KEY_RIGHT: {
				ny = gamestate.cury;
				nx = gamestate.curx + 1;
			}
			case 'y', '7', OS.KEY_A1: {
				ny = gamestate.cury + BDEPTH - 1;
				nx = gamestate.curx + BWIDTH - 1;
			}
			case 'b', '1', OS.KEY_C1: {
				ny = gamestate.cury + 1;
				nx = gamestate.curx + BWIDTH - 1;
			}
			case 'u', '9', OS.KEY_A3: {
				ny = gamestate.cury + BDEPTH - 1;
				nx = gamestate.curx + 1;
			}
			case 'n', '3', OS.KEY_C3: {
				ny = gamestate.cury + 1;
				nx = gamestate.curx + 1;
			}
			case FF: {
				nx = gamestate.curx;
				ny = gamestate.cury;
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

		gamestate.curx = nx % BWIDTH;
		gamestate.cury = ny % BDEPTH;
	}
	panic("!");
	return 123; // shouldn't happen
}



enum {
	ERR_HANGING = 1,
	ERR_COLLISION = 2,
};



game.ship_t *hitship(int x, int y) {
	int oldx;
	int oldy;
	OS.getyx(OS.stdscr, oldy, oldx);

	char sym = gamestate.board[1 - gamestate.turn][x][y];
	if (sym == 0) {
		return NULL;
	}

	game.ship_t *sb;
	if (gamestate.turn == COMPUTER) {
		sb = gamestate.plyship;
	} else {
		sb = gamestate.cpuship;
	}

	game.ship_t *ss;
	for (ss = sb; ss < sb + SHIPTYPES; ++ss) {
		if (ss->symbol == sym) {
			break;
		}
	}
	if (!ss) return NULL;

	if (++ss->hits < ss->length) { /* still afloat? */
		return NULL;
	}
	
	int i;
	if (!closepack) {
		for (int j = -1; j <= 1; j++) {
			int bx = ss->x + j * xincr[(ss->dir + 2) % 8];
			int by = ss->y + j * yincr[(ss->dir + 2) % 8];

			for (i = -1; i <= ss->length; ++i) {
				int x1 = bx + i * xincr[ss->dir];
				int y1 = by + i * yincr[ss->dir];
				if (game_coords_valid(x1, y1)) {
					gamestate.hits[gamestate.turn][x1][y1] = MARK_MISS;
					if (gamestate.turn % 2 == PLAYER) {
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

	render_reveal_ship(ss);
	OS.move(oldy, oldx);
	return (ss);
}



int input_key() {
	return OS.getch();
}

void input_any() {
	OS.getch();
}

void input_player_shot() {
	render_prompt(1, "Where do you want to shoot? ", "");
	for (;;) {
		getcoord(COMPUTER);
		if (gamestate.hits[PLAYER][gamestate.curx][gamestate.cury]) {
			render_prompt(1, "You shelled this spot already! Try again.", "");
			OS.beep();
		} else {
			break;
		}
	}
}

int input_keychoice(const char *s) {
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


/////////////////////////////////////////////////////////
// -------------------- game --------------------

int game_playerturn() {
	gamestate.player_hit = game_is_ship(gamestate.board[COMPUTER][gamestate.curx][gamestate.cury]);
	if (gamestate.player_hit) {
		gamestate.hits[PLAYER][gamestate.curx][gamestate.cury] = MARK_HIT;
	} else {
		gamestate.hits[PLAYER][gamestate.curx][gamestate.cury] = MARK_MISS;
	}
	if (gamestate.player_hit) {
		gamestate.sunk_by_player = hitship(gamestate.curx, gamestate.cury);
	} else {
		gamestate.sunk_by_player = NULL;
	}
	if (gamestate.sunk_by_player) {
		return game_winner() == -1;
	}
	return gamestate.player_hit;
}

// /*
//  * This code implements a fairly irregular FSM, so please forgive the rampant
//  * unstructuredness below. The five labels are states which need to be held
//  * between computer turns.
//  */
typedef {
	int x, y, hit, d;
} turn_state_t;
bool game_cputurn() {
	turn_state_t st = {.hit = S_MISS};

	switch (gamestate.ai_next) {
		/* last shot was random and missed */
		case RANDOM_FIRE: {
			game_randomfire(&st.x, &st.y);
			int result = game_cpufire(st.x, st.y);
			st.hit = result;
			if (!st.hit) {
				gamestate.ai_next = RANDOM_FIRE;
			} else {
				gamestate.ts.x = st.x;
				gamestate.ts.y = st.y;
				gamestate.ts.hits = 1;
				if (st.hit == S_SUNK) {
					gamestate.ai_next = RANDOM_FIRE;
				} else {
					gamestate.ai_next = RANDOM_HIT;
				}
			}
		}

		/* last shot was random and hit */
		case RANDOM_HIT: {
			gamestate.used[E / 2] = false;
			gamestate.used[S / 2] = false;
			gamestate.used[W / 2] = false;
			gamestate.used[N / 2] = false;
			game_do_hunt_direct(&st);
		}

		/* last shot hit, we're looking for ship's long axis */
		case HUNT_DIRECT: {
			game_do_hunt_direct(&st);
		}

		/* we have a start and a direction now */
		case FIRST_PASS: {
			st.x = gamestate.ts.x + xincr[gamestate.ts.dir];
			st.y = gamestate.ts.y + yincr[gamestate.ts.dir];
			int foo = game_coord_possible(st.x, st.y);
			if (foo) {
				int result = game_cpufire(st.x, st.y);
				st.hit = result;
				foo = st.hit;
			}
			if (foo) {
				gamestate.ts.x = st.x;
				gamestate.ts.y = st.y;
				gamestate.ts.hits++;
				if (st.hit == S_SUNK) {
					gamestate.ai_next = RANDOM_FIRE;
				} else {
					gamestate.ai_next = FIRST_PASS;
				}
			} else {
				gamestate.ai_next = REVERSE_JUMP;
			}
		}

		/* nail down the ship's other end */
		case REVERSE_JUMP: {
			st.d = gamestate.ts.dir + 4;
			st.x = gamestate.ts.x + gamestate.ts.hits * xincr[st.d];
			st.y = gamestate.ts.y + gamestate.ts.hits * yincr[st.d];
			int foo = game_coord_possible(st.x, st.y);
			if (foo) {
				int result = game_cpufire(st.x, st.y);
				st.hit = result;
				foo = st.hit;
			}
			if (foo) {
				gamestate.ts.x = st.x;
				gamestate.ts.y = st.y;
				gamestate.ts.dir = st.d;
				gamestate.ts.hits++;
				if (st.hit == S_SUNK) {
					gamestate.ai_next = RANDOM_FIRE;
				} else {
					gamestate.ai_next = SECOND_PASS;
				}
			} else {
				gamestate.ai_next = RANDOM_FIRE;
			}
		}

		/* kill squares not caught on first pass */
		case SECOND_PASS: {
			st.x = gamestate.ts.x + xincr[gamestate.ts.dir];
			st.y = gamestate.ts.y + yincr[gamestate.ts.dir];
			int foo = game_coord_possible(st.x, st.y);
			if (foo) {
				int result = game_cpufire(st.x, st.y);
				st.hit = result;
				foo = st.hit;
			}
			if (foo) {
				gamestate.ts.x = st.x;
				gamestate.ts.y = st.y;
				gamestate.ts.hits++;
				if (st.hit == S_SUNK) {
					gamestate.ai_next = RANDOM_FIRE;
				} else {
					gamestate.ai_next = SECOND_PASS;
				}
			} else {
				gamestate.ai_next = RANDOM_FIRE;
			}
		}
	}
	/* check for continuation and/or winner */
	if (salvo) {
		OS.refresh();
		time.sleep(1);
	}
	if (game_winner() != -1) {
		return false;
	}

	if (DEBUG) {
		OS.mvprintw(PROMPTLINE + 2, 0, "New state %d, x=%d, y=%d, d=%d", gamestate.ai_next, st.x, st.y, st.d);
	}
	return st.hit;
}

// implements simple diagonal-striping strategy
void game_randomfire(int *px, int *py) {
	int ypossible[BWIDTH * BDEPTH];
	int xpossible[BWIDTH * BDEPTH];
	int ypreferred[BWIDTH * BDEPTH];
	int xpreferred[BWIDTH * BDEPTH];

	if (gamestate.ai_turncount++ == 0) {
		gamestate.ai_huntoffs = rnd(gamestate.ai_srchstep);
	}

	/* first, list all possible moves */
	int nposs = 0;
	int npref = 0;
	for (int x = 0; x < BWIDTH; x++) {
		for (int y = 0; y < BDEPTH; y++) {
			if (!gamestate.hits[COMPUTER][x][y]) {
				xpossible[nposs] = x;
				ypossible[nposs] = y;
				nposs++;
				if (((x + gamestate.ai_huntoffs) % gamestate.ai_srchstep) !=
				    (y % gamestate.ai_srchstep)) {
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
		if (gamestate.ai_srchstep > 1) {
			--gamestate.ai_srchstep;
		}
	} else {
		panic("No moves possible?? Help!");
	}
}

void game_reset() {
	memset(gamestate.board, 0, sizeof(char) * BWIDTH * BDEPTH * 2);
	memset(gamestate.hits, 0, sizeof(char) * BWIDTH * BDEPTH * 2);
	for (int i = 0; i < SHIPTYPES; i++) {
		game.ship_t *ss = gamestate.cpuship + i;
		ss->x = ss->y = ss->dir = ss->hits = 0;
		ss->placed = false;
		ss = gamestate.plyship + i;
		ss->x = ss->y = ss->dir = ss->hits = 0;
		ss->placed = false;
	}
	/* have the computer place ships */
	game.ship_t *ss = NULL;
	for (ss = gamestate.cpuship; ss < gamestate.cpuship + SHIPTYPES; ss++) {
		game_randomplace(COMPUTER, ss);
		game_placeship(COMPUTER, ss);
	}
}

int game_cpufire(int x, y) {
	gamestate.ai_last_x = x;
	gamestate.ai_last_y = y;
	int result;
	bool hit = gamestate.board[PLAYER][x][y];
	if (hit) {
		gamestate.hits[COMPUTER][x][y] = MARK_HIT;
		game.ship_t *sunk = hitship(x, y);
		gamestate.sunk_by_ai = sunk;
		if (sunk) {
			result = S_SUNK;
		} else {
			result = S_HIT;
		}
	} else {
		gamestate.hits[COMPUTER][x][y] = MARK_MISS;
		result = S_MISS;
	}
	return result;
}

int game_winner() {
	int i;
	int j;
	for (i = 0; i < 2; ++i) {
		game.ship_t *ss;
		if (i) ss = gamestate.cpuship;
		else ss = gamestate.plyship;

		for (j = 0; j < SHIPTYPES; ) {
			if (ss->length > ss->hits) {
				break;
			}
			++j;
			++ss;
		}
		if (j == SHIPTYPES) {
			return 1 - gamestate.turn;
		}
	}
	return -1;
}

/* is this location on the selected zboard adjacent to a ship? */
bool game_has_collision(int b, int y, int x) {
	bool collide;

	/* anything on the square */
	if ((collide = game_is_ship(gamestate.board[b][x][y])) != false) {
		return (collide);
	}

	/* anything on the neighbors */
	if (!closepack) {
		for (int i = 0; i < 8; i++) {
			int yend = y + yincr[i];
			int xend = x + xincr[i];
			if (game_coords_valid(xend, yend) && game_is_ship(gamestate.board[b][xend][yend])) {
				collide = true;
				break;
			}
		}
	}
	return collide;
}

bool game_coords_valid(int x, y) {
	return (x >= 0 && x < BWIDTH && y >= 0 && y < BDEPTH);
}

int game_checkplacement(int b, game.ship_t *ss) {
	int err = 0;

	// Check board edges
	int xend = ss->x + (ss->length - 1) * xincr[ss->dir];
	int yend = ss->y + (ss->length - 1) * yincr[ss->dir];
	if (!game_coords_valid(xend, yend)) {
		err = ERR_HANGING;
	}
	if (!err) {
		for (int l = 0; l < ss->length; ++l) {
			if (game_has_collision(b, ss->y + l * yincr[ss->dir], ss->x + l * xincr[ss->dir])) {
				err = ERR_COLLISION;
				break;
			}
		}
	}
	return err;
}

void game_placeship(int player, game.ship_t *ss) {
	for (int l = 0; l < ss->length; ++l) {
		int x = ss->x + l * xincr[ss->dir];
		int y = ss->y + l * yincr[ss->dir];
		gamestate.board[player][x][y] = ss->symbol;
	}
	ss->hits = 0;
}

// Generates a valid random ship placement.
void game_randomplace(int player, game.ship_t *ss) {
	while (true) {
		if (rnd(2)) {
			ss->dir = E;
			ss->x = rnd(BWIDTH - ss->length);
			ss->y = rnd(BDEPTH);
		} else {
			ss->dir = S;
			ss->x = rnd(BWIDTH);
			ss->y = rnd(BDEPTH - ss->length);
		}
		int err = game_checkplacement(player, ss);
		if (!err) break;
	}
}

void game_do_hunt_direct(turn_state_t *st) {
	int navail = 0;
	st->d = 0;
	while (st->d < 4) {
		st->x = gamestate.ts.x + xincr[st->d * 2];
		st->y = gamestate.ts.y + yincr[st->d * 2];
		if (!gamestate.used[st->d] && game_coord_possible(st->x, st->y)) {
			navail++;
		} else {
			gamestate.used[st->d] = true;
		}
		st->d++;
	}
	if (navail == 0) /* no valid places for shots adjacent... */ {
		game_randomfire(&st->x, &st->y);
		int result = game_cpufire(st->x, st->y);
		st->hit = result;
		if (!st->hit) {
			gamestate.ai_next = RANDOM_FIRE;
		} else {
			gamestate.ts.x = st->x;
			gamestate.ts.y = st->y;
			gamestate.ts.hits = 1;
			if (st->hit == S_SUNK) {
				gamestate.ai_next = RANDOM_FIRE;
			} else {
				gamestate.ai_next = RANDOM_HIT;
			}
		}
	} else {
		st->d = 0;
		int n = rnd(navail) + 1;
		while (n) {
			while (gamestate.used[st->d]) {
				st->d++;
			}
			n--;
		}

		assert(st->d <= 4);

		gamestate.used[st->d] = false;
		st->x = gamestate.ts.x + xincr[st->d * 2];
		st->y = gamestate.ts.y + yincr[st->d * 2];

		assert(game_coord_possible(st->x, st->y));
		int result = game_cpufire(st->x, st->y);
		st->hit = result;
		if (!st->hit) {
			gamestate.ai_next = HUNT_DIRECT;
		} else {
			gamestate.ts.x = st->x;
			gamestate.ts.y = st->y;
			gamestate.ts.dir = st->d * 2;
			gamestate.ts.hits++;
			if (st->hit == S_SUNK) {
				gamestate.ai_next = RANDOM_FIRE;
			} else {
				gamestate.ai_next = FIRST_PASS;
			}
		}
	}
}

bool game_is_ship(int c) {
	return OS.isupper(c);
}

bool game_coord_possible(int x, y) {
	return game_coords_valid(x, y) && !gamestate.hits[COMPUTER][x][y];
}

int game_shipscount(int player) {
	game.ship_t *ships;
	if (player == COMPUTER) {
		ships = gamestate.cpuship;
	} else {
		ships = gamestate.plyship;
	}

	int n = 0;
	for (int i = 0; i < SHIPTYPES; i++) {
		game.ship_t *sp = &ships[i];
		if (sp->hits >= sp->length) {
			continue; /* dead ship */
		}
		n++;
	}
	return n;
}

/////////////////////////////////////////////////////////
// -------------------- render --------------------

#define SHOWHIT '*'
#define SHOWSPLASH ' '
int COLWIDTH = 80;
int PROMPTLINE = 21;

const char *numbers = "   0  1  2  3  4  5  6  7  8  9";
const char *name = "stranger";

const char *shipname(int s) {
	switch (s) {
		case game.SHIP_CARRIER: { return "Aircraft Carrier"; }
		case game.SHIP_BATTLESHIP: { return "Battleship"; }
		case game.SHIP_SUBMARINE: { return "Submarine"; }
		case game.SHIP_DESTROYER: { return "Destroyer"; }
		case game.SHIP_PTBOAT: { return "PT Boat"; }
	}
	return "unknown";
}

void render_placement_prompt() {
	render_prompt(1, "Type one of [%s] to pick a ship.", gamestate.docked + 1);
}

void render_reset() {
	OS.clear();
	OS.mvaddstr(0, 35, "BATTLESHIPS");
	OS.move(PROMPTLINE + 2, 0);
	render_options();
	render_empty_boards();
	OS.mvaddstr(CYBASE + BDEPTH, CXBASE - 3, numbers);
	render_manual();
	render_keys();
}

void render_ai_shot() {
	int x = gamestate.ai_last_x;
	int y = gamestate.ai_last_y;
	if (gamestate.hits[COMPUTER][x][y] == MARK_HIT) {
		render_ai_hit();
	} else {
		render_ai_miss();
	}
}

void render_init() {
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

void render_exit() {
	OS.clear();
	OS.refresh();
	OS.resetterm();
	OS.echo();
	OS.endwin();
}

void render_penguin() {
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

void render_options() {
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

void render_placed_ship(game.ship_t *ss) {
	for (int l = 0; l < ss->length; ++l) {
		int x = ss->x + l * xincr[ss->dir];
		int y = ss->y + l * yincr[ss->dir];
		pgoto(y, x);
		OS.addch(ss->symbol);
	}
}

void render_current_coords(int atcpu) {
	if (atcpu) {
		OS.mvprintw(CYBASE + BDEPTH + 1, CXBASE + 11, "(%d, %c)", gamestate.curx, 'A' + gamestate.cury);
		cgoto(gamestate.cury, gamestate.curx);
	} else {
		OS.mvprintw(PYBASE + BDEPTH + 1, PXBASE + 11, "(%d, %c)", gamestate.curx, 'A' + gamestate.cury);
		pgoto(gamestate.cury, gamestate.curx);
	}
}

void render_empty_boards() {
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
}

void render_manual() {
	OS.mvprintw(HYBASE, HXBASE, "To position your ships: move the cursor to a spot, then");
	OS.mvprintw(HYBASE + 1, HXBASE, "type the first letter of a ship type to select it, then");
	OS.mvprintw(HYBASE + 2, HXBASE, "type a direction ([hjkl] or [4862]), indicating how the");
	OS.mvprintw(HYBASE + 3, HXBASE, "ship should be pointed. You may also type a ship letter");
	OS.mvprintw(HYBASE + 4, HXBASE, "followed by `r' to position it randomly, or type `R' to");
	OS.mvprintw(HYBASE + 5, HXBASE, "place all remaining ships randomly.");
}

void render_keys() {
	OS.mvaddstr(MYBASE, MXBASE, "Aiming keys:");
	OS.mvaddstr(SYBASE, SXBASE, "y k u    7 8 9");
	OS.mvaddstr(SYBASE + 1, SXBASE, " \\|/      \\|/ ");
	OS.mvaddstr(SYBASE + 2, SXBASE, "h-+-l    4-+-6");
	OS.mvaddstr(SYBASE + 3, SXBASE, " /|\\      /|\\ ");
	OS.mvaddstr(SYBASE + 4, SXBASE, "b j n    1 2 3");
}

void render_manual2() {
	OS.mvprintw(HYBASE, HXBASE, "To fire, move the cursor to your chosen aiming point   ");
	OS.mvprintw(HYBASE + 1, HXBASE, "and strike any key other than a motion key.            ");
	OS.mvprintw(HYBASE + 2, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 3, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 4, HXBASE, "                                                       ");
	OS.mvprintw(HYBASE + 5, HXBASE, "                                                       ");
	render_prompt(0, "Press any key to start...", "");
}

void render_placement_error(int err) {
	if (err == ERR_HANGING) {
		switch (rnd(3)) {
			case 0: { render_error("Ship is hanging from the edge of the world"); }
			case 1: { render_error("Try fitting it on the board"); }
			case 2: { render_error("Figure I won't find it if you put it there?"); }
		}
	}
	if (err == ERR_COLLISION) {
		switch (rnd(3)) {
			case 0: { render_error("There's already a ship there"); }
			case 1: { render_error("Collision alert!  Aaaaaagh!"); }
			case 2: { render_error("Er, Admiral, what about the other ship?"); }
		}
	}
}

void render_prompt(int n, const char *f, const char *s) {
	OS.move(PROMPTLINE + n, 0);
	OS.clrtoeol();
	OS.printw(f, s);
	OS.refresh();
}

void render_error(const char *s) {
	OS.move(PROMPTLINE + 2, 0);
	OS.clrtoeol();
	if (s) {
		OS.addstr(s);
		OS.beep();
	}
}

void render_player_shot() {
	int cury = gamestate.cury;
	int curx = gamestate.curx;
	bool hit = gamestate.player_hit;
	game.ship_t *sunk = gamestate.sunk_by_player;
	cgoto(cury, curx);
	if (OS.has_colors()) {
		if (hit) {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
		} else {
			OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
		}
	}
	OS.addch(gamestate.hits[PLAYER][curx][cury]);
	OS.attrset(0);
	if (hit) {
		render_prompt(1, "You scored a hit.", "");
	} else {
		render_prompt(1, "You missed.", "");
	}
	if (sunk) {
		char *m;
		switch (rnd(5)) {
			case 0: { m = " You sank my %s!"; }
			case 1: { m = " I have this sinking feeling about my %s...."; }
			case 2: { m = " My %s has gone to Davy Jones's locker!"; }
			case 3: { m = " Glub, glub -- my %s is headed for the bottom!"; }
			case 4: { m = " You'll pick up survivors from my %s, I hope...!"; }
		}
		OS.printw(m, shipname(sunk->kind));
		OS.beep();
	}
}

void render_ai_hit() {
	int x = gamestate.ai_last_x;
	int y = gamestate.ai_last_y;
	OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "hit");
	if (gamestate.sunk_by_ai) {
		OS.printw(" I've sunk your %s", shipname(gamestate.sunk_by_ai->kind));
	}
	OS.clrtoeol();
	pgoto(y, x);
	if (OS.has_colors()) {
		OS.attron(OS.COLOR_PAIR(OS.COLOR_RED));
	}
	OS.addch(SHOWHIT);
	OS.attrset(0);
}

void render_ai_miss() {
	int x = gamestate.ai_last_x;
	int y = gamestate.ai_last_y;
	OS.mvprintw(PROMPTLINE, 0, "I shoot at %c%d. I %s!", y + 'A', x, "miss");
	OS.clrtoeol();
	pgoto(y, x);
	if (OS.has_colors()) {
		OS.attron(OS.COLOR_PAIR(OS.COLOR_GREEN));
	}
	OS.addch(SHOWSPLASH);
	OS.attrset(0);
}

void render_cpu_ships() {
	int j;
	game.ship_t *ss;
	for (ss = gamestate.cpuship; ss < gamestate.cpuship + SHIPTYPES; ss++) {
		for (j = 0; j < ss->length; j++) {
			cgoto(ss->y + j * yincr[ss->dir], ss->x + j * xincr[ss->dir]);
			OS.addch(ss->symbol);
		}
	}
}

void render_winner() {
	int j = 18 + strlen(name);
	if (gamestate.plywon >= 10) {
		++j;
	}
	if (gamestate.cpuwon >= 10) {
		++j;
	}
	OS.mvprintw(1, (COLWIDTH - j) / 2, "%s: %d     Computer: %d", name, gamestate.plywon, gamestate.cpuwon);
}

void render_playgain_prompt() {
	if (gamestate.winner == COMPUTER) {
		render_prompt(2, "Want to be humiliated again [yn]?", "");
	} else {
		render_prompt(2, "Going to give me a chance for revenge [yn]?", "");
	}
}

void render_turn() {
	if (gamestate.turn == COMPUTER) {
		render_ai_shot();
	} else {
		render_player_shot();
	}
}

void render_reveal_ship(game.ship_t *ss) {
	for (int i = 0; i < ss->length; ++i) {
		int x1 = ss->x + i * xincr[ss->dir];
		int y1 = ss->y + i * yincr[ss->dir];

		gamestate.hits[gamestate.turn][x1][y1] = ss->symbol;
		if (gamestate.turn % 2 == PLAYER) {
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
