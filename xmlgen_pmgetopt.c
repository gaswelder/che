int next = 1;
int pp = 0;
char *pmoptarg = NULL;
int pmoptind = -1;
char dash='-';

int main(int argc, char *argv[]) {
    int opt;
    while((opt=pmgetopt(argc,argv, "edf:o:ihvs:tw:"))!=-1) {
        printf("opt = %c, arg = %s\n", opt, pmoptarg);
    }
    return 0;
}

int pmgetopt(int argc, char *argv[], const char *optstring)
{
    int i=0,found=0,len=0;
    char *option,*tr;
    pmoptind=next;
    pmoptarg=0;
    if (next==argc || ( !pp && (argv[next][0])!=dash)) return -1;
    option=&argv[next++][pp+1];
    while(optstring+len && optstring[len]!='\0') len++;
    while(i<len && !(found=(optstring[i]==*option))) i++;
    if (!found) { pp=0; return '?'; }
    tr=option+1;
    if (optstring[i+1]==':')
        {
            if (*tr) pmoptarg=tr;
            else
                if (next<argc && argv[next][0]!=dash)
                    pmoptarg=&argv[next++][0];
                else { pp=0; return '?'; }
        }
    else if (*tr) { pp++; next--; }
    return (int)*option;
}