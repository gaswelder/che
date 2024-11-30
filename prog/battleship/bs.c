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

#define CTRLC '\003' /* used as terminate command */

// options
bool salvo = false;
bool blitz = false;
bool closepack = false;

// /* direction constants */
enum {
	E = 0,
	S = 2,
	W = 4,
	N = 6,
};

#define BWIDTH 10
#define BDEPTH 10

game.state_t gamestate = {};
ai.state_t AI = {};

const int SHIPTYPES = 5;

enum {
	PLAYER = 0,
	COMPUTER = 1,
};

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
		strcpy(gamestate.status, "Arrows: move the cursor; any key: shoot");
		render.draw_status(&gamestate);
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

void play_regular() {
	while (game.winner(&gamestate) == -1) {
		turn();
		render.render_turn(&gamestate, &AI);
		gamestate.turn = 1 - gamestate.turn;
	}
}

void play_blitz() {
	while (game.winner(&gamestate) == -1) {
		int result = turn();
		render.render_turn(&gamestate, &AI);
		if (!result) {
			gamestate.turn = 1 - gamestate.turn;
		}
	}
}

void play_salvo() {
	while (game.winner(&gamestate) == -1) {
		int nships = game.shipscount(&gamestate, gamestate.turn);
		while (nships--) {
			turn();
			render.render_turn(&gamestate, &AI);
			if (game.winner(&gamestate) != -1) {
				break;
			}
			if (gamestate.turn == COMPUTER) {
				OS.refresh();
				time.sleep(1);
			}
		}
		gamestate.turn = 1 - gamestate.turn;
	}
}

int turn() {
	if (gamestate.turn == COMPUTER) {
		int result = ai.turn(&gamestate, &AI);
		int x = AI.ai_last_x;
		int y = AI.ai_last_y;
		switch (result) {
			case game.S_SUNK: {
				game.ship_t *ss = gamestate.sunk_by_ai;
				game.log(&gamestate, "computer: %c%d - hit", y + 'A', x);
				game.log(&gamestate, "computer has sunk your %s", game.shipname(ss->kind));
				render.render_hit_ship(&gamestate, ss);
			}
			case game.S_HIT: {
				game.log(&gamestate, "computer: %c%d - hit", y + 'A', x);
			}
			case game.S_MISS: {
				game.log(&gamestate, "computer: %c%d - miss", y + 'A', x);
			}
		}
		render.refresh_log(&gamestate);
		if (game.winner(&gamestate) != -1) {
			return false;
		}
		return AI.hit;
	} else {
		game.xy_t xy = playerturn_getcoords();
		int result = game.shoot(&gamestate, PLAYER, xy.x, xy.y);
		int x = xy.x;
		int y = xy.y;
		if (result == game.S_SUNK) {
			game.ship_t *ss = game.getshipat(&gamestate, COMPUTER, x, y);
			game.log(&gamestate, "player: %c%d - hit", y + 'A', x);
			game.log(&gamestate, "player has sunk computer's %s", game.shipname(ss->kind));
			gamestate.player_hit = true;
			gamestate.sunk_by_player = ss;
			render.render_hit_ship(&gamestate, ss);
			return game.winner(&gamestate) == -1;
		}
		if (result == game.S_HIT) {
			game.log(&gamestate, "player: %c%d - hit", y + 'A', x);
			gamestate.player_hit = true;
			gamestate.sunk_by_player = NULL;
			return true;
		}
		game.log(&gamestate, "player: %c%d - miss", y + 'A', x);
		gamestate.player_hit = false;
		gamestate.sunk_by_player = NULL;
		return false;
	}
}





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
	while (unplaced() > 0) {
		/* figure which ships still wait to be placed */
		char docked[10]; // ships that the player has to place
		char *cp = docked;
		*cp++ = 'R';
		for (int i = 0; i < SHIPTYPES; i++) {
			if (!gamestate.players[PLAYER].ships[i].placed) {
				*cp++ = gamestate.players[PLAYER].ships[i].symbol;
			}
		}
		*cp = '\0';

		sprintf(gamestate.status, "[%s]: place ship at current position; r: place remaining ships at random.", docked + 1);
		render.draw_status(&gamestate);

		int c;
		while (true) {
			game.xy_t xy = {
				.x = gamestate.curx,
				.y = gamestate.cury
			};
			c = toupper(getcoord(PLAYER, &xy));
			gamestate.curx = xy.x;
			gamestate.cury = xy.y;
			if (!strchr(docked, c)) continue;
			else break;
		}

		game.ship_t *ss = NULL;
		if (c == 'R') {
			OS.ungetch('R');
		} else {
			/* map that into the corresponding symbol */
			for (ss = gamestate.players[PLAYER].ships; ss < gamestate.players[PLAYER].ships + SHIPTYPES; ss++) {
				if (ss->symbol == c) {
					break;
				}
			}
			sprintf(gamestate.status, "Use arrows to place the %s.", game.shipname(ss->kind));
			render.draw_status(&gamestate);
			pgoto(gamestate.cury, gamestate.curx);
		}

		bool ok = false;
		while (!ok) {
			ok = true;
			c = OS.getch();
			switch (c) {
				case 'r': { placeship_random(ss); }
				case 'R': { placeall_random(); }
				case OS.KEY_UP: { placeship(ss, N); }
				case OS.KEY_DOWN: { placeship(ss, S); }
				case OS.KEY_LEFT: { placeship(ss, W); }
				case OS.KEY_RIGHT: { placeship(ss, E); }
				default: {
					ok = false;
				}
			}
		}
	}
}

