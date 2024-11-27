#define BWIDTH 10
#define BDEPTH 10

pub enum {
	SHIP_CARRIER = 1,
	SHIP_BATTLESHIP,
	SHIP_SUBMARINE,
	SHIP_DESTROYER,
	SHIP_PTBOAT,
};

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
	int STATE;
	int turn; /* 0=player, 1=computer */
	int plywon, cpuwon;
	int curx, cury;
	ship_t plyship[5];
	ship_t cpuship[5];
	bool used[4];
	ship_t ts;
	char hits[2][BWIDTH][BDEPTH];
	char board[2][BWIDTH][BDEPTH];

	// placing mode
	char docked[10]; // ships that the player has to place
	// --placing mode

	int ai_next;
	int ai_turncount;
	int ai_srchstep;
	int ai_huntoffs; /* Offset on search strategy */

	int ai_last_x, ai_last_y;
	ship_t *sunk_by_ai;

	int player_hit;
	ship_t *sunk_by_player;

	int winner;

	int nx, ny;
} state_t;
