/*
 * bs.c - original author: Bruce Holloway
 *		salvo option by: Chuck A DeGaul
 * with improved user interface, autoconfiguration and code cleanup
 *
 * SPDX-FileCopyrightText: (C) Eric S. Raymond <esr@thyrsus.com>
 * SPDX-License-Identifier: BSD-2-Clause
 */
#import ai.c
#import game.c
#import opt
#import render.c
#import rnd
#import time

// options
bool salvo = false;
bool blitz = false;
bool closepack = false;
bool PENGUIN = true;

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

ai.state_t AI = {};

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
	srand(time(NULL));

	signal(SIGINT, closegame);
	signal(SIGINT, closegame);
	signal(SIGABRT, closegame); /* for assert(3) */
	if (signal(OS.SIGQUIT, SIG_IGN) != SIG_IGN) {
		signal(OS.SIGQUIT, closegame);
	}

	render.init();
	if (PENGUIN) {
		render.render_penguin();
		input_any();
	}

// #ifdef NCURSES_MOUSE_VERSION
	// OS.mousemask(OS.BUTTON1_CLICKED, NULL);
// #endif /* NCURSES_MOUSE_VERSION*/

	while (true) {
		game.reset(&gamestate);
		ai.reset(&AI);
		ai_place_ships();
		render.reset();
		place_ships();
		if (rnd.intn(2)) {
			gamestate.turn = COMPUTER;
		} else {
			gamestate.turn = PLAYER;
		}
		render.render_manual2();
		input_any();
		play();
		int winner = game.winner(&gamestate);
		gamestate.winner = winner;
		gamestate.players[winner].wins++;
		render.render_cpu_ships(&gamestate);
		render.render_winner(&gamestate);
		render.render_playagain_prompt(&gamestate);
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
			render.render_turn(&gamestate, &AI);
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
			render.render_turn(&gamestate, &AI);
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
		render.render_turn(&gamestate, &AI);
		gamestate.turn = 1 - gamestate.turn;
	}
}

enum {
	PLAYER = 0,
	COMPUTER = 1,
};

#define CTRLC '\003' /* used as terminate command */

#define PYBASE 3
#define PXBASE 3

void pgoto(int y, x) {
	OS.move(PYBASE + y, PXBASE + x*3);
}

void closegame(int sig) {
	render.end();
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

		render.render_placement_prompt(&gamestate);

		char c;
		while (true) {
			c = toupper(getcoord(PLAYER));
			render.render_clear_coords(PLAYER);
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

			render.render_prompt(1, "Type one of [hjklrR] to place your %s.", game.shipname(ss->kind));
			pgoto(gamestate.cury, gamestate.curx);
		}

		while (true) {
			c = OS.getch();
			if (!strchr("hjklrR", c)) continue;
			break;
		}

		if (c == 'r') {
			render.render_prompt(1, "Random-placing your %s", game.shipname(ss->kind));

			game.randomplace(&gamestate, PLAYER, ss);
			game.placeship(&gamestate, PLAYER, ss);
			ss->placed = true;

			render.draw_placed_ship(ss);
			render.render_error(NULL);
			
		} else if (c == 'R') {
			render.render_prompt(1, "Placing the rest of your fleet at random...", "");
			for (ss = gamestate.players[PLAYER].ships; ss < gamestate.players[PLAYER].ships + SHIPTYPES; ss++) {
				if (!ss->placed) {
					game.randomplace(&gamestate, PLAYER, ss);
					game.placeship(&gamestate, PLAYER, ss);
					ss->placed = true;

					render.draw_placed_ship(ss);					
				}
			}
			render.render_error(NULL);
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
				render.render_placement_error(err);
			} else {
				game.placeship(&gamestate, PLAYER, ss);
				ss->placed = true;
				
				render.draw_placed_ship(ss);
				render.render_error(NULL);
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
	render.render_cursor(&gamestate, atcpu);
	int c;
	for (;;) {
		render.render_current_coords(&gamestate, atcpu);
		c = input_key();
		if (!game_applycoord(c)) {
			break;
		}
	}
	return c;
}

void s_input_player_shot() {
	render.render_prompt(1, "Where do you want to shoot? ", "");
	while (true) {
		getcoord(COMPUTER);
		render.render_clear_coords(COMPUTER);
		if (gamestate.players[PLAYER].shots[gamestate.curx][gamestate.cury]) {
			render.render_prompt(1, "You shelled this spot already! Try again.", "");
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
		render.render_hit_ship(&gamestate, gamestate.sunk_by_player);
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

bool seq_cputurn() {
	int result = ai.turn(&gamestate, &AI);
	if (result == game.S_SUNK) {
		render.render_hit_ship(&gamestate, gamestate.sunk_by_ai);
	}
	/* check for continuation and/or winner */
	if (salvo) {
		OS.refresh();
		time.sleep(1);
	}
	if (game.winner(&gamestate) != -1) {
		return false;
	}
	return AI.hit;
}
