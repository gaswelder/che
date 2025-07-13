#import words.c
#import rnd

int GenContents_lstname = 0;
int GenContents_email = 0;

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
