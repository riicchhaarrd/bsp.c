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

#include <linmath.h/linmath.h>

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

static void write_plane(FILE *fp, const char *material, vec3 n, float dist)
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
			a[0],
			a[1],
			a[2],
			b[0],
			b[1],
			b[2],
			c[0],
			c[1],
			c[2],
			material ? material : "caulk");
}

void export_to_map(const char *path)
{
	FILE *mapfile = NULL;
	mapfile = fopen(path, "w");
	if(!mapfile)
	{
		printf("Failed to open '%s'\n", path);
		return;
	}
	printf("Exporting to '%s'\n", path);
	fprintf(mapfile, "iwmap 4\n// entity 0\n{\n\"classname\" \"worldspawn\"\n");
	size_t side_offset = 0;
	LumpData *brushes = &lumpdata[LUMP_BRUSHES];
	LumpData *lbrushsides = &lumpdata[LUMP_BRUSHSIDES];
	cbrushside_t *brushsides = (cbrushside_t*)lbrushsides->data;

	dmaterial_t *materials = (dmaterial_t*)lumpdata[LUMP_MATERIALS].data;
	DiskPlane *planes = (DiskPlane*)lumpdata[LUMP_PLANES].data;
	
	for(size_t i = 0; i < brushes->count; ++i)
	{
		fprintf(mapfile, "// brush %d\n{\n", i);
		DiskBrush *src = &((DiskBrush*)brushes->data)[i];
		size_t numsides = src->numSides - 6;
		vec3 mins, maxs;
		u32 axialMaterialNum[6];
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
					maxs[axis] = f;
				}
				else
				{
					mins[axis] = f;
				}
				++side_offset;
			}
		}
		DiskPlane planes[6];
		planes_from_aabb(mins, maxs, planes);
		for(size_t h = 0; h < 6; ++h)
		{
			vec3 n;
			n[0] = -planes[h].normal[0];
			n[1] = -planes[h].normal[1];
			n[2] = -planes[h].normal[2];
			write_plane(mapfile, materials[axialMaterialNum[h]].material, n, -planes[h].dist);
		}
		if(numsides > 0)
		{
			for(size_t k = 0; k < numsides; ++k)
			{
				cbrushside_t *side = &brushsides[side_offset + k];
				DiskPlane *plane = &planes[side->plane];
				vec3 n;
				n[0] = -plane->normal[0];
				n[1] = -plane->normal[1];
				n[2] = -plane->normal[2];
				write_plane(mapfile, materials[side->materialNum].material, n, -plane->dist);
			}

			side_offset += numsides;
		}
		fprintf(mapfile, "}\n");
	}

	fprintf(mapfile, "}\n");
	fclose(mapfile);
}

void print_info(dheader_t *hdr, const char *path)
{
	printf("bsp.c v0.1 (c) 2024\n");
	printf("---------------------\n");
	printf("%s: %d\n", path, filelen);
	
	info(hdr, LUMP_MODELS);
	info(hdr, LUMP_MATERIALS);
	info(hdr, LUMP_BRUSHES);
	info(hdr, LUMP_BRUSHSIDES);
	info(hdr, LUMP_PLANES);
	info(hdr, LUMP_ENTITIES);
	printf("\n");
	info(hdr, LUMP_NODES);
	info(hdr, LUMP_LEAFS);
	info(hdr, LUMP_LEAFBRUSHES);
	info(hdr, LUMP_LEAFSURFACES);
	info(hdr, LUMP_COLLISIONVERTS);
	info(hdr, LUMP_COLLISIONEDGES);
	info(hdr, LUMP_COLLISIONTRIS);
	info(hdr, LUMP_COLLISIONBORDERS);
	info(hdr, LUMP_COLLISIONAABBS);
	info(hdr, LUMP_DRAWVERTS);
	info(hdr, LUMP_DRAWINDICES);
	info(hdr, LUMP_TRIANGLES);
	
	info(hdr, LUMP_OBSOLETE_1);
	info(hdr, LUMP_OBSOLETE_2);
	info(hdr, LUMP_OBSOLETE_3);
	info(hdr, LUMP_OBSOLETE_4);
	info(hdr, LUMP_OBSOLETE_5);

	info(hdr, LUMP_LIGHTBYTES);
	info(hdr, LUMP_LIGHTGRIDENTRIES);
	info(hdr, LUMP_LIGHTGRIDCOLORS);
	// Not sure if it's stored as a lump or just parsed from entdata with classname "light"
	// TODO: parse entdata
	printf("     0 lights                   0 B      0 KB   0.0%\n");
	info(hdr, LUMP_VISIBILITY);
	info(hdr, LUMP_PORTALVERTS);
	info(hdr, LUMP_OCCLUDERS);
	info(hdr, LUMP_OCCLUDERPLANES);
	info(hdr, LUMP_OCCLUDEREDGES);
	info(hdr, LUMP_OCCLUDERINDICES);
	info(hdr, LUMP_AABBTREES);
	info(hdr, LUMP_CELLS);
	info(hdr, LUMP_PORTALS);
	info(hdr, LUMP_CULLGROUPS);
	info(hdr, LUMP_CULLGROUPINDICES);
	printf("\n");
	info(hdr, LUMP_PATHCONNECTIONS);
	printf("---------------------\n");
}

typedef struct
{
	bool print_info;
	bool export_to_map;
	const char *input_file;
	const char *format;
	const char *export_file;
} ProgramOptions;

static void print_usage()
{
	printf(R"(
Usage: ./bsp [options] <input_file>

Options:
  -info                 Print information about the input file.
  -export            	Export the input file to a .MAP.
                        If no export path is provided, it will write to the input file with _exported appended.
                        Example: /path/to/your/bsp.d3dbsp will write to /path/to/your/bsp_exported.map

  -export_path <path> 	Specify the path where the export should be saved. Requires an argument.
  -help              	Display this help message and exit.

Arguments:
  <input_file>       	The input file to be processed.

Examples:
  ./bsp -info input_file.d3dbsp
  ./bsp -export -export_path /path/to/exported_file.map input_file.d3dbsp
	
)");
	exit(0);
}

static bool parse_arguments(int argc, char **argv, ProgramOptions *opts)
{
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
			export_to_map(opts.export_file);
		else
			export_to_map(output_file);
	}
	return 0;
}
