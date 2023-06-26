#import opt
#import strings
#import os/misc
#import fs

#import lkconfig.c
#import configparser.c
#import lkhostconfig.c
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

    puts("Loading configs");

    // Create the config.
    lkconfig.LKConfig *cfg = NULL;

    if (configfile) {
        cfg = configparser.read_file(configfile);
        if (!cfg) {
            fprintf(stderr, "failed to parse config file %s: %s", configfile, strerror(errno));
            return 1;
        }
    } else {
        cfg = lkconfig.lk_config_new();
    }
    // Override port and host, if set in the flags.
    if (port) {
        free(cfg->port);
        cfg->port = strings.newstr("%s", port);
    }
    if (host) {
        free(cfg->serverhost);
        cfg->serverhost = strings.newstr("%s", host);
    }

    // TODO
    // signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    // signal(SIGINT, handle_sigint);      // exit on CTRL-C
    // signal(SIGCHLD, handle_sigchld);

    if (cgidir) {
        lkhostconfig.LKHostConfig *default_hc = lkconfig.lk_config_create_get_hostconfig(cfg, "*");
        strcpy(default_hc->cgidir, cgidir);
    }
    
    if (homedir) {
        lkhostconfig.LKHostConfig *hc0 = lkconfig.lk_config_create_get_hostconfig(cfg, "*");
        strcpy(hc0->homedir, homedir);
        // cgidir default to cgi-bin
        if (strlen(hc0->cgidir) == 0) {
            strcpy(hc0->cgidir, "/cgi-bin/");
        }
    }

    // If no hostconfigs, add a fallthrough '*' hostconfig.
    if (cfg->hostconfigs_size == 0) {
        lkhostconfig.LKHostConfig *hc = lkconfig.lk_hostconfig_new("*");
        if (!hc) {
            panic("failed to create a host config");
        }
        if (!misc.getcwd(hc->homedir, sizeof(hc->homedir))) {
            panic("failed to get current working directory: %s", strerror(errno));
        }
        strcpy(hc->cgidir, "/cgi-bin/");
        lkconfig.add_hostconfig(cfg, hc);
    }

    // Set homedir absolute paths for hostconfigs.
    // Adjust /cgi-bin/ paths.
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];

        // Skip hostconfigs that don't have have homedir.
        if (strlen(hc->homedir) == 0) {
            continue;
        }

        if (!fs.realpath(hc->homedir, hc->homedir_abspath, sizeof(hc->homedir_abspath))) {
            panic("realpath failed");
        }

        if (strlen(hc->cgidir) > 0) {
            char *tmp = strings.newstr("%s/%s/", hc->homedir_abspath, hc->cgidir);
            if (!fs.realpath(tmp, hc->cgidir_abspath, sizeof(hc->cgidir_abspath))) {
                panic("realpath failed: %s", strerror(errno));
            }
            free(tmp);
        }
    }
    lkconfig.print(cfg);
    if (!lkhttpserver.lk_httpserver_serve(cfg)) {
        fprintf(stderr, "server failed: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

// void handle_sigint(int sig) {
//     printf("SIGINT received: %d\n", sig);
//     fflush(stdout);
//     exit(0);
// }

// void handle_sigchld(int sig) {
//     printf("sigchild %d\n", sig);
//     abort();
//     // int tmp_errno = errno;
//     // while (waitpid(-1, NULL, WNOHANG) > 0) {}
//     // errno = tmp_errno;
// }
