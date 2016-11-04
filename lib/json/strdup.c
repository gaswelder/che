
char *strdup(const char *s)
{
	char *copy = malloc(strlen(s) + 1);
	if(!copy) return NULL;
	strcpy(copy, s);
	return copy;
}
