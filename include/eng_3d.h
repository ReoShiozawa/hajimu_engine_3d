/**
 * include/eng_3d.h — はじむ用 3D レンダリングエンジン API
 *
 * SDL2 + OpenGL 3.3 Core Profile + Phong 照明モデル
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
typedef int ENG_3D_MeshID;   /* 1-origin; 0=無効 */
typedef int ENG_3D_TexID;    /* 1-origin; 0=なし */

#define ENG_3D_MAX_MESHES  64
#define ENG_3D_MAX_TEXTURES 32
#define ENG_3D_MAX_LIGHTS   4

/* ════════════════════════════════════════════════════════
 * ライフサイクル
 * ════════════════════════════════════════════════════════*/

/** 3D ウィンドウ + OpenGL コンテキストを生成する。 */
ENG_3D* eng3d_create(const char* title, int w, int h);

/** 全リソースを開放する。 */
void    eng3d_destroy(ENG_3D* ctx);

/** イベント処理・バッファスワップ。終了要求で false を返す。 */
bool    eng3d_update(ENG_3D* ctx);

/** デルタ時間 (秒) */
float   eng3d_delta(ENG_3D* ctx);

/** FPS (直近) */
int     eng3d_fps(ENG_3D* ctx);

/* ════════════════════════════════════════════════════════
 * カメラ
 * ════════════════════════════════════════════════════════*/

/** パースペクティブ設定 (fov=度, near/far=距離)。 */
void eng3d_cam_perspective(ENG_3D* ctx, float fov_deg, float near_z, float far_z);

/** カメラ位置 */
void eng3d_cam_pos(ENG_3D* ctx, float x, float y, float z);

/** 注視点 */
void eng3d_cam_target(ENG_3D* ctx, float tx, float ty, float tz);

/** lookAt ショートカット。位置と注視点を同時設定。 */
void eng3d_cam_lookat(ENG_3D* ctx,
                       float ex, float ey, float ez,
                       float tx, float ty, float tz);

/* ════════════════════════════════════════════════════════
 * ライティング
 * ════════════════════════════════════════════════════════*/

/** 環境光 (0.0〜1.0) */
void eng3d_ambient(ENG_3D* ctx, float r, float g, float b);

/** 方向ライト (方向ベクトルは光源→シーン; 内部で正規化)。 */
void eng3d_dir_light(ENG_3D* ctx,
                      float dx, float dy, float dz,
                      float r, float g, float b);

/** ポイントライト (slot=0〜3)。radius=0 で無効化。 */
void eng3d_point_light(ENG_3D* ctx, int slot,
                        float x, float y, float z,
                        float r, float g, float b,
                        float radius);

/* ════════════════════════════════════════════════════════
 * メッシュ
 * ════════════════════════════════════════════════════════*/

/** 直方体メッシュを生成 (w,h,d = 各辺の長さ)。 */
ENG_3D_MeshID eng3d_mesh_cube(ENG_3D* ctx, float w, float h, float d);

/** 球メッシュ (r=半径, slices/stacks=分割数)。 */
ENG_3D_MeshID eng3d_mesh_sphere(ENG_3D* ctx, float r, int slices, int stacks);

/** 平面メッシュ (XZ 平面, w×d)。 */
ENG_3D_MeshID eng3d_mesh_plane(ENG_3D* ctx, float w, float d);

/** 簡易 OBJ ファイル読込 (頂点位置・法線・UV に対応)。 */
ENG_3D_MeshID eng3d_mesh_load_obj(ENG_3D* ctx, const char* path);

/** メッシュを削除する。 */
void eng3d_mesh_destroy(ENG_3D* ctx, ENG_3D_MeshID id);

/** メッシュの頂点数を返す。 */
int  eng3d_mesh_vertex_count(ENG_3D* ctx, ENG_3D_MeshID id);

/* ════════════════════════════════════════════════════════
 * テクスチャ
 * ════════════════════════════════════════════════════════*/

/** PNG/JPG/BMP を読み込む。 */
ENG_3D_TexID eng3d_tex_load(ENG_3D* ctx, const char* path);

/** テクスチャを削除する。 */
void         eng3d_tex_destroy(ENG_3D* ctx, ENG_3D_TexID id);

/* ════════════════════════════════════════════════════════
 * マテリアル
 * ════════════════════════════════════════════════════════*/

/** メッシュのベースカラーを設定 (テクスチャなしの場合に使用)。 */
void eng3d_mesh_color(ENG_3D* ctx, ENG_3D_MeshID id, float r, float g, float b, float a);

/** メッシュにテクスチャを設定 (tex_id=0 で解除)。 */
void eng3d_mesh_texture(ENG_3D* ctx, ENG_3D_MeshID id, ENG_3D_TexID tex_id);

/** スペキュラー強度 / シャープネス */
void eng3d_mesh_specular(ENG_3D* ctx, ENG_3D_MeshID id, float intensity, float shininess);

/* ════════════════════════════════════════════════════════
 * 描画
 * ════════════════════════════════════════════════════════*/

/** フレーム開始 (バッファクリア + ビュー行列更新)。 */
void eng3d_begin(ENG_3D* ctx, float r, float g, float b);

/** メッシュを描画する。
 *  px/py/pz: ワールド位置
 *  rx/ry/rz: オイラー回転 (度)
 *  sx/sy/sz: スケール (1.0=等倍) */
void eng3d_draw(ENG_3D* ctx, ENG_3D_MeshID mesh_id,
                 float px, float py, float pz,
                 float rx, float ry, float rz,
                 float sx, float sy, float sz);

/** フレーム終了 (スワップ)。 */
void eng3d_end(ENG_3D* ctx);

/* ════════════════════════════════════════════════════════
 * 入力 (render エンジンが使えない場合のフォールバック)
 * ════════════════════════════════════════════════════════*/
bool eng3d_key(ENG_3D* ctx, int sdl_scancode);
float eng3d_mouse_x(ENG_3D* ctx);
float eng3d_mouse_y(ENG_3D* ctx);
float eng3d_mouse_dx(ENG_3D* ctx);
float eng3d_mouse_dy(ENG_3D* ctx);
bool eng3d_mouse_btn(ENG_3D* ctx, int btn);  /* 1=左,2=中,3=右 */

#ifdef __cplusplus
}
#endif
