pub void *xmalloc (size_t size) {
  void *p = calloc (size, 1);
  if (!p) panic("calloc failed");
  return p;
}

pub void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}

pub char *xstrdup (const char *str)
{
  char *newstr = xmalloc (strlen (str) + 1);
  strcpy (newstr, str);
  return newstr;
}

pub char *pathcat (const char *prefix, char *path)
{
  char *str = xmalloc (strlen (prefix) + strlen (path) + 2);
  strcpy (str, prefix);
  str[strlen (prefix)] = '/';	/* Extra / don't hurt. */
  strcpy (str + strlen (prefix) + 1, path);
  return str;
}

pub char *process_str(char *rawstr) {
  char *str = xstrdup (rawstr + 1);	/* trim leading quote */

  /* Remove backquotes. */
  char *p = str;

  char *eq = strchr (p, '\\');
  while (eq != NULL) {
      /* replace \ with next character */
      *eq = *(eq + 1);
      memmove (eq + 1, eq + 2, strlen (eq) + 1);
      p = eq + 1;
      eq = strchr (p, '\\');
    }

  str[strlen (str) - 1] = '\0';	/* remove trailing quote */

  return str;
}

pub char *unprocess_str (char *cleanstr) {
  /* Count the quotes and backquotes. */
  char *p = cleanstr;
  int cnt = 0;
  while (*p != '\0')
    {
      if (*p == '\\' || *p == '"')
	cnt++;
      p++;
    }

  /* Two extra for quotes and one for each character that needs
     escaping. */
  char *str = xmalloc (strlen (cleanstr) + (size_t)cnt + 2 + 1);
  strcpy (str + 1, cleanstr);

  /* Place backquotes. */
  char *c = str + 1;
  while (*c != '\0')
    {
      if (*c == '\\' || *c == '"')
	{
	  memmove (c + 1, c, strlen (c) + 1);
	  *c = '\\';
	  c++;
	}
      c++;
    }

  /* Surrounding quotes. */
  str[0] = '"';
  str[strlen (str) + 1] = '\0';
  str[strlen (str)] = '"';

  return str;
}
