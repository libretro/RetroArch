#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "rarchdb.h"

#include "rmsgpack_dom.h"
#include <compat/fnmatch.h>

#define MAX_ERROR_LEN 256
#define MAX_ARGS 50

static char tmp_error_buff [MAX_ERROR_LEN] = {};

struct buffer {
	const char * data;
	size_t len;
	off_t offset;
};

/* Errors */
static void raise_too_many_arguments(const char ** error) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "Too many arguments in function call"
	);
	*error = tmp_error_buff;
}

static void raise_expected_number(
        off_t where,
        const char ** error
) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "%lu::Expected number",
	        where
	);
	*error = tmp_error_buff;
}

static void raise_expected_string(
        off_t where,
        const char ** error
) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "%lu::Expected string",
	        where
	);
	*error = tmp_error_buff;
}

static void raise_unexpected_eof(
        off_t where,
        const char ** error
) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "%lu::Unexpected EOF",
	        where
	);
	*error = tmp_error_buff;
}

static void raise_enomem(const char ** error) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "Out of memory"
	);
	*error = tmp_error_buff;
}

static void raise_unknown_function(
        off_t where,
        const char * name,
        size_t len,
        const char ** error
) {
	int n = snprintf(
	                tmp_error_buff,
	                MAX_ERROR_LEN,
	                "%lu::Unknown function '",
	                where
	        );
	if (len < (MAX_ERROR_LEN - n - 3)) {
		strncpy(tmp_error_buff + n, name, len);
	}
	strcpy(tmp_error_buff + n + len, "'");
	*error = tmp_error_buff;
}
static void raise_expected_eof(
        off_t where,
        char found,
        const char ** error
) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "%lu::Expected EOF found '%c'",
	        where,
	        found
	);
	*error = tmp_error_buff;
}

static void raise_unexpected_char(
        off_t where,
        char expected,
        char found,
        const char ** error
) {
	snprintf(
	        tmp_error_buff,
	        MAX_ERROR_LEN,
	        "%lu::Expected '%c' found '%c'",
	        where,
	        expected,
	        found
	);
	*error = tmp_error_buff;
}

enum argument_type {
	AT_FUNCTION,
	AT_VALUE
};

struct argument;

typedef struct rmsgpack_dom_value (* rarch_query_func)(
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
);

struct invocation {
	rarch_query_func func;
	unsigned argc;
	struct argument * argv;
};

struct argument {
	enum argument_type type;
	union {
		struct rmsgpack_dom_value value;
		struct invocation invocation;
	};
};

static void argument_free(struct argument * arg) {
	unsigned i;
	if (arg->type == AT_FUNCTION) {
		for (i = 0; i < arg->invocation.argc; i++) {
			argument_free(&arg->invocation.argv[i]);
		}
	} else {
		rmsgpack_dom_value_free(&arg->value);
	}
}


struct query {
	unsigned ref_count;
	struct invocation root;
};

struct registered_func {
	const char * name;
	rarch_query_func func;
};

static struct buffer parse_argument(
        struct buffer buff,
        struct argument * arg,
        const char ** error
);

static struct rmsgpack_dom_value is_true(
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	res.type = RDT_BOOL;
	res.bool_ = 0;
	if (argc > 0 || input.type != RDT_BOOL) {
		res.bool_ = 0;
	} else {
		res.bool_ = input.bool_;
	}
	return res;
}

static struct rmsgpack_dom_value equals (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	struct argument arg;
	res.type = RDT_BOOL;
	if (argc != 1) {
		res.bool_ = 0;
	} else {
		arg = argv[0];
		if (arg.type != AT_VALUE) {
			res.bool_ = 0;
		} else {
			if (input.type == RDT_UINT && arg.value.type == RDT_INT) {
				arg.value.type = RDT_UINT;
				arg.value.uint_ = arg.value.int_;
			}
			res.bool_ = (rmsgpack_dom_value_cmp(&input, &arg.value) == 0);
		}
	}
	return res;
}

