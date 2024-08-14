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
#include "entity_parser.h"
#include <growable-buf/buf.h>

#include "stream_file.h"
#include "stream_buffer.h"

#include <linmath.h/linmath.h>

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

typedef struct
{
	bool print_info;
	bool export_to_map;
	const char *input_file;
	const char *format;
	const char *export_file;
	bool try_fix_portals;
	bool exclude_patches;
} ProgramOptions;

LumpData lumpdata[LUMP_MAX];

s64 filelen;

Entity *entities;

void info(dheader_t *hdr, int type, int *count)
{
	lump_t *l = &hdr->lumps[type];
	char amount[256] = { 0 };
	if(count)
	{
		snprintf(amount, sizeof(amount), "%6d", *count);
	} else
	{
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

void planes_from_aabb(vec3 mins, vec3 maxs, DiskPlane planes[6])
{
	planes[0].normal[0] = -1.0f;
	planes[0].normal[1] = 0.0f;
	planes[0].normal[2] = 0.0f;
	planes[0].dist = -mins[0];

	planes[1].normal[0] = 1.0f;
	planes[1].normal[1] = 0.0f;
	planes[1].normal[2] = 0.0f;
	planes[1].dist = maxs[0];

	planes[2].normal[0] = 0.0f;
	planes[2].normal[1] = -1.0f;
	planes[2].normal[2] = 0.0f;
	planes[2].dist = -mins[1];

	planes[3].normal[0] = 0.0f;
	planes[3].normal[1] = 1.0f;
	planes[3].normal[2] = 0.0f;
	planes[3].dist = maxs[1];

	planes[4].normal[0] = 0.0f;
	planes[4].normal[1] = 0.0f;
	planes[4].normal[2] = -1.0f;
	planes[4].dist = -mins[2];

	planes[5].normal[0] = 0.0f;
	planes[5].normal[1] = 0.0f;
	planes[5].normal[2] = 1.0f;
	planes[5].dist = maxs[2];
}

static void write_plane(FILE *fp, const char *material, vec3 n, float dist, vec3 origin)
{
	vec3 tangent, bitangent;
	vec3 up = { 0, 0, 1.f };
	vec3 fw = { 0, 1.f, 0 };
	float d = vec3_mul_inner(up, n);
	if(fabs(d) < 0.01f)
	{
		vec3_mul_cross(tangent, n, up);
	}
	else
	{
		vec3_mul_cross(tangent, n, fw);
	}
	vec3_mul_cross(bitangent, n, tangent);

	vec3 a, b, c;
	vec3 t;
	vec3_scale(a, n, dist);
	vec3_scale(t, tangent, 100.f);
	vec3_add(b, a, t);
	vec3_scale(t, bitangent, 100.f);
	vec3_add(c, a, t);

	fprintf(fp,
			" ( %f %f %f ) ( %f %f %f ) ( %f %f %f ) %s 128 128 0 0 0 0 lightmap_gray 16384 16384 0 "
			"0 0 0\n",
			c[0] + origin[0],
			c[1] + origin[1],
			c[2] + origin[2],
			b[0] + origin[0],
			b[1] + origin[1],
			b[2] + origin[2],
			a[0] + origin[0],
			a[1] + origin[1],
			a[2] + origin[2],
			material ? material : "caulk");
}

const char *entity_key_by_value(Entity *ent, const char *key)
{
	for(size_t i = 0; i < buf_size(ent->keyvalues); ++i)
	{
		if(!strcmp(ent->keyvalues[i].key, key))
			return ent->keyvalues[i].value;
	}
	return "";
}

typedef struct
{
	s32 vertex[3];
} Triangle;

typedef struct
{
	Triangle *triangles;
	s32 materialIndex;
} Patch;

static int triangle_vertex_compare(const void *a, const void *b)
{
	return (*(int *)a - *(int *)b);
}

// vertices should be sorted
bool patch_has_triangle(Patch *patch, int *vertices)
{
	for(size_t i = 0; i < buf_size(patch->triangles); ++i)
	{
		Triangle *tri = &patch->triangles[i];
		if(tri->vertex[0] == vertices[0] && tri->vertex[1] == vertices[1] && tri->vertex[2] == vertices[2])
			return true;
	}
	return false;
}

bool patches_has_triangle(Patch *patches, int *vertices)
{
	for(size_t i = 0; i < buf_size(patches); ++i)
	{
		if(patch_has_triangle(&patches[i], vertices))
			return true;
	}
	return false;
}

static bool vec3_fuzzy_zero(float *v)
{
	float e = 0.0001f;
	return fabs(v[0]) < e && fabs(v[1]) < e && fabs(v[2]) < e;
}

static void write_patches(FILE *fp)
{
	dmaterial_t *materials = (dmaterial_t*)lumpdata[LUMP_MATERIALS].data;
	DiskCollisionVertex *vertices = lumpdata[LUMP_COLLISIONVERTS].data;
	DiskCollisionTriangle *tris = lumpdata[LUMP_COLLISIONTRIS].data;
	DiskCollisionAabbTree *collaabbtrees = lumpdata[LUMP_COLLISIONAABBS].data;
	DiskCollisionPartition *collpartitions = lumpdata[LUMP_COLLISIONPARTITIONS].data;

	Patch *patches = NULL;
	Patch *patch = NULL;

	for(size_t i = 0; i < lumpdata[LUMP_COLLISIONAABBS].count; ++i)
	{
		DiskCollisionAabbTree *tree = &collaabbtrees[i];
		DiskCollisionPartition *part = &collpartitions[tree->u.partitionIndex];
		if(tree->childCount > 0)
			continue;
		bool created_new_patch = false;
		if(part->triCount > 0)
		{
			for(size_t j = 0; j < part->triCount; ++j)
			{
				DiskCollisionTriangle *tri = &tris[part->firstTriIndex + j];

				Triangle triangle;
				triangle.vertex[0] = tri->vertIndices[0];
				triangle.vertex[1] = tri->vertIndices[1];
				triangle.vertex[2] = tri->vertIndices[2];

				if(vec3_fuzzy_zero(vertices[triangle.vertex[0]].xyz)
				   || vec3_fuzzy_zero(vertices[triangle.vertex[1]].xyz)
				   || vec3_fuzzy_zero(vertices[triangle.vertex[2]].xyz))
				{
					continue;
				}

				if(patch && buf_size(patch->triangles) >= 7)
				{
					buf_push(patches, ((Patch) { .triangles = NULL, .materialIndex = tree->materialIndex }));
					patch = &patches[buf_size(patches) - 1];
					created_new_patch = true;
				}
				qsort(triangle.vertex, 3, sizeof(int), triangle_vertex_compare);

				if(!patches_has_triangle(patches, triangle.vertex))
				{
					if(!created_new_patch)
					{
						buf_push(patches, ((Patch) { .triangles = NULL, .materialIndex = tree->materialIndex }));
						patch = &patches[buf_size(patches) - 1];
						created_new_patch = true;
					}
					buf_push(patch->triangles, triangle);
				}
			}
		}
	}
	
	// TODO: better way of converting triangles into patches

	for(size_t i = 0; i < buf_size(patches); ++i)
	{
		Patch *patch = &patches[i];
		if(buf_size(patch->triangles) == 0)
			continue;

		fprintf(fp, "  {\n");
		fprintf(fp, "   mesh\n");
		fprintf(fp, "   {\n");
		fprintf(fp, "   %s\n", materials[patch->materialIndex].material);
		// TODO: write contentFlags and contentFlags info
		fprintf(fp, "   lightmap_gray\n");
		fprintf(fp, "   %d 2 16 8\n", buf_size(patch->triangles) * 2);

		for(size_t j = 0; j < buf_size(patch->triangles); ++j)
		{
			Triangle *tri = &patch->triangles[j];
			DiskCollisionVertex *v1 = &vertices[tri->vertex[0]];
			DiskCollisionVertex *v2 = &vertices[tri->vertex[1]];
			DiskCollisionVertex *v3 = &vertices[tri->vertex[2]];
			fprintf(fp, "   (\n");
			fprintf(fp, "	v %f %f %f t -1024 1024 -4 4\n", v1->xyz[0], v1->xyz[1], v1->xyz[2]);
			fprintf(fp, "	v %f %f %f t -1024 1024 -4 4\n", v2->xyz[0], v2->xyz[1], v2->xyz[2]);
			fprintf(fp, "   )\n");
			fprintf(fp, "   (\n");
			fprintf(fp, "	v %f %f %f t -1024 1024 -4 4\n", v3->xyz[0], v3->xyz[1], v3->xyz[2]);
			fprintf(fp, "	v %f %f %f t -1024 1024 -4 4\n", v1->xyz[0], v1->xyz[1], v1->xyz[2]);
			fprintf(fp,"   )\n");
		}
		fprintf(fp, "   }\n");
		fprintf(fp, "  }\n");

	}
}

void triangle_normal(vec3 n, const vec3 a, const vec3 b, const vec3 c)
{
    vec3 e1, e2;
    vec3_sub(e1, b, a);
    vec3_sub(e2, c, a);
    vec3_mul_cross(n, e1, e2);
    vec3_norm(n, n);
}

static bool vec3_fuzzy_eq(float *a, float *b)
{
	for(size_t k = 0; k < 3; ++k)
	{
		if(fabs(a[k] - b[k]) > 0.0001)
			return false;
	}
	return true;
}

static void write_portals(FILE *fp)
{
	DiskGfxPortal *portals = lumpdata[LUMP_PORTALS].data;
	DiskGfxPortalVertex *vertices = lumpdata[LUMP_PORTALVERTS].data;
	DiskPlane *planes = (DiskPlane*)lumpdata[LUMP_PLANES].data;
	
	int *written = malloc(lumpdata[LUMP_PORTALS].count * sizeof(int));
	memset(written, -1, lumpdata[LUMP_PORTALS].count * sizeof(int));
	size_t written_count = 0;

	for(size_t i = 0; i < lumpdata[LUMP_PORTALS].count; ++i)
	{
		DiskGfxPortal *portal = &portals[i];
		bool found = false;
		for(size_t k = 0; k < written_count; ++k)
		{
			DiskGfxPortal *other = &portals[written[k]];
			if(other->portalVertexCount != portal->portalVertexCount)
				continue;
			size_t match_count = 0;
			for(size_t g = 0; g < other->portalVertexCount; ++g)
			{
				for(size_t h = 0; h < portal->portalVertexCount; ++h)
				{
					if(vec3_fuzzy_eq(vertices[portal->firstPortalVertex + h].xyz, vertices[other->firstPortalVertex + g].xyz))
						++match_count;
				}
			}
			if(match_count == portal->portalVertexCount)
			{
				found = true;
				break;
			}
		}
		if(found)
			continue;
		fprintf(fp, "{\n");
		written[written_count++] = i;

		DiskPlane *plane = &planes[portal->planeIndex];

		vec3 portal_normal;
		triangle_normal(portal_normal, vertices[portal->firstPortalVertex].xyz, vertices[portal->firstPortalVertex + 1].xyz, vertices[portal->firstPortalVertex + 2].xyz);
		float portal_distance = vec3_mul_inner(portal_normal, vertices[portal->firstPortalVertex].xyz);

		write_plane(fp, "portal", portal_normal, portal_distance, (vec3) { 0.f, 0.f, 0.f });
		for(int k = 0; k < 3; ++k)
			portal_normal[k] = -portal_normal[k];
		write_plane(fp, "portal_nodraw", portal_normal, -portal_distance + 8.f, (vec3) { 0.f, 0.f, 0.f });
		for(size_t i = 0; i < portal->portalVertexCount; ++i)
		{
			DiskGfxPortalVertex *a = &vertices[portal->firstPortalVertex + i];
			int next_idx = i + 1 >= portal->portalVertexCount ? 0 : i + 1;
			DiskGfxPortalVertex *b = &vertices[portal->firstPortalVertex + next_idx];

			vec3 ba;
			vec3_sub(ba, b->xyz, a->xyz);
			vec3_norm(ba, ba);
			vec3 n;
			vec3_mul_cross(n, ba, plane->normal);

			float d = vec3_mul_inner(n, a->xyz);
			for(int k = 0; k < 3; ++k)
				n[k] = -n[k];
			write_plane(fp, "portal_nodraw", n, -d, (vec3) { 0.f, 0.f, 0.f });
		}
		fprintf(fp, "}\n");
	}
}

static bool ignore_material(const char *material)
{
	static const char *ignored[] = { "portal", "portal_nodraw", NULL };
	for(size_t i = 0; ignored[i]; ++i)
	{
		if(!strcmp(material, ignored[i]))
			return true;
	}
	return false;
}

typedef struct
{
	vec3 normal;
	float distance;
	char material[256];
} MapPlane;

typedef struct
{
	vec3 mins, maxs;
	MapPlane *planes;
} MapBrush;

typedef struct
{
	vec3 *points;
	uint32_t *indices;
	vec2 *uvs;
	MapPlane *plane;
} Polygon;

MapBrush *mapbrushes = NULL;

void map_planes_from_aabb(vec3 mins, vec3 maxs, MapPlane planes[6])
{
	planes[0].normal[0] = -1.0f;
	planes[0].normal[1] = 0.0f;
	planes[0].normal[2] = 0.0f;
	planes[0].distance = -mins[0];

	planes[1].normal[0] = 1.0f;
	planes[1].normal[1] = 0.0f;
	planes[1].normal[2] = 0.0f;
	planes[1].distance = maxs[0];

	planes[2].normal[0] = 0.0f;
	planes[2].normal[1] = -1.0f;
	planes[2].normal[2] = 0.0f;
	planes[2].distance = -mins[1];

	planes[3].normal[0] = 0.0f;
	planes[3].normal[1] = 1.0f;
	planes[3].normal[2] = 0.0f;
	planes[3].distance = maxs[1];

	planes[4].normal[0] = 0.0f;
	planes[4].normal[1] = 0.0f;
	planes[4].normal[2] = -1.0f;
	planes[4].distance = -mins[2];

	planes[5].normal[0] = 0.0f;
	planes[5].normal[1] = 0.0f;
	planes[5].normal[2] = 1.0f;
	planes[5].distance = maxs[2];
}

static void load_map_brushes()
{
	size_t side_offset = 0;
	LumpData *brushes = &lumpdata[LUMP_BRUSHES];
	cbrushside_t *brushsides = (cbrushside_t*)lumpdata[LUMP_BRUSHSIDES].data;
	dmaterial_t *materials = (dmaterial_t*)lumpdata[LUMP_MATERIALS].data;
	DiskPlane *diskplanes = (DiskPlane*)lumpdata[LUMP_PLANES].data;
	
	for(size_t i = 0; i < brushes->count; ++i)
	{
		MapBrush dst = { 0 };

		DiskBrush *src = &((DiskBrush*)brushes->data)[i];
		cbrushside_t *sides = &brushsides[side_offset];
		size_t numsides = src->numSides - 6;
		s32 axialMaterialNum[6] = {0};
		for(size_t axis = 0; axis < 3; axis++)
		{
			for(size_t sign = 0; sign < 2; sign++)
			{
				union f2i
				{
					float f;
					int i;
				} u;
				u.i = brushsides[side_offset].plane;
				axialMaterialNum[sign + axis * 2] = brushsides[side_offset].materialNum;
				float f = u.f;
				if(sign)
				{
					dst.maxs[axis] = f;
				}
				else
				{
					dst.mins[axis] = f;
				}
				++side_offset;
			}
		}

		MapPlane axial_planes[6];
		map_planes_from_aabb(dst.mins, dst.maxs, axial_planes);
		for(size_t h = 0; h < 6; ++h)
		{
			MapPlane *plane = &axial_planes[h];
			snprintf(plane->material, sizeof(plane->material), "%s", materials[axialMaterialNum[h]].material);
			buf_push(dst.planes, *plane);
			}
			for(size_t k = 0; k < numsides; ++k)
			{
				cbrushside_t *side = &brushsides[side_offset + k];
			DiskPlane *diskplane = &diskplanes[side->plane];

			MapPlane plane = {0};
			plane.distance = diskplane->dist;
			snprintf(plane.material, sizeof(plane.material), "%s", materials[side->materialNum].material);
			vec3_dup(plane.normal, diskplane->normal);
			buf_push(dst.planes, plane);
		}
		buf_push(mapbrushes, dst);
		side_offset += numsides;
	}
}

static float mat3_determinant(mat3 m)
{
	return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
		   + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

static bool mat3_inverse(mat3 result, mat3 m)
{
	float a, b, c, d, e, f, g, h, i;
	a = m[0][0];
	b = m[0][1];
	c = m[0][2];
	d = m[1][0];
	e = m[1][1];
	f = m[1][2];
	g = m[2][0];
	h = m[2][1];
	i = m[2][2];

	float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

	if(det == 0.0)
		return false;

	float inv_det = 1.f / det;

	result[0][0] = (e * i - f * h) * inv_det;
	result[0][1] = (c * h - b * i) * inv_det;
	result[0][2] = (b * f - c * e) * inv_det;

	result[1][0] = (f * g - d * i) * inv_det;
	result[1][1] = (a * i - c * g) * inv_det;
	result[1][2] = (c * d - a * f) * inv_det;

	result[2][0] = (d * h - e * g) * inv_det;
	result[2][1] = (b * g - a * h) * inv_det;
	result[2][2] = (a * e - b * d) * inv_det;
	return true;
}

static void mat3_vec3_mul(vec3 result, mat3 m, vec3 v)
{
	result[0] = result[1] = result[2] = 0.f;
	for(size_t i = 0; i < 3; ++i)
	{
		for(size_t j = 0; j < 3; ++j)
		{
			result[i] = result[i] + m[i][j] * v[j];
		}
	}
}

bool polygon_has_pt(Polygon *polygon, vec3 pt)
{
	for(size_t i = 0; i < buf_size(polygon->points); ++i)
	{
		vec3 v;
		vec3_sub(v, pt, polygon->points[i]);
		if(vec3_len(v) < 0.001)
			return true;
	}
	return false;
		}

#define buf_set_size(v, new_size)                                                                               \
	do                                                                                                          \
	{                                                                                                           \
		if(v)                                                                                                   \
		{                                                                                                       \
			buf_ptr((v))->size = (size_t)new_size > buf_ptr((v))->capacity ? buf_ptr((v))->capacity : new_size; \
		}                                                                                                       \
	} while(0)

bool polygonize_brush(MapBrush *brush, Polygon **polygons_out)
{
	Polygon *polygons = NULL;
	size_t plane_count = buf_size(brush->planes);
	for(size_t i = 0; i < plane_count; ++i)
	{
		MapPlane *p0 = &brush->planes[i];
		Polygon polygon = { 0 };
		polygon.plane = p0;
		for(size_t j = 0; j < plane_count; ++j)
		{
			if(j == i)
				continue;
			for(size_t k = 0; k < plane_count; ++k)
			{
				MapPlane *p1 = &brush->planes[j];
				MapPlane *p2 = &brush->planes[k];

				if(j == k || i == k)
					continue;
				mat3 P = {
					{ p0->normal[0], p0->normal[1], p0->normal[2] },
					{ p1->normal[0], p1->normal[1], p1->normal[2] },
					{ p2->normal[0], p2->normal[1], p2->normal[2] }
				};
				float det = mat3_determinant(P);
				if(det == 0.0f)
					continue;
				vec3 b = { p0->distance, p1->distance, p2->distance };
				// P * v = -b
				// v = -inverse(P) * b
				mat3 inv_P;
				mat3_inverse(inv_P, P);
				vec3 v;
				mat3_vec3_mul(v, inv_P, b);
				bool invalid = false;

				for(size_t m = 0; m < plane_count; ++m)
				{
					float d = vec3_mul_inner(brush->planes[m].normal, v) - brush->planes[m].distance;
					if(d > 0.008f)
					{
						invalid = true;
						break;
					}
				}
				if(!invalid && !polygon_has_pt(&polygon, v))
				{
					buf_grow(polygon.points, 1);
					buf_set_size(polygon.points, buf_size(polygon.points) + 1);
					memcpy(polygon.points[buf_size(polygon.points) - 1], v, sizeof(vec3));
				}
			}
		}

		if(buf_size(polygon.points) >= 3)
		{
			buf_push(polygons, polygon);
		}
		else
		{
			buf_free(polygon.indices);
			buf_free(polygon.points);
		}
	}
	*polygons_out = polygons;
	return true;
}

static void write_brushes(FILE *fp, dmodel_t *model, vec3 origin)
{
	for(size_t i = 0; i < model->numBrushes; ++i)
			{
		fprintf(fp, "{\n");
		MapBrush *brush = &mapbrushes[model->firstBrush + i];
		// for(size_t j = 0; j < buf_size(brush->planes); ++j)
		// {
		// 	MapPlane *plane = &brush->planes[j];
		// 	write_plane(fp, plane->material, plane->normal, plane->distance);
		// }
		Polygon *polys = NULL;
		polygonize_brush(brush, &polys);
		for(size_t j = 0; j < buf_size(polys); ++j)
		{
			Polygon *poly = &polys[j];
			MapPlane *plane = poly->plane;
			write_plane(fp, plane->material, plane->normal, plane->distance, origin);
			}
		fprintf(fp, "}\n");	
	}
}

void export_to_map(ProgramOptions *opts, const char *path)
{
	FILE *mapfile = NULL;
	mapfile = fopen(path, "w");
	if(!mapfile)
	{
		printf("Failed to open '%s'\n", path);
		return;
	}
	printf("Exporting to '%s'\n", path);
	Entity *worldspawn = &entities[0];
	fprintf(mapfile, "iwmap 4\n");
	fprintf(mapfile, "// entity 0\n{\n");
	for(size_t i = 0; i < buf_size(worldspawn->keyvalues); ++i)
	{
		KeyValuePair *kvp = &worldspawn->keyvalues[i];
		fprintf(mapfile, "\"%s\" \"%s\"\n", kvp->key, kvp->value);
	}
	dmodel_t *models = lumpdata[LUMP_MODELS].data;

	write_brushes(mapfile, &models[0], (vec3) { 0.f, 0.f, 0.f });

	if(!opts->exclude_patches)
	{
		write_patches(mapfile);
	}
	fprintf(mapfile, "}\n");
	for(size_t i = 1; i < buf_size(entities); ++i)
	{
		Entity *e = &entities[i];
		const char *classname = entity_key_by_value(e, "classname");
		fprintf(mapfile, "// entity %d\n{\n", i);

		bool has_brushes = !strcmp(classname, "script_brushmodel") || strstr(classname, "trigger_");
		for(size_t j = 0; j < buf_size(e->keyvalues); ++j)
		{
			KeyValuePair *kvp = &e->keyvalues[j];
			if(has_brushes)
			{
				if(!strcmp(kvp->key, "origin") || !strcmp(kvp->key, "model"))
					continue;
			}
			fprintf(mapfile, "\"%s\" \"%s\"\n", kvp->key, kvp->value);
		}
		if(has_brushes)
		{
			const char *modelstr = entity_key_by_value(e, "model");
			vec3 origin = {0};
			const char *originstr = entity_key_by_value(e, "origin");
			if(originstr)
			{
				sscanf(originstr, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			}
			int modelidx = 0;
			sscanf(modelstr, "*%d", &modelidx);
			write_brushes(mapfile, &models[modelidx], origin);
		}
		fprintf(mapfile, "}\n");
	}
	fclose(mapfile);
}

void print_info(dheader_t *hdr, const char *path)
{
	printf("bsp.c v0.1 (c) 2024\n");
	printf("---------------------\n");
	printf("%s: %d\n", path, filelen);
	
	info(hdr, LUMP_MODELS, NULL);
	info(hdr, LUMP_MATERIALS, NULL);
	info(hdr, LUMP_BRUSHES, NULL);
	info(hdr, LUMP_BRUSHSIDES, NULL);
	info(hdr, LUMP_PLANES, NULL);
	int entity_count = buf_size(entities);
	info(hdr, LUMP_ENTITIES, &entity_count);
	printf("\n");
	info(hdr, LUMP_NODES, NULL);
	info(hdr, LUMP_LEAFS, NULL);
	info(hdr, LUMP_LEAFBRUSHES, NULL);
	info(hdr, LUMP_LEAFSURFACES, NULL);
	info(hdr, LUMP_COLLISIONVERTS, NULL);
	info(hdr, LUMP_COLLISIONEDGES, NULL);
	info(hdr, LUMP_COLLISIONTRIS, NULL);
	info(hdr, LUMP_COLLISIONBORDERS, NULL);
	info(hdr, LUMP_COLLISIONAABBS, NULL);
	info(hdr, LUMP_DRAWVERTS, NULL);
	info(hdr, LUMP_DRAWINDICES, NULL);
	info(hdr, LUMP_TRIANGLES, NULL);
	
	info(hdr, LUMP_OBSOLETE_1, NULL);
	info(hdr, LUMP_OBSOLETE_2, NULL);
	info(hdr, LUMP_OBSOLETE_3, NULL);
	info(hdr, LUMP_OBSOLETE_4, NULL);
	info(hdr, LUMP_OBSOLETE_5, NULL);

	info(hdr, LUMP_LIGHTBYTES, NULL);
	info(hdr, LUMP_LIGHTGRIDENTRIES, NULL);
	info(hdr, LUMP_LIGHTGRIDCOLORS, NULL);
	// Not sure if it's stored as a lump or just parsed from entdata with classname "light"
	size_t light_entity_count = 0;
	for(size_t i = 0; i < buf_size(entities); ++i)
	{
		Entity *e = &entities[i];
		const char *classname = entity_key_by_value(e, "classname");
		if(!strcmp(classname, "light"))
			++light_entity_count;
	}
	printf("     %d lights                   0 B      0 KB   0.0%\n", light_entity_count);
	info(hdr, LUMP_VISIBILITY, NULL);
	info(hdr, LUMP_PORTALVERTS, NULL);
	info(hdr, LUMP_OCCLUDERS, NULL);
	info(hdr, LUMP_OCCLUDERPLANES, NULL);
	info(hdr, LUMP_OCCLUDEREDGES, NULL);
	info(hdr, LUMP_OCCLUDERINDICES, NULL);
	info(hdr, LUMP_AABBTREES, NULL);
	info(hdr, LUMP_CELLS, NULL);
	info(hdr, LUMP_PORTALS, NULL);
	info(hdr, LUMP_CULLGROUPS, NULL);
	info(hdr, LUMP_CULLGROUPINDICES, NULL);
	printf("\n");
	info(hdr, LUMP_PATHCONNECTIONS, NULL);
	printf("---------------------\n");
}

static void print_usage()
{
	printf("Usage: ./bsp [options] <input_file>\n");
	printf("\n");
	printf("Options:\n");
	printf("  -info                 	Print information about the input file.\n");
	printf("  -export            		Export the input file to a .MAP.\n");
	printf("                        	If no export path is provided, it will write to the input file with _exported appended.\n");
	printf("                        	Example: /path/to/your/bsp.d3dbsp will write to /path/to/your/bsp_exported.map\n");
	printf("  -original_brush_portals 	By default portals are converted to brushes instead of using the portals that are in brushes.\n");
	printf("  -exclude_patches 			Don't export patches.\n");
	printf("\n");
	printf("\n");
	printf("  -export_path <path> 	Specify the path where the export should be saved. Requires an argument.\n");
	printf("  -help              	Display this help message and exit.\n");
	printf("\n");
	printf("Arguments:\n");
	printf("  <input_file>       	The input file to be processed.\n");
	printf("\n");
	printf("Examples:\n");
	printf("./bsp -info input_file.d3dbsp\n");
	printf("./bsp -export -export_path /path/to/exported_file.map input_file.d3dbsp\n");
	exit(0);
}

static bool parse_arguments(int argc, char **argv, ProgramOptions *opts)
{
	opts->try_fix_portals = true;

    for (int i = 1; i < argc; i++)
	{
		switch(argv[i][0])
		{
			case '-':
				if (!strcmp(argv[i], "-info"))
				{
					opts->print_info = true;
				} else if(!strcmp(argv[i], "-help") || !strcmp(argv[i], "-?") || !strcmp(argv[i], "-usage"))
				{
					print_usage();
				} else if (!strcmp(argv[i], "-exclude_patches"))
				{
					opts->exclude_patches = true;
				} else if (!strcmp(argv[i], "-original_brush_portals"))
				{
					opts->try_fix_portals = false;
				} else if (!strcmp(argv[i], "-export"))
				{
					opts->export_to_map = true;
				} else if (!strcmp(argv[i], "-export_path"))
				{
					if (i + 1 < argc)
					{
						opts->export_file = argv[++i];
					} else {
						fprintf(stderr, "Error: -export_path requires a argument.\n");
						return false;
					}
				} else if (!strcmp(argv[i], "-format"))
				{
					if (i + 1 < argc)
					{
						opts->format = argv[++i];
					} else {
						fprintf(stderr, "Error: -format requires a argument.\n");
						return false;
					}
				} else {
					fprintf(stderr, "Unknown option: %s\n", argv[i]);
					return false;
				}
			break;

			default:
				// printf("%s\n", argv[i]);
				opts->input_file = argv[i];
			break;
		}
    }
	return true;
}

static void pathinfo(const char *path,
					 char *directory, size_t directory_max_length,
					 char *basename, size_t basename_max_length,
					 char *extension, size_t extension_max_length, char *sep)
{

	size_t offset = 0;
	const char *it = path;
	while(*it)
	{
		if(*it == '/' || *it == '\\')
		{
			if(sep)
				*sep = *it;
			offset = it - path;
		}
		++it;
	}
	directory[0] = 0;
	snprintf(directory, directory_max_length, "%.*s", offset, path);
	const char *filename = path + offset;
	
	if(*filename == '/' || *filename == '\\')
		++filename;

	char *delim = strrchr(filename, '.');
	basename[0] = 0;
	extension[0] = 0;
	if(!delim)
	{
		snprintf(basename, basename_max_length, "%s", filename);
	} else
	{
		snprintf(basename, basename_max_length, "%.*s", delim - filename, filename);
		snprintf(extension, extension_max_length, "%s", delim + 1);
	}
}

int main(int argc, char **argv)
{
	ProgramOptions opts = {0};
	if(!parse_arguments(argc, argv, &opts))
	{
		return 1;
	}

	TEST(dmodel_t, 48);

	Stream s = {0};
	assert(0 == stream_open_file(&s, opts.input_file, "rb"));

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
	for(size_t i = 0; i < LUMP_MAX; ++i)
	{
		lump_t *l = &hdr.lumps[i];
		if(l->filelen != 0 && lumpsizes[i] != 0)
		{
			LumpData *ld = &lumpdata[i];
			assert(l->filelen % lumpsizes[i] == 0);
			ld->count = l->filelen / lumpsizes[i];
			ld->data = calloc(ld->count, lumpsizes[i]);
			s.seek(&s, l->fileofs, SEEK_SET);
			s.read(&s, ld->data, lumpsizes[i], ld->count);
		}
	}
	
	Entity *parse_entities();
	entities = parse_entities();
	
	load_map_brushes();

	if(opts.print_info)
		print_info(&hdr, opts.input_file);

	if(opts.export_to_map)
	{
		char directory[256] = {0};
		char basename[256] = {0};
		char extension[256] = {0};
		char sep = 0;
		pathinfo(opts.input_file,
				 directory,
				 sizeof(directory),
				 basename,
				 sizeof(basename),
				 extension,
				 sizeof(extension),
				 &sep);
				 
		char output_file[256] = {0};
		snprintf(output_file, sizeof(output_file), "%s%c%s_exported.map", directory, sep, basename);
		if(opts.export_file)
			export_to_map(&opts, opts.export_file);
		else
			export_to_map(&opts, output_file);
	}
	return 0;
}
