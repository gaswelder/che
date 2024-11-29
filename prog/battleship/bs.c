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

// ai_next values
enum {
	RANDOM_FIRE = 0,
	HUNT_DIRECT = 2,
	FIRST_PASS = 3,
	REVERSE_JUMP = 4,
	SECOND_PASS = 5,
};

// /* direction constants */
enum {
	E = 0,
	S = 2,
	W = 4,
	N = 6,
};

#define BWIDTH 10
#define BDEPTH 10

game.state_t gamestate = {
	.curx = BWIDTH / 2,
	.cury = BDEPTH / 2,
};

typedef {
	int ai_next;
	int ai_last_x;
	int ai_last_y;
	int ai_turncount;
	int ai_srchstep;
	int ai_huntoffs; /* Offset on search strategy */
	bool used[4];
	game.ship_t ts;
} ai_state_t;

ai_state_t AI = {
	//
};

const int SHIPTYPES = 5;

xy_t shipxy(int x, y, dir, dist) {
	const int xincr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
	const int yincr[8] = {0, 1, 1, 1, 0, -1, -1, -1};
	xy_t r = {
		.x = x + dist * xincr[dir],
		.y = y + dist * yincr[dir]
	};
	return r;
}

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
		game.reset(&gamestate);
		memset(&AI, 0, sizeof(AI));
		AI.ai_srchstep = BEGINSTEP;
		ai_place_ships();
		render_reset();
		place_ships();
		if (rnd(2)) {
			gamestate.turn = COMPUTER;
		} else {
			gamestate.turn = PLAYER;
		}
		render_manual2();
		input_any();
		play();
		int winner = game.winner(&gamestate);
		gamestate.winner = winner;
		gamestate.players[winner].wins++;
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
	if (blitz) {
		play_blitz();
	} else if (salvo) {
		play_salvo();
	} else {
		play_regular();
	}
}

void play_blitz() {
	while (game.winner(&gamestate) == -1) {
		while (true) {
			bool ok;
			if (gamestate.turn == COMPUTER) {
				ok = seq_cputurn();
			} else {
				s_input_player_shot();
				ok = seq_playerturn();
			}
			render_turn();
			if (!ok) {
				break;
			}
		}
		gamestate.turn = 1 - gamestate.turn;
	}
}

void play_salvo() {
	while (game.winner(&gamestate) == -1) {
		int i = game.shipscount(&gamestate, gamestate.turn);
		while (i--) {
			if (gamestate.turn == COMPUTER) {
				if (seq_cputurn() && game.winner(&gamestate) != -1) {
					i = 0;
				}
			} else {
				s_input_player_shot();
				int result = seq_playerturn();
				if (result && game.winner(&gamestate) != -1) {
					i = 0;
				}
			}
			render_turn();
		}
		gamestate.turn = 1 - gamestate.turn;
	}
}

