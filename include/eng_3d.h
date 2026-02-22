/**
 * include/eng_3d.h — はじむ用 3D エンジン API  v2.0.0
 *
 * SDL2 + OpenGL 3.3 Core Profile
 * Blinn-Phong + 法線マップ + シャドウマップ(PCF) + スポットライト
 * フォグ, スカイボックス, パーティクル, シーングラフ, アニメーション,
 * レイキャスト, ポストプロセス(ブルーム)
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 不透明構造体 ──────────────────────────────────────*/
typedef struct ENG_3D ENG_3D;

/* ── ID 型 ─────────────────────────────────────────────*/
typedef int ENG_3D_MeshID;     /* 1-origin; 0=無効 */
typedef int ENG_3D_TexID;      /* 1-origin; 0=なし */
typedef int ENG_3D_EmitterID;  /* 1-origin; 0=無効 */
typedef int ENG_3D_NodeID;     /* 1-origin; 0=無効 */
typedef int ENG_3D_AnimID;     /* 1-origin; 0=無効 */

/* ── 上限 ──────────────────────────────────────────────*/
#define ENG_3D_MAX_MESHES    256
#define ENG_3D_MAX_TEXTURES  128
#define ENG_3D_MAX_LIGHTS      8   /* ポイントライト */
#define ENG_3D_MAX_SPOTS       4   /* スポットライト */
#define ENG_3D_MAX_EMITTERS   16
#define ENG_3D_MAX_NODES     512
#define ENG_3D_MAX_ANIMS      32

/* ── レイキャスト結果 ──────────────────────────────────*/
typedef struct {
    bool  hit;
    float x, y, z;            /* ヒット座標 */
    float nx, ny, nz;         /* 法線 */
    float dist;               /* 距離 */
    ENG_3D_MeshID mesh_id;
} ENG_3D_RayHit;

/* ── AABB ─────────────────────────────────────────────*/
typedef struct {
    float min[3];
    float max[3];
} ENG_3D_AABB;

/* ══════════════════════════════════════════════════════
 * ライフサイクル
 * ══════════════════════════════════════════════════════*/
ENG_3D* eng3d_create(const char* title, int w, int h);
void    eng3d_destroy(ENG_3D* ctx);
bool    eng3d_update(ENG_3D* ctx);
float   eng3d_delta(ENG_3D* ctx);
int     eng3d_fps(ENG_3D* ctx);
int     eng3d_width(ENG_3D* ctx);
int     eng3d_height(ENG_3D* ctx);

/* ══════════════════════════════════════════════════════
 * カメラ
 * ══════════════════════════════════════════════════════*/
void eng3d_cam_perspective(ENG_3D* ctx, float fov_deg, float near_z, float far_z);
void eng3d_cam_pos(ENG_3D* ctx, float x, float y, float z);
void eng3d_cam_target(ENG_3D* ctx, float tx, float ty, float tz);
void eng3d_cam_lookat(ENG_3D* ctx,
                       float ex, float ey, float ez,
                       float tx, float ty, float tz);
/* カメラの front/right/up ベクトルを取得 */
void eng3d_cam_vectors(ENG_3D* ctx,
                        float* fx, float* fy, float* fz,   /* front  */
                        float* rx, float* ry, float* rz,   /* right  */
                        float* ux, float* uy, float* uz);  /* up     */

/* ══════════════════════════════════════════════════════
 * ライティング
 * ══════════════════════════════════════════════════════*/
void eng3d_ambient(ENG_3D* ctx, float r, float g, float b);
void eng3d_dir_light(ENG_3D* ctx,
                      float dx, float dy, float dz,
                      float r, float g, float b);
/** ポイントライト  slot=0..7  radius=0 で無効 */
void eng3d_point_light(ENG_3D* ctx, int slot,
                        float x, float y, float z,
                        float r, float g, float b,
                        float radius);
