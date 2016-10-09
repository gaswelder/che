import "prng/mt"

int main()
{
	mt_seed( (uint32_t) time(NULL) );

    for(int i = 0; i < 1000; i++) {
		printf("%f\n", mt_randf());
	}
    return 0;
}
