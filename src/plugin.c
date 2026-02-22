/*
 * jp-engine_3d  plugin.c  v2.0.0
 * Unity レベル 3D エンジン - はじむプラグインバインディング
 */
#define GL_SILENCE_DEPRECATION
#include "../../jp/include/hajimu_plugin.h"
#include "../include/eng_3d.h"
#include <SDL2/SDL_scancode.h>

/* ──────────────────────────────────────────────
 * 共通ヘルパー
 * ──────────────────────────────────────────────*/
static ENG_3D* g_ctx = NULL;  /* シングルトン */

static double    NUM(Value* v){ return (v&&v->type==VALUE_NUMBER)?v->number:0.0; }
static bool      BOL(Value* v){ return (v&&v->type==VALUE_BOOL)?v->boolean:(v&&v->type==VALUE_NUMBER)?v->number!=0:false; }
static const char* STR(Value* v){ return (v&&v->type==VALUE_STRING)?v->string.data:""; }
static Value vN(double n){ Value v={0}; v.type=VALUE_NUMBER; v.number=n; return v; }
static Value vB(bool b) { Value v={0}; v.type=VALUE_BOOL;   v.boolean=b; return v; }
static Value vNULL(void){ Value v={0}; v.type=VALUE_NULL; return v; }

/* ══════════════════════════════════════════════
 * ライフサイクル
 * ══════════════════════════════════════════════*/
static Value p_create(int argc, Value* argv){
    const char* title = argc>=1?STR(&argv[0]):"3D";
    int w = argc>=2?(int)NUM(&argv[1]):800;
    int h = argc>=3?(int)NUM(&argv[2]):600;
    g_ctx = eng3d_create(title,w,h);
    return vB(g_ctx!=NULL);
}
static Value p_destroy(int argc, Value* argv){ (void)argc;(void)argv; if(g_ctx){eng3d_destroy(g_ctx);g_ctx=NULL;} return vNULL(); }
static Value p_update (int argc, Value* argv){ (void)argc;(void)argv; return g_ctx?vB(eng3d_update(g_ctx)):vB(false); }
static Value p_delta  (int argc, Value* argv){ (void)argc;(void)argv; return g_ctx?vN(eng3d_delta(g_ctx)):vN(0); }
static Value p_fps    (int argc, Value* argv){ (void)argc;(void)argv; return g_ctx?vN(eng3d_fps(g_ctx)):vN(0); }
static Value p_width  (int argc, Value* argv){ (void)argc;(void)argv; return g_ctx?vN(eng3d_width(g_ctx)):vN(0); }
static Value p_height (int argc, Value* argv){ (void)argc;(void)argv; return g_ctx?vN(eng3d_height(g_ctx)):vN(0); }
/* 描画 */
static Value p_begin  (int argc, Value* argv){
    float r=argc>=1?(float)NUM(&argv[0]):0.1f;
    float g=argc>=2?(float)NUM(&argv[1]):0.1f;
    float b=argc>=3?(float)NUM(&argv[2]):0.1f;
    if(g_ctx) eng3d_begin(g_ctx,r,g,b);
    return vNULL();
}
static Value p_draw(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    ENG_3D_MeshID id=(ENG_3D_MeshID)(int)NUM(&argv[0]);
    float px=argc>=2?(float)NUM(&argv[1]):0;
    float py=argc>=3?(float)NUM(&argv[2]):0;
    float pz=argc>=4?(float)NUM(&argv[3]):0;
    float rx=argc>=5?(float)NUM(&argv[4]):0;
    float ry=argc>=6?(float)NUM(&argv[5]):0;
    float rz=argc>=7?(float)NUM(&argv[6]):0;
    float sx=argc>=8?(float)NUM(&argv[7]):1;
    float sy=argc>=9?(float)NUM(&argv[8]):1;
    float sz=argc>=10?(float)NUM(&argv[9]):1;
    eng3d_draw(g_ctx,id,px,py,pz,rx,ry,rz,sx,sy,sz);
    return vNULL();
}
static Value p_end(int argc, Value* argv){ (void)argc;(void)argv; if(g_ctx) eng3d_end(g_ctx); return vNULL(); }

