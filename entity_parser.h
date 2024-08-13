#pragma once

enum
{
	PARSE_NODE_DEPTH_ROOT,
	PARSE_NODE_DEPTH_ENTITY,
	PARSE_NODE_DEPTH_BRUSH
};

typedef struct KeyValuePair_s
{
    // struct KeyValuePair_s *next;
	char *key;
	char *value;
} KeyValuePair;

typedef struct Entity_s
{
	KeyValuePair *keyvalues;
} Entity;