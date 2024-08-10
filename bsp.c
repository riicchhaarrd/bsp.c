#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];

#include "stream_file.h"


typedef float f32;
typedef double f64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

#pragma pack(push, 1)

typedef struct lump_s
{
	u32 filelen;
	u32 fileofs;
} lump_t;

enum LumpType
{
  LUMP_MATERIALS = 0,
  LUMP_LIGHTBYTES = 1,
  LUMP_LIGHTGRIDENTRIES = 2,
  LUMP_LIGHTGRIDCOLORS = 3,
  LUMP_PLANES = 4,
  LUMP_BRUSHSIDES = 5,
  LUMP_BRUSHES = 6,
  LUMP_TRIANGLES = 7,
  LUMP_DRAWVERTS = 8,
  LUMP_DRAWINDICES = 9,
  LUMP_CULLGROUPS = 10,
  LUMP_CULLGROUPINDICES = 11,
  
  LUMP_OBSOLETE_1 = 12,
  LUMP_OBSOLETE_2 = 13,
  LUMP_OBSOLETE_3 = 14,
  LUMP_OBSOLETE_4 = 15,
  LUMP_OBSOLETE_5 = 16,

  LUMP_PORTALVERTS = 17,
  LUMP_OCCLUDERS = 18,
  LUMP_OCCLUDERPLANES = 19,
  LUMP_OCCLUDEREDGES = 20,
  LUMP_OCCLUDERINDICES = 21,
  LUMP_AABBTREES = 22,
  LUMP_CELLS = 23,
  LUMP_PORTALS = 24,
  LUMP_NODES = 25,
  LUMP_LEAFS = 26,
  LUMP_LEAFBRUSHES = 27,
  LUMP_LEAFSURFACES = 28,
  LUMP_COLLISIONVERTS = 29,
  LUMP_COLLISIONEDGES = 30,
  LUMP_COLLISIONTRIS = 31,
  LUMP_COLLISIONBORDERS = 32,
  LUMP_COLLISIONPARTITIONS = 33,
  LUMP_COLLISIONAABBS = 34,
  LUMP_MODELS = 35,
  LUMP_VISIBILITY = 36,
  LUMP_ENTITIES = 37,
  LUMP_PATHCONNECTIONS = 38,
  LUMP_MAX = 39
};

static const char *lumpnames[] = {
	[LUMP_MATERIALS] = "materials",
	[LUMP_LIGHTBYTES] = "lightmaps",
	[LUMP_LIGHTGRIDENTRIES] = "light grid hash",
	[LUMP_LIGHTGRIDCOLORS] = "light grid values",
	[LUMP_PLANES] = "planes",
	[LUMP_BRUSHSIDES] = "brushsides",
	[LUMP_BRUSHES] = "brushes",
	[LUMP_TRIANGLES] = "trianglesoups",
	[LUMP_DRAWVERTS] = "drawverts",
	[LUMP_DRAWINDICES] = "drawindexes",
	[LUMP_CULLGROUPS] = "cullgroups",
	[LUMP_CULLGROUPINDICES] = "cullgroupindexes",
	[LUMP_OBSOLETE_1] = "shadowverts",
	[LUMP_OBSOLETE_2] = "shadowindices",
	[LUMP_OBSOLETE_3] = "shadowclusters",
	[LUMP_OBSOLETE_4] = "shadowaabbtrees",
	[LUMP_OBSOLETE_5] = "shadowsources",
	[LUMP_PORTALVERTS] = "portalverts",
	[LUMP_OCCLUDERS] = "occluders",
	[LUMP_OCCLUDERPLANES] = "occluderplanes",
	[LUMP_OCCLUDEREDGES] = "occluderedges",
	[LUMP_OCCLUDERINDICES] = "occluderindexes",
	[LUMP_AABBTREES] = "aabbtrees",
	[LUMP_CELLS] = "cells",
	[LUMP_PORTALS] = "portals",
	[LUMP_NODES] = "nodes",
	[LUMP_LEAFS] = "leafs",
	[LUMP_LEAFBRUSHES] = "leafbrushes",
	[LUMP_LEAFSURFACES] = "leafsurfaces",
	[LUMP_COLLISIONVERTS] = "collisionverts",
	[LUMP_COLLISIONEDGES] = "collisionedges",
	[LUMP_COLLISIONTRIS] = "collisiontris",
	[LUMP_COLLISIONBORDERS] = "collisionborders",
	[LUMP_COLLISIONPARTITIONS] = "collisionparts",
	[LUMP_COLLISIONAABBS] = "collisionaabbs",
	[LUMP_MODELS] = "models",
	[LUMP_VISIBILITY] = "visibility",
	[LUMP_ENTITIES] = "entdata",
	[LUMP_PATHCONNECTIONS] = "paths",
	NULL
};

