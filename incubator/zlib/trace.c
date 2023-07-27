int z_verbose = 0;

#  define Trace(x) {if (z_verbose>=0) fprintf x ;}

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

#  define Tracevv(x) {if (z_verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (z_verbose>0 && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (z_verbose>1 && (c)) fprintf x ;}
