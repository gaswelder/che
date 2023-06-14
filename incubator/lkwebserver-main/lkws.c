#import opt
#import lkconfig.c
#import lkstring.c
#import lkhostconfig.c
#import lkalloc.c
#import lkhttpserver.c

// lkws -d /var/www/testsite/ -c=cgi-bin
int main(int argc, char *argv[]) {
    char *configfile = NULL;
    char *cgidir = NULL;
    char *homedir = NULL;
    char *port = NULL;
    char *host = NULL;
    // homedir    = absolute or relative path to a home directory
    //              defaults to current working directory if not specified
    opt.opt_str("d", "homedir", &homedir);
    // port       = port number to bind to server
    //              defaults to 8000
    opt.opt_str("p", "port", &port);
    // host       = IP address to bind to server
    //              defaults to localhost
    opt.opt_str("h", "host", &host);
    opt.opt_str("f", "configfile", &configfile);
    // cgidir     = root directory for cgi files, relative path to homedir
    //              defaults to cgi-bin
    opt.opt_str("d", "CGI directory", &cgidir);

    char **rest = opt.opt_parse(argc, argv);
    if (*rest) {
        fprintf(stderr, "too many arguments\n");
        exit(1);
    }

    lkalloc.lk_alloc_init();

    // Create the config.
    lkconfig.LKConfig *cfg = lkconfig.lk_config_new();

    // Update the config from the file, if it's given.
    if (configfile) {
        lkconfig.lk_config_read_configfile(cfg, configfile);
    }
    // Override port and host, if set in the flags.
    if (port) {
        lkstring.lk_string_assign(cfg->port, port);
    } 
    if (host) {
        lkstring.lk_string_assign(cfg->serverhost, host);
    }

    // TODO
    // signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    // signal(SIGINT, handle_sigint);      // exit on CTRL-C
    // signal(SIGCHLD, handle_sigchld);

    if (cgidir) {
        lkhostconfig.LKHostConfig *default_hc = lkconfig.lk_config_create_get_hostconfig(cfg, "*");
        lkstring.lk_string_assign(default_hc->cgidir, cgidir);
    }
    
    if (homedir) {
        lkhostconfig.LKHostConfig *hc0 = lkconfig.lk_config_create_get_hostconfig(cfg, "*");
        lkstring.lk_string_assign(hc0->homedir, homedir);
        // cgidir default to cgi-bin
        if (hc0->cgidir->s_len == 0) {
            lkstring.lk_string_assign(hc0->cgidir, "/cgi-bin/");
        }
    }

    lkconfig.lk_config_finalize(cfg);
    return lkhttpserver.lk_httpserver_serve(cfg);
}

// void handle_sigint(int sig) {
//     printf("SIGINT received: %d\n", sig);
//     fflush(stdout);
//     lkalloc.lk_print_allocitems();
//     exit(0);
// }

// void handle_sigchld(int sig) {
//     printf("sigchild %d\n", sig);
//     abort();
//     // int tmp_errno = errno;
//     // while (waitpid(-1, NULL, WNOHANG) > 0) {}
//     // errno = tmp_errno;
// }