static struct rmsgpack_dom_value operator_or (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	unsigned i;
	res.type = RDT_BOOL;
	res.bool_ = 0;
	for (i = 0; i < argc; i++) {
		if (argv[i].type == AT_VALUE) {
			res = equals(input, 1, &argv[i]);
		} else {
			res = is_true(
			                argv[i].invocation.func(input,
			                                        argv[i].invocation.argc,
			                                        argv[i].invocation.argv
			                ),
			                0,
			                NULL
			        );
		}

		if (res.bool_) {
			return res;
		}
	}
	return res;
}

static struct rmsgpack_dom_value between (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	unsigned i;
	res.type = RDT_BOOL;
	res.bool_ = 0;
	if (argc != 2) {
		return res;
	}
	if (argv[0].type != AT_VALUE || argv[1].type != AT_VALUE) {
		return res;
	}
	if (argv[0].value.type != RDT_INT || argv[1].value.type != RDT_INT) {
		return res;
	}
	switch (input.type) {
	case RDT_INT:
		res.bool_ = input.int_ >= argv[0].value.int_ && input.int_ <= argv[1].value.int_;
		break;
	case RDT_UINT:
		res.bool_ = input.int_ >= argv[0].value.uint_ && input.int_ <= argv[1].value.int_;
		break;
	default:
		return res;
	}
	return res;
}

static struct rmsgpack_dom_value operator_and (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	unsigned i;
	res.type = RDT_BOOL;
	res.bool_ = 0;
	for (i = 0; i < argc; i++) {
		if (argv[i].type == AT_VALUE) {
			res = equals(input, 1, &argv[i]);
		} else {
			res = is_true(
			                argv[i].invocation.func(input,
			                                        argv[i].invocation.argc,
			                                        argv[i].invocation.argv
			                ),
			                0,
			                NULL
			        );
		}

		if (!res.bool_) {
			return res;
		}
	}
	return res;
}

static struct rmsgpack_dom_value q_glob (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	unsigned i;
	res.type = RDT_BOOL;
	res.bool_ = 0;
	if (argc != 1) {
		return res;
	}
	if (argv[0].type != AT_VALUE || argv[0].value.type != RDT_STRING) {
		return res;
	}
	if (input.type != RDT_STRING) {
		return res;
	}
	res.bool_ = rl_fnmatch(
		argv[0].value.string.buff,
		input.string.buff,
		0
	) == 0;
	return res;
}

static struct rmsgpack_dom_value all_map (
        struct rmsgpack_dom_value input,
        unsigned argc,
        const struct argument * argv
) {
	struct rmsgpack_dom_value res;
	struct rmsgpack_dom_value key;
	struct rmsgpack_dom_value * value = NULL;
	struct argument arg;
	struct rmsgpack_dom_value nil_value;
	unsigned i;
	nil_value.type = RDT_NULL;
	res.type = RDT_BOOL;
	res.bool_ = 1;

	if (argc % 2 != 0) {
		res.bool_ = 0;
		return res;
	}
	if (input.type != RDT_MAP) {
		return res;
	}

	for (i = 0; i < argc; i += 2) {
		arg = argv[i];
		if (arg.type != AT_VALUE) {
			res.bool_ = 0;
			goto clean;
		}
		value = rmsgpack_dom_value_map_value(&input, &arg.value);
		if (!value) {
			// All missing fields are nil
			value = &nil_value;
		}
		arg = argv[i + 1];
		if (arg.type == AT_VALUE) {
			res = equals(*value, 1, &arg);
		} else {
			res = is_true(arg.invocation.func(
			                      *value,
			                      arg.invocation.argc,
			                      arg.invocation.argv
			              ), 0, NULL);
			value = NULL;
		}
		if (!res.bool_) {
			break;
		}
	}
clean:
	return res;
}

struct registered_func registered_functions[100] = {
	{"is_true", is_true},
	{"or", operator_or},
	{"and", operator_and},
	{"between", between},
	{"glob", q_glob},
	{NULL, NULL}
};