/** スポットライト  slot=0..3  cutoff/outer=コーン角(度) */
void eng3d_spot_light(ENG_3D* ctx, int slot,
                       float x, float y, float z,
                       float dx, float dy, float dz,
                       float r, float g, float b,
                       float radius,
                       float cutoff_deg, float outer_deg);
void eng3d_spot_light_off(ENG_3D* ctx, int slot);

/* ══════════════════════════════════════════════════════
 * シャドウマッピング
 * ══════════════════════════════════════════════════════*/
void eng3d_shadow_enable(ENG_3D* ctx, bool on);
void eng3d_shadow_bias(ENG_3D* ctx, float bias);
void eng3d_shadow_size(ENG_3D* ctx, float ortho_size);  /* 平行光源の正射影範囲 */

/* ══════════════════════════════════════════════════════
 * フォグ
 * ══════════════════════════════════════════════════════*/
void eng3d_fog_enable(ENG_3D* ctx, bool on);
/** mode: 0=線形, 1=指数(exp), 2=指数2(exp2) */
void eng3d_fog(ENG_3D* ctx, float r, float g, float b,
                int mode, float start, float end, float density);

/* ══════════════════════════════════════════════════════
 * スカイボックス
 * ══════════════════════════════════════════════════════*/
/** 6 面画像(+X,-X,+Y,-Y,+Z,-Z)をロードしてスカイボックスを設定 */
bool eng3d_skybox_load(ENG_3D* ctx,
                        const char* px, const char* nx,
                        const char* py, const char* ny,
                        const char* pz, const char* nz);
void eng3d_skybox_draw(ENG_3D* ctx);
void eng3d_skybox_unload(ENG_3D* ctx);

/* ══════════════════════════════════════════════════════
 * メッシュ
 * ══════════════════════════════════════════════════════*/
ENG_3D_MeshID eng3d_mesh_cube(ENG_3D* ctx, float w, float h, float d);
ENG_3D_MeshID eng3d_mesh_sphere(ENG_3D* ctx, float r, int slices, int stacks);
ENG_3D_MeshID eng3d_mesh_plane(ENG_3D* ctx, float w, float d);
ENG_3D_MeshID eng3d_mesh_cylinder(ENG_3D* ctx, float r, float h, int segs);
ENG_3D_MeshID eng3d_mesh_capsule(ENG_3D* ctx, float r, float h, int segs);
ENG_3D_MeshID eng3d_mesh_torus(ENG_3D* ctx, float R, float r, int segsR, int segsr);
ENG_3D_MeshID eng3d_mesh_load_obj(ENG_3D* ctx, const char* path);
void           eng3d_mesh_destroy(ENG_3D* ctx, ENG_3D_MeshID id);
int            eng3d_mesh_vertex_count(ENG_3D* ctx, ENG_3D_MeshID id);
ENG_3D_AABB    eng3d_mesh_bounds(ENG_3D* ctx, ENG_3D_MeshID id);

/* ══════════════════════════════════════════════════════
 * テクスチャ
 * ══════════════════════════════════════════════════════*/
ENG_3D_TexID eng3d_tex_load(ENG_3D* ctx, const char* path);
void         eng3d_tex_destroy(ENG_3D* ctx, ENG_3D_TexID id);

/* ══════════════════════════════════════════════════════
 * マテリアル
 * ══════════════════════════════════════════════════════*/
void eng3d_mesh_color(ENG_3D* ctx, ENG_3D_MeshID id, float r, float g, float b, float a);
void eng3d_mesh_texture(ENG_3D* ctx, ENG_3D_MeshID id, ENG_3D_TexID tex_id);
void eng3d_mesh_normal_map(ENG_3D* ctx, ENG_3D_MeshID id, ENG_3D_TexID tex_id);
void eng3d_mesh_specular(ENG_3D* ctx, ENG_3D_MeshID id, float intensity, float shininess);
void eng3d_mesh_emissive(ENG_3D* ctx, ENG_3D_MeshID id, float r, float g, float b, float intensity);
void eng3d_mesh_wireframe(ENG_3D* ctx, ENG_3D_MeshID id, bool on);
void eng3d_mesh_cast_shadow(ENG_3D* ctx, ENG_3D_MeshID id, bool on);
void eng3d_mesh_receive_shadow(ENG_3D* ctx, ENG_3D_MeshID id, bool on);
void eng3d_mesh_transparent(ENG_3D* ctx, ENG_3D_MeshID id, bool on);

