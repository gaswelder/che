#import words.c
#import rnd

pub void fprovince(FILE *out) {
    int n = words.dictlen("provinces");
    fprintf(out, "%s", words.dictentry("provinces", rnd.intn(n)));
}

pub void fcity(FILE *out) {
    int n = words.dictlen("cities");
    fprintf(out, "%s", words.dictentry("cities", rnd.intn(n)));
}

pub void fzipcode(FILE *out) {
    int r = randr(3,4);
    int cd = 10;
    for (int j = 0; j < r; j++) {
        cd*=10;
    }
    r=randr(r, 10*r - 1);
    fprintf(out,"%d",r);
}

pub void fstreet(FILE *out) {
    int n = randr(1, 100);
    fprintf(out, "%d %s St", n, lastname());
}

pub void fphone(FILE *out) {
    // +country (area) number
    fprintf(out, "+%d (%d) %d", randr(1,99), randr(10,999), randr(123456,98765432));
}

pub void fgender(FILE *out) {
    if (randr(0, 1)) {
        fprintf(out, "%s", "male");
    } else {
        fprintf(out, "%s", "female");
    }
}

pub void fprice(FILE *out) {
    fprintf(out,"%.2f", rnd.exponential(100));
}

pub void flastname(FILE *out) {
    fprintf(out, "%s", lastname());
}

pub void fage(FILE *out) {
    int r = (int) (30 + 15 * rnd.gauss());
    if (r < 18) r = 18;
    fprintf(out,"%d", r);
}

pub void fcreditcard(FILE *out) {
    for (int i=0; i<4; i++) {
        char *s = "";
        if (i < 3) {
            s = " ";
        }
        fprintf(out, "%d%s", randr(1000,9999), s);
    }
}

pub void fdate(FILE *out) {
    int month = randr(1,12);
    int day = randr(1,28);
    int year = randr(1998,2001);
    fprintf(out,"%02d/%02d/%4d",month,day,year);
}

pub void ftime(FILE *out) {
    int hrs = randr(0, 23);
    int min = randr(0, 59);
    int sec = randr(0, 59);
    fprintf(out, "%02d:%02d:%02d", hrs, min, sec);
}

const char *lastname() {
    int n = words.dictlen("lastnames");
    return words.dictentry("lastnames", rnd.intn(n));
}

int randr(int min, max) {
    return min + (int) rnd.intn(max-min + 1);
}

pub void fint(FILE *out, int min, max) {
    fprintf(out, "%d", randr(min, max));
}

char *SHIPPING[] = {
    "Will ship only within country",
    "Will ship internationally",
    "Buyer pays fixed shipping charges",
    "See description for charges"
};

pub void fshipping(FILE *out) {
    int r = 0;
    for (int i=0;i<4;i++) {
        if (randr(0, 1)) {
            char *s = "";
            if (r++) s = ", ";
            fprintf(out, "%s%s", s, SHIPPING[i % 4]);
        }
    }
}

pub void fsentence(FILE *out, int nwords) {
    int n = words.dictlen("words");
    for (int i = 0; i < nwords; i++) {
        int index = (int)rnd.exponential(n/5.0);
        if (index >= n - 1) {
            index = n - 1;
        }
        fprintf(out, "%s", words.dictentry("words", index));
        fprintf(out, " ");
    }
}