static struct buffer chomp(struct buffer buff) {
	off_t i = 0;
	for (; buff.offset < buff.len && isspace(buff.data[buff.offset]); buff.offset++) ;
	return buff;
}

static struct buffer expect_char(
        struct buffer buff,
        char c,
        const char ** error
) {
	if (buff.offset >= buff.len) {
		raise_unexpected_eof(buff.offset, error);
	} else if (buff.data[buff.offset] != c) {
		raise_unexpected_char(
		        buff.offset,
		        c,
		        buff.data[buff.offset],
		        error
		);
	} else {
		buff.offset++;
	}
	return buff;
}

static struct buffer expect_eof(
        struct buffer buff,
        const char ** error
) {
	buff = chomp(buff);
	if (buff.offset < buff.len) {
		raise_expected_eof(buff.offset, buff.data[buff.offset], error);
	}
	return buff;
}

static struct buffer parse_table(
        struct buffer buff,
        struct invocation * invocation,
        const char ** error
);

static int peek(
        struct buffer buff,
        const char * data
) {
	size_t remain = buff.len - buff.offset;
	if (remain < strlen(data)) {
		return 0;
	}
	return (strncmp(
	                buff.data + buff.offset,
	                data,
	                strlen(data)
	        ) == 0);
}

static int is_eot(struct buffer buff) {
	return (buff.offset >= buff.len);
}

static void peek_char(
        struct buffer buff,
        char * c,
        const char ** error
) {
	if (is_eot(buff)) {
		raise_unexpected_eof(buff.offset, error);
		return;
	}
	*c = buff.data[buff.offset];
}

static struct buffer get_char(
        struct buffer buff,
        char * c,
        const char ** error
) {
	if (is_eot(buff)) {
		raise_unexpected_eof(buff.offset, error);
		return buff;
	}
	*c = buff.data[buff.offset];
	buff.offset++;
	return buff;
}

static struct buffer parse_string(
        struct buffer buff,
        struct rmsgpack_dom_value * value,
        const char ** error
) {
	char terminator = '\0';
	char c;
	const char * str_start;
	buff = get_char(buff, &terminator, error);
	if (*error) {
		return buff;
	}
	if (terminator != '"' && terminator != '\'') {
		buff.offset--;
		raise_expected_string(buff.offset, error);
	}
	str_start = buff.data + buff.offset;
	buff = get_char(buff, &c, error);
	while (!*error) {
		if (c == terminator) {
			break;
		}
		buff = get_char(buff, &c, error);
	}
	if (!*error) {
		value->type = RDT_STRING;
		value->string.len = (buff.data + buff.offset) - str_start - 1;
		value->string.buff = calloc(
		                value->string.len + 1,
		                sizeof(char)
		        );
		if (!value->string.buff) {
			raise_enomem(error);
		} else {
			memcpy(
			        value->string.buff,
			        str_start,
			        value->string.len
			);
		}
	}
	return buff;
}

static struct buffer parse_integer(
        struct buffer buff,
        struct rmsgpack_dom_value * value,
        const char ** error
) {
	value->type = RDT_INT;
	if (sscanf(buff.data + buff.offset, "%ld", &value->int_) == 0) {
		raise_expected_number(buff.offset, error);
	} else {
		while (isdigit(buff.data[buff.offset])) {
			buff.offset++;
		}
	}
	return buff;
}

static struct buffer parse_value(
        struct buffer buff,
        struct rmsgpack_dom_value * value,
        const char ** error
) {
	buff = chomp(buff);
	if (peek(buff, "nil")) {
		buff.offset += strlen("nil");
		value->type = RDT_NULL;
	} else if (peek(buff, "true")) {
		buff.offset += strlen("true");
		value->type = RDT_BOOL;
		value->bool_ = 1;
	} else if (peek(buff, "false")) {
		buff.offset += strlen("false");
		value->type = RDT_BOOL;
		value->bool_ = 0;
		//} else if (peek(buff, "[")) {
		//} else if (peek(buff, "b\"") || peek(buff, "b'")) {
	} else if (peek(buff, "\"") || peek(buff, "'")) {
		buff = parse_string(buff, value, error);
	} else if (isdigit(buff.data[buff.offset])) {
		buff = parse_integer(buff, value, error);
	}
	return buff;
}

static struct buffer get_ident(
        struct buffer buff,
        const char ** ident,
        size_t * len,
        const char ** error
) {
	char c;
	if (is_eot(buff)) {
		raise_unexpected_eof(buff.offset, error);
		return buff;
	}

	*ident = buff.data + buff.offset;
	*len = 0;
	peek_char(buff, &c, error);
	if (*error) {
		goto clean;
	}
	if (!isalpha(c)) {
		return buff;
	}
	buff.offset++;
	*len = *len + 1;
	peek_char(buff, &c, error);
	while (!*error) {
		if (!(isalpha(c) || isdigit(c) || c == '_')) {
			break;
		}
		buff.offset++;
		*len = *len + 1;
		peek_char(buff, &c, error);
	}
clean:
	return buff;
}

static struct buffer parse_method_call(
        struct buffer buff,
        struct invocation * invocation,
        const char ** error
) {
	const char * func_name;
	size_t func_name_len;
	struct argument args[MAX_ARGS];
	unsigned argi = 0;
	unsigned i;
	struct registered_func * rf = registered_functions;

	invocation->func = NULL;

	buff = get_ident(buff, &func_name, &func_name_len, error);
	if (*error) {
		goto clean;
	}

	buff = chomp(buff);
	buff = expect_char(buff, '(', error);
	if (*error) {
		goto clean;
	}

	while (rf->name) {
		if (strncmp(rf->name, func_name, func_name_len) == 0) {
			invocation->func = rf->func;
			break;
		}
		rf++;
	}

	if (!invocation->func) {
		raise_unknown_function(
		        buff.offset,
		        func_name,
		        func_name_len,
		        error
		);
		goto clean;
	}

	buff = chomp(buff);
	while (!peek(buff, ")")) {
		if (argi >= MAX_ARGS) {
			raise_too_many_arguments(error);
			goto clean;
		}
		buff = parse_argument(buff, &args[argi], error);
		if (*error) {
			goto clean;
		}
		argi++;
		buff = chomp(buff);
		buff = expect_char(buff, ',', error);
		if (*error) {
			*error = NULL;
			break;
		}
		buff = chomp(buff);
	}
	buff = expect_char(buff, ')', error);
	if (*error) {
		goto clean;
	}

	invocation->argc = argi;
	invocation->argv = malloc(sizeof(struct argument) * argi);
	if (!invocation->argv) {
		raise_enomem(error);
		goto clean;
	}
	memcpy(invocation->argv, args, sizeof(struct argument) * argi);

	goto success;
clean:
	for (i = 0; i < argi; i++) {
		argument_free(&args[i]);
	}
success:
	return buff;
}

static struct buffer parse_argument(
        struct buffer buff,
        struct argument * arg,
        const char ** error
) {
	buff = chomp(buff);
	if (
	        isalpha(buff.data[buff.offset])
	        && !(
	                peek(buff, "nil")
	                || peek(buff, "true")
	                || peek(buff, "false")
	        )
	) {
		arg->type = AT_FUNCTION;
		buff = parse_method_call(buff, &arg->invocation, error);
	} else if (peek(buff, "{")) {
		arg->type = AT_FUNCTION;
		buff = parse_table(buff, &arg->invocation, error);
	} else {
		arg->type = AT_VALUE;
		buff = parse_value(buff, &arg->value, error);
	}
	return buff;
}