/* ══════════════════════════════════════════════
 * カメラ
 * ══════════════════════════════════════════════*/
static Value p_cam_perspective(int argc, Value* argv){
    if(!g_ctx) return vNULL();
    float fov=argc>=1?(float)NUM(&argv[0]):60;
    float n  =argc>=2?(float)NUM(&argv[1]):0.1f;
    float f  =argc>=3?(float)NUM(&argv[2]):500;
    eng3d_cam_perspective(g_ctx,fov,n,f); return vNULL();
}
static Value p_cam_pos(int argc, Value* argv){
    if(!g_ctx) return vNULL();
    eng3d_cam_pos(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));
    return vNULL();
}
static Value p_cam_target(int argc, Value* argv){
    if(!g_ctx) return vNULL();
    eng3d_cam_target(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));
    return vNULL();
}
static Value p_cam_lookat(int argc, Value* argv){
    if(!g_ctx||argc<6) return vNULL();
    eng3d_cam_lookat(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),
                           (float)NUM(&argv[3]),(float)NUM(&argv[4]),(float)NUM(&argv[5]));
    return vNULL();
}

/* ══════════════════════════════════════════════
 * ライティング
 * ══════════════════════════════════════════════*/
static Value p_ambient(int argc, Value* argv){
    if(!g_ctx||argc<3) return vNULL();
    eng3d_ambient(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));
    return vNULL();
}
static Value p_dir_light(int argc, Value* argv){
    if(!g_ctx||argc<6) return vNULL();
    eng3d_dir_light(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),
                          (float)NUM(&argv[3]),(float)NUM(&argv[4]),(float)NUM(&argv[5]));
    return vNULL();
}
static Value p_point_light(int argc, Value* argv){
    if(!g_ctx||argc<8) return vNULL();
    eng3d_point_light(g_ctx,(int)NUM(&argv[0]),
        (float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),
        (float)NUM(&argv[4]),(float)NUM(&argv[5]),(float)NUM(&argv[6]),
        (float)NUM(&argv[7]));
    return vNULL();
}
static Value p_spot_light(int argc, Value* argv){
    if(!g_ctx||argc<12) return vNULL();
    eng3d_spot_light(g_ctx,(int)NUM(&argv[0]),
        (float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),
        (float)NUM(&argv[4]),(float)NUM(&argv[5]),(float)NUM(&argv[6]),
        (float)NUM(&argv[7]),(float)NUM(&argv[8]),(float)NUM(&argv[9]),
        (float)NUM(&argv[10]),(float)NUM(&argv[11]),
        argc>=13?(float)NUM(&argv[12]):30.f);
    return vNULL();
}
static Value p_spot_light_off(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    eng3d_spot_light_off(g_ctx,(int)NUM(&argv[0])); return vNULL();
}
/* シャドウ / フォグ / ブルーム */
static Value p_shadow_enable(int argc, Value* argv){if(!g_ctx)return vNULL();eng3d_shadow_enable(g_ctx,argc>=1?BOL(&argv[0]):true);return vNULL();}
static Value p_shadow_bias  (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_shadow_bias(g_ctx,(float)NUM(&argv[0]));return vNULL();}
static Value p_shadow_size  (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_shadow_size(g_ctx,(float)NUM(&argv[0]));return vNULL();}
static Value p_fog_enable   (int argc, Value* argv){if(!g_ctx)return vNULL();eng3d_fog_enable(g_ctx,argc>=1?BOL(&argv[0]):true);return vNULL();}
static Value p_fog(int argc, Value* argv){
    if(!g_ctx||argc<7) return vNULL();
    eng3d_fog(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),
              (int)NUM(&argv[3]),(float)NUM(&argv[4]),(float)NUM(&argv[5]),(float)NUM(&argv[6]));
    return vNULL();
}
static Value p_bloom_enable   (int argc, Value* argv){if(!g_ctx)return vNULL();eng3d_bloom_enable(g_ctx,argc>=1?BOL(&argv[0]):true);return vNULL();}
static Value p_bloom_threshold(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_bloom_threshold(g_ctx,(float)NUM(&argv[0]));return vNULL();}
static Value p_bloom_intensity(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_bloom_intensity(g_ctx,(float)NUM(&argv[0]));return vNULL();}

