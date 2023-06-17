#define LK_BUFSIZE_XXL 8192

int main(int argc, char *argv[]) {

    lkstring_test();
    lkstringmap_test();
    lkbuffer_test();
    lkreflist_test();
    lkconfig_test();

    lk_print_allocitems();

    return 0;
}

void lkstring_test() {
    LKString *lks, *lks2;

    printf("Running LKString tests... ");
    lks = lk_string_new("");
    assert(lks->s_len == 0);
    assert(lks->s_size >= lks->s_len);
    assert(strcmp(lks->s, "") == 0);
    lkstring.lk_string_free(lks);

    // lks, lks2
    lks = lk_string_new("abc def");
    lks2 = lk_string_new("abc ");
    assert(lks->s_len == 7);
    assert(lks->s_size >= lks->s_len);
    assert(strcmp(lks->s, "abc "));
    assert(strcmp(lks->s, "abc def2"));
    assert(!strcmp(lks->s, "abc def"));

    lk_string_append(lks2, "def");
    assert(!strcmp(lks->s, lks2->s));

    lk_string_assign(lks2, "abc def");

    lk_string_assign(lks2, "");

    lk_string_assign_sprintf(lks2, "abc %s", "def");
    assert(!strcmp(lks->s, lks2->s));

    lk_string_assign_sprintf(lks, "abc%ddef%d", 123, 456);
    lk_string_assign_sprintf(lks2, "%s123%s456", "abc", "def");
    assert(!strcmp(lks->s, lks2->s));
    lkstring.lk_string_free(lks);
    lkstring.lk_string_free(lks2);

    // Test very large strings.
    lks = lk_string_new("");
    char sbuf[LK_BUFSIZE_XXL];
    memset(sbuf, 'a', sizeof(sbuf));
    sbuf[sizeof(sbuf)-1] = '\0';
    lk_string_assign(lks, sbuf);

    assert(lks->s_len == sizeof(sbuf)-1);
    for (int i=0; i < sizeof(sbuf)-1; i++) {
        assert(lks->s[i] == 'a');
    }

    lk_string_assign_sprintf(lks, "sbuf: %s", sbuf);
    assert(lks->s_len == sizeof(sbuf)-1 + 6);
    assert(strncmp(lks->s, "sbuf: aaaaa", 11) == 0);
    assert(lks->s[lks->s_len-2] == 'a');
    assert(lks->s[lks->s_len-3] == 'a');
    assert(lks->s[lks->s_len-4] == 'a');
    lkstring.lk_string_free(lks);

    lks = lk_string_new("abcdefghijkl mnopqrst");
    assert(lkstring.lk_string_starts_with(lks, "abc"));
    assert(lkstring.lk_string_starts_with(lks, "abcdefghijkl mnopqrst"));
    assert(!lkstring.lk_string_starts_with(lks, "abcdefghijkl mnopqrstuvwxyz"));
    assert(!lkstring.lk_string_starts_with(lks, "abcdefghijkl mnopqrst "));
    assert(lkstring.lk_string_starts_with(lks, ""));

    assert(lk_string_ends_with(lks, "rst"));
    assert(lk_string_ends_with(lks, " mnopqrst"));
    assert(lk_string_ends_with(lks, "abcdefghijkl mnopqrst"));
    assert(!lk_string_ends_with(lks, " abcdefghijkl mnopqrst"));
    assert(lk_string_ends_with(lks, ""));

    lk_string_assign(lks, "");
    assert(lkstring.lk_string_starts_with(lks, ""));
    assert(lk_string_ends_with(lks, ""));
    assert(!lkstring.lk_string_starts_with(lks, "a"));
    assert(!lk_string_ends_with(lks, "a"));
    lkstring.lk_string_free(lks);

    lks = lk_string_new("  abc  ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, "abc"));

    lk_string_assign(lks, " \tabc \n  ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, "abc"));

    lk_string_assign(lks, "   ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, ""));
    lkstring.lk_string_free(lks);

    lks = lk_string_new("/testsite/");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    assert(lk_string_sz_equal(lks, "/testsite/"));
    assert(lk_string_sz_equal(lks, "/testsite/"));

    assert(lk_string_sz_equal(lks, "testsite/"));
    assert(lk_string_sz_equal(lks, "site/"));
    assert(lk_string_sz_equal(lks, "site/"));

    lk_string_assign(lks, "/testsite/");
    lk_string_chop_end(lks, "abc");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_end(lks, "");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_end(lks, "///");
    assert(lk_string_sz_equal(lks, "/testsite/"));

    lk_string_chop_end(lks, "/");
    assert(lk_string_sz_equal(lks, "/testsite"));
    lk_string_chop_end(lks, "site");
    assert(lk_string_sz_equal(lks, "/test"));
    lk_string_chop_end(lks, "1st");
    assert(lk_string_sz_equal(lks, "/test"));

    lk_string_assign(lks, "");
    assert(lk_string_sz_equal(lks, ""));
    lk_string_chop_end(lks, "");
    assert(lk_string_sz_equal(lks, ""));
    assert(lk_string_sz_equal(lks, ""));
    lk_string_chop_end(lks, "def");
    assert(lk_string_sz_equal(lks, ""));
    lkstring.lk_string_free(lks);

    lks = lk_string_new("abc");
    assert(lk_string_sz_equal(lks, "abc"));
    lk_string_append_char(lks, 'd');
    assert(lk_string_sz_equal(lks, "abcd"));
    lk_string_append_char(lks, 'e');
    lk_string_append_char(lks, 'f');
    lk_string_append_char(lks, 'g');
    assert(lk_string_sz_equal(lks, "abcdefg"));
    assert(lks->s_len == 7);
    lkstring.lk_string_free(lks);

    lks = lk_string_new("abc");
    lk_string_prepend(lks, "def ");
    assert(lk_string_sz_equal(lks, "def abc"));
    lkstring.lk_string_free(lks);

    printf("Done.\n");
}

void lkstringmap_test() {
    LKStringTable *st;
    void *v;

    printf("Running LKStringTable tests... ");
    st = lk_stringtable_new();
    assert(st->items_len == 0);

    lk_stringtable_set(st, "abc", "ABC");
    lk_stringtable_set(st, "def", "DEF");
    lk_stringtable_set(st, "ghi", "GHI");
    lk_stringtable_set(st, "hij", "HIJ");
    lk_stringtable_set(st, "klm", "KLM");
    assert(st->items_len == 5);

    lk_stringtable_set(st, "123", "one two three");
    lk_stringtable_set(st, "  ", "space space");
    lk_stringtable_set(st, "", "(blank)");
    assert(st->items_len == 5);

    v = lk_stringtable_get(st, "abc");
    assert(!strcmp(v, "ABC"));

    lk_stringtable_set(st, "abc", "A B C");
    v = lk_stringtable_get(st, "abc");
    assert(st->items_len == 5);
    assert(strcmp(v, "ABC"));
    assert(!strcmp(v, "A B C"));

    lk_stringtable_set(st, "abc ", "ABC(space)");
    assert(st->items_len == 6);
    v = lk_stringtable_get(st, "abc ");
    assert(!strcmp(v, "ABC(space)"));
    v = lk_stringtable_get(st, "abc");
    assert(!strcmp(v, "A B C"));

    v = lk_stringtable_get(st, "ABC");
    assert(v == NULL);
    v = lk_stringtable_get(st, "123");
    assert(!strcmp(v, "one two three"));
    v = lk_stringtable_get(st, "  ");
    assert(!strcmp(v, "space space"));
    v = lk_stringtable_get(st, " ");
    assert(v == NULL);
    v = lk_stringtable_get(st, "");
    assert(!strcmp(v, "(blank)"));

    lk_stringtable_free(st);
    printf("Done.\n");
}

void lkbuffer_test() {
    LKBuffer *buf;

    printf("Running LKBuffer tests... ");
    buf = lk_buffer_new(0);
    assert(buf->bytes_size > 0);
    lk_buffer_free(buf);

    buf = lk_buffer_new(10);
    assert(buf->bytes_size == 10);
    lk_buffer_append(buf, "1234567", 7);
    assert(buf->bytes_len == 7);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[0] == '1');
    assert(buf->bytes[1] == '2');
    assert(buf->bytes[2] == '3');
    assert(buf->bytes[5] == '6');

    lk_buffer_append(buf, "890", 3);
    assert(buf->bytes_len == 10);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[7] == '8');
    assert(buf->bytes[8] == '9');
    assert(buf->bytes[9] == '0');
    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    lk_buffer_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lk_buffer_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    lk_buffer_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lk_buffer_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lk_buffer_free(buf);

    // Test buffer append on very large strings.
    buf = lk_buffer_new(0);
    char sbuf[LK_BUFSIZE_XXL];
    memset(sbuf, 'a', sizeof(sbuf));
    lk_buffer_append(buf, sbuf, sizeof(sbuf));

    assert(buf->bytes_len == sizeof(sbuf));
    for (int i=0; i < sizeof(sbuf); i++) {
        assert(buf->bytes[i] == 'a');
    }

    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    sbuf[sizeof(sbuf)-1] = '\0';
    lk_buffer_append_sprintf(buf, "buf: %s", sbuf);
    assert(buf->bytes_len == sizeof(sbuf)-1 + 5);
    assert(strncmp(buf->bytes, "buf: aaaaa", 10) == 0);
    assert(buf->bytes[buf->bytes_len-1] == 'a');
    assert(buf->bytes[buf->bytes_len-2] == 'a');
    assert(buf->bytes[buf->bytes_len-3] == 'a');
    lk_buffer_free(buf);

    printf("Done.\n");
}

void lkreflist_test() {
    printf("Running LKRefList tests... ");

    LKString *abc = lk_string_new("abc");
    LKString *def = lk_string_new("def");
    LKString *ghi = lk_string_new("ghi");
    LKString *jkl = lk_string_new("jkl");
    LKString *mno = lk_string_new("mno");
    LKRefList *l = lk_reflist_new();
    lkreflist.lk_reflist_append(l, abc);
    lkreflist.lk_reflist_append(l, def);
    lkreflist.lk_reflist_append(l, ghi);
    lkreflist.lk_reflist_append(l, jkl);
    lkreflist.lk_reflist_append(l, mno);
    assert(l->items_len == 5);

    LKString *s = lk_reflist_get(l, 0);
    assert(lk_string_sz_equal(s, "abc"));
    s = lk_reflist_get(l, 5);
    assert(s == NULL);
    s = lk_reflist_get(l, 4);
    assert(lk_string_sz_equal(s, "mno"));
    s = lk_reflist_get(l, 3);
    assert(lk_string_sz_equal(s, "jkl"));
    lk_reflist_remove(l, 3);
    assert(l->items_len == 4);
    s = lk_reflist_get(l, 3);
    assert(lk_string_sz_equal(s, "mno"));

    lkstring.lk_string_free(abc);
    lkstring.lk_string_free(def);
    lkstring.lk_string_free(ghi);
    lkstring.lk_string_free(jkl);
    lkstring.lk_string_free(mno);
    lk_reflist_free(l);
    printf("Done.\n");
}

void lkconfig_test() {
    printf("Running LKConfig tests... \n");

    LKConfig *cfg = lk_config_new();
    lk_config_read_configfile(cfg, "lktest.conf");
    assert(cfg != NULL);
    lk_config_print(cfg);
    lk_config_free(cfg);

    printf("Done.\n");
}

