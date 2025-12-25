pub char *addslashes(char *cleanstr) {
	/* Count the quotes and backquotes. */
	char *p = cleanstr;
	int n = 0;
	while (*p != '\0') {
		if (*p == '\\' || *p == '"') n++;
		p++;
	}

	/* Two extra for quotes and one for each character that needs
		 escaping. */
	char *str = calloc!(strlen (cleanstr) + (size_t) n + 2 + 1, 1);
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