/* ══════════════════════════════════════════════
 * スカイボックス
 * ══════════════════════════════════════════════*/
static Value p_skybox_load(int argc, Value* argv){
    if(!g_ctx||argc<6) return vB(false);
    return vB(eng3d_skybox_load(g_ctx,STR(&argv[0]),STR(&argv[1]),
        STR(&argv[2]),STR(&argv[3]),STR(&argv[4]),STR(&argv[5])));
}
static Value p_skybox_draw  (int argc, Value* argv){(void)argc;(void)argv;if(g_ctx)eng3d_skybox_draw(g_ctx);return vNULL();}
static Value p_skybox_unload(int argc, Value* argv){(void)argc;(void)argv;if(g_ctx)eng3d_skybox_unload(g_ctx);return vNULL();}

/* ══════════════════════════════════════════════
 * メッシュ
 * ══════════════════════════════════════════════*/
static Value p_mesh_cube    (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_cube(g_ctx,argc>=1?(float)NUM(&argv[0]):1,argc>=2?(float)NUM(&argv[1]):1,argc>=3?(float)NUM(&argv[2]):1));}
static Value p_mesh_sphere  (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_sphere(g_ctx,argc>=1?(float)NUM(&argv[0]):0.5f,argc>=2?(int)NUM(&argv[1]):16,argc>=3?(int)NUM(&argv[2]):8));}
static Value p_mesh_plane   (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_plane(g_ctx,argc>=1?(float)NUM(&argv[0]):1,argc>=2?(float)NUM(&argv[1]):1));}
static Value p_mesh_cylinder(int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_cylinder(g_ctx,argc>=1?(float)NUM(&argv[0]):0.5f,argc>=2?(float)NUM(&argv[1]):1,argc>=3?(int)NUM(&argv[2]):16));}
static Value p_mesh_capsule (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_capsule(g_ctx,argc>=1?(float)NUM(&argv[0]):0.5f,argc>=2?(float)NUM(&argv[1]):1,argc>=3?(int)NUM(&argv[2]):16));}
static Value p_mesh_torus   (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_mesh_torus(g_ctx,argc>=1?(float)NUM(&argv[0]):1,argc>=2?(float)NUM(&argv[1]):0.3f,argc>=3?(int)NUM(&argv[2]):32,argc>=4?(int)NUM(&argv[3]):16));}
static Value p_mesh_load_obj(int argc, Value* argv){if(!g_ctx||argc<1)return vN(0);return vN(eng3d_mesh_load_obj(g_ctx,STR(&argv[0])));}
static Value p_mesh_destroy (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_mesh_destroy(g_ctx,(ENG_3D_MeshID)(int)NUM(&argv[0]));return vNULL();}
static Value p_mesh_vertex_count(int argc, Value* argv){if(!g_ctx||argc<1)return vN(0);return vN(eng3d_mesh_vertex_count(g_ctx,(ENG_3D_MeshID)(int)NUM(&argv[0])));}

/* ══════════════════════════════════════════════
 * テクスチャ
 * ══════════════════════════════════════════════*/
static Value p_tex_load   (int argc, Value* argv){if(!g_ctx||argc<1)return vN(0);return vN(eng3d_tex_load(g_ctx,STR(&argv[0])));}
static Value p_tex_destroy(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_tex_destroy(g_ctx,(ENG_3D_TexID)(int)NUM(&argv[0]));return vNULL();}

/* ══════════════════════════════════════════════
 * マテリアル
 * ══════════════════════════════════════════════*/
