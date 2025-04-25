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
}

#define BWIDTH 10
#define BDEPTH 10

game.state_t gamestate = {};
ai.state_t AI = {};

const int SHIPTYPES = 5;

enum {
	PLAYER = 0,
	COMPUTER = 1,
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
		render.reset();

		placeall_random(COMPUTER);
		place_ships();

		gamestate.state = game.ST_PLAYING;
		if (rnd.intn(2)) {
			gamestate.turn = COMPUTER;
		} else {
			gamestate.turn = PLAYER;
		}

		
		strcpy(gamestate.status, "Arrows: move the cursor; any key: shoot");
		render.refresh(&gamestate);

		OS.getch();
		play();

		gamestate.state = game.ST_FINISHED;
		int winner = game.winner(&gamestate);
		gamestate.winner = winner;
		gamestate.players[winner].wins++;
		render.refresh(&gamestate);
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
		render.refresh(&gamestate);
		gamestate.turn = 1 - gamestate.turn;
	}
}

void play_blitz() {
	while (game.winner(&gamestate) == -1) {
		int result = turn();
		render.refresh(&gamestate);
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
			render.refresh(&gamestate);
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
				game.mark_sunk(&gamestate, gamestate.turn, ss);
			}
			case game.S_HIT: {
				game.log(&gamestate, "computer: %c%d - hit", y + 'A', x);
			}
			case game.S_MISS: {
				game.log(&gamestate, "computer: %c%d - miss", y + 'A', x);
			}
		}
		render.refresh(&gamestate);
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
			game.mark_sunk(&gamestate, gamestate.turn, ss);
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

void closegame(int sig) {
	render.end();
	exit(sig);
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
		render.refresh(&gamestate);

		game.xy_t xy = {};
		xy.x = gamestate.curx;
		xy.y = gamestate.cury;
		int c = input_coords_and_command(PLAYER, &xy);
		gamestate.curx = xy.x;
		gamestate.cury = xy.y;

		if (c == 'r') {
			placeall_random(PLAYER);
			render.refresh(&gamestate);
			break;
		}

		// See if a ship has been selected.
		game.ship_t *ships = gamestate.players[PLAYER].ships;
		game.ship_t *ss = NULL;
		for (int i = 0; i < SHIPTYPES; i++) {
			if (toupper(ships[i].symbol) == toupper(c) && !ships[i].placed) {
				ss = &ships[i];
				break;
			}
		}
		if (!ss) {
			continue;
		}

		sprintf(gamestate.status, "Use arrows to place the %s.", game.shipname(ss->kind));
		gamestate.render_cursor = xy;
		gamestate.render_cursor_at = PLAYER;
		render.refresh(&gamestate);

		switch (OS.getch()) {
			case OS.KEY_UP: { placeship(ss, N); }
			case OS.KEY_DOWN: { placeship(ss, S); }
			case OS.KEY_LEFT: { placeship(ss, W); }
			case OS.KEY_RIGHT: { placeship(ss, E); }
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

void placeall_random(int player) {
	game.ship_t *ss;
	for (ss = gamestate.players[player].ships; ss < gamestate.players[player].ships + SHIPTYPES; ss++) {
		if (ss->placed) continue;
		placeship_random(player, ss);
	}
}

void placeship_random(int player, game.ship_t *ss) {
	game.randomplace(&gamestate, player, ss);
	game.placeship(&gamestate, player, ss);
	ss->placed = true;
}

void placeship(game.ship_t *ss, int dir) {
	ss->x = gamestate.curx;
	ss->y = gamestate.cury;
	ss->dir = dir;
	int err = game.placeship(&gamestate, PLAYER, ss);
	if (err) {
		gamestate.error = err;
	} else {
		gamestate.error = 0;
		ss->placed = true;
	}
	render.refresh(&gamestate);
}

int input_coords_and_command(int atcpu, game.xy_t *xy) {
	gamestate.render_cursor = *xy;
	gamestate.render_cursor_at = atcpu;
	render.render_cursor(&gamestate);
	int c;
	for (;;) {
		c = OS.getch();
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
		input_coords_and_command(COMPUTER, &xy);
		gamestate.curx = xy.x;
		gamestate.cury = xy.y;
		if (gamestate.players[PLAYER].shots[gamestate.curx][gamestate.cury]) {
			sprintf(gamestate.status, "You shelled this spot already! Try again.");
			render.refresh(&gamestate);
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
