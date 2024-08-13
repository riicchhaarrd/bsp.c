#include "entity_parser.h"
#include "stream.h"
#include "stream_buffer.h"
#include "lump.h"
#include <stdio.h>
#include <growable-buf/buf.h>
#include <assert.h>

extern LumpData lumpdata[LUMP_MAX];

Entity *parse_entities()
{
	Stream s = {0};
	StreamBuffer sb = {0};
	init_stream_from_buffer(&s, &sb, lumpdata[LUMP_ENTITIES].data, lumpdata[LUMP_ENTITIES].count);
	
    char line[2048];
	unsigned int node_depth = PARSE_NODE_DEPTH_ROOT;
	Entity *entities = NULL;
    Entity *entity = NULL;

	while(!stream_read_line(&s, line, sizeof(line)))
	{
		size_t offsz = 0;
		while(line[offsz] && line[offsz] == ' ')
			offsz++;

		switch(line[offsz])
		{
			case '{':
				++node_depth;
                assert(node_depth == PARSE_NODE_DEPTH_ENTITY);
                buf_push(entities, (Entity) { 0 });
                entity = &entities[buf_size(entities) - 1];
			break;
			case '}':
				assert(node_depth != PARSE_NODE_DEPTH_ROOT);
				--node_depth;
			break;
			case '(':
			{
                fprintf(stderr, "No support for parsing brushes.\n");
                exit(1);
			} break;
			case '"':
			{
				if(node_depth == PARSE_NODE_DEPTH_ENTITY)
				{
                    char key[512];
                    char value[512];
					sscanf(line, "\"%511[^\"]\" \"%511[^\"]\"", key, value);
					assert(entity);
					buf_push(entity->keyvalues, (KeyValuePair) { 0 });
					KeyValuePair *kvp = &entity->keyvalues[buf_size(entity->keyvalues) - 1];
                    // Leaking memory, but fine for this particular instance.
                    kvp->key = strdup(key);
                    kvp->value = strdup(value);
				}
			} break;
		}
	}
    return entities;
}