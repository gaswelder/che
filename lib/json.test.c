#import json
#import test

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

int main() {
	/*
	 * Read-write-read-write
	 */
	json.json_node *n1 = json.json_parse(test);
	if (json.json_err(n1)) {
		panic("Couldn't parse: %s", json.json_err(n1));
	}
	char *s1 = json.format(n1);
	test.streq(s1, "{\"defaults\":{\"login\":\"foo\",\"password\":\"123\",\"prob\":0.100000},\"bots\":[{\"login\":\"foo1\"},{\"login\":\"foo2\",\"prob\":0.200000}]}");

	json.json_node *n2 = json.json_parse(s1);
	if (json.json_err(n2)) {
		panic("Couldn't parse the copy: %s", json.json_err(n2));
	}
	char *s2 = json.format(n2);

	test.streq(s1, s2);
	free(s1);
	free(s2);
	json.json_free(n1);
	json.json_free(n2);

	return test.fails();
}