static struct buffer parse_table(
        struct buffer buff,
        struct invocation * invocation,
        const char ** error
) {
	struct argument args[MAX_ARGS];
	unsigned argi = 0;
	unsigned i;

	const char * ident_name;
	size_t ident_len;

	memset(args, 0, sizeof(struct argument) * MAX_ARGS);
	buff = chomp(buff);
	buff = expect_char(buff, '{', error);
	if (*error) {
		goto clean;
	}

	buff = chomp(buff);
	while (!peek(buff, "}")) {
		if (argi >= MAX_ARGS) {
			raise_too_many_arguments(error);
			goto clean;
		}
		if (isalpha(buff.data[buff.offset])) {
			buff = get_ident(buff, &ident_name, &ident_len, error);
			if (!*error) {
				args[argi].value.type = RDT_STRING;
				args[argi].value.string.len = ident_len;
				args[argi].value.string.buff = calloc(
				                ident_len + 1,
				                sizeof(char)
				        );
				if (!args[argi].value.string.buff) {
					goto clean;
				}
				strncpy(
				        args[argi].value.string.buff,
				        ident_name,
				        ident_len
				);
			}
		} else {
			buff = parse_string(buff, &args[argi].value, error);
		}
		if (*error) {
			goto clean;
		}
		args[argi].type = AT_VALUE;
		buff = chomp(buff);
		argi++;
		buff = expect_char(buff, ':', error);
		if (*error) {
			goto clean;
		}
		buff = chomp(buff);
		if (argi >= MAX_ARGS) {
			raise_too_many_arguments(error);
			goto clean;
		}
		buff = parse_argument(buff, &args[argi], error);
		if (*error) {
			goto clean;
		}
		argi++;
		buff = chomp(buff);
		buff = expect_char(buff, ',', error);
		if (*error) {
			*error = NULL;
			break;
		}
		buff = chomp(buff);
	}
	buff = expect_char(buff, '}', error);
	if (*error) {
		goto clean;
	}

	invocation->func = all_map;
	invocation->argc = argi;
	invocation->argv = malloc(sizeof(struct argument) * argi);
	if (!invocation->argv) {
		raise_enomem(error);
		goto clean;
	}
	memcpy(invocation->argv, args, sizeof(struct argument) * argi);

	goto success;
clean:
	for (i = 0; i < argi; i++) {
		argument_free(&args[i]);
	}
success:
	return buff;
}


void rarchdb_query_free(rarchdb_query * q) {
	struct query * real_q = q;
	unsigned i;

	real_q->ref_count--;
	if (real_q->ref_count > 0) {
		return;
	}

	for (i = 0; i < real_q->root.argc; i++) {
		argument_free(&real_q->root.argv[i]);
	}
}

rarchdb_query * rarchdb_query_compile(
        struct rarchdb * db,
        const char * query,
        size_t buff_len,
        const char ** error
) {
	struct buffer buff;
	struct query * q;
	q = malloc(sizeof(struct query));
	if (!q) {
		goto clean;
	}
	memset(q, 0, sizeof(struct query));
	q->ref_count = 1;
	buff.data = query;
	buff.len = buff_len;
	buff.offset = 0;

	*error = NULL;

	buff = chomp(buff);
	if (peek(buff, "{")) {
		buff = parse_table(buff, &q->root, error);
		if (*error) {
			goto clean;
		}
	} else if (isalpha(buff.data[buff.offset])) {
		buff = parse_method_call(buff, &q->root, error);
	}

	buff = expect_eof(buff, error);
	if (*error) {
		goto clean;
	}

	if (q->root.func == NULL) {
		raise_unexpected_eof(buff.offset, error);
		return NULL;
	}
	goto success;
clean:
	if (q) {
		rarchdb_query_free(q);
	}
success:
	return q;
}

void rarchdb_query_inc_ref(rarchdb_query * q) {
	struct query * rq = q;
	rq->ref_count += 1;
}

int rarchdb_query_filter(
        rarchdb_query * q,
        struct rmsgpack_dom_value * v
) {
	struct invocation inv = ((struct query *)q)->root;
	struct rmsgpack_dom_value res = inv.func(*v, inv.argc, inv.argv);
	return (res.type == RDT_BOOL && res.bool_);
}
