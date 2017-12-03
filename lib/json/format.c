import "zio"
import "clip/str"

pub char *json_format(json_node *n)
{
	zio *z = zopen("mem", "", "");
	json_write(z, n);
	zrewind(z);

	str *s = str_new();
	while(1) {
		char c = zgetc(z);
		if(c == EOF) break;
		str_addc(s, c);
	}
	char *str1 = strdup(str_raw(s));
	str_free(s);
	zclose(z);
	return str1;
}

void json_write(zio *z, json_node *n)
{
	switch(json_type(n))
	{
		case JSON_OBJ:
			size_t len = json_size(n);
			zprintf(z, "{");
			for(size_t i = 0; i < len; i++) {
				zprintf(z, "\"%s\": ", json_key(n, i));
				json_write(z, json_val(n, i));
				if(i + 1 < len) {
					zprintf(z, ",");
				}
			}
			zprintf(z, "}");
			break;

		case JSON_ARR:
			size_t len = json_len(n);
			zprintf(z, "[");
			for(size_t i = 0; i < len; i++) {
				json_write(z, json_at(n, i));
				if(i + 1 < len) {
					zprintf(z, ",");
				}
			}
			zprintf(z, "]");
			break;

		case JSON_STR:
			zprintf(z, "\"%s\"", json_str(n));
			break;

		case JSON_NUM:
			zprintf(z, "%f", json_dbl(n));
			break;

		case JSON_BOOL:
			zprintf(z, json_bool(n)? "true" : "false");
			break;

		case JSON_NULL:
			zprintf(z, "null");
			break;
	}
}
