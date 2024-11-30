#import rnd

pub enum {
	SHIP_CARRIER = 1,
	SHIP_BATTLESHIP,
	SHIP_SUBMARINE,
	SHIP_DESTROYER,
	SHIP_PTBOAT,
};

pub const char *shipname(int s) {
	switch (s) {
		case SHIP_CARRIER: { return "Aircraft Carrier"; }
		case SHIP_BATTLESHIP: { return "Battleship"; }
		case SHIP_SUBMARINE: { return "Submarine"; }
		case SHIP_DESTROYER: { return "Destroyer"; }
		case SHIP_PTBOAT: { return "PT Boat"; }
	}
	return "unknown";
}

// /* direction constants */
enum {
	E = 0,
	S = 2,
	// W = 4,
	// N = 6,
};

pub enum {
	MARK_HIT = 'H',
	MARK_MISS = 'o',
};

pub enum {
	ERR_HANGING = 1,
	ERR_COLLISION = 2,
};

// Result of shoot.
pub enum {
	S_MISS = 0,
	S_HIT = 1,
	S_SUNK = -1,
};

const int SHIPTYPES = 5;
const int xincr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
const int yincr[8] = {0, 1, 1, 1, 0, -1, -1, -1};
const int BWIDTH = 10;
const int BDEPTH = 10;

pub typedef {
	int kind;
	int hits;          /* how many times has this ship been hit? */
	char symbol;       /* symbol for game purposes */
	int length;        /* length of ship */
	char x, y;         /* coordinates of ship start point */
	uint8_t dir; /* direction of `bow' */
	bool placed;       /* has it been placed on the board? */
} ship_t;

pub typedef {
	int wins;
	char board[10][10];

	// This player's shots.
	// Also serves as a foggy view of the opponent's map.
	char shots[10][10];

	ship_t ships[5];
} player_t;

pub typedef { int x, y; } xy_t;

pub typedef {
	bool closepack;

	player_t players[2];
	int turn;

	int curx, cury;

	xy_t render_cursor;
	int render_cursor_at;

	// render state
	ship_t *sunk_by_ai;
	int player_hit;
	ship_t *sunk_by_player;

	//
	int winner;

	char status[200];
	int error;

	char log[10][200];
	int logsize;
} state_t;

pub void log(state_t *g, const char *fmt, ...) {
	if (g->logsize == 10) {
		for (int i = 0; i < 10-1; i++) {
			strcpy(g->log[i], g->log[i+1]);
		}
		g->logsize--;
	}
	va_list args = {};
	va_start(args, fmt);
	char *buf = g->log[g->logsize];
	vsprintf(buf, fmt, args);
	va_end(args);
	g->logsize++;
}


pub xy_t shipxy(int x, y, dir, dist) {
	const int xincr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
	const int yincr[8] = {0, 1, 1, 1, 0, -1, -1, -1};
	xy_t r = {
		.x = x + dist * xincr[dir],
		.y = y + dist * yincr[dir]
	};
	return r;
}

pub void reset(state_t *g) {
	memset(g->players, 0, 2 * sizeof(player_t));
	for (int player = 0; player < 2; player++) {
		ship_t *ships = g->players[player].ships;
		memset(ships, 0, sizeof(ship_t) * 5);

		ships[0].kind = SHIP_CARRIER;
		ships[1].kind = SHIP_BATTLESHIP;
		ships[2].kind = SHIP_DESTROYER;
		ships[3].kind = SHIP_SUBMARINE;
		ships[4].kind = SHIP_PTBOAT;

		ships[0].symbol = 'A';
		ships[1].symbol = 'B';
		ships[2].symbol = 'D';
		ships[3].symbol = 'S';
		ships[4].symbol = 'P';

		ships[0].length = 5;
		ships[1].length = 4;
		ships[2].length = 3;
		ships[3].length = 3;
		ships[4].length = 2;
	}
}

pub bool coords_possible(state_t *g, int x, y) {
	return coords_valid(x, y) && !g->players[1].shots[x][y];
}

pub bool coords_valid(int x, y) {
	return (x >= 0 && x < 10 && y >= 0 && y < 10);
}

pub int winner(state_t *g) {
	int j;
	for (int i = 0; i < 2; ++i) {
		ship_t *ss = g->players[i].ships;
		for (j = 0; j < 5; ) {
			if (ss->length > ss->hits) {
				break;
			}
			++j;
			++ss;
		}
		if (j == 5) {
			return 1 - g->turn;
		}
	}
	return -1;
}

