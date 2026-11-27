#ifndef TILEWORLD_H
#define TILEWORLD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

enum TW_SurfaceType {
	TW_SURFACE_NO_INC = 0,
	TW_SURFACE_INC_PLUSX,
	TW_SURFACE_INC_MINUSX,
	TW_SURFACE_INC_PLUSZ,
	TW_SURFACE_INC_MINUSZ,
};

enum TW_TileFlags {
	TW_TILE_HAS_WALL = (1 << 0),
	TW_TILE_DYNAMIC  = (1 << 1),
};

typedef float TW_Vec3[3];
typedef float TW_Vec2[2];

typedef struct TW_Color {
	uint8_t r, g, b;
} TW_Color;

typedef struct TW_Vertex {
	TW_Vec3 pos;
	TW_Vec3 normal;
	TW_Vec3 color;
	TW_Vec2 uv;
	float id;
} TW_Vertex;

typedef struct TW_TextureConfig {
	uint8_t offset_x;
	uint8_t offset_y;
	uint8_t id;
} TW_TextureConfig;

typedef struct TW_SurfaceConfig {
	float height;
	float inclination;
	uint32_t type;
} TW_SurfaceConfig;

typedef struct TW_Tile {
	struct {
		TW_SurfaceConfig floor, ceil;
	} surfaces;

	struct {
		TW_TextureConfig floor, ceil;
		TW_TextureConfig window_floor, window_ceil;
		TW_TextureConfig wall;
	} textures;

	TW_Color color;
	uint16_t flags;
} TW_Tile;

typedef struct TW_World {
	TW_Tile *tiles;
	uint32_t width, height;
} TW_World;

typedef struct TW_VertexContext {
	const TW_World *world;
	void *userdata;
	void (*addVertexToUserdata)(void *, const TW_Vertex *);
} TW_VertexContext;

void TW_ComputeTileVertices(TW_VertexContext *ctx, uint32_t x, uint32_t z);

void TW_ComputeChunkVertices(TW_VertexContext *ctx, int min[2], int max[2]);