void play_regular() {
	while (game.winner(&gamestate) == -1) {
		if (gamestate.turn == COMPUTER) {
			seq_cputurn();
		} else {
			s_input_player_shot();
			seq_playerturn();
		}
		render_turn();
		gamestate.turn = 1 - gamestate.turn;
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
};

#define CTRLC '\003' /* used as terminate command */

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

void ai_place_ships() {
	game.ship_t *ss = NULL;
	for (ss = gamestate.players[COMPUTER].ships; ss < gamestate.players[COMPUTER].ships + SHIPTYPES; ss++) {
		game.randomplace(&gamestate, COMPUTER, ss);
		game.placeship(&gamestate, COMPUTER, ss);
	}
}

void place_ships() {
	int unplaced;
	game.ship_t *ss = NULL;
	while (true) {
		/* figure which ships still wait to be placed */
		char *cp = gamestate.docked;
		*cp++ = 'R';
		for (int i = 0; i < SHIPTYPES; i++) {
			if (!gamestate.players[PLAYER].ships[i].placed) {
				*cp++ = gamestate.players[PLAYER].ships[i].symbol;
			}
		}
		*cp = '\0';


		render_placement_prompt();

		char c;
		while (true) {
			c = toupper(getcoord(PLAYER));
			render_clear_coords(PLAYER);
			if (!strchr(gamestate.docked, c)) continue;
			else break;
		}

		if (c == 'R') {
			OS.ungetch('R');
		} else {
			/* map that into the corresponding symbol */
			for (ss = gamestate.players[PLAYER].ships; ss < gamestate.players[PLAYER].ships + SHIPTYPES; ss++) {
				if (ss->symbol == c) {
					break;
				}
			}

			render_prompt(1, "Type one of [hjklrR] to place your %s.", shipname(ss->kind));
			pgoto(gamestate.cury, gamestate.curx);
		}

		while (true) {
			c = OS.getch();
			if (!strchr("hjklrR", c)) continue;
			break;
		}

		if (c == 'r') {
			render_prompt(1, "Random-placing your %s", shipname(ss->kind));

			game.randomplace(&gamestate, PLAYER, ss);
			game.placeship(&gamestate, PLAYER, ss);
			ss->placed = true;

			render_placed_ship(ss);
			render_error(NULL);
			
		} else if (c == 'R') {
			render_prompt(1, "Placing the rest of your fleet at random...", "");
			for (ss = gamestate.players[PLAYER].ships; ss < gamestate.players[PLAYER].ships + SHIPTYPES; ss++) {
				if (!ss->placed) {
					game.randomplace(&gamestate, PLAYER, ss);
					game.placeship(&gamestate, PLAYER, ss);
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

			int err = game.checkplacement(&gamestate, PLAYER, ss);
			if (err) {
				render_placement_error(err);
			} else {
				game.placeship(&gamestate, PLAYER, ss);
				ss->placed = true;
				
				render_placed_ship(ss);
				render_error(NULL);
			}
		}
		unplaced = 0;
		for (int i = 0; i < SHIPTYPES; i++) {
			unplaced += !gamestate.players[PLAYER].ships[i].placed;
		}
		if (unplaced) continue;
		else break;
	}
}

int getcoord(int atcpu) {
	render_cursor(atcpu);
	int c;
	for (;;) {
		render_current_coords(atcpu);
		c = input_key();
		if (!game_applycoord(c)) {
			break;
		}
	}
	return c;
}

void s_input_player_shot() {
	render_prompt(1, "Where do you want to shoot? ", "");
	while (true) {
		getcoord(COMPUTER);
		render_clear_coords(COMPUTER);
		if (gamestate.players[PLAYER].shots[gamestate.curx][gamestate.cury]) {
			render_prompt(1, "You shelled this spot already! Try again.", "");
			OS.beep();
			continue;
		}
		break;
	}
}

int input_key() {
	return OS.getch();
}

void input_any() {
	OS.getch();
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

bool game_applycoord(int c) {
	switch (c) {
		case 'k', '8', OS.KEY_UP: {
			gamestate.ny = gamestate.cury + BDEPTH - 1;
			gamestate.nx = gamestate.curx;
		}
		case 'j', '2', OS.KEY_DOWN: {
			gamestate.ny = gamestate.cury + 1;
			gamestate.nx = gamestate.curx;
		}
		case 'h', '4', OS.KEY_LEFT: {
			gamestate.ny = gamestate.cury;
			gamestate.nx = gamestate.curx + BWIDTH - 1;
		}
		case 'l', '6', OS.KEY_RIGHT: {
			gamestate.ny = gamestate.cury;
			gamestate.nx = gamestate.curx + 1;
		}
		case 'y', '7', OS.KEY_A1: {
			gamestate.ny = gamestate.cury + BDEPTH - 1;
			gamestate.nx = gamestate.curx + BWIDTH - 1;
		}
		case 'b', '1', OS.KEY_C1: {
			gamestate.ny = gamestate.cury + 1;
			gamestate.nx = gamestate.curx + BWIDTH - 1;
		}
		case 'u', '9', OS.KEY_A3: {
			gamestate.ny = gamestate.cury + BDEPTH - 1;
			gamestate.nx = gamestate.curx + 1;
		}
		case 'n', '3', OS.KEY_C3: {
			gamestate.ny = gamestate.cury + 1;
			gamestate.nx = gamestate.curx + 1;
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
			return false;
		}
	}
	gamestate.curx = gamestate.nx % BWIDTH;
	gamestate.cury = gamestate.ny % BDEPTH;
	return true;
}

int seq_playerturn() {
	int x = gamestate.curx;
	int y = gamestate.cury;
	int result = game.shoot(&gamestate, PLAYER, x, y);

	if (result == game.S_SUNK) {
		gamestate.player_hit = true;
		gamestate.sunk_by_player = game.getshipat(&gamestate, COMPUTER, x, y);
		render_hit_ship(gamestate.sunk_by_player);
		return game.winner(&gamestate) == -1;
	}
	if (result == game.S_HIT) {
		gamestate.player_hit = true;
		gamestate.sunk_by_player = NULL;
		return true;
	}
	gamestate.player_hit = false;
	gamestate.sunk_by_player = NULL;
	return false;
}

typedef {
	int x, y, hit, d;
} turn_state_t;



bool seq_cputurn() {
	turn_state_t st = {.hit = game.S_MISS};

	switch (AI.ai_next) {
		case RANDOM_FIRE: { ai_random_fire(&st); }

		/* last shot hit, we're looking for ship's axis */
		case HUNT_DIRECT: { ai_do_hunt_direct(&st); }

		/* we have a start and a direction now */
		case FIRST_PASS: { ai_first_pass(&st); }

		/* nail down the ship's other end */
		case REVERSE_JUMP: {
			st.d = AI.ts.dir + 4;
			xy_t xy = shipxy(AI.ts.x, AI.ts.y, st.d, AI.ts.hits);
			st.x = xy.x;
			st.y = xy.y;
			int foo = game.coords_possible(&gamestate, st.x, st.y);
			if (foo) {
				int result = ai_cpufire(st.x, st.y);
				st.hit = result;
				foo = st.hit;
			}
			if (foo) {
				AI.ts.x = st.x;
				AI.ts.y = st.y;
				AI.ts.dir = st.d;
				AI.ts.hits++;
				if (st.hit == game.S_SUNK) {
					AI.ai_next = RANDOM_FIRE;
				} else {
					AI.ai_next = SECOND_PASS;
				}
			} else {
				AI.ai_next = RANDOM_FIRE;
			}
		}

		/* kill squares not caught on first pass */
		case SECOND_PASS: {
			ai_second_pass(&st);
		}
	}
	/* check for continuation and/or winner */
	if (salvo) {
		OS.refresh();
		time.sleep(1);
	}
	if (game.winner(&gamestate) != -1) {
		return false;
	}

	if (DEBUG) {
		OS.mvprintw(PROMPTLINE + 2, 0, "New state %d, x=%d, y=%d, d=%d", AI.ai_next, st.x, st.y, st.d);
	}
	return st.hit;
}

void ai_second_pass(turn_state_t *st) {
	xy_t xy = shipxy(AI.ts.x, AI.ts.y, AI.ts.dir, 1);
	st->x = xy.x;
	st->y = xy.y;
	if (!game.coords_possible(&gamestate, st->x, st->y)) {
		AI.ai_next = RANDOM_FIRE;
		return;
	}
	int result = ai_cpufire(st->x, st->y);
	st->hit = result;
	switch (result) {
		case game.S_MISS, game.S_SUNK: {
			AI.ai_next = RANDOM_FIRE;
		}
		case game.S_HIT: {
			AI.ts.x = st->x;
			AI.ts.y = st->y;
			AI.ts.hits++;
		}
		default: {
			panic("!");
		}
	}
}

void ai_first_pass(turn_state_t *st) {
	xy_t xy = shipxy(AI.ts.x, AI.ts.y, AI.ts.dir, 1);
	st->x = xy.x;
	st->y = xy.y;
	if (!game.coords_possible(&gamestate, st->x, st->y)) {
		AI.ai_next = REVERSE_JUMP;
		return;
	}
	int result = ai_cpufire(st->x, st->y);
	st->hit = result;
	switch (result) {
		case game.S_MISS: { AI.ai_next = REVERSE_JUMP; }
		case game.S_SUNK: { AI.ai_next = RANDOM_FIRE; }
		case game.S_HIT: {
			AI.ts.x = st->x;
			AI.ts.y = st->y;
			AI.ts.hits++;
			// continue first pass
		}
		default: { panic("!"); }
	}
}

void ai_choose_move(int *px, int *py) {
	// List all possible moves.
	int nposs = 0;
	int ypossible[BWIDTH * BDEPTH];
	int xpossible[BWIDTH * BDEPTH];
	for (int x = 0; x < BWIDTH; x++) {
		for (int y = 0; y < BDEPTH; y++) {
			if (gamestate.players[COMPUTER].shots[x][y]) continue;
			xpossible[nposs] = x;
			ypossible[nposs] = y;
			nposs++;
		}
	}
	if (!nposs) {
		panic("No moves possible?? Help!");
	}

	// List preferred possible moves.
	if (AI.ai_turncount++ == 0) {
		AI.ai_huntoffs = rnd(AI.ai_srchstep);
	}
	int npref = 0;
	int ypreferred[BWIDTH * BDEPTH];
	int xpreferred[BWIDTH * BDEPTH];
	for (int i = 0; i < nposs; i++) {
		int x = xpossible[i];
		int y = ypossible[i];
		if (((x + AI.ai_huntoffs) % AI.ai_srchstep) != (y % AI.ai_srchstep)) {
			xpreferred[npref] = x;
			ypreferred[npref] = y;
			npref++;
		}
	}

	if (npref) {
		int i = rnd(npref);
		*px = xpreferred[i];
		*py = ypreferred[i];
	} else {
		int i = rnd(nposs);
		*px = xpossible[i];
		*py = ypossible[i];
		if (AI.ai_srchstep > 1) {
			--AI.ai_srchstep;
		}
	}
}

int ai_cpufire(int x, y) {
	AI.ai_last_x = x;
	AI.ai_last_y = y;

	int result = game.shoot(&gamestate, COMPUTER, x, y);
	if (result == game.S_SUNK) {
		game.ship_t *sunk = game.getshipat(&gamestate, PLAYER, x, y);
		render_hit_ship(sunk);
		gamestate.sunk_by_ai = sunk;
	}
	return result;
}

void ai_random_fire(turn_state_t *st) {
	ai_choose_move(&st->x, &st->y);
	switch (ai_cpufire(st->x, st->y)) {
		case game.S_HIT: {
			AI.ts.x = st->x;
			AI.ts.y = st->y;
			AI.ts.hits = 1;
			AI.used[E / 2] = false;
			AI.used[S / 2] = false;
			AI.used[W / 2] = false;
			AI.used[N / 2] = false;
			AI.ai_next = HUNT_DIRECT;
		}
		case game.S_MISS, game.S_SUNK: {
			// keep random shooting
		}
		default: { panic("!"); }
	}
}

void ai_do_hunt_direct(turn_state_t *st) {
	int navail = 0;
	st->d = 0;
	while (st->d < 4) {
		xy_t xy = shipxy(AI.ts.x, AI.ts.y, st->d * 2, 1);
		st->x = xy.x;
		st->y = xy.y;
		if (!AI.used[st->d] && game.coords_possible(&gamestate, st->x, st->y)) {
			navail++;
		} else {
			AI.used[st->d] = true;
		}
		st->d++;
	}
	if (navail == 0) /* no valid places for shots adjacent... */ {
		ai_random_fire(st);
		return;
	}

	st->d = 0;
	int n = rnd(navail) + 1;
	while (n) {
		while (AI.used[st->d]) {
			st->d++;
		}
		n--;
	}

	assert(st->d <= 4);

	AI.used[st->d] = false;
	xy_t xy = shipxy(AI.ts.x, AI.ts.y, st->d * 2, 1);
	st->x = xy.x;
	st->y = xy.y;

	assert(game.coords_possible(&gamestate, st->x, st->y));
	int result = ai_cpufire(st->x, st->y);
	st->hit = result;
	if (!st->hit) {
		AI.ai_next = HUNT_DIRECT;
	} else {
		AI.ts.x = st->x;
		AI.ts.y = st->y;
		AI.ts.dir = st->d * 2;
		AI.ts.hits++;
		if (st->hit == game.S_SUNK) {
			AI.ai_next = RANDOM_FIRE;
		} else {
			AI.ai_next = FIRST_PASS;
		}
	}
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
	int x = AI.ai_last_x;
	int y = AI.ai_last_y;
	if (gamestate.players[COMPUTER].shots[x][y] == game.MARK_HIT) {
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

typedef { int x, y; } xy_t;



void render_placed_ship(game.ship_t *ss) {
	for (int l = 0; l < ss->length; ++l) {
		xy_t xy = shipxy(ss->x, ss->y, ss->dir, l);
		pgoto(xy.y, xy.x);
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
	if (err == game.ERR_HANGING) {
		switch (rnd(3)) {
			case 0: { render_error("Ship is hanging from the edge of the world"); }
			case 1: { render_error("Try fitting it on the board"); }
			case 2: { render_error("Figure I won't find it if you put it there?"); }
		}
	}
	if (err == game.ERR_COLLISION) {
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
	OS.addch(gamestate.players[PLAYER].shots[curx][cury]);
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
	int x = AI.ai_last_x;
	int y = AI.ai_last_y;
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
	int x = AI.ai_last_x;
	int y = AI.ai_last_y;
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
	for (ss = gamestate.players[COMPUTER].ships; ss < gamestate.players[COMPUTER].ships + SHIPTYPES; ss++) {
		for (j = 0; j < ss->length; j++) {
			xy_t xy = shipxy(ss->x, ss->y, ss->dir, j);
			cgoto(xy.y, xy.x);
			OS.addch(ss->symbol);
		}
	}
}

void render_winner() {
	int j = 18 + strlen(name);
	int pwins = gamestate.players[PLAYER].wins;
	int cwins = gamestate.players[COMPUTER].wins;
	if (pwins >= 10) ++j;
	if (cwins >= 10) ++j;
	OS.mvprintw(1, (COLWIDTH - j) / 2, "%s: %d     Computer: %d", name, pwins, cwins);
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
		xy_t xy = shipxy(ss->x, ss->y, ss->dir, i);
		int x1 = xy.x;
		int y1 = xy.y;

		gamestate.players[gamestate.turn].shots[x1][y1] = ss->symbol;
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

void render_empty_water(game.ship_t *ss) {
	if (closepack) return;
	for (int j = -1; j <= 1; j++) {
		xy_t xy = shipxy(ss->x, ss->y, (ss->dir + 2) % 8, j);
		int bx = xy.x;
		int by = xy.y;
		for (int i = -1; i <= ss->length; ++i) {
			xy_t xy1 = shipxy(bx, by, ss->dir, i);
			int x1 = xy1.x;
			int y1 = xy1.y;
			if (game.coords_valid(x1, y1)) {
				if (gamestate.turn % 2 == PLAYER) {
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

void render_hit_ship(game.ship_t *ss) {
	int oldx;
	int oldy;
	OS.getyx(OS.stdscr, oldy, oldx);
	render_empty_water(ss);
	render_reveal_ship(ss);
	OS.move(oldy, oldx);
}

void render_cursor(int atcpu) {
	if (atcpu) {
		cgoto(gamestate.cury, gamestate.curx);
	} else {
		pgoto(gamestate.cury, gamestate.curx);
	}
	OS.refresh();
}

void render_clear_coords(int atcpu) {
	if (atcpu) {
		OS.mvaddstr(CYBASE + BDEPTH + 1, CXBASE + 11,"      ");
	} else {
		OS.mvaddstr(PYBASE + BDEPTH + 1, PXBASE + 11,"      ");
	}
}