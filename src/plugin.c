/**
 * src/plugin.c — はじむ 3D エンジンプラグイン
 *
 * グローバルコンテキストで 1 ウィンドウ管理。
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "../../jp/include/hajimu_plugin.h"
#include "../include/eng_3d.h"
#include <SDL2/SDL_scancode.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── マクロ ─────────────────────────────────────────── */
#define ARG_NUM(i) ((i)<argc&&args[i].type==VALUE_NUMBER?args[i].number:0.0)
#define ARG_STR(i) ((i)<argc&&args[i].type==VALUE_STRING?args[i].string.data:"")
#define ARG_INT(i) ((int)ARG_NUM(i))
#define ARG_F(i)   ((float)ARG_NUM(i))
#define ARG_B(i)   ((i)<argc&&args[i].type==VALUE_BOOL?args[i].boolean:false)
#define NUM(v)     hajimu_number((double)(v))
#define BVAL(b)    hajimu_bool((bool)(b))
#define NUL        hajimu_null()
#define STR(s)     hajimu_string(s)

/* ─── グローバル状態 ─────────────────────────────────── */
static ENG_3D* g_ctx = NULL;

/* ════════════════════════════════════════════════════════
 * ライフサイクル
 * ════════════════════════════════════════════════════════*/
static Value fn_create(int argc, Value* args) {
    const char* title = argc>=1&&args[0].type==VALUE_STRING ? args[0].string.data : "3D";
    int w = ARG_INT(1); if(w<=0) w=800;
    int h = ARG_INT(2); if(h<=0) h=600;
    if (g_ctx) eng3d_destroy(g_ctx);
    g_ctx = eng3d_create(title, w, h);
    return BVAL(g_ctx!=NULL);
}

static Value fn_destroy(int argc, Value* args) {
    (void)argc;(void)args;
    if (g_ctx) { eng3d_destroy(g_ctx); g_ctx=NULL; }
    return NUL;
}

static Value fn_update(int argc, Value* args) {
    (void)argc;(void)args;
    if (!g_ctx) return BVAL(false);
    return BVAL(eng3d_update(g_ctx));
}

static Value fn_delta(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_delta(g_ctx) : 0.0f);
}

static Value fn_fps(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_fps(g_ctx) : 0);
}

/* ════════════════════════════════════════════════════════
 * カメラ
 * ════════════════════════════════════════════════════════*/
static Value fn_cam_perspective(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_cam_perspective(g_ctx, ARG_F(0), ARG_F(1), ARG_F(2));
    return NUL;
}
static Value fn_cam_pos(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_cam_pos(g_ctx, ARG_F(0), ARG_F(1), ARG_F(2));
    return NUL;
}
static Value fn_cam_target(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_cam_target(g_ctx, ARG_F(0), ARG_F(1), ARG_F(2));
    return NUL;
}
static Value fn_cam_lookat(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_cam_lookat(g_ctx, ARG_F(0),ARG_F(1),ARG_F(2),
                            ARG_F(3),ARG_F(4),ARG_F(5));
    return NUL;
}

/* ════════════════════════════════════════════════════════
 * ライティング
 * ════════════════════════════════════════════════════════*/
static Value fn_ambient(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_ambient(g_ctx, ARG_F(0), ARG_F(1), ARG_F(2));
    return NUL;
}
static Value fn_dir_light(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_dir_light(g_ctx, ARG_F(0),ARG_F(1),ARG_F(2),
                           ARG_F(3),ARG_F(4),ARG_F(5));
    return NUL;
}
static Value fn_point_light(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_point_light(g_ctx, ARG_INT(0),
                      ARG_F(1),ARG_F(2),ARG_F(3),
                      ARG_F(4),ARG_F(5),ARG_F(6),
                      ARG_F(7));
    return NUL;
}

/* ════════════════════════════════════════════════════════
 * メッシュ
 * ════════════════════════════════════════════════════════*/
