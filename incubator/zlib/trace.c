int z_verbose = 0;

pub int level() {
    return z_verbose;
}

pub void Trace(FILE *f, const char *format, ...) {
    if (z_verbose < 0) {
        return;
    }
    fprintf(f, "[trace] ");
    va_list l = {0};
	va_start(l, format);
	vfprintf(f, format, l);
	va_end(l);
    fprintf(f, "\n");
}

pub void Tracev(FILE *f, const char *format, ...) {
    if (z_verbose == 0) {
        return;
    }
    fprintf(f, "[trace] ");
    va_list l = {0};
	va_start(l, format);
	vfprintf(f, format, l);
	va_end(l);
    fprintf(f, "\n");
}

pub void Tracevv(FILE *f, const char *format, ...) {
    if (z_verbose <= 1) {
        return;
    }
    fprintf(f, "[trace] ");
    va_list l = {0};
	va_start(l, format);
	vfprintf(f, format, l);
	va_end(l);
    fprintf(f, "\n");
}

#  define Tracec(c,x) {if (z_verbose>0 && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (z_verbose>1 && (c)) fprintf x ;}
