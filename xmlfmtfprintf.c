int main() {
    char buf[4096] = "";
    while (!feof(stdin)) {
        fgets(buf, 4096, stdin);
        xmlfmtprintf(stdout, "%s", buf);
    }
    return 0;
}

char buf[20000] = {};
char *blank=0;
char *lstblank=0;
char *start=0;
char *write=0;
int width=0;
int indent=0;

int indent_level=0;
int fmt_width=79;

int xmlfmtprintf(FILE *xfp, const char *fmt, ...) {
    int newindent=-1;
    char *trail;
    
    if (!blank) {
        write=start=lstblank=blank=buf;
        indent=indent_level;
        width = indent - 1;
    }

    // Printf the input string, use the "write" cursor.
    va_list ap;
    va_start (ap,fmt);
    vsprintf(write, fmt, ap);
    va_end(ap);

    if (start == write) {
        indent = indent_level;
    }

    trail=write;
    while (*trail) {
        width++;
        if (*trail=='\n') newindent=indent_level;
        if (*trail=='\t') *trail=' ';
        if (*trail==' ') {
            blank=trail;
            if (width == fmt_width && lstblank != blank) {
                *trail='\n';
            }
        }
        if (*trail=='\n') {
            *trail='\0';
            fprintf(xfp,"%*s%s\n",indent,"",start);
            start=lstblank=blank=trail+1;
            width=indent-1;
            if (newindent>=0) indent=newindent;
            if (!trail+1) blank=0;
        }
        if (width==fmt_width && lstblank!=blank)
            {
                *blank='\0';
                fprintf(xfp,"%*s%s\n",indent,"",start);
                lstblank=start=trail=++blank;
                width=indent;
            }
        trail++;
    }
    write=trail;
    return width;
}