static Value fn_mesh_cube(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    float w=ARG_F(0),h=ARG_F(1),d=ARG_F(2);
    if(w<=0)w=1; if(h<=0)h=1; if(d<=0)d=1;
    return NUM(eng3d_mesh_cube(g_ctx, w,h,d));
}
static Value fn_mesh_sphere(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    float r=ARG_F(0); if(r<=0)r=1.0f;
    int sl=ARG_INT(1); if(sl<3)sl=16;
    int st=ARG_INT(2); if(st<2)st=16;
    return NUM(eng3d_mesh_sphere(g_ctx, r,sl,st));
}
static Value fn_mesh_plane(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    float w=ARG_F(0),d=ARG_F(1);
    if(w<=0)w=10; if(d<=0)d=10;
    return NUM(eng3d_mesh_plane(g_ctx, w,d));
}
static Value fn_mesh_obj(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    return NUM(eng3d_mesh_load_obj(g_ctx, ARG_STR(0)));
}
static Value fn_mesh_destroy(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_mesh_destroy(g_ctx, ARG_INT(0));
    return NUL;
}
static Value fn_mesh_vertex_count(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    return NUM(eng3d_mesh_vertex_count(g_ctx, ARG_INT(0)));
}

/* ════════════════════════════════════════════════════════
 * テクスチャ / マテリアル
 * ════════════════════════════════════════════════════════*/
static Value fn_tex_load(int argc, Value* args) {
    if (!g_ctx) return NUM(0);
    return NUM(eng3d_tex_load(g_ctx, ARG_STR(0)));
}
static Value fn_tex_destroy(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_tex_destroy(g_ctx, ARG_INT(0));
    return NUL;
}
static Value fn_mesh_color(int argc, Value* args) {
    if (!g_ctx) return NUL;
    float a = argc>=5 ? ARG_F(4) : 1.0f;
    eng3d_mesh_color(g_ctx, ARG_INT(0), ARG_F(1),ARG_F(2),ARG_F(3), a);
    return NUL;
}
static Value fn_mesh_texture(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_mesh_texture(g_ctx, ARG_INT(0), ARG_INT(1));
    return NUL;
}
static Value fn_mesh_specular(int argc, Value* args) {
    if (!g_ctx) return NUL;
    eng3d_mesh_specular(g_ctx, ARG_INT(0), ARG_F(1), ARG_F(2));
    return NUL;
}

/* ════════════════════════════════════════════════════════
 * 描画
 * ════════════════════════════════════════════════════════*/
static Value fn_begin(int argc, Value* args) {
    if (!g_ctx) return NUL;
    float r=ARG_F(0),g=ARG_F(1),b=ARG_F(2);
    eng3d_begin(g_ctx, r,g,b);
    return NUL;
}
static Value fn_draw(int argc, Value* args) {
    if (!g_ctx) return NUL;
    int id=ARG_INT(0);
    float px=ARG_F(1),py=ARG_F(2),pz=ARG_F(3);
    float rx=ARG_F(4),ry=ARG_F(5),rz=ARG_F(6);
    float sx=ARG_F(7),sy=ARG_F(8),sz=ARG_F(9);
    if(argc<8){sx=sy=sz=1.0f;}
    eng3d_draw(g_ctx, id, px,py,pz, rx,ry,rz, sx,sy,sz);
    return NUL;
}
static Value fn_end(int argc, Value* args) {
    (void)argc;(void)args;
    if (!g_ctx) return NUL;
    eng3d_end(g_ctx);
    return NUL;
}

/* ════════════════════════════════════════════════════════
 * 入力
 * ════════════════════════════════════════════════════════*/
/* キー名 → SDL_Scancode 変換 (最低限) */
static int key_to_scancode(const char* name) {
    if (!name) return 0;
    if (!strcmp(name,"w")||!strcmp(name,"W"))  return SDL_SCANCODE_W;
    if (!strcmp(name,"a")||!strcmp(name,"A"))  return SDL_SCANCODE_A;
    if (!strcmp(name,"s")||!strcmp(name,"S"))  return SDL_SCANCODE_S;
    if (!strcmp(name,"d")||!strcmp(name,"D"))  return SDL_SCANCODE_D;
    if (!strcmp(name,"q")||!strcmp(name,"Q"))  return SDL_SCANCODE_Q;
    if (!strcmp(name,"e")||!strcmp(name,"E"))  return SDL_SCANCODE_E;
    if (!strcmp(name,"上")||!strcmp(name,"up"))    return SDL_SCANCODE_UP;
    if (!strcmp(name,"下")||!strcmp(name,"down"))  return SDL_SCANCODE_DOWN;
    if (!strcmp(name,"左")||!strcmp(name,"left"))  return SDL_SCANCODE_LEFT;
    if (!strcmp(name,"右")||!strcmp(name,"right")) return SDL_SCANCODE_RIGHT;
    if (!strcmp(name,"スペース")||!strcmp(name,"space")) return SDL_SCANCODE_SPACE;
    /* 数字 */
    if (name[0]>='0'&&name[0]<='9'&&name[1]==0) return SDL_SCANCODE_0+(name[0]-'0');
    return 0;
}