/* is this location on the selected zboard adjacent to a ship? */
bool has_collision(state_t *g, int b, int y, int x) {
	/* anything on the square */
	bool collide = is_ship(g->players[b].board[x][y]);
	if (collide) {
		return true;
	}

	/* anything on the neighbors */
	if (!g->closepack) {
		for (int i = 0; i < 8; i++) {
			int yend = y + yincr[i];
			int xend = x + xincr[i];
			if (coords_valid(xend, yend) && is_ship(g->players[b].board[xend][yend])) {
				collide = true;
				break;
			}
		}
	}
	return collide;
}

pub int checkplacement(state_t *g, int b, ship_t *ss) {
	int err = 0;

	// Check board edges
	int xend = ss->x + (ss->length - 1) * xincr[ss->dir];
	int yend = ss->y + (ss->length - 1) * yincr[ss->dir];
	if (!coords_valid(xend, yend)) {
		err = ERR_HANGING;
	}
	if (!err) {
		for (int l = 0; l < ss->length; ++l) {
			if (has_collision(g, b, ss->y + l * yincr[ss->dir], ss->x + l * xincr[ss->dir])) {
				err = ERR_COLLISION;
				break;
			}
		}
	}
	return err;
}

// Generates a valid random ship placement.
pub void randomplace(state_t *g, int player, ship_t *ss) {
	while (true) {
		if (rnd.intn(2)) {
			ss->dir = E;
			ss->x = rnd.intn(BWIDTH - ss->length);
			ss->y = rnd.intn(BDEPTH);
		} else {
			ss->dir = S;
			ss->x = rnd.intn(BWIDTH);
			ss->y = rnd.intn(BDEPTH - ss->length);
		}
		int err = checkplacement(g, player, ss);
		if (!err) break;
	}
}

pub void placeship(state_t *g, int player, ship_t *ss) {
	for (int l = 0; l < ss->length; ++l) {
		int x = ss->x + l * xincr[ss->dir];
		int y = ss->y + l * yincr[ss->dir];
		g->players[player].board[x][y] = ss->symbol;
	}
	ss->hits = 0;
}

pub bool is_ship(int c) {
	return OS.isupper(c);
}

pub int shoot(state_t *g, int player, int x, y) {
	bool hit = g->players[1-player].board[x][y];
	if (!hit) {
		g->players[player].shots[x][y] = MARK_MISS;
		return S_MISS;
	}
	g->players[player].shots[x][y] = MARK_HIT;
	ship_t *ss = getshipat(g, 1 - player, x, y);
	ss->hits++;
	if (!ship_afloat(ss)) {
		mark_empty_water(g, ss);
		return S_SUNK;
	}
	return S_HIT;
}

pub ship_t *getshipat(state_t *g, int player, int x, y) {
	char symbol = g->players[player].board[x][y];
	if (symbol == 0) {
		return NULL;
	}
	ship_t *sb = g->players[player].ships;
	ship_t *ss;
	for (ss = sb; ss < sb + SHIPTYPES; ++ss) {
		if (ss->symbol == symbol) {
			return ss;
		}
	}
	return NULL;
}

pub bool ship_afloat(ship_t *ss) {
	return ss->hits < ss->length;
}



pub void mark_empty_water(state_t *g, ship_t *ss) {
	if (g->closepack) return;
	for (int j = -1; j <= 1; j++) {
		int bx = ss->x + j * xincr[(ss->dir + 2) % 8];
		int by = ss->y + j * yincr[(ss->dir + 2) % 8];
		for (int i = -1; i <= ss->length; ++i) {
			int x1 = bx + i * xincr[ss->dir];
			int y1 = by + i * yincr[ss->dir];
			if (coords_valid(x1, y1)) {
				g->players[g->turn].shots[x1][y1] = MARK_MISS;
			}
		}
	}
}

pub int shipscount(state_t *g, int player) {
	ship_t *ships = g->players[player].ships;
	int n = 0;
	for (int i = 0; i < SHIPTYPES; i++) {
		ship_t *sp = &ships[i];
		if (!ship_afloat(sp)) {
			continue;
		}
		n++;
	}
	return n;
}

