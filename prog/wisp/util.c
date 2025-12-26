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

pub uint32_t hash(void *key, size_t keylen) {
	uint32_t hash = 0;
	char *skey = key;
	for (uint32_t i = 0; i < keylen; ++i) {
		hash += skey[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}
