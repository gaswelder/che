
#define W 8         /* required for MAKECRCH */

   {
        /*
          The crc32.h header file contains tables for both 32-bit and 64-bit
          z_word_t's, and so requires a 64-bit type be available. In that case,
          z_word_t must be defined to be 64-bits. This code then also generates
          and writes out the tables for the case that z_word_t is 32 bits.
         */
        FILE *out;
        int k, n;
        uint32_t ltl[8][256];
        z_word_t big[8][256];

        out = fopen("crc32.h", "w");
        if (out == NULL) return;

        /* write out little-endian CRC table to crc32.h */
        fprintf(out,
            "/* crc32.h -- tables for rapid CRC calculation\n"
            " * Generated automatically by crc32.c\n */\n"
            "\n"
            "local const uint32_t FAR crc_table[] = {\n"
            "    ");
        write_table(out, crc_table, 256);
        fprintf(out,
            "};\n");

        /* write out big-endian CRC table for 64-bit z_word_t to crc32.h */
        fprintf(out,
            "\n"
            "#ifdef W\n"
            "\n"
            "#if W == 8\n"
            "\n"
            "local const z_word_t FAR crc_big_table[] = {\n"
            "    ");
        write_table64(out, crc_big_table, 256);
        fprintf(out,
            "};\n");

        /* write out big-endian CRC table for 32-bit z_word_t to crc32.h */
        fprintf(out,
            "\n"
            "#else /* W == 4 */\n"
            "\n"
            "local const z_word_t FAR crc_big_table[] = {\n"
            "    ");
        write_table32hi(out, crc_big_table, 256);
        fprintf(out,
            "};\n"
            "\n"
            "#endif\n");

        /* write out braid tables for each value of N */
        for (n = 1; n <= 6; n++) {
            fprintf(out,
            "\n"
            "#if N == %d\n", n);

            /* compute braid tables for this N and 64-bit word_t */
            braid(ltl, big, n, 8);

            /* write out braid tables for 64-bit z_word_t to crc32.h */
            fprintf(out,
            "\n"
            "#if W == 8\n"
            "\n"
            "local const uint32_t FAR crc_braid_table[][256] = {\n");
            for (k = 0; k < 8; k++) {
                fprintf(out, "   {");
                write_table(out, ltl[k], 256);
                fprintf(out, "}%s", k < 7 ? ",\n" : "");
            }
            fprintf(out,
            "};\n"
            "\n"
            "local const z_word_t FAR crc_braid_big_table[][256] = {\n");
            for (k = 0; k < 8; k++) {
                fprintf(out, "   {");
                write_table64(out, big[k], 256);
                fprintf(out, "}%s", k < 7 ? ",\n" : "");
            }
            fprintf(out,
            "};\n");

            /* compute braid tables for this N and 32-bit word_t */
            braid(ltl, big, n, 4);

            /* write out braid tables for 32-bit z_word_t to crc32.h */
            fprintf(out,
            "\n"
            "#else /* W == 4 */\n"
            "\n"
            "local const uint32_t FAR crc_braid_table[][256] = {\n");
            for (k = 0; k < 4; k++) {
                fprintf(out, "   {");
                write_table(out, ltl[k], 256);
                fprintf(out, "}%s", k < 3 ? ",\n" : "");
            }
            fprintf(out,
            "};\n"
            "\n"
            "local const z_word_t FAR crc_braid_big_table[][256] = {\n");
            for (k = 0; k < 4; k++) {
                fprintf(out, "   {");
                write_table32hi(out, big[k], 256);
                fprintf(out, "}%s", k < 3 ? ",\n" : "");
            }
            fprintf(out,
            "};\n"
            "\n"
            "#endif\n"
            "\n"
            "#endif\n");
        }
        fprintf(out,
            "\n"
            "#endif\n");

        /* write out zeros operator table to crc32.h */
        fprintf(out,
            "\n"
            "local const uint32_t FAR x2n_table[] = {\n"
            "    ");
        write_table(out, x2n_table, 32);
        fprintf(out,
            "};\n");
        fclose(out);
    }

    /*
   Write the 32-bit values in table[0..k-1] to out, five per line in
   hexadecimal separated by commas.
 */
local void write_table(out, table, k)
    FILE *out;
    const uint32_t FAR *table;
    int k;
{
    int n;

    for (n = 0; n < k; n++)
        fprintf(out, "%s0x%08lx%s", n == 0 || n % 5 ? "" : "    ",
                (unsigned long)(table[n]),
                n == k - 1 ? "" : (n % 5 == 4 ? ",\n" : ", "));
}

/*
   Write the high 32-bits of each value in table[0..k-1] to out, five per line
   in hexadecimal separated by commas.
 */
local void write_table32hi(out, table, k)
FILE *out;
const z_word_t FAR *table;
int k;
{
    int n;

    for (n = 0; n < k; n++)
        fprintf(out, "%s0x%08lx%s", n == 0 || n % 5 ? "" : "    ",
                (unsigned long)(table[n] >> 32),
                n == k - 1 ? "" : (n % 5 == 4 ? ",\n" : ", "));
}

/*
  Write the 64-bit values in table[0..k-1] to out, three per line in
  hexadecimal separated by commas. This assumes that if there is a 64-bit
  type, then there is also a long long integer type, and it is at least 64
  bits. If not, then the type cast and format string can be adjusted
  accordingly.
 */
local void write_table64(out, table, k)
    FILE *out;
    const z_word_t FAR *table;
    int k;
{
    int n;

    for (n = 0; n < k; n++)
        fprintf(out, "%s0x%016llx%s", n == 0 || n % 3 ? "" : "    ",
                (unsigned long long)(table[n]),
                n == k - 1 ? "" : (n % 3 == 2 ? ",\n" : ", "));
}

/* Actually do the deed. */
int main()
{
    make_crc_table();
    return 0;
}
