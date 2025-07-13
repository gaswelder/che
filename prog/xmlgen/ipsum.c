#import words.c
#import rnd

int GenContents_lstname = 0;
int GenContents_email = 0;
char *GenContents_education[]={"High School","College", "Graduate School","Other"};
char *GenContents_money[]={"Money order","Creditcard", "Personal Check","Cash"};
char *GenContents_yesno[]={"Yes","No"};
int GenContents_country = -1;
int GenContents_quantity = 0;
double GenContents_initial = 0;
double GenContents_increases = 0;
const int COUNTRIES_USA = 0;
char *markup[3]={"emph","keyword","bold"};
int PrintANY_st[3] = {};
char tick[3] = "";

pub void emit(const char *s) {
	FILE *out = stdout;

	switch str (s) {
		case "max(18, gauss(30, 15))": {
			int r = (int)(30 + 15 * rnd.gauss());
			if (r < 18) r = 18;
			printf("%d", r);
		}
		case "int(1, 10)": { printf("%d", randr(1, 10)); }
		case "cc": {
			for (int i=0; i<4; i++) {
				char *s = "";
				if (i < 3) { s = " "; }
				fprintf(out, "%d%s", randr(1000,9999), s);
			}
		}
		case "sentence1": { fsentence(stdout, 1 + rnd.intn(4)); }
		case "sentence2": { fsentence(stdout, 1 + (int)rnd.exponential(4)); }
		case "dict(shipping)": { fromdict("shipping"); }
		case "dict(provinces)": { fromdict("provinces"); }
		case "dict(cities)": { fromdict("cities"); }
		case "dict(lastnames)": { fromdict("lastnames"); }
		case "dict(auction_type)": { fromdict("auction_type"); }
		case "zip": {
			int r = randr(3,4);
			int cd = 10;
			for (int j = 0; j < r; j++) {
				cd*=10;
			}
			r=randr(r, 10*r - 1);
			fprintf(out,"%d",r);
		}
		case "street": {
			int n = randr(1, 100);
			const char *ln = words.dictentry("lastnames", rnd.intn(words.dictlen("lastnames")));
    		fprintf(out, "%d %s St", n, ln);
		}
		case "phone": {
			// +country (area) number
    		fprintf(out, "+%d (%d) %d", randr(1,99), randr(10,999), randr(123456,98765432));
		}
		case "gender": {
			if (randr(0, 1)) fprintf(out, "%s", "male");
    		else fprintf(out, "%s", "female");
		}
		case "price": {
			fprintf(out,"%.2f", rnd.exponential(100));
		}
		case "date": {
			int month = randr(1,12);
			int day = randr(1,28);
			int year = randr(1998,2001);
			fprintf(out,"%02d/%02d/%4d",month,day,year);
		}
		case "time": {
			int hrs = randr(0, 23);
			int min = randr(0, 59);
			int sec = randr(0, 59);
			fprintf(out, "%02d:%02d:%02d", hrs, min, sec);
		}
		case "name": {
			int flen = words.dictlen("firstnames");
			int fst = (int)rnd.exponential(flen/3);
			if (fst >= flen-1) {
				fst = flen-1;
			}
			int llen = words.dictlen("lastnames");
			int lst = (int)rnd.exponential(llen/3);
			if (lst >= llen-1) {
				lst = llen-1;
			}
			fprintf(stdout, "%s %s", words.dictentry("firstnames", fst), words.dictentry("lastnames", lst));
			GenContents_lstname = lst;
		}
		case "name ": {
			emit("name");
            fprintf(out," ");
		}
		case "email": {
			GenContents_email = rnd.intn(words.dictlen("emails"));
            fprintf(out, "mailto:%s@%s",
                words.dictentry("lastnames", GenContents_lstname),
                words.dictentry("emails", GenContents_email));
		}
		case "webpage": {
			fprintf(out, "http://www.%s/~%s",
                words.dictentry("emails", GenContents_email),
                words.dictentry("lastnames", GenContents_lstname));
		}
		case "payment": {
			int r=0;
            for (int i=0; i<4; i++) {
                if (rnd.intn(2)) {
                    char *x = "";
                    if (r++) {
                        x = ", ";
                    }
                    fprintf(out, "%s%s", x, GenContents_money[i % 4]);
                }
            }
		}
		case "education": {
			fprintf(out, "%s", GenContents_education[rnd.intn(4)]);
		}
		case "location": {
			if (rnd.urange(0, 1) < 0.75) {
                GenContents_country = COUNTRIES_USA;
            } else {
                GenContents_country = rnd.intn(words.dictlen("countries"));
            }
            fprintf(out, "%s", words.dictentry("countries", GenContents_country));
		}
		case "type": {
			emit("dict(auction_type)");
            if (GenContents_quantity > 1 && rnd.intn(2) > 0) {
				fprintf(out,", Dutch");
			}
		}
		case "province": {
			if (GenContents_country == COUNTRIES_USA) {
                emit("dict(provinces)");
            } else {
                emit("dict(lastnames)");
            }
		}
		case "yesno": {
			fprintf(out, "%s", GenContents_yesno[rnd.intn(2)]);
		}
		case "quantity": {
			GenContents_quantity=1+(int)rnd.exponential(0.4);
            fprintf(out,"%d",GenContents_quantity);
		}
		case "increase": {
			double d=1.5 *(1+(int)rnd.exponential(10));
            fprintf(out,"%.2f",d);
            GenContents_increases+=d;
		}
		case "current": {
			fprintf(out,"%.2f",GenContents_initial+GenContents_increases);
		}
		case "init_price": {
			GenContents_initial=rnd.exponential(100);
            GenContents_increases=0;
            fprintf(out,"%.2f",GenContents_initial);
		}
		case "reserve": {
			fprintf(out,"%.2f",GenContents_initial*(1.2+rnd.exponential(2.5)));
		}
		case "any": {
			PrintANY(out);
		}
		default: {
			panic("unknown spec: %s", s);
		}
	}
}

void fromdict(char *name) {
	int n = words.dictlen(name);
    fprintf(stdout, "%s", words.dictentry(name, rnd.intn(n)));
}

int randr(int min, max) {
    return min + (int) rnd.intn(max-min + 1);
}

void fsentence(FILE *out, int nwords) {
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

void PrintANY(FILE *out) {
    int sen=1+(int)rnd.exponential(20);
    int stptr = 0;
    for (int i=0; i < sen; i++) {
        if (rnd.urange(0, 1) < 0.1 && stptr < 2) {
            while (true) {
                PrintANY_st[stptr] = rnd.intn(3);
                if (!tick[PrintANY_st[stptr]]) {
                    break;
                }
            }
            tick[PrintANY_st[stptr]] = 1;
            fprintf(out,"<%s> ", markup[PrintANY_st[stptr]]);
            stptr++;
        }
        else if (rnd.urange(0, 1) < 0.8 && stptr != 0) {
            --stptr;
            fprintf(out,"</%s> ",markup[PrintANY_st[stptr]]);
            tick[PrintANY_st[stptr]] = 0;
        }
		emit("sentence2");
    }
    while (stptr) {
        --stptr;
        fprintf(out,"</%s> ",markup[PrintANY_st[stptr]]);
        tick[PrintANY_st[stptr]]=0;
    }
}
