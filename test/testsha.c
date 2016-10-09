import "crypt/sha1"

int main()
{
	sha1sum a;
	
	char s[1000001];
	memset(s, 'a', 1000000);
	s[1000000] = '\0';
	
	sha1_str(s, a);
	sha1_print(a);
	
	return 0;
}
