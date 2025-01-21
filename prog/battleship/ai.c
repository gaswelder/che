#import game.c
#import rnd

#define BWIDTH 10
#define BDEPTH 10


// /* direction constants */
enum {
	E = 0,
	S = 2,
	W = 4,
	N = 6,
};

enum {
	PLAYER = 0,
	COMPUTER = 1,
};

pub typedef {
	int ai_next;
	int ai_last_x;
	int ai_last_y;
	int ai_turncount;
	int ai_srchstep;
	int ai_huntoffs; /* Offset on search strategy */
	bool used[4];
	game.ship_t ts;

	int x;
	int y;
	int hit;
	int d;
} state_t;

enum {
	RANDOM_FIRE = 0,
	HUNT_DIRECT = 2,
	FIRST_PASS = 3,
	REVERSE_JUMP = 4,
	SECOND_PASS = 5,
};

pub void reset(state_t *s) {
	memset(s, 0, sizeof(state_t));
	s->ai_srchstep = 3;
}

pub int turn(game.state_t *g, state_t *s) {
	switch (s->ai_next) {
		case RANDOM_FIRE: { return ai_random_fire(g, s); }

		/* last shot hit, we're looking for ship's axis */
		case HUNT_DIRECT: { return ai_find_orientation(g, s); }

		/* we have a start and a direction now */
		case FIRST_PASS: { return ai_first_pass(g, s); }

		/* nail down the ship's other end */
		case REVERSE_JUMP: { return ai_reverse_jump(g, s); }

		/* kill squares not caught on first pass */
		case SECOND_PASS: { return ai_second_pass(g, s); }
	}
	panic("!");
}

int ai_random_fire(game.state_t *g, state_t *s) {
	ai_choose_move(g, s, &s->x, &s->y);
	int result = ai_cpufire(g, s, s->x, s->y);
	switch (result) {
		case game.S_HIT: {
			s->ts.x = s->x;
			s->ts.y = s->y;
			s->ts.hits = 1;
			s->used[E / 2] = false;
			s->used[S / 2] = false;
			s->used[W / 2] = false;
			s->used[N / 2] = false;
			s->ai_next = HUNT_DIRECT;
		}
		case game.S_MISS, game.S_SUNK: {
			// keep random shooting
		}
		default: { panic("!"); }
	}
	return result;
}

int ai_find_orientation(game.state_t *g, state_t *s) {
	int navail = 0;
	for (int i = 0; i < 4; i++) {
		if (s->used[i]) continue;
		game.xy_t xy = game.shipxy(s->ts.x, s->ts.y, i * 2, 1);
		if (!game.coords_possible(g, xy.x, xy.y)) {
			s->used[i] = true;
			continue;
		}
		navail++;
	}
	if (navail == 0) {
		ai_warn("orientation: ship not sunk and no valid coords");
		return ai_random_fire(g, s);
	}

	s->d = 0;
	int n = (int) rnd.intn(navail) + 1;
	while (n) {
		while (s->used[s->d]) {
			s->d++;
		}
		n--;
	}
	s->used[s->d] = false;
	game.xy_t xy = game.shipxy(s->ts.x, s->ts.y, s->d * 2, 1);
	s->x = xy.x;
	s->y = xy.y;


	int result = ai_cpufire(g, s, s->x, s->y);
	s->hit = result;
	switch (result) {
		case game.S_MISS: {
			// keep trying
		}
		case game.S_SUNK: {
			s->ai_next = RANDOM_FIRE;
		}
		case game.S_HIT: {
			s->ts.x = s->x;
			s->ts.y = s->y;
			s->ts.dir = s->d * 2;
			s->ts.hits++;
			s->ai_next = FIRST_PASS;
		}
		default: {
			panic("!");
		}
	}
	return result;
}

int ai_first_pass(game.state_t *g, state_t *s) {
	game.xy_t xy = game.shipxy(s->ts.x, s->ts.y, s->ts.dir, 1);
	s->x = xy.x;
	s->y = xy.y;
	if (!game.coords_possible(g, s->x, s->y)) {
		s->ai_next = REVERSE_JUMP;
		ai_warn("first pass ended with nothing");
		return game.S_MISS;
	}
	int result = ai_cpufire(g, s, s->x, s->y);
	s->hit = result;
	switch (result) {
		case game.S_MISS: { s->ai_next = REVERSE_JUMP; }
		case game.S_SUNK: { s->ai_next = RANDOM_FIRE; }
		case game.S_HIT: {
			s->ts.x = s->x;
			s->ts.y = s->y;
			s->ts.hits++;
			// continue first pass
		}
		default: { panic("!"); }
	}
	return result;
}