typedef struct dheader_s
{
	u8 ident[4];
	u32 version;
	lump_t lumps[LUMP_MAX];
} dheader_t;

typedef struct
{
	char material[64];
	u32 surfaceFlags;
	u32 contentFlags;
} dmaterial_t;

typedef struct
{
	u16 materialIndex;
	u16 lightmapIndex;
	u32 firstVertex;
	u16 vertexCount;
	u16 indexCount;
	u32 firstIndex;
} DiskTriangleSoup;

typedef struct
{
	vec3 xyz;
	vec3 normal;
	u32 color;
	vec2 texCoord;
	vec2 lmapCoord;
	vec3 tangent;
	vec3 binormal;
} DiskGfxVertex;

typedef struct
{
	u8 r, g, b, a;
} RGBA;

typedef struct
{
	RGBA r[512 * 512];
	RGBA g[512 * 512];
	RGBA b[512 * 512];
	u8 shadowMap[1024 * 1024];
} DiskGfxLightmap;

typedef struct
{
	s32 planeNum; // Plane index.
	s32 children[2]; // Children indices. Negative numbers are leaf indices: -(leaf+1). 
	s32 mins[3]; // Integer bounding box min coord. 
	s32 maxs[3]; // Integer bounding box max coord. 
} dnode_t;

typedef struct
{
    vec3 normal;
    f32 dist;
} DiskPlane;

typedef struct
{
	u16 numSides;
	u16 materialNum;
} DiskBrush;

typedef struct
{
	s32 plane;
	s32 materialNum;
} cbrushside_t;

typedef struct
{
	vec3 xyz;
} CollisionVertex;

typedef struct
{
	vec3 origin;
	vec3 axis[3];
} CollisionEdge;

typedef struct
{
	vec3 distEq;
	s32 zBase;
	s32 zSlope;
	s32 start;
	s32 length;
} CollisionBorder;

typedef struct
{
	u8 triCount;
	u8 borderCount;
	u8 pad0[2];
	s32 tris;
	s32 borders;
} CollisionPartition;

typedef struct
{
	vec3 mins;
	vec3 maxs;
	u32 firstTriangle;
	u32 numTriangles;
	u32 firstSurface;
	u32 numSurfaces;
	u32 firstBrush;
	u32 numBrushes;
} dmodel_t;

typedef union
{
	s32 firstChildIndex;
	s32 partitionIndex;
} CollisionAabbTreeIndex;

typedef struct
{
    vec3 origin;
    vec3 halfSize;
	s16 materialIndex;
	s16 childCount;
	CollisionAabbTreeIndex u;
} CollisionAabbTree;

typedef struct
{
	vec4 plane;
	vec4 svec;
	vec4 tvec;
	s32 verts[3];
	s32 edges[3];
} CollisionTriangle;

