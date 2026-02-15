#pragma once
#include "lump.h"
#include <string.h>

#define IBSP_VERSION_COD1 59  // CoD1 & CoD:UO
#define IBSP_VERSION_COD2  4  // CoD2

#define LUMP_MAX_V59 33

typedef struct {
    u32 version;
    const char *name;
    u32 num_lumps;
    size_t lump_sizes[LUMP_MAX];
    void (*remap_lumps)(dheader_t *hdr);
} BspFormat;

/*
 * V59 (CoD1/UO) lump layout -> V4 (CoD2) canonical indices.
 *
 * V59 has 33 lump slots. Missing vs V4: LightGridColors, 5 shadow lumps.
 *
 * V59[23] = CollisionEdges   -> V4[30]  (u32, 4 bytes)
 * V59[24] = CollisionAABBs   -> V4[34]  (DiskCollisionLeafV59, 16 bytes)
 * V59[25] = CollisionVerts   -> V4[29]  (vec3, 12 bytes)
 * V59[26] = CollisionTris    -> V4[31]  (u16, 2 bytes)
 */
static void remap_v59_lumps(dheader_t *hdr)
{
	lump_t original[LUMP_MAX];
	memcpy(original, hdr->lumps, sizeof(original));
	memset(hdr->lumps, 0, sizeof(hdr->lumps));

	hdr->lumps[LUMP_MATERIALS]          = original[0];
	hdr->lumps[LUMP_LIGHTBYTES]         = original[1];
	hdr->lumps[LUMP_PLANES]             = original[2];
	hdr->lumps[LUMP_BRUSHSIDES]         = original[3];
	hdr->lumps[LUMP_BRUSHES]            = original[4];
	/* V59[5] = empty/Fogs, skipped */
	hdr->lumps[LUMP_TRIANGLES]          = original[6];
	hdr->lumps[LUMP_DRAWVERTS]          = original[7];
	hdr->lumps[LUMP_DRAWINDICES]        = original[8];
	hdr->lumps[LUMP_CULLGROUPS]         = original[9];
	hdr->lumps[LUMP_CULLGROUPINDICES]   = original[10];
	hdr->lumps[LUMP_PORTALVERTS]        = original[11];
	hdr->lumps[LUMP_OCCLUDERS]          = original[12];
	hdr->lumps[LUMP_OCCLUDERPLANES]     = original[13];
	hdr->lumps[LUMP_OCCLUDEREDGES]      = original[14];
	hdr->lumps[LUMP_OCCLUDERINDICES]    = original[15];
	hdr->lumps[LUMP_AABBTREES]          = original[16];
	hdr->lumps[LUMP_CELLS]              = original[17];
	hdr->lumps[LUMP_PORTALS]            = original[18];
	hdr->lumps[LUMP_NODES]              = original[19];
	hdr->lumps[LUMP_LEAFS]              = original[20];
	hdr->lumps[LUMP_LEAFBRUSHES]        = original[21];
	hdr->lumps[LUMP_LEAFSURFACES]       = original[22];
	hdr->lumps[LUMP_COLLISIONEDGES]     = original[23];
	hdr->lumps[LUMP_COLLISIONAABBS]     = original[24];
	hdr->lumps[LUMP_COLLISIONVERTS]     = original[25];
	hdr->lumps[LUMP_COLLISIONTRIS]      = original[26];
	hdr->lumps[LUMP_MODELS]             = original[27];
	hdr->lumps[LUMP_VISIBILITY]         = original[28];
	hdr->lumps[LUMP_ENTITIES]           = original[29];
	hdr->lumps[LUMP_LIGHTGRIDENTRIES]   = original[30];
	hdr->lumps[LUMP_PATHCONNECTIONS]    = original[31];
}

