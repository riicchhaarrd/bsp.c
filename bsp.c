#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include "type.h"
#include "lump.h"

#include "stream_file.h"

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

typedef struct
{
	void *data;
	size_t count;
} LumpData;

static LumpData lumpdata[LUMP_MAX];

s64 filelen;

void info(dheader_t *hdr, int type)
{
	lump_t *l = &hdr->lumps[type];
	char amount[256] = { 0 };
	if(lumpsizes[type] == 0)
		snprintf(amount, sizeof(amount), "     ?");
	else if(lumpsizes[type] == 1)
	{
		snprintf(amount, sizeof(amount), "      ");
	}
	else if(lumpsizes[type] > 1)
	{
		snprintf(amount, sizeof(amount), "%6d", l->filelen / lumpsizes[type]);
	}
	printf("%s %-19s %6d B\t%2d KB %5.1f%%\n",
		amount,
		lumpnames[type],
		l->filelen,
		(int)ceilf((float)l->filelen / 1000.f),
		(float)l->filelen / (float)filelen * 100.f);
}

static void test(const char *type, size_t a, size_t b)
{
	if(a != b)
	{
		fprintf(stderr, "%d != %d, sizeof(%s) = %d\n", a, b, type, a);
		exit(1);
	}
}
#define TEST(a, b) test(#a, sizeof(a), b)

// gcc -I../../third_party/linmath.h bsp.c -lm
int main(int argc, char **argv)
{

	TEST(dmodel_t, 48);

	Stream s = {0};
	assert(0 == stream_open_file(&s, argv[1], "rb"));

	s.seek(&s, 0, SEEK_END);
	filelen = s.tell(&s);
	s.seek(&s, 0, SEEK_SET);

	dheader_t hdr = { 0 };
	stream_read(s, hdr);
	
	if(memcmp(hdr.ident, "IBSP", 4))
	{
		fprintf(stderr, "Magic mismatch");
		exit(1);
	}
	if(hdr.version != 4)
	{
		fprintf(stderr, "Version mismatch");
		exit(1);
	}
	printf("bsp.c v0.1 (c) 2024\n");
	printf("---------------------\n");
	printf("%s: %d\n", argv[1], filelen);
	
	info(&hdr, LUMP_MODELS);
	info(&hdr, LUMP_MATERIALS);
	info(&hdr, LUMP_BRUSHES);
	info(&hdr, LUMP_BRUSHSIDES);
	info(&hdr, LUMP_PLANES);
	info(&hdr, LUMP_ENTITIES);
	printf("\n");
	info(&hdr, LUMP_NODES);
	info(&hdr, LUMP_LEAFS);
	info(&hdr, LUMP_LEAFBRUSHES);
	info(&hdr, LUMP_LEAFSURFACES);
	info(&hdr, LUMP_COLLISIONVERTS);
	info(&hdr, LUMP_COLLISIONEDGES);
	info(&hdr, LUMP_COLLISIONTRIS);
	info(&hdr, LUMP_COLLISIONBORDERS);
	info(&hdr, LUMP_COLLISIONAABBS);
	info(&hdr, LUMP_DRAWVERTS);
	info(&hdr, LUMP_DRAWINDICES);
	info(&hdr, LUMP_TRIANGLES);
	
	info(&hdr, LUMP_OBSOLETE_1);
	info(&hdr, LUMP_OBSOLETE_2);
	info(&hdr, LUMP_OBSOLETE_3);
	info(&hdr, LUMP_OBSOLETE_4);
	info(&hdr, LUMP_OBSOLETE_5);

	info(&hdr, LUMP_LIGHTBYTES);
	info(&hdr, LUMP_LIGHTGRIDENTRIES);
	info(&hdr, LUMP_LIGHTGRIDCOLORS);
	// Not sure if it's stored as a lump or just parsed from entdata with classname "light"
	// TODO: parse entdata
	printf("     0 lights                   0 B      0 KB   0.0%\n");
	info(&hdr, LUMP_VISIBILITY);
	info(&hdr, LUMP_PORTALVERTS);
	info(&hdr, LUMP_OCCLUDERS);
	info(&hdr, LUMP_OCCLUDERPLANES);
	info(&hdr, LUMP_OCCLUDEREDGES);
	info(&hdr, LUMP_OCCLUDERINDICES);
	info(&hdr, LUMP_AABBTREES);
	info(&hdr, LUMP_CELLS);
	info(&hdr, LUMP_PORTALS);
	info(&hdr, LUMP_CULLGROUPS);
	info(&hdr, LUMP_CULLGROUPINDICES);
	printf("\n");
	info(&hdr, LUMP_PATHCONNECTIONS);
	printf("---------------------\n");
	return 0;
}
