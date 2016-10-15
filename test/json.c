import "json"
import "cli"

const char *test = "{\n\"defaults\": {\n"
	"\t\t\"login\": \"foo\",\n"
	"\t\t\"password\": \"123\",\n"
	"\t\t\"prob\": 0.1},\n"
	"\n"
	"\"bots\": [\n"
	"\t{\n"
	"\t\t\"login\": \"foo1\"\n"
	"\t},\n"
	"\t{\n"
	"\t\t\"login\": \"foo2\",\n"
	"\t\t\"prob\": 0.2\n"
	"\t}\n"
	"]\n}\n";

int main()
{
	/*
	 * Read-write-read-write
	 */
	json_node *n1 = json_parse(test);
	if(json_err(n1)) {
		fatal("Couldn't parse: %s", json_err(n1));
	}
	char *s1 = json_format(n1);

	json_node *n2 = json_parse(s1);
	if(json_err(n2)) {
		fatal("Couldn't parse the copy: %s", json_err(n2));
	}
	char *s2 = json_format(n2);

	/*
	 * Compare the write results
	 */
	if(strcmp(s1, s2) == 0) {
		puts("* OK");
	}
	else {
		puts("* FAIL");
	}

	puts(s1);
	puts(s2);

	free(s1);
	free(s2);

	json_free(n1);
	json_free(n2);
}