static const BspFormat bsp_format_cod2 = {
	.version = IBSP_VERSION_COD2,
	.name = "CoD2",
	.num_lumps = LUMP_MAX,
	.lump_sizes = {
		[LUMP_MATERIALS]           = sizeof(dmaterial_t),
		[LUMP_LIGHTBYTES]          = sizeof(DiskGfxLightmap),
		[LUMP_LIGHTGRIDENTRIES]    = 0,
		[LUMP_LIGHTGRIDCOLORS]     = 0,
		[LUMP_PLANES]              = sizeof(DiskPlane),
		[LUMP_BRUSHSIDES]          = sizeof(cbrushside_t),
		[LUMP_BRUSHES]             = sizeof(DiskBrush),
		[LUMP_TRIANGLES]           = sizeof(DiskTriangleSoup),
		[LUMP_DRAWVERTS]           = sizeof(DiskGfxVertex),
		[LUMP_DRAWINDICES]         = sizeof(u16),
		[LUMP_CULLGROUPS]          = sizeof(DiskGfxCullGroup),
		[LUMP_CULLGROUPINDICES]    = 0,
		[LUMP_OBSOLETE_1]          = 0,
		[LUMP_OBSOLETE_2]          = 0,
		[LUMP_OBSOLETE_3]          = 0,
		[LUMP_OBSOLETE_4]          = 0,
		[LUMP_OBSOLETE_5]          = 0,
		[LUMP_PORTALVERTS]         = sizeof(DiskGfxPortalVertex),
		[LUMP_OCCLUDERS]           = 0,
		[LUMP_OCCLUDERPLANES]      = 0,
		[LUMP_OCCLUDEREDGES]       = 0,
		[LUMP_OCCLUDERINDICES]     = 0,
		[LUMP_AABBTREES]           = sizeof(DiskGfxAabbTree),
		[LUMP_CELLS]               = sizeof(DiskGfxCell),
		[LUMP_PORTALS]             = sizeof(DiskGfxPortal),
		[LUMP_NODES]               = sizeof(dnode_t),
		[LUMP_LEAFS]               = sizeof(dleaf_t),
		[LUMP_LEAFBRUSHES]         = sizeof(dleafbrush_t),
		[LUMP_LEAFSURFACES]        = sizeof(dleafface_t),
		[LUMP_COLLISIONVERTS]      = sizeof(DiskCollisionVertex),
		[LUMP_COLLISIONEDGES]      = sizeof(DiskCollisionEdge),
		[LUMP_COLLISIONTRIS]       = sizeof(DiskCollisionTriangle),
		[LUMP_COLLISIONBORDERS]    = sizeof(DiskCollisionBorder),
		[LUMP_COLLISIONPARTITIONS] = sizeof(DiskCollisionPartition),
		[LUMP_COLLISIONAABBS]      = sizeof(DiskCollisionAabbTree),
		[LUMP_MODELS]              = sizeof(dmodel_t),
		[LUMP_VISIBILITY]          = 1,
		[LUMP_ENTITIES]            = 1,
		[LUMP_PATHCONNECTIONS]     = 0
	},
	.remap_lumps = NULL
};

static const BspFormat bsp_format_cod1 = {
	.version = IBSP_VERSION_COD1,
	.name = "CoD1/UO",
	.num_lumps = LUMP_MAX_V59,
	.lump_sizes = {
		[LUMP_MATERIALS]           = sizeof(dmaterial_t),
		[LUMP_LIGHTBYTES]          = sizeof(DiskGfxLightmap),
		[LUMP_LIGHTGRIDENTRIES]    = 0,
		[LUMP_LIGHTGRIDCOLORS]     = 0,
		[LUMP_PLANES]              = sizeof(DiskPlane),
		[LUMP_BRUSHSIDES]          = sizeof(cbrushside_t),
		[LUMP_BRUSHES]             = sizeof(DiskBrush),
		[LUMP_TRIANGLES]           = sizeof(DiskTriangleSoup),
		[LUMP_DRAWVERTS]           = sizeof(DiskGfxVertexV59),
		[LUMP_DRAWINDICES]         = sizeof(u16),
		[LUMP_CULLGROUPS]          = sizeof(DiskGfxCullGroup),
		[LUMP_CULLGROUPINDICES]    = 0,
		[LUMP_OBSOLETE_1]          = 0,
		[LUMP_OBSOLETE_2]          = 0,
		[LUMP_OBSOLETE_3]          = 0,
		[LUMP_OBSOLETE_4]          = 0,
		[LUMP_OBSOLETE_5]          = 0,
		[LUMP_PORTALVERTS]         = sizeof(DiskGfxPortalVertex),
		[LUMP_OCCLUDERS]           = 0,
		[LUMP_OCCLUDERPLANES]      = 0,
		[LUMP_OCCLUDEREDGES]       = 0,
		[LUMP_OCCLUDERINDICES]     = 0,
		[LUMP_AABBTREES]           = sizeof(DiskGfxAabbTree),
		[LUMP_CELLS]               = sizeof(DiskGfxCell),
		[LUMP_PORTALS]             = sizeof(DiskGfxPortal),
		[LUMP_NODES]               = sizeof(dnode_t),
		[LUMP_LEAFS]               = 0,
		[LUMP_LEAFBRUSHES]         = sizeof(dleafbrush_t),
		[LUMP_LEAFSURFACES]        = sizeof(dleafface_t),
		[LUMP_COLLISIONVERTS]      = sizeof(DiskCollisionVertexV59),
		[LUMP_COLLISIONEDGES]      = sizeof(u32),
		[LUMP_COLLISIONTRIS]       = sizeof(u16),
		[LUMP_COLLISIONBORDERS]    = 0,
		[LUMP_COLLISIONPARTITIONS] = 0,
		[LUMP_COLLISIONAABBS]      = sizeof(DiskCollisionLeafV59),
		[LUMP_MODELS]              = sizeof(dmodel_t),
		[LUMP_VISIBILITY]          = 1,
		[LUMP_ENTITIES]            = 1,
		[LUMP_PATHCONNECTIONS]     = 0
	},
	.remap_lumps = remap_v59_lumps
};

static const BspFormat *bsp_format_for_version(u32 version)
{
	switch(version)
	{
		case IBSP_VERSION_COD2: return &bsp_format_cod2;
		case IBSP_VERSION_COD1: return &bsp_format_cod1;
		default: return NULL;
	}
}
