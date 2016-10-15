# json

The library allows parsing JSON texts, building and manipulating data
and formatting data back as JSON.

The data is a tree where each node is an object
of type `json_node`. A node can be of one of the following types:

* object,
* array,
* string,
* number,
* boolean
* null.


## Parsing

To parse a JSON string, the `json_parse` function is used. It returns a
pointer to a root node of the data. If the parsing succeeds, the node
will be a valid one. In the case of error, a special error node is
returned. In any case the user should check the returned node using the
`json_error` function like in the example:

	json_node *t = json_parse(str);
	if(json_error(t)) {
		printf(stderr, "Could not parse JSON string: %s\n", json_error(t));
	}
	else {
		<process the data>
	}
	json_free(t);

The returned node has to be freed using the `json_free` in both cases.
Also, there is no need to check the returned value for equality to
`NULL` because `json_error` will do that and return the "not enough
memory" message (which is the sole cause for the `NULL` return value).

The `json_free` function must be called only on root nodes.
