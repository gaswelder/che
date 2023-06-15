#import strings
#import test

int main() {
    char *sample = strings.newstr("%s", "   123 \r\n ");

    // trim
    sample = strings.trim(sample);
    test.streq(sample, "123");

    // casecmp
    test.truth("GET == get", strings.casecmp("GET", "get"));
    test.truth("GET != GET1", !strings.casecmp("GET", "GET1"));

    // starts_with
    test.truth("abcdefg starts with abc", strings.starts_with("abcdefg", "abc"));
    test.truth("abc doesn't start with abcdefg", !strings.starts_with("abc", "abcdefg"));
    test.truth("abcdefg doesn't start with bcd", !strings.starts_with("abcdefg", "bcd"));

    // ends_with
    test.truth("abc ends with bc", strings.ends_with("abc", "bc"));
    test.truth("bc doesn't end with abc", !strings.ends_with("bc", "abc"));
    test.truth("abc doesn't end with ab", !strings.ends_with("abc", "ab"));
    return test.fails();
}
