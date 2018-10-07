#ifndef __JCP_H_
#define __JCP_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JCP_UNDEFINED = 0,
	JCP_MAP = 1,
	JCP_ARRAY = 2,
	JCP_STRING = 3,
	JCP_PRIMITIVE = 4,
	JCP_CBOR = 5
} jcptype_t;

enum jcperr {
	/* Not enough tokens were provided */
	JCP_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JCP_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JCP_ERROR_PART = -3
};

/**
 * JSON token description.
 * type		type (object, array, string etc.)
 * start	start position in JSON data string
 * end		end position in JSON data string
 */
typedef struct {
	jcptype_t type;
	int start;
	int end;
	int size;
#ifdef JCP_PARENT_LINKS
	int parent;
#endif
} jcptok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	unsigned int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jcp_parser_t;

/**
 * Create JSON parser over an array of tokens
 */
void jcp_init(jcp_parser_t *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 */
int jcp_parse(jcp_parser_t *parser, const char *js, size_t len,
		jcptok_t *tokens, unsigned int num_tokens);

#ifdef __cplusplus
}
#endif

#endif /* __JCP_H_ */