int ai_reverse_jump(game.state_t *g, state_t *s) {
	s->d = s->ts.dir + 4;
	game.xy_t xy = game.shipxy(s->ts.x, s->ts.y, s->d, s->ts.hits);
	s->x = xy.x;
	s->y = xy.y;
	if (!game.coords_possible(g, s->x, s->y)) {
		s->ai_next = RANDOM_FIRE;
		ai_warn("reverse jump ended with nothing");
		return game.S_MISS;
	}
	int result = ai_cpufire(g, s, s->x, s->y);
	s->hit = result;
	switch (result) {
		case game.S_MISS, game.S_SUNK: {
			s->ai_next = RANDOM_FIRE;
		}
		case game.S_HIT: {
			s->ts.x = s->x;
			s->ts.y = s->y;
			s->ts.dir = s->d;
			s->ts.hits++;
			s->ai_next = SECOND_PASS;
		}
		default: { panic("!"); }
	}
	return result;
}

int ai_second_pass(game.state_t *g, state_t *s) {
	game.xy_t xy = game.shipxy(s->ts.x, s->ts.y, s->ts.dir, 1);
	s->x = xy.x;
	s->y = xy.y;
	if (!game.coords_possible(g, s->x, s->y)) {
		s->ai_next = RANDOM_FIRE;
		ai_warn("second pass ended with nothing");
		return game.S_MISS;
	}
	int result = ai_cpufire(g, s, s->x, s->y);
	s->hit = result;
	switch (result) {
		case game.S_MISS, game.S_SUNK: {
			s->ai_next = RANDOM_FIRE;
		}
		case game.S_HIT: {
			s->ts.x = s->x;
			s->ts.y = s->y;
			s->ts.hits++;
		}
		default: {
			panic("!");
		}
	}
	return result;
}

void ai_choose_move(game.state_t *g, state_t *s, int *px, int *py) {
	// List all possible moves.
	int nposs = 0;
	int ypossible[BWIDTH * BDEPTH];
	int xpossible[BWIDTH * BDEPTH];
	for (int x = 0; x < BWIDTH; x++) {
		for (int y = 0; y < BDEPTH; y++) {
			if (g->players[COMPUTER].shots[x][y]) continue;
			xpossible[nposs] = x;
			ypossible[nposs] = y;
			nposs++;
		}
	}
	if (!nposs) {
		panic("No moves possible?? Help!");
	}

	// List preferred possible moves.
	if (s->ai_turncount++ == 0) {
		s->ai_huntoffs = (int) rnd.intn(s->ai_srchstep);
	}
	int npref = 0;
	int ypreferred[BWIDTH * BDEPTH];
	int xpreferred[BWIDTH * BDEPTH];
	for (int i = 0; i < nposs; i++) {
		int x = xpossible[i];
		int y = ypossible[i];
		if (((x + s->ai_huntoffs) % s->ai_srchstep) != (y % s->ai_srchstep)) {
			xpreferred[npref] = x;
			ypreferred[npref] = y;
			npref++;
		}
	}

	if (npref) {
		int i = (int) rnd.intn(npref);
		*px = xpreferred[i];
		*py = ypreferred[i];
	} else {
		int i = (int) rnd.intn(nposs);
		*px = xpossible[i];
		*py = ypossible[i];
		if (s->ai_srchstep > 1) {
			--s->ai_srchstep;
		}
	}
}

int ai_cpufire(game.state_t *g, state_t *s, int x, y) {
	s->ai_last_x = x;
	s->ai_last_y = y;

	int result = game.shoot(g, COMPUTER, x, y);
	if (result == game.S_SUNK) {
		g->sunk_by_ai = game.getshipat(g, PLAYER, x, y);
	}
	return result;
}

void ai_warn(const char *msg) {
	(void) msg;
}
