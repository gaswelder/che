import "prng/lcg"
import "clip/map"

int main()
{
	int x = 42;
	lcg_seed(445, 700001, 2097152, x);

	map *h = map_new();

	int max = 0;
	char maxword[7] = "";

	for (int i = 0; i < 900000; i++)
	{
		char word[7] = "";
		genword(word, 7);

		int c = 0;
		if(!map_exists(h, word)) {
			c = 0;
		}
		else {
			c = map_geti(h, word);
		}

		c++;
		map_seti(h, word, c);

		if(c > max) {
			strcpy(maxword, word);
			max = c;
		}
	}

	map_free(h);

	puts(maxword);
	return 0;
}

char *consonants = "bcdfghjklmnprstvwxz";
char *vowels = "aeiou";

void genword(char *buffer, int size)
{
	int i = 0;
	for (i = 0; i < size-1; i++)
	{
		if(i % 2 == 0) {
			buffer[i] = consonants[ lcg_rand() % 19 ];
		} else {
			buffer[i] = vowels[ lcg_rand() % 5 ];
		}
	}
	buffer[i] = '\0';
}
