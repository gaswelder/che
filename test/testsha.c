#import crypt/sha1

int main()
{
	sha1sum_t a = {0};

	char s[1000001] = {0};
	memset(s, 'a', 1000000);
	s[1000000] = '\0';
	
	sha1_str(s, a);
	sha1_print(a);
	
	return 0;
}