void TW_ComputeStaticVertices(TW_VertexContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TILEWORLD_H */

#ifdef TILEWORLD_IMPLEMENTATION

#define TW_Vec3_Cross(res, a, b) \
	do {\
		res[0] = a[1] * b[2] - a[2] * b[1]; \
		res[1] = a[2] * b[0] - a[0] * b[2]; \
		res[2] = a[0] * b[1] - a[1] * b[0]; \
	} while(0)

#define TW_Vec3_Sub(res, a, b) \
	do {\
		res[0] = a[0] - b[0]; \
		res[1] = a[1] - b[1]; \
		res[2] = a[2] - b[2]; \
	} while(0)

#define TW_Vec3_Copy(res, a) \
	do {\
		res[0] = a[0]; \
		res[1] = a[1]; \
		res[2] = a[2]; \
	} while(0);

#define TW_GetTriangleNormal(res, a, b, c) \
	do {\
		TW_Vec3 ab, ac; \
		TW_Vec3_Sub(ab, b, a); \
		TW_Vec3_Sub(ab, c, a); \
		TW_Vec3_Cross(res, ab, ac); \
	} while(0);

#define TW_WorldInBounds(world, x, z) \
	((x) >= 0 && (z) >= 0 && (x) < (int) (world).width && (z) < (int) (world).height)

#define TW_WorldGetTile(world, x, z) (&((world).tiles[(x) + (z) * (world).width]))

#define TW_WorldTileHasWall(world, x, z) \
	(TW_WorldInBounds(world, x, z) ? \
	(TW_WorldGetTile(world, x, z)->textures.wall.id != 0) : \
	1)

#define TW_WorldGetTileColor(world, x, z) \
	(TW_WorldInBounds(world, x, z) ? \
	(TW_WorldGetTile(world, x, z))->color : \
	(TW_Color){0, 0, 0})

#define TW_AddVertexToVertices(ctx, vertex) \
	(ctx)->addVertexToUserdata((ctx)->userdata, &(vertex))

#define TW_ComputeTriangleNormal(vertices) \
	do {\
		TW_Vec3 normal; \
		TW_GetTriangleNormal(normal, (vertices)[0].pos, (vertices)[1].pos, (vertices)[2].pos); \
		TW_Vec3_Copy((vertices)[0].normal, normal); \
		TW_Vec3_Copy((vertices)[1].normal, normal); \
		TW_Vec3_Copy((vertices)[2].normal, normal); \
	} while(0)

#define TW_TriangulateQuad(vertices, quad) \
	do {\
		(vertices)[0] = (quad)[2]; \
		(vertices)[1] = (quad)[1]; \
		(vertices)[2] = (quad)[0]; \
		(vertices)[3] = (quad)[2]; \
		(vertices)[4] = (quad)[3]; \
		(vertices)[5] = (quad)[1]; \
	} while(0)

#define TW_AddTriangleToVertices(ctx, vertices) \
	do {\
		for(int i = 0; i < 3; i++) { \
			(ctx)->addVertexToUserdata((ctx)->userdata, &((vertices)[i])); \
		} \
	} while(0)

#define TW_FindQuadMaxHeight(height, vertices) \
	do {\
		height = vertices[0].pos[1]; \
		for(int i = 1; i < 4; i++) { \
			if(vertices[i].pos[1] > height) height = vertices[i].pos[1]; \
		} \
	} while(0)

typedef enum WindowType {
	WINDOW_LEFT = 0, // -x
	WINDOW_RIGHT, // +x
	WINDOW_DOWN, // -z
	WINDOW_UP, // +z
} WindowType;

typedef struct P2i {
	int x, y;
} P2i;

static float TW_GetSurfaceHeight(const TW_SurfaceConfig *cfg, float x, float z);

static void TW_AddSurfaceToVertices(TW_VertexContext *ctx,
                                    const TW_SurfaceConfig *cfg,
                                    const TW_TextureConfig *tex, P2i point,
                                    TW_Color color, bool rev_order);

static void TW_AddWallToVertices(TW_VertexContext *ctx,
                                 const TW_SurfaceConfig *sur[2],
                                 const TW_TextureConfig *tex, P2i origin[2],
                                 P2i line[2], TW_Color color);

static void TW_AddWindowToVertices(TW_VertexContext *ctx, int x, int z, WindowType type);
static void TW_AddFloorCeilToVertices(TW_VertexContext *ctx, int x, int z);
static void TW_AddFullWallToVertices(TW_VertexContext *ctx, int x, int z);
static void TW_AddFullWindowsToVertices(TW_VertexContext *ctx, int x, int z);

static float TW_GetSurfaceHeight(const TW_SurfaceConfig *cfg, float x, float z) {
	float res = 0.0f;

	switch(cfg->type) {
		case TW_SURFACE_NO_INC:
			res = cfg->height;
			break;

		case TW_SURFACE_INC_PLUSX:
			res = cfg->height + x * cfg->inclination;
			break;

		case TW_SURFACE_INC_MINUSX:
			res = cfg->height + (1.0f - x) * cfg->inclination;
			break;

		case TW_SURFACE_INC_PLUSZ:
			res = cfg->height + z * cfg->inclination;
			break;

		case TW_SURFACE_INC_MINUSZ:
			res = cfg->height + (1.0f - z) * cfg->inclination;
			break;
	}

	return res;
}

static void TW_AddSurfaceToVertices(TW_VertexContext *ctx,
                                    const TW_SurfaceConfig *cfg,
                                    const TW_TextureConfig *tex, P2i point,
                                    TW_Color color, bool rev_order) {
	if(tex->id == 0) return;

	TW_Vertex vertices[4];
	TW_Vertex triangles[6];

	for(int i = 0; i < 4; i++) {
		int dx = i % 2;
		int dz = i / 2;
		
		vertices[i] = (TW_Vertex) {
			.pos = {
				point.x + dx,
				TW_GetSurfaceHeight(cfg, dx, dz),
				point.y + dz,
			},
			.uv = {
				dx + ((float) tex->offset_x / 256.0f),
				dz + ((float) tex->offset_y / 256.0f),
			},
			.id = tex->id,
			.color = {
				(float) color.r / 255.0f,
				(float) color.g / 255.0f,
				(float) color.b / 255.0f,
			},
		};
	}

	if(rev_order) {
		TW_Vertex tmp = vertices[0];
		vertices[0] = vertices[3];
		vertices[3] = tmp;
	}

	TW_TriangulateQuad(triangles, vertices);
	TW_ComputeTriangleNormal(triangles + 0);
	TW_ComputeTriangleNormal(triangles + 3);

	TW_AddTriangleToVertices(ctx, triangles + 0);
	TW_AddTriangleToVertices(ctx, triangles + 3);
}

static void TW_AddWallToVertices(TW_VertexContext *ctx,
                                 const TW_SurfaceConfig *sur[2],
                                 const TW_TextureConfig *tex, P2i origin[2],
                                 P2i line[2], TW_Color color) {
	if(tex->id == 0) return;

	TW_Vertex triangles[6];
	TW_Vertex vertices[4];

	for(int i = 0; i < 4; i++) {
		int dx = i % 2;
		int dz = i / 2;
		int x = line[dx].x;
		int z = line[dx].y;
		float y = TW_GetSurfaceHeight(sur[dz], x - origin[dz].x, z - origin[dz].y);

		vertices[i] = (TW_Vertex) {
			.pos = {
				x,
				y,
				z,
			},
			.uv = {
				dx + ((float) tex->offset_x / 256.0f),
				y + ((float) tex->offset_x / 256.0f),
			},
			.id = tex->id,
			.color = {
				(float) color.r / 255.0f,
				(float) color.g / 255.0f,
				(float) color.b / 255.0f,
			},
		};
	}

	bool check0above2 = vertices[0].pos[1] > vertices[2].pos[1];
	bool check1above3 = vertices[1].pos[1] > vertices[3].pos[1];

	if(!check0above2 && !check1above3) return;

	float max_height;
	TW_FindQuadMaxHeight(max_height, vertices);

	if(!check0above2) {
		vertices[0].pos[1] = max_height;
	}
	if(!check1above3) {
		vertices[1].pos[1] = max_height;
	}

	TW_TriangulateQuad(triangles, vertices);
	TW_ComputeTriangleNormal(triangles + 0);
	TW_ComputeTriangleNormal(triangles + 3);

	TW_AddTriangleToVertices(ctx, triangles + 0);
	TW_AddTriangleToVertices(ctx, triangles + 3);
}

static void TW_AddWindowToVertices(TW_VertexContext *ctx, int x, int z, WindowType type) {
	const P2i full_points_around_quad[4] = {
		{x, z},
		{x, z + 1},
		{x + 1, z + 1},
		{x + 1, z},
	};

	const P2i full_adj_points[4] = {
		{x - 1, z},
		{x, z + 1},
		{x + 1, z},
		{x, z - 1},
	};

	const P2i adj_origin = full_adj_points[type];

	if(!TW_WorldInBounds(*ctx->world, adj_origin.x, adj_origin.y)) {
		return;
	}

	if(TW_WorldTileHasWall(*ctx->world, adj_origin.x, adj_origin.y)) {
		return;
	}

	const TW_Tile *tile = TW_WorldGetTile(*ctx->world, x, z);
	const TW_Tile *adj = TW_WorldGetTile(*ctx->world, adj_origin.x, adj_origin.y);

	P2i line[2] = {
		full_points_around_quad[type],
		full_points_around_quad[(type + 1) % 4],
	};

	const TW_SurfaceConfig *sur1[] = {
		&tile->surfaces.floor,
		&adj->surfaces.floor,
	};

	TW_AddWallToVertices(
			ctx,
			sur1,
			&tile->textures.window_floor,
			(P2i[2]){{x, z}, adj_origin},
			line,
			adj->color
			);

	const TW_SurfaceConfig *sur2[] = {
		&adj->surfaces.ceil,
		&tile->surfaces.ceil,
	};

	TW_AddWallToVertices(
			ctx,
			sur2,
			&tile->textures.window_ceil,
			(P2i[2]){adj_origin, {x, z}},
			line,
			adj->color
			);
}

static void TW_AddFloorCeilToVertices(TW_VertexContext *ctx, int x, int z) {
	if(TW_WorldTileHasWall(*ctx->world, x, z)) {
		return;
	}

	const TW_Tile *tile = TW_WorldGetTile(*ctx->world, x, z);

	TW_AddSurfaceToVertices(
			ctx,
			&tile->surfaces.floor,
			&tile->textures.floor,
			(P2i) {x, z},
			tile->color,
			false
			);

	TW_AddSurfaceToVertices(
			ctx,
			&tile->surfaces.ceil,
			&tile->textures.ceil,
			(P2i) {x, z},
			tile->color,
			true
			);
}

static void TW_AddFullWallToVertices(TW_VertexContext *ctx, int x, int z) {
	const TW_Tile *tile = TW_WorldGetTile(*ctx->world, x, z);

	const TW_SurfaceConfig *sur[2] = {
		&tile->surfaces.ceil,
		&tile->surfaces.floor,
	};

	P2i points[] = {
		{x + 0, z + 0},
		{x + 0, z + 1},
		{x + 1, z + 1},
		{x + 1, z + 0},
	};

	P2i adj_tiles[] = {
		{x - 1, z},
		{x, z + 1},
		{x + 1, z},
		{x, z - 1},
	};

	for(int i = 0; i < 4; i++) {
		if(TW_WorldTileHasWall(*ctx->world, adj_tiles[i].x, adj_tiles[i].y)) {
			continue;
		}

		TW_AddWallToVertices(
				ctx,
				sur,
				&tile->textures.wall,
				(P2i[2]) {{x, z}, {x, z}},
				(P2i[2]) {
					points[i],
					points[(i + 1) % 4]
				},
				TW_WorldGetTileColor(*ctx->world, adj_tiles[i].x, adj_tiles[i].y)
				);
	}
}

static void TW_AddFullWindowsToVertices(TW_VertexContext *ctx, int x, int z) {
	TW_AddWindowToVertices(ctx, x, z, WINDOW_LEFT);
	TW_AddWindowToVertices(ctx, x, z, WINDOW_RIGHT);
	TW_AddWindowToVertices(ctx, x, z, WINDOW_UP);
	TW_AddWindowToVertices(ctx, x, z, WINDOW_DOWN);
}

void TW_ComputeTileVertices(TW_VertexContext *ctx, uint32_t x, uint32_t z) {
	if(x >= ctx->world->width || z >= ctx->world->height) {
		return;
	}

	TW_AddFloorCeilToVertices(ctx, x, z);
	TW_AddFullWallToVertices(ctx, x, z);
	TW_AddFullWindowsToVertices(ctx, x, z);
}

void TW_ComputeChunkVertices(TW_VertexContext *ctx, int min[2], int max[2]) {
	for(int j = min[1]; j < max[1]; j++) {
		for(int i = min[0]; i < max[0]; i++) {
			if(i < 0 || j < 0 || i >= (int) ctx->world->width || j >= (int) ctx->world->height) {
				continue;
			}

			TW_ComputeTileVertices(ctx, i, j);
		}
	}
}

void TW_ComputeStaticVertices(TW_VertexContext *ctx) {
	uint32_t i, j;
	const TW_Tile *tile;

	for(j = 0; j < ctx->world->height; j++) {
		for(i = 0; i < ctx->world->width; i++) {
			tile = TW_WorldGetTile(*ctx->world, i, j);

			if(tile->flags & TW_TILE_DYNAMIC) continue;

			TW_ComputeTileVertices(ctx, i, j);
		}
	}
}

#endif /* TILEWORLD_IMPLEMENTATION */