/* ══════════════════════════════════════════════════════
 * 描画
 * ══════════════════════════════════════════════════════*/
void eng3d_begin(ENG_3D* ctx, float r, float g, float b);
void eng3d_draw(ENG_3D* ctx, ENG_3D_MeshID mesh_id,
                 float px, float py, float pz,
                 float rx, float ry, float rz,
                 float sx, float sy, float sz);
void eng3d_end(ENG_3D* ctx);

/* ══════════════════════════════════════════════════════
 * ポストプロセス — ブルーム
 * ══════════════════════════════════════════════════════*/
void eng3d_bloom_enable(ENG_3D* ctx, bool on);
void eng3d_bloom_threshold(ENG_3D* ctx, float threshold);
void eng3d_bloom_intensity(ENG_3D* ctx, float intensity);

/* ══════════════════════════════════════════════════════
 * パーティクルシステム
 * ══════════════════════════════════════════════════════*/
ENG_3D_EmitterID eng3d_emitter_create(ENG_3D* ctx, int max_particles);
void             eng3d_emitter_destroy(ENG_3D* ctx, ENG_3D_EmitterID id);
void             eng3d_emitter_pos(ENG_3D* ctx, ENG_3D_EmitterID id, float x, float y, float z);
void             eng3d_emitter_rate(ENG_3D* ctx, ENG_3D_EmitterID id, float rate);   /* 個/秒 */
void             eng3d_emitter_life(ENG_3D* ctx, ENG_3D_EmitterID id, float mn, float mx);
void             eng3d_emitter_velocity(ENG_3D* ctx, ENG_3D_EmitterID id,
                                         float vx, float vy, float vz, float spread);
void             eng3d_emitter_gravity(ENG_3D* ctx, ENG_3D_EmitterID id, float gx, float gy, float gz);
void             eng3d_emitter_color(ENG_3D* ctx, ENG_3D_EmitterID id, float r, float g, float b, float a);
void             eng3d_emitter_color_end(ENG_3D* ctx, ENG_3D_EmitterID id, float r, float g, float b, float a);
void             eng3d_emitter_size(ENG_3D* ctx, ENG_3D_EmitterID id, float start, float end);
void             eng3d_emitter_texture(ENG_3D* ctx, ENG_3D_EmitterID id, ENG_3D_TexID tex);
void             eng3d_emitter_active(ENG_3D* ctx, ENG_3D_EmitterID id, bool on);
void             eng3d_emitter_burst(ENG_3D* ctx, ENG_3D_EmitterID id, int count);
void             eng3d_emitter_update_draw(ENG_3D* ctx, ENG_3D_EmitterID id);

/* ══════════════════════════════════════════════════════
 * シーングラフ (ノード)
 * ══════════════════════════════════════════════════════*/
ENG_3D_NodeID eng3d_node_create(ENG_3D* ctx);
void          eng3d_node_destroy(ENG_3D* ctx, ENG_3D_NodeID id);
void          eng3d_node_parent(ENG_3D* ctx, ENG_3D_NodeID child, ENG_3D_NodeID parent);
void          eng3d_node_mesh(ENG_3D* ctx, ENG_3D_NodeID id, ENG_3D_MeshID mesh);
void          eng3d_node_pos(ENG_3D* ctx, ENG_3D_NodeID id, float x, float y, float z);
void          eng3d_node_rot(ENG_3D* ctx, ENG_3D_NodeID id, float x, float y, float z);
void          eng3d_node_scale(ENG_3D* ctx, ENG_3D_NodeID id, float x, float y, float z);
void          eng3d_node_active(ENG_3D* ctx, ENG_3D_NodeID id, bool on);
void          eng3d_node_draw(ENG_3D* ctx, ENG_3D_NodeID id);
void          eng3d_node_world_pos(ENG_3D* ctx, ENG_3D_NodeID id, float* x, float* y, float* z);

