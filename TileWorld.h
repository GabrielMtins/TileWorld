#ifndef TILEWORLD_H
#define TILEWORLD_H

#define TILEWORLD_IMPLEMENTATION

#include <stddef.h>
#include <stdint.h>

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
void TW_ComputeStaticVertices(TW_VertexContext *ctx);

#endif /* TILEWORLD_H */

#ifdef TILEWORLD_IMPLEMENTATION

#define TW_WorldInBounds(world, x, z) \
	((x) >= 0 && (z) >= 0 && (x) < (int) (world).width && (z) < (int) (world).height)

#define TW_WorldGetTile(world, x, z) (&((world).tiles[(x) + (z) * (world).width]))

#define TW_WorldGetTileColor(world, x, z) \
	(TW_WorldInBounds(world, x, z) ? \
	(TW_WorldGetTile(world, x, z))->color : \
	(TW_Color){0, 0, 0})

#define TW_AddVertexToVertices(ctx, vertex) \
	(ctx)->addVertexToUserdata((ctx)->userdata, &(vertex))

#define TW_AddTriangleToVertices(ctx, vertices) \
	do {\
		for(int i = 0; i < 3; i++) { \
			(ctx)->addVertexToUserdata((ctx)->userdata, &((vertices)[i])); \
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
                                    TW_Color color);

static void TW_AddWallToVertices(TW_VertexContext *ctx,
                                 const TW_SurfaceConfig *sur[2],
                                 const TW_TextureConfig *tex, P2i line[2],
                                 TW_Color color);

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
                                    TW_Color color) {
	if(tex->id == 0) return;

	TW_Vertex vertices[4];

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

	TW_AddVertexToVertices(ctx, vertices[0]);
	TW_AddVertexToVertices(ctx, vertices[1]);
	TW_AddVertexToVertices(ctx, vertices[2]);

	TW_AddVertexToVertices(ctx, vertices[1]);
	TW_AddVertexToVertices(ctx, vertices[3]);
	TW_AddVertexToVertices(ctx, vertices[2]);
}

static void TW_AddWallToVertices(TW_VertexContext *ctx,
                                 const TW_SurfaceConfig *sur[2],
                                 const TW_TextureConfig *tex, P2i line[2],
                                 TW_Color color) {
	if(tex->id == 0) return;

	TW_Vertex vertices[4];

	for(int i = 0; i < 4; i++) {
		int dx = i % 2;
		int dz = i / 2;
		int x = line[dx].x;
		int z = line[dx].y;
		float y = TW_GetSurfaceHeight(sur[dz], x, z);

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

	if(vertices[0].pos[1] > vertices[2].pos[1]) {
		TW_AddVertexToVertices(ctx, vertices[0]);
		TW_AddVertexToVertices(ctx, vertices[1]);
		TW_AddVertexToVertices(ctx, vertices[2]);
	}

	if(vertices[1].pos[1] > vertices[3].pos[1]) {
		TW_AddVertexToVertices(ctx, vertices[1]);
		TW_AddVertexToVertices(ctx, vertices[3]);
		TW_AddVertexToVertices(ctx, vertices[2]);
	}
}

static void TW_AddWindowToVertices(TW_VertexContext *ctx, int x, int z, WindowType type) {
	P2i line[2];
	const TW_Tile *tile = TW_WorldGetTile(*ctx->world, x, z);
	const TW_Tile *adj = NULL;
	
	switch(type) {
		case WINDOW_LEFT:
			if(x <= 0) break;

			adj = TW_WorldGetTile(*ctx->world, x - 1, z);
			line[0] = (P2i) {x, z};
			line[1] = (P2i) {x, z + 1};
			break;

		case WINDOW_UP:
			if(z + 1 >= (int) ctx->world->height) break;

			adj = TW_WorldGetTile(*ctx->world, x, z + 1);
			line[0] = (P2i) {x, z + 1};
			line[1] = (P2i) {x + 1, z + 1};
			break;

		case WINDOW_RIGHT:
			if(x + 1 >= (int) ctx->world->width) break;

			adj = TW_WorldGetTile(*ctx->world, x + 1, z);
			line[0] = (P2i) {x + 1, z + 1};
			line[1] = (P2i) {x + 1, z};
			break;

		case WINDOW_DOWN:
			if(z <= 0) break;

			adj = TW_WorldGetTile(*ctx->world, x, z - 1);
			line[0] = (P2i) {x + 1, z};
			line[1] = (P2i) {x, z};
			break;
	}

	if(adj == NULL) return;

	const TW_SurfaceConfig *sur1[] = {
		&tile->surfaces.floor,
		&adj->surfaces.floor
	};

	TW_AddWallToVertices(
			ctx,
			sur1,
			&tile->textures.window_floor,
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
			line,
			adj->color
			);
}

static void TW_AddFloorCeilToVertices(TW_VertexContext *ctx, int x, int z) {
	const TW_Tile *tile = TW_WorldGetTile(*ctx->world, x, z);

	TW_AddSurfaceToVertices(
			ctx,
			&tile->surfaces.floor,
			&tile->textures.floor,
			(P2i) {x, z},
			tile->color
			);

	TW_AddSurfaceToVertices(
			ctx,
			&tile->surfaces.ceil,
			&tile->textures.ceil,
			(P2i) {x, z},
			tile->color
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

	TW_Color colors[] = {
		TW_WorldGetTileColor(*ctx->world, x - 1, z),
		TW_WorldGetTileColor(*ctx->world, x, z + 1),
		TW_WorldGetTileColor(*ctx->world, x + 1, z),
		TW_WorldGetTileColor(*ctx->world, x, z - 1),
	};

	for(int i = 0; i < 4; i++) {
		TW_AddWallToVertices(
				ctx,
				sur,
				&tile->textures.wall,
				(P2i[2]) {
					points[i],
					points[(i + 1) % 4]
				},
				colors[i]
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