static Value fn_key(int argc, Value* args) {
    if (!g_ctx) return BVAL(false);
    return BVAL(eng3d_key(g_ctx, key_to_scancode(ARG_STR(0))));
}
static Value fn_mouse_x(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_mouse_x(g_ctx) : 0);
}
static Value fn_mouse_y(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_mouse_y(g_ctx) : 0);
}
static Value fn_mouse_dx(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_mouse_dx(g_ctx) : 0);
}
static Value fn_mouse_dy(int argc, Value* args) {
    (void)argc;(void)args;
    return NUM(g_ctx ? eng3d_mouse_dy(g_ctx) : 0);
}
static Value fn_mouse_btn(int argc, Value* args) {
    if (!g_ctx) return BVAL(false);
    return BVAL(eng3d_mouse_btn(g_ctx, ARG_INT(0)));
}

/* ════════════════════════════════════════════════════════
 * プラグイン登録テーブル
 * ════════════════════════════════════════════════════════*/
static HajimuPluginFunc funcs[] = {
    /* ライフサイクル */
    {"3Dウィンドウ作成",    fn_create,           3,  3},
    {"3Dウィンドウ削除",    fn_destroy,          0,  0},
    {"3D更新",              fn_update,           0,  0},
    {"3Dデルタ時間",        fn_delta,            0,  0},
    {"3DFPS",               fn_fps,              0,  0},
    /* カメラ */
    {"3D視野角設定",        fn_cam_perspective,  3,  3},
    {"3Dカメラ位置",        fn_cam_pos,          3,  3},
    {"3Dカメラ注視点",      fn_cam_target,       3,  3},
    {"3Dカメラ設定",        fn_cam_lookat,       6,  6},
    /* ライティング */
    {"3D環境光設定",        fn_ambient,          3,  3},
    {"3D平行光設定",        fn_dir_light,        6,  6},
    {"3Dポイントライト",    fn_point_light,      8,  8},
    /* メッシュ */
    {"3Dキューブ作成",      fn_mesh_cube,        3,  3},
    {"3Dスフィア作成",      fn_mesh_sphere,      3,  3},
    {"3Dプレーン作成",      fn_mesh_plane,       2,  2},
    {"3DOBJ読込",           fn_mesh_obj,         1,  1},
    {"3Dメッシュ削除",      fn_mesh_destroy,     1,  1},
    {"3Dメッシュ頂点数",    fn_mesh_vertex_count,1,  1},
    /* テクスチャ / マテリアル */
    {"3Dテクスチャ読込",    fn_tex_load,         1,  1},
    {"3Dテクスチャ削除",    fn_tex_destroy,      1,  1},
    {"3Dメッシュ色設定",    fn_mesh_color,       4,  5},
    {"3Dメッシュテクスチャ",fn_mesh_texture,     2,  2},
    {"3Dスペキュラー設定",  fn_mesh_specular,    3,  3},
    /* 描画 */
    {"3D描画開始",          fn_begin,            3,  3},
    {"3Dメッシュ描画",      fn_draw,             7, 10},
    {"3D描画終了",          fn_end,              0,  0},
    /* 入力 */
    {"3Dキー押下中",        fn_key,              1,  1},
    {"3Dマウス X",          fn_mouse_x,          0,  0},
    {"3Dマウス Y",          fn_mouse_y,          0,  0},
    {"3Dマウス DX",         fn_mouse_dx,         0,  0},
    {"3Dマウス DY",         fn_mouse_dy,         0,  0},
    {"3Dマウスボタン",      fn_mouse_btn,        1,  1},
};

HAJIMU_PLUGIN_EXPORT HajimuPluginInfo* hajimu_plugin_init(void) {
    static HajimuPluginInfo info = {
        .name     = "engine_3d",
        .version  = "1.0.0",
        .author      = "Reo Shiozawa",
        .description = "SDL2+OpenGL 3.3 3Dレンダリングエンジン",
        .functions = funcs,
        .function_count = sizeof(funcs)/sizeof(funcs[0]),
    };
    return &info;
}