int unplaced() {
	int n = 0;
	for (int i = 0; i < SHIPTYPES; i++) {
		if (!gamestate.players[PLAYER].ships[i].placed) {
			n++;
		}
	}
	return n;
}

void placeall_random() {
	game.ship_t *ss;
	sprintf(gamestate.status, "Placing the rest of your fleet at random...");
	render.draw_status(&gamestate);
	for (ss = gamestate.players[PLAYER].ships; ss < gamestate.players[PLAYER].ships + SHIPTYPES; ss++) {
		if (ss->placed) continue;
		game.randomplace(&gamestate, PLAYER, ss);
		game.placeship(&gamestate, PLAYER, ss);
		ss->placed = true;
		render.draw_placed_ships(&gamestate);
	}
}

void placeship_random(game.ship_t *ss) {
	game.randomplace(&gamestate, PLAYER, ss);
	game.placeship(&gamestate, PLAYER, ss);
	ss->placed = true;
	render.draw_placed_ships(&gamestate);
}

void placeship(game.ship_t *ss, int dir) {
	ss->x = gamestate.curx;
	ss->y = gamestate.cury;
	ss->dir = dir;
	gamestate.error = game.checkplacement(&gamestate, PLAYER, ss);
	render.draw_error(&gamestate);
	game.placeship(&gamestate, PLAYER, ss);
	ss->placed = true;
	render.draw_placed_ships(&gamestate);
}

int getcoord(int atcpu, game.xy_t *xy) {
	gamestate.render_cursor = *xy;
	gamestate.render_cursor_at = atcpu;
	render.render_cursor(&gamestate);
	int c;
	for (;;) {
		c = input_key();
		if (update_coords(xy, c)) {
			xy->x = xy->x % BWIDTH;
			xy->y = xy->y % BDEPTH;	
			gamestate.render_cursor = *xy;
			gamestate.render_cursor_at = atcpu;
			render.render_cursor(&gamestate);
		} else {
			break;
		}
	}
	render.render_clear_coords();
	return c;
}

bool update_coords(game.xy_t *xy, int c) {
	switch (c) {
		case OS.KEY_UP: { xy->y += (BDEPTH - 1); }
		case OS.KEY_DOWN: { xy->y += 1; }
		case OS.KEY_LEFT: { xy->x += (BWIDTH - 1); }
		case OS.KEY_RIGHT: { xy->x += 1; }
		default: { return false; }
	}
	return true;
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

game.xy_t playerturn_getcoords() {
	while (true) {
		game.xy_t xy = {
			.x = gamestate.curx,
			.y = gamestate.cury
		};
		getcoord(COMPUTER, &xy);
		gamestate.curx = xy.x;
		gamestate.cury = xy.y;
		if (gamestate.players[PLAYER].shots[gamestate.curx][gamestate.cury]) {
			sprintf(gamestate.status, "You shelled this spot already! Try again.");
			render.draw_status(&gamestate);
			OS.beep();
			continue;
		}
		break;
	}
	game.xy_t xy = {
		.x = gamestate.curx,
		.y = gamestate.cury
	};
	return xy;
}
