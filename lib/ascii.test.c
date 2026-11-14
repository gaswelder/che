#import ascii
#import test

int main() {
	// trim
	char *sample = ascii.newstr("%s", "   123 \r\n ");
	ascii.trim(sample);
	test.streq(sample, "123");

	sample = ascii.newstr("%s", "     1.	The Sun	0:43.520");
	ascii.trim(sample);
	test.streq(sample, "1.	The Sun	0:43.520");

	// casecmp
	test.truth("GET == get", ascii.casecmp("GET", "get"));
	test.truth("GET != GET1", !ascii.casecmp("GET", "GET1"));

	// starts_with
	test.truth("abcdefg starts with abc", ascii.starts_with("abcdefg", "abc"));
	test.truth("abc doesn't start with abcdefg", !ascii.starts_with("abc", "abcdefg"));
	test.truth("abcdefg doesn't start with bcd", !ascii.starts_with("abcdefg", "bcd"));

	// ends_with
	test.truth("abc ends with bc", ascii.ends_with("abc", "bc"));
	test.truth("bc doesn't end with abc", !ascii.ends_with("bc", "abc"));
	test.truth("abc doesn't end with ab", !ascii.ends_with("abc", "ab"));

	// split
	char *split_output[10] = {};
	test.truth("n(split) == 2", ascii.split("bb", "aabbcc", split_output, sizeof(split_output)) == 2);
	test.streq(split_output[0], "aa");
	test.streq(split_output[1], "cc");

	return test.fails();
}