/* ══════════════════════════════════════════════════════
 * キーフレームアニメーション
 * ══════════════════════════════════════════════════════*/
ENG_3D_AnimID eng3d_anim_create(ENG_3D* ctx);
void          eng3d_anim_destroy(ENG_3D* ctx, ENG_3D_AnimID id);
void          eng3d_anim_key_pos(ENG_3D* ctx, ENG_3D_AnimID id, float t, float x, float y, float z);
void          eng3d_anim_key_rot(ENG_3D* ctx, ENG_3D_AnimID id, float t, float x, float y, float z);
void          eng3d_anim_key_scale(ENG_3D* ctx, ENG_3D_AnimID id, float t, float x, float y, float z);
void          eng3d_anim_play(ENG_3D* ctx, ENG_3D_AnimID id);
void          eng3d_anim_stop(ENG_3D* ctx, ENG_3D_AnimID id);
void          eng3d_anim_loop(ENG_3D* ctx, ENG_3D_AnimID id, bool on);
void          eng3d_anim_seek(ENG_3D* ctx, ENG_3D_AnimID id, float t);
void          eng3d_anim_update(ENG_3D* ctx, ENG_3D_AnimID id, float delta);
/** 現在時刻の TRS を取得 */
void          eng3d_anim_get_pos  (ENG_3D* ctx, ENG_3D_AnimID id, float* x, float* y, float* z);
void          eng3d_anim_get_rot  (ENG_3D* ctx, ENG_3D_AnimID id, float* x, float* y, float* z);
void          eng3d_anim_get_scale(ENG_3D* ctx, ENG_3D_AnimID id, float* x, float* y, float* z);
bool          eng3d_anim_is_playing(ENG_3D* ctx, ENG_3D_AnimID id);

/* ══════════════════════════════════════════════════════
 * レイキャスト
 * ══════════════════════════════════════════════════════*/
/** スクリーン座標からレイを飛ばして全メッシュの AABB テスト */
ENG_3D_RayHit eng3d_raycast_screen(ENG_3D* ctx, float sx, float sy);
/** ワールド座標 + 方向でレイを飛ばす */
ENG_3D_RayHit eng3d_raycast(ENG_3D* ctx,
                              float ox, float oy, float oz,
                              float dx, float dy, float dz);
bool           eng3d_aabb_overlap(ENG_3D_AABB a, ENG_3D_AABB b);
ENG_3D_AABB    eng3d_aabb_transform(ENG_3D_AABB aabb,
                                     float px, float py, float pz,
                                     float rx, float ry, float rz,
                                     float sx, float sy, float sz);

/* ══════════════════════════════════════════════════════
 * 入力
 * ══════════════════════════════════════════════════════*/
bool  eng3d_key(ENG_3D* ctx, int sdl_scancode);
bool  eng3d_key_down(ENG_3D* ctx, int sdl_scancode);  /* このフレームで押した */
bool  eng3d_key_up(ENG_3D* ctx, int sdl_scancode);    /* このフレームで離した */
float eng3d_mouse_x(ENG_3D* ctx);
float eng3d_mouse_y(ENG_3D* ctx);
float eng3d_mouse_dx(ENG_3D* ctx);
float eng3d_mouse_dy(ENG_3D* ctx);
bool  eng3d_mouse_btn(ENG_3D* ctx, int btn);           /* 1=左 2=中 3=右 */
bool  eng3d_mouse_btn_down(ENG_3D* ctx, int btn);
void  eng3d_mouse_relative(ENG_3D* ctx, bool on);     /* SDL 相対モード */
float eng3d_scroll(ENG_3D* ctx);                       /* マウスホイール */

#ifdef __cplusplus
}
#endif
