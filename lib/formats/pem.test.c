#import formats/pem
#import error
#import test

int main() {
	error.t err = {};
	pem.reader_t *r = pem.new("blabla\n-----BEGIN PRIVATE KEY-----\n Zg== \n-----END PRIVATE KEY-----\n"
		"-----BEGIN CERTIFICATE-----\n Zm9v\r\nYmFy\t\n-----END CERTIFICATE-----\n");
	pem.next(r, &err);
	test.streq(r->label, "PRIVATE KEY");
	test.streq(r->data, "Zg==");

	pem.next(r, &err);
	test.streq(r->label, "CERTIFICATE");
	test.streq(r->data, "Zm9vYmFy");

	test.truth("EOF", pem.next(r, &err) == false);
	if (err.set) {
		panic("%s", err.msg);
	}
	pem.close(r);

	return test.fails();
}
