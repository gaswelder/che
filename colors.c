enum {
    RESET_ALL = 0,
    BRIGHT = 1,
    DIM = 2,
    UNDERSCORE   = 4,
    BLINK = 5,
    REVERSE = 7,
    HIDDEN = 8,
    // Foreground color
    BLACK = 30,
    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    MAGENTA = 35,
    CYAN = 36,
    WHITE = 37,
    // Background color
    BLACK_BG = 40,
    RED_BG = 41,
    GREEN_BG = 42,
    YELLOW_BG = 43,
    BLUE_BG = 44,
    MAGENTA_BG = 45,
    CYAN_BG = 46,
    WHITE_BG = 47
};

void ttycolor(int color) {
    printf("\033[%dm", color);
}

int main() {
    ttycolor(RED);
    printf("ERROR");
    ttycolor(RESET_ALL);
    printf(" - sample error\n");
    return 0;
}