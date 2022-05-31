#import fileutil
#import cli

int main(int argc, char *argv[])
{
	if(argc != 2) {
		fatal("exactly one argument expected");
	}

	char *data = readfile_str(argv[1]);
	if(!data) {
		fatal("failed");
	}

	puts(data);
	return 0;
}