/*
The leafs lump stores the leaves of the map's BSP tree.
Each leaf is a convex region that contains, among other things,
a cluster index (for determining the other leafs potentially visible from within the leaf),
a list of faces (for rendering), and a list of brushes (for collision detection).
There are a total of length / sizeof(leaf) records in the lump, where length is the size of the lump itself, as specified in the lump directory. 
*/
typedef struct
{
	s32 cluster; // If cluster is negative, the leaf is outside the map or otherwise invalid.
	s32 area;
	s32 firstLeafSurface;
	u32 numLeafSurfaces;
	s32 firstLeafBrush;
	u32 numLeafBrushes;
	s32 cellNum;
	s32 firstLightIndex;
	u32 numLights;
} dleaf_t;

typedef struct
{
	s32 brush;
} dleafbrush_t;

typedef struct
{
	s32 face;
} dleafface_t;

typedef struct
{
	u16 vertex[3];
} DiskGfxTriangle;

typedef struct
{
	vec3 mins, maxs;
	s32 firstSurface;
	s32 surfaceCount;
} DiskGfxCullGroup;

typedef struct
{
	vec3 mins, maxs;
	s32 aabbTreeIndex;
	s32 firstPortal;
	s32 portalCount;
	s32 firstCullGroup;
	s32 cullGroupCount;
	s32 firstOccluder;
	s32 occluderCount;
} DiskGfxCell;

typedef struct
{
	s32 firstSurface;
	s32 surfaceCount;
	s32 childCount;
} DiskGfxAabbTree;
#pragma pack(pop)

static const size_t lumpsizes[] = {
	[LUMP_MATERIALS] = sizeof(dmaterial_t),
	[LUMP_LIGHTBYTES] = sizeof(DiskGfxLightmap),
	[LUMP_LIGHTGRIDENTRIES] = 0,
	[LUMP_LIGHTGRIDCOLORS] = 0,
	[LUMP_PLANES] = sizeof(DiskPlane),
	[LUMP_BRUSHSIDES] = sizeof(cbrushside_t),
	[LUMP_BRUSHES] = sizeof(DiskBrush),
	[LUMP_TRIANGLES] = sizeof(DiskTriangleSoup),
	[LUMP_DRAWVERTS] = sizeof(DiskGfxVertex),
	[LUMP_DRAWINDICES] = sizeof(u16),
	[LUMP_CULLGROUPS] = sizeof(DiskGfxCullGroup),
	[LUMP_CULLGROUPINDICES] = 0,
	[LUMP_OBSOLETE_1] = 0,
	[LUMP_OBSOLETE_2] = 0,
	[LUMP_OBSOLETE_3] = 0,
	[LUMP_OBSOLETE_4] = 0,
	[LUMP_OBSOLETE_5] = 0,
	[LUMP_PORTALVERTS] = 0,
	[LUMP_OCCLUDERS] = 0,
	[LUMP_OCCLUDERPLANES] = 0,
	[LUMP_OCCLUDEREDGES] = 0,
	[LUMP_OCCLUDERINDICES] = 0,
	[LUMP_AABBTREES] = sizeof(DiskGfxAabbTree),
	[LUMP_CELLS] = sizeof(DiskGfxCell),
	[LUMP_PORTALS] = 0,
	[LUMP_NODES] = sizeof(dnode_t),
	[LUMP_LEAFS] = sizeof(dleaf_t),
	[LUMP_LEAFBRUSHES] = sizeof(dleafbrush_t),
	[LUMP_LEAFSURFACES] = sizeof(dleafface_t),
	[LUMP_COLLISIONVERTS] = sizeof(CollisionVertex),
	[LUMP_COLLISIONEDGES] = sizeof(CollisionEdge),
	[LUMP_COLLISIONTRIS] = sizeof(CollisionTriangle),
	[LUMP_COLLISIONBORDERS] = sizeof(CollisionBorder),
	[LUMP_COLLISIONPARTITIONS] = sizeof(CollisionPartition),
	[LUMP_COLLISIONAABBS] = sizeof(CollisionAabbTree),
	[LUMP_MODELS] = sizeof(dmodel_t),
	[LUMP_VISIBILITY] = 1,
	[LUMP_ENTITIES] = 1,
	[LUMP_PATHCONNECTIONS] = 0
};

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