static Value p_mesh_color(int argc, Value* argv){
    if(!g_ctx||argc<5)return vNULL();
    eng3d_mesh_color(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));
    return vNULL();
}
static Value p_mesh_texture   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_texture(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_mesh_normal_map(int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_normal_map(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_mesh_specular  (int argc, Value* argv){if(!g_ctx||argc<3)return vNULL();eng3d_mesh_specular(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));return vNULL();}
static Value p_mesh_emissive  (int argc, Value* argv){
    if(!g_ctx||argc<5)return vNULL();
    eng3d_mesh_emissive(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));
    return vNULL();
}
static Value p_mesh_wireframe     (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_wireframe(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_mesh_cast_shadow   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_cast_shadow(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_mesh_recv_shadow   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_receive_shadow(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_mesh_transparent   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_mesh_transparent(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}

/* ══════════════════════════════════════════════
 * パーティクル
 * ══════════════════════════════════════════════*/
static Value p_emit_create  (int argc, Value* argv){if(!g_ctx)return vN(0);return vN(eng3d_emitter_create(g_ctx,argc>=1?(int)NUM(&argv[0]):100));}
static Value p_emit_destroy (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_emitter_destroy(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_emit_pos(int argc, Value* argv){if(!g_ctx||argc<4)return vNULL();eng3d_emitter_pos(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]));return vNULL();}
static Value p_emit_rate(int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_emitter_rate(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]));return vNULL();}
static Value p_emit_life(int argc, Value* argv){if(!g_ctx||argc<3)return vNULL();eng3d_emitter_life(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));return vNULL();}
static Value p_emit_velocity(int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_emitter_velocity(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_emit_gravity (int argc, Value* argv){if(!g_ctx||argc<4)return vNULL();eng3d_emitter_gravity(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]));return vNULL();}
static Value p_emit_color   (int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_emitter_color(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_emit_color_end(int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_emitter_color_end(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_emit_size    (int argc, Value* argv){if(!g_ctx||argc<3)return vNULL();eng3d_emitter_size(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]));return vNULL();}
static Value p_emit_texture (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_emitter_texture(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_emit_active  (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_emitter_active(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_emit_burst   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_emitter_burst(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_emit_update  (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_emitter_update_draw(g_ctx,(int)NUM(&argv[0]));return vNULL();}

/* ══════════════════════════════════════════════
 * シーングラフ
 * ══════════════════════════════════════════════*/
static Value p_node_create (int argc, Value* argv){(void)argc;(void)argv;if(!g_ctx)return vN(0);return vN(eng3d_node_create(g_ctx));}
static Value p_node_destroy(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_node_destroy(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_node_parent (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_node_parent(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_node_mesh   (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_node_mesh(g_ctx,(int)NUM(&argv[0]),(int)NUM(&argv[1]));return vNULL();}
static Value p_node_pos    (int argc, Value* argv){if(!g_ctx||argc<4)return vNULL();eng3d_node_pos(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]));return vNULL();}
static Value p_node_rot    (int argc, Value* argv){if(!g_ctx||argc<4)return vNULL();eng3d_node_rot(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]));return vNULL();}
static Value p_node_scale  (int argc, Value* argv){if(!g_ctx||argc<4)return vNULL();eng3d_node_scale(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]));return vNULL();}
static Value p_node_active (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_node_active(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_node_draw   (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_node_draw(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_node_world_pos(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    float x=0,y=0,z=0;
    eng3d_node_world_pos(g_ctx,(int)NUM(&argv[0]),&x,&y,&z);
    /* 配列 [x,y,z] を返す */
    Value arr={0}; arr.type=VALUE_ARRAY;
    arr.array.elements=(Value*)calloc(3,sizeof(Value));
    arr.array.elements[0]=vN(x); arr.array.elements[1]=vN(y); arr.array.elements[2]=vN(z);
    arr.array.length=arr.array.capacity=3;
    return arr;
}

/* ══════════════════════════════════════════════
 * アニメーション
 * ══════════════════════════════════════════════*/
static Value p_anim_create (int argc, Value* argv){(void)argc;(void)argv;if(!g_ctx)return vN(0);return vN(eng3d_anim_create(g_ctx));}
static Value p_anim_destroy(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_anim_destroy(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_anim_key_pos  (int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_anim_key_pos(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_anim_key_rot  (int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_anim_key_rot(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_anim_key_scale(int argc, Value* argv){if(!g_ctx||argc<5)return vNULL();eng3d_anim_key_scale(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),(float)NUM(&argv[3]),(float)NUM(&argv[4]));return vNULL();}
static Value p_anim_play      (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_anim_play(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_anim_stop      (int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_anim_stop(g_ctx,(int)NUM(&argv[0]));return vNULL();}
static Value p_anim_loop      (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_anim_loop(g_ctx,(int)NUM(&argv[0]),BOL(&argv[1]));return vNULL();}
static Value p_anim_seek      (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_anim_seek(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]));return vNULL();}
static Value p_anim_update    (int argc, Value* argv){if(!g_ctx||argc<2)return vNULL();eng3d_anim_update(g_ctx,(int)NUM(&argv[0]),(float)NUM(&argv[1]));return vNULL();}
static Value p_anim_is_playing(int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_anim_is_playing(g_ctx,(int)NUM(&argv[0])));}

static Value p_anim_get_pos(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    float x=0,y=0,z=0; eng3d_anim_get_pos(g_ctx,(int)NUM(&argv[0]),&x,&y,&z);
    Value a={0}; a.type=VALUE_ARRAY; a.array.elements=(Value*)calloc(3,sizeof(Value));
    a.array.elements[0]=vN(x); a.array.elements[1]=vN(y); a.array.elements[2]=vN(z);
    a.array.length=a.array.capacity=3; return a;
}
static Value p_anim_get_rot(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    float x=0,y=0,z=0; eng3d_anim_get_rot(g_ctx,(int)NUM(&argv[0]),&x,&y,&z);
    Value a={0}; a.type=VALUE_ARRAY; a.array.elements=(Value*)calloc(3,sizeof(Value));
    a.array.elements[0]=vN(x); a.array.elements[1]=vN(y); a.array.elements[2]=vN(z);
    a.array.length=a.array.capacity=3; return a;
}
static Value p_anim_get_scale(int argc, Value* argv){
    if(!g_ctx||argc<1) return vNULL();
    float x=1,y=1,z=1; eng3d_anim_get_scale(g_ctx,(int)NUM(&argv[0]),&x,&y,&z);
    Value a={0}; a.type=VALUE_ARRAY; a.array.elements=(Value*)calloc(3,sizeof(Value));
    a.array.elements[0]=vN(x); a.array.elements[1]=vN(y); a.array.elements[2]=vN(z);
    a.array.length=a.array.capacity=3; return a;
}

/* ══════════════════════════════════════════════
 * レイキャスト
 * ══════════════════════════════════════════════*/
static Value p_raycast(int argc, Value* argv){
    if(!g_ctx||argc<6) return vNULL();
    ENG_3D_RayHit h=eng3d_raycast(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]),(float)NUM(&argv[2]),
                                        (float)NUM(&argv[3]),(float)NUM(&argv[4]),(float)NUM(&argv[5]));
    Value d={0}; d.type=VALUE_DICT;
    d.dict.keys=(char**)calloc(5,sizeof(char*)); d.dict.values=(Value*)calloc(5,sizeof(Value));
    d.dict.keys[0]=strdup("当たり"); d.dict.values[0]=vB(h.hit);
    d.dict.keys[1]=strdup("距離");   d.dict.values[1]=vN(h.dist);
    d.dict.keys[2]=strdup("x");     d.dict.values[2]=vN(h.x);
    d.dict.keys[3]=strdup("y");     d.dict.values[3]=vN(h.y);
    d.dict.keys[4]=strdup("z");     d.dict.values[4]=vN(h.z);
    d.dict.length=d.dict.capacity=5; return d;
}
static Value p_raycast_screen(int argc, Value* argv){
    if(!g_ctx||argc<2) return vNULL();
    ENG_3D_RayHit h=eng3d_raycast_screen(g_ctx,(float)NUM(&argv[0]),(float)NUM(&argv[1]));
    Value d={0}; d.type=VALUE_DICT;
    d.dict.keys=(char**)calloc(5,sizeof(char*)); d.dict.values=(Value*)calloc(5,sizeof(Value));
    d.dict.keys[0]=strdup("当たり"); d.dict.values[0]=vB(h.hit);
    d.dict.keys[1]=strdup("距離");   d.dict.values[1]=vN(h.dist);
    d.dict.keys[2]=strdup("x");     d.dict.values[2]=vN(h.x);
    d.dict.keys[3]=strdup("y");     d.dict.values[3]=vN(h.y);
    d.dict.keys[4]=strdup("z");     d.dict.values[4]=vN(h.z);
    d.dict.length=d.dict.capacity=5; return d;
}

/* ══════════════════════════════════════════════
 * 入力
 * ══════════════════════════════════════════════*/
static Value p_key       (int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_key(g_ctx,(int)NUM(&argv[0])));}
static Value p_key_down  (int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_key_down(g_ctx,(int)NUM(&argv[0])));}
static Value p_key_up    (int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_key_up(g_ctx,(int)NUM(&argv[0])));}
static Value p_mouse_x   (int argc, Value* argv){(void)argc;(void)argv;return g_ctx?vN(eng3d_mouse_x(g_ctx)):vN(0);}
static Value p_mouse_y   (int argc, Value* argv){(void)argc;(void)argv;return g_ctx?vN(eng3d_mouse_y(g_ctx)):vN(0);}
static Value p_mouse_dx  (int argc, Value* argv){(void)argc;(void)argv;return g_ctx?vN(eng3d_mouse_dx(g_ctx)):vN(0);}
static Value p_mouse_dy  (int argc, Value* argv){(void)argc;(void)argv;return g_ctx?vN(eng3d_mouse_dy(g_ctx)):vN(0);}
static Value p_scroll    (int argc, Value* argv){(void)argc;(void)argv;return g_ctx?vN(eng3d_scroll(g_ctx)):vN(0);}
static Value p_mouse_btn (int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_mouse_btn(g_ctx,(int)NUM(&argv[0])));}
static Value p_mouse_btn_down(int argc, Value* argv){if(!g_ctx||argc<1)return vB(false);return vB(eng3d_mouse_btn_down(g_ctx,(int)NUM(&argv[0])));}
static Value p_mouse_relative(int argc, Value* argv){if(!g_ctx||argc<1)return vNULL();eng3d_mouse_relative(g_ctx,BOL(&argv[0]));return vNULL();}

/* SDL スキャンコード定数 */
static Value p_key_code(int argc, Value* argv){
    if(argc<1) return vN(0);
    const char* name=STR(&argv[0]);
    /* よく使うキー */
    if(!strcmp(name,"W")||!strcmp(name,"w")) return vN(SDL_SCANCODE_W);
    if(!strcmp(name,"A")||!strcmp(name,"a")) return vN(SDL_SCANCODE_A);
    if(!strcmp(name,"S")||!strcmp(name,"s")) return vN(SDL_SCANCODE_S);
    if(!strcmp(name,"D")||!strcmp(name,"d")) return vN(SDL_SCANCODE_D);
    if(!strcmp(name,"Q")||!strcmp(name,"q")) return vN(SDL_SCANCODE_Q);
    if(!strcmp(name,"E")||!strcmp(name,"e")) return vN(SDL_SCANCODE_E);
    if(!strcmp(name,"空白")||!strcmp(name,"SPACE")) return vN(SDL_SCANCODE_SPACE);
    if(!strcmp(name,"ESC")||!strcmp(name,"脱出")) return vN(SDL_SCANCODE_ESCAPE);
    if(!strcmp(name,"上")||!strcmp(name,"UP"))    return vN(SDL_SCANCODE_UP);
    if(!strcmp(name,"下")||!strcmp(name,"DOWN"))  return vN(SDL_SCANCODE_DOWN);
    if(!strcmp(name,"左")||!strcmp(name,"LEFT"))  return vN(SDL_SCANCODE_LEFT);
    if(!strcmp(name,"右")||!strcmp(name,"RIGHT")) return vN(SDL_SCANCODE_RIGHT);
    if(!strcmp(name,"SHIFT")||!strcmp(name,"シフト")) return vN(SDL_SCANCODE_LSHIFT);
    if(!strcmp(name,"CTRL")||!strcmp(name,"制御")) return vN(SDL_SCANCODE_LCTRL);
    if(!strcmp(name,"ENTER")||!strcmp(name,"入力")) return vN(SDL_SCANCODE_RETURN);
    if(!strcmp(name,"TAB")||!strcmp(name,"タブ")) return vN(SDL_SCANCODE_TAB);
    if(strlen(name)==1&&name[0]>='1'&&name[0]<='9') return vN(SDL_SCANCODE_1+(name[0]-'1'));
    if(strlen(name)==1&&name[0]>='A'&&name[0]<='Z') return vN(SDL_SCANCODE_A+(name[0]-'A'));
    if(strlen(name)==1&&name[0]>='a'&&name[0]<='z') return vN(SDL_SCANCODE_A+(name[0]-'a'));
    return vN(0);
}

/* ══════════════════════════════════════════════
 * 関数テーブル
 * ══════════════════════════════════════════════*/
static HajimuPluginFunc functions[] = {
    /* ライフサイクル */
    {"作成",       p_create,  0, 3},
    {"破壊",       p_destroy, 0, 0},
    {"更新",       p_update,  0, 0},
    {"デルタ取得", p_delta,   0, 0},
    {"FPS取得",    p_fps,     0, 0},
    {"幅取得",     p_width,   0, 0},
    {"高さ取得",   p_height,  0, 0},
    /* 描画 */
    {"描画開始",   p_begin,   0, 3},
    {"描画",       p_draw,    1,10},
    {"描画終了",   p_end,     0, 0},
    /* カメラ */
    {"視野設定",   p_cam_perspective, 0, 3},
    {"カメラ位置", p_cam_pos,    3, 3},
    {"カメラ向き", p_cam_target, 3, 3},
    {"視点設定",   p_cam_lookat, 6, 6},
    /* ライティング */
    {"環境光",     p_ambient,   3, 3},
    {"平行光",     p_dir_light, 6, 6},
    {"点光源",     p_point_light, 8, 8},
    {"スポット光", p_spot_light,  12,13},
    {"スポット消灯",p_spot_light_off,1,1},
    /* シャドウ */
    {"影有効",     p_shadow_enable, 0,1},
    {"影バイアス", p_shadow_bias,   1,1},
    {"影サイズ",   p_shadow_size,   1,1},
    /* フォグ */
    {"霧有効",     p_fog_enable, 0,1},
    {"霧設定",     p_fog,        7,7},
    /* ブルーム */
    {"ブルーム有効",    p_bloom_enable,    0,1},
    {"ブルーム閾値",    p_bloom_threshold, 1,1},
    {"ブルーム強度",    p_bloom_intensity, 1,1},
    /* スカイボックス */
    {"スカイボックス読込", p_skybox_load,   6,6},
    {"スカイボックス描画", p_skybox_draw,   0,0},
    {"スカイボックス解放", p_skybox_unload, 0,0},
    /* メッシュ */
    {"立方体作成",   p_mesh_cube,    0,3},
    {"球体作成",     p_mesh_sphere,  0,3},
    {"平面作成",     p_mesh_plane,   0,2},
    {"円柱作成",     p_mesh_cylinder,0,3},
    {"カプセル作成", p_mesh_capsule, 0,3},
    {"トーラス作成", p_mesh_torus,   0,4},
    {"OBJ読込",      p_mesh_load_obj,1,1},
    {"メッシュ破壊", p_mesh_destroy, 1,1},
    {"頂点数取得",   p_mesh_vertex_count,1,1},
    /* テクスチャ */
    {"テクスチャ読込", p_tex_load,    1,1},
    {"テクスチャ破壊", p_tex_destroy, 1,1},
    /* マテリアル */
    {"色設定",         p_mesh_color,      5,5},
    {"テクスチャ設定", p_mesh_texture,    2,2},
    {"法線マップ設定", p_mesh_normal_map, 2,2},
    {"鏡面設定",       p_mesh_specular,   3,3},
    {"発光設定",       p_mesh_emissive,   5,5},
    {"ワイヤーフレーム",p_mesh_wireframe,  2,2},
    {"影投射",         p_mesh_cast_shadow, 2,2},
    {"影受取",         p_mesh_recv_shadow, 2,2},
    {"透明設定",       p_mesh_transparent, 2,2},
    /* パーティクル */
    {"発射器作成",p_emit_create,  0,1},
    {"発射器破壊",p_emit_destroy, 1,1},
    {"発射器位置",p_emit_pos,     4,4},
    {"発射率",    p_emit_rate,    2,2},
    {"寿命",      p_emit_life,    3,3},
    {"速度設定",  p_emit_velocity,5,5},
    {"重力設定",  p_emit_gravity, 4,4},
    {"粒子色",    p_emit_color,   5,5},
    {"粒子色末",  p_emit_color_end,5,5},
    {"粒子サイズ",p_emit_size,    3,3},
    {"粒子テクスチャ",p_emit_texture,2,2},
    {"発射器有効",p_emit_active,  2,2},
    {"一斉発射",  p_emit_burst,   2,2},
    {"発射器更新",p_emit_update,  1,1},
    /* シーングラフ */
    {"ノード作成",p_node_create,  0,0},
    {"ノード破壊",p_node_destroy, 1,1},
    {"親設定",    p_node_parent,  2,2},
    {"ノードメッシュ",p_node_mesh,2,2},
    {"ノード位置",p_node_pos,     4,4},
    {"ノード回転",p_node_rot,     4,4},
    {"ノード拡縮",p_node_scale,   4,4},
    {"ノード有効",p_node_active,  2,2},
    {"ノード描画",p_node_draw,    1,1},
    {"ワールド位置",p_node_world_pos,1,1},
    /* アニメーション */
    {"動作作成",  p_anim_create,   0,0},
    {"動作破壊",  p_anim_destroy,  1,1},
    {"位置キー",  p_anim_key_pos,  5,5},
    {"回転キー",  p_anim_key_rot,  5,5},
    {"拡縮キー",  p_anim_key_scale,5,5},
    {"動作再生",  p_anim_play,     1,1},
    {"動作停止",  p_anim_stop,     1,1},
    {"ループ設定",p_anim_loop,     2,2},
    {"シーク",    p_anim_seek,     2,2},
    {"動作更新",  p_anim_update,   2,2},
    {"再生中",    p_anim_is_playing,1,1},
    {"動作位置取得",p_anim_get_pos,  1,1},
    {"動作回転取得",p_anim_get_rot,  1,1},
    {"動作拡縮取得",p_anim_get_scale,1,1},
    /* レイキャスト */
    {"レイキャスト",      p_raycast,        6,6},
    {"画面レイキャスト",  p_raycast_screen, 2,2},
    /* 入力 */
    {"キー",          p_key,          1,1},
    {"キー押し",      p_key_down,     1,1},
    {"キー離し",      p_key_up,       1,1},
    {"マウスX",       p_mouse_x,      0,0},
    {"マウスY",       p_mouse_y,      0,0},
    {"マウスΔX",      p_mouse_dx,     0,0},
    {"マウスΔY",      p_mouse_dy,     0,0},
    {"スクロール",    p_scroll,       0,0},
    {"マウスボタン",     p_mouse_btn,      1,1},
    {"マウスボタン押し", p_mouse_btn_down, 1,1},
    {"マウス相対",    p_mouse_relative, 1,1},
    {"キーコード",    p_key_code,     1,1},
};

HAJIMU_PLUGIN_EXPORT HajimuPluginInfo* hajimu_plugin_init(void){
    static HajimuPluginInfo info = {
        .name         = "jp_engine_3d",
        .version      = "2.0.0",
        .author       = "jp-engine_3d contributors",
        .description  = "Unity レベル 3D エンジン - シャドウ/法線マップ/パーティクル/アニメ/レイキャスト",
        .functions    = functions,
        .function_count = sizeof(functions)/sizeof(functions[0]),
    };
    return &info;
}
