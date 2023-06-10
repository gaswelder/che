#import regexp9.c

int main(int ac, char **av) {
	(void) ac;
	test(av[1], "^[^!@]+$", "/bin/upas/aliasmail '&'");
	test(av[1], "^local!(.*)$", "/mail/box/\\1/mbox");
	test(av[1], "^plan9!(.*)$", "\\1");
	test(av[1], "^helix!(.*)$", "\\1");
	test(av[1], "^([^!]+)@([^!@]+)$", "\\2!\\1");
	test(av[1], "^(uk\\.[^!]*)(!.*)$", "/bin/upas/uk2uk '\\1' '\\2'");
	test(av[1], "^[^!]*\\.[^!]*!.*$", "inet!&");
	test(av[1], "^\xE2\x98\xBA$", "smiley");
	test(av[1], "^(coma|research|pipe|pyxis|inet|hunny|gauss)!(.*)$", "/mail/lib/qmail '\\s' 'net!\\1' '\\2'");
	test(av[1], "^.*$", "/mail/lib/qmail '\\s' 'net!research' '&'");
	return 0;
}

void test(char *av, *re, *s) {
	regexp9.Resub rs[10] = {0};
	char dst[128];

	printf("%s VIA %s", av, re);
	regexp9.Reprog *p = regexp9.regcomp(re);
	if (regexp9.regexec(p, av, rs, 10)){
		regexp9.regsub(s, dst, sizeof(dst), rs, 10);
		printf(" sub %s -> %s", s, dst);
	}
	printf("\n");
}
