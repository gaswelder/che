#import clip/stringtable

pub typedef {
    char hostname[1000];
    char homedir[1000];
    char homedir_abspath[1000];
    char cgidir[1000];
    char cgidir_abspath[1000];
    char proxyhost[1000];

    stringtable.t *aliases;
} LKHostConfig;

// void print_sample_config() {
//     printf(
// "Sample config file:\n"
// "\n"
// "serverhost=127.0.0.1\n"
// "port=5000\n"
// "\n"
// "# Matches all other hostnames\n"
// "hostname *\n"
// "homedir=/var/www/testsite\n"
// "alias latest=latest.html\n"
// "\n"
// "# Matches http://localhost\n"
// "hostname localhost\n"
// "homedir=/var/www/testsite\n"
// "\n"
// "# http://littlekitten.xyz\n"
// "hostname littlekitten.xyz\n"
// "homedir=/var/www/testsite\n"
// "cgidir=cgi-bin\n"
// "alias latest=latest.html\n"
// "alias about=about.html\n"
// "alias guestbook=cgi-bin/guestbook.pl\n"
// "alias blog=cgi-bin/blog.pl\n"
// "\n"
// "# http://newsboard.littlekitten.xyz\n"
// "hostname newsboard.littlekitten.xyz\n"
// "proxyhost=localhost:8001\n"
// "\n"
// "# Format description:\n"
// "# The host and port number is defined first, followed by one or more\n"
// "# host config sections. The host config section always starts with the\n"
// "# 'hostname <domain>' line followed by the settings for that hostname.\n"
// "# The section ends on either EOF or when a new 'hostname <domain>' line\n"
// "# is read, indicating the start of the next host config section.\n"
// "\n"
//     );
// }


