#import eval.c

/* Error testing */
void assert (int b, char *msg);
int err_cnt = 0, test_cnt = 0;

/* Tests */
void symbol_tests ();
void string_tests ();
void wisp_tests ();

int main ()
{
  eval.wisp_init ();

  printf ("Running symbol tests ...\n");
  symbol_tests ();
  printf ("Running string tests ...\n");
  string_tests ();
  printf ("Running Wisp code tests ...\n");
  wisp_tests ();

  /* Count up errors and exit */
  if (err_cnt == 0)
    {
      printf ("All %d tests passed.\n", test_cnt);
      exit(0);
    }
  else
    {
      printf ("%d tests failed.\n", err_cnt);
      exit(1);
    }
}

void assert (int b, char *msg)
{
  test_cnt++;
  if (!b)
    {
      fprintf (stderr, "failed: %s\n", msg);
      err_cnt++;
    }
}

void string_tests ()
{
  assert (strcmp (common.process_str ("\"Hello\\\" there!\""), "Hello\" there!") ==
	  0, "process_str()");
  assert (strcmp (common.unprocess_str ("Hello\" there!"), "\"Hello\\\" there!\"") ==
	  0, "unprocess_str()");
}

void symbol_tests ()
{
  assert (c_sym ("symbol") == c_sym ("symbol"), "symbol interning");
  assert (strcmp (SYMNAME (c_sym ("str")), "str") == 0, "SYMNAME test");

  /* dynamic scoping */
  eval.object_t *so = c_sym ("s");
  eval.object_t *a = c_sym ("a");
  eval.object_t *b = c_sym ("b");
  eval.object_t *c = c_sym ("c");
  SET (so, a);
  sympush (so, b);
  sympush (so, c);
  assert (GET (so) == c, "symbol push/pop 1");
  sympop (so);
  assert (GET (so) == b, "symbol push/pop 2");
  sympop (so);
  assert (GET (so) == a, "symbol push/pop 3");
}

int run_wisp_test (char *file)
{
  /* fork() so that failures don't kill this process */
  if (fork () == 0)
    {
      execl ("wisp", "wisp", file, NULL);
      fprintf (stderr, "%s\n", strerror (errno));
      exit (1);
    }
  int r;
  wait (&r);
  return r == 0;
}

void wisp_tests ()
{
  assert (run_wisp_test ("test/stress.wisp"), "Wisp stress test");
  assert (run_wisp_test ("test/eq-test.wisp"), "Wisp equality");
}
