/**
 * src/eng_3d.c — 3D エンジン実装  v2.0.0
 *
 * SDL2 + OpenGL 3.3 Core Profile
 * 法線マップ, シャドウ(PCF), スポットライト, フォグ, スカイボックス,
 * パーティクル, シーングラフ, キーフレームアニメーション, レイキャスト,
 * ブルーム(ポストプロセス)
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

#include "eng_3d.h"

#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#elif defined(__linux__)
#  define GL_GLEXT_PROTOTYPES 1
#  include <GL/gl.h>
#  include <GL/glext.h>
#elif defined(_WIN32)
#  include <SDL2/SDL_opengl.h>
#  include <SDL2/SDL_opengl_glext.h>
#  include "win_gl.h"
#endif

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ══════════════════════════════════════════════════════
 * 定数 / マクロ
 * ══════════════════════════════════════════════════════*/
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#define DEG2RAD(d)   ((float)((d) * M_PI / 180.0))
#define RAD2DEG(r)   ((float)((r) * 180.0 / M_PI))
#define CLAMP(v,a,b) ((v)<(a)?(a):(v)>(b)?(b):(v))

#define SHADOW_MAP_W 2048
#define SHADOW_MAP_H 2048
#define MAX_ANIM_KEYS 128
#define MAX_PARTICLES 4096

/* ══════════════════════════════════════════════════════
 * 線形代数
 * ══════════════════════════════════════════════════════*/
typedef float mat4[16];
typedef float vec3[3];
typedef float vec4[4];

static void m4_id(mat4 m) { memset(m,0,64); m[0]=m[5]=m[10]=m[15]=1.f; }
static void m4_copy(mat4 d, const mat4 s) { memcpy(d,s,64); }

static void m4_mul(mat4 dst, const mat4 a, const mat4 b) {
    mat4 t;
    for(int c=0;c<4;c++) for(int r=0;r<4;r++){
        t[c*4+r]=0;
        for(int k=0;k<4;k++) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    memcpy(dst,t,64);
}

static void m4_perspective(mat4 m, float fov_rad, float aspect, float n, float f) {
    float t=tanf(fov_rad*0.5f);
    memset(m,0,64);
    m[0]=1.f/(aspect*t); m[5]=1.f/t;
    m[10]=-(f+n)/(f-n); m[11]=-1.f;
    m[14]=-(2.f*f*n)/(f-n);
}
static void m4_ortho(mat4 m, float l, float r, float b, float t, float n, float f) {
    memset(m,0,64);
    m[0]=2.f/(r-l); m[5]=2.f/(t-b); m[10]=-2.f/(f-n);
    m[12]=-(r+l)/(r-l); m[13]=-(t+b)/(t-b); m[14]=-(f+n)/(f-n); m[15]=1.f;
}

static float v3_dot(const vec3 a, const vec3 b){ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void  v3_cross(vec3 r, const vec3 a, const vec3 b){
    r[0]=a[1]*b[2]-a[2]*b[1]; r[1]=a[2]*b[0]-a[0]*b[2]; r[2]=a[0]*b[1]-a[1]*b[0];
}
static float v3_len(const vec3 v){ return sqrtf(v3_dot(v,v)); }
static void  v3_norm(vec3 v){ float l=v3_len(v); if(l>1e-8f){v[0]/=l;v[1]/=l;v[2]/=l;} }
static void  v3_sub(vec3 r,const vec3 a,const vec3 b){r[0]=a[0]-b[0];r[1]=a[1]-b[1];r[2]=a[2]-b[2];}
static void  v3_add(vec3 r,const vec3 a,const vec3 b){r[0]=a[0]+b[0];r[1]=a[1]+b[1];r[2]=a[2]+b[2];}
static void  v3_scale(vec3 r,const vec3 v,float s){r[0]=v[0]*s;r[1]=v[1]*s;r[2]=v[2]*s;}
static void  v3_lerp(vec3 r,const vec3 a,const vec3 b,float t){
    r[0]=a[0]+(b[0]-a[0])*t; r[1]=a[1]+(b[1]-a[1])*t; r[2]=a[2]+(b[2]-a[2])*t;
}

static void m4_lookat(mat4 m, const vec3 eye, const vec3 at, const vec3 up_hint) {
    vec3 f,s,u;
    v3_sub(f,at,eye); v3_norm(f);
    v3_cross(s,f,up_hint); v3_norm(s);
    v3_cross(u,s,f);
    memset(m,0,64);
    m[0]=s[0]; m[4]=s[1]; m[8] =s[2];
    m[1]=u[0]; m[5]=u[1]; m[9] =u[2];
    m[2]=-f[0];m[6]=-f[1];m[10]=-f[2];
    m[12]=-v3_dot(s,eye); m[13]=-v3_dot(u,eye); m[14]=v3_dot(f,eye); m[15]=1.f;
}

static void m4_normal(float nm[9], const mat4 m) {
    nm[0]=m[0]; nm[1]=m[1]; nm[2]=m[2];
    nm[3]=m[4]; nm[4]=m[5]; nm[5]=m[6];
    nm[6]=m[8]; nm[7]=m[9]; nm[8]=m[10];
}

static void m4_trs(mat4 out,
    float px,float py,float pz,
    float rx,float ry,float rz,
    float sx,float sy,float sz)
{
    float cx=cosf(DEG2RAD(rx)),sx_=sinf(DEG2RAD(rx));
    float cy=cosf(DEG2RAD(ry)),sy_=sinf(DEG2RAD(ry));
    float cz=cosf(DEG2RAD(rz)),sz_=sinf(DEG2RAD(rz));
    mat4 T,S,Rx,Ry,Rz,R,tmp;
    m4_id(T); T[12]=px; T[13]=py; T[14]=pz;
    m4_id(S); S[0]=sx; S[5]=sy; S[10]=sz;
    m4_id(Rx); Rx[5]=cx; Rx[6]=-sx_; Rx[9]=sx_; Rx[10]=cx;
    m4_id(Ry); Ry[0]=cy; Ry[2]=sy_; Ry[8]=-sy_; Ry[10]=cy;
    m4_id(Rz); Rz[0]=cz; Rz[1]=sz_; Rz[4]=-sz_; Rz[5]=cz;
    m4_mul(tmp,Ry,Rx); m4_mul(R,tmp,Rz);
    m4_mul(tmp,R,S);   m4_mul(out,T,tmp);
}

/* AABB をワールド変換 */
static ENG_3D_AABB aabb_transform_internal(ENG_3D_AABB aabb,
    float px,float py,float pz,
    float rx,float ry,float rz,
    float sx,float sy,float sz)
{
    mat4 M; m4_trs(M,px,py,pz,rx,ry,rz,sx,sy,sz);
    float corners[8][3];
    float mins[3]={aabb.min[0],aabb.min[1],aabb.min[2]};
    float maxs[3]={aabb.max[0],aabb.max[1],aabb.max[2]};
    for(int i=0;i<8;i++){
        corners[i][0]=(i&1)?maxs[0]:mins[0];
        corners[i][1]=(i&2)?maxs[1]:mins[1];
        corners[i][2]=(i&4)?maxs[2]:mins[2];
    }
    ENG_3D_AABB out;
    out.min[0]=out.min[1]=out.min[2]=FLT_MAX;
    out.max[0]=out.max[1]=out.max[2]=-FLT_MAX;
    for(int i=0;i<8;i++){
        float wx=M[0]*corners[i][0]+M[4]*corners[i][1]+M[8] *corners[i][2]+M[12];
        float wy=M[1]*corners[i][0]+M[5]*corners[i][1]+M[9] *corners[i][2]+M[13];
        float wz=M[2]*corners[i][0]+M[6]*corners[i][1]+M[10]*corners[i][2]+M[14];
        if(wx<out.min[0])out.min[0]=wx; if(wx>out.max[0])out.max[0]=wx;
        if(wy<out.min[1])out.min[1]=wy; if(wy>out.max[1])out.max[1]=wy;
        if(wz<out.min[2])out.min[2]=wz; if(wz>out.max[2])out.max[2]=wz;
    }
    return out;
}

/* ══════════════════════════════════════════════════════
 * 内部構造体
 * ══════════════════════════════════════════════════════*/
typedef struct { float p[3], n[3], uv[2], t[3]; } Vertex3D; /* pos/normal/uv/tangent */

typedef struct {
    unsigned int vao, vbo, ebo;
    int          index_count, vertex_count;
    float        color[4];
    float        emissive[3]; float emissive_int;
    float        spec_intensity, shininess;
    int          tex_id, normal_map_id;
    bool         wireframe, cast_shadow, receive_shadow, transparent, used;
    ENG_3D_AABB  bounds;
} Mesh3D;

typedef struct { float x,y,z,r,g,b,radius; bool active; } PointLight3D;
typedef struct {
    float x,y,z; float dx,dy,dz;
    float r,g,b; float radius;
    float cutoff_cos, outer_cos;
    bool  active;
} SpotLight3D;

/* ── パーティクル ─*/
typedef struct {
    float pos[3], vel[3], color[4], color_end[4];
    float size, size_end, life, life_max;
    bool  alive;
} Particle;

typedef struct {
    Particle  parts[MAX_PARTICLES];
    int       max_parts;
    float     pos[3];
    float     vel[3]; float spread;
    float     grav[3];
    float     rate, accum;
    float     life_min, life_max;
    float     color_s[4], color_e[4];
    float     size_s, size_e;
    int       tex_id;
    bool      active, used;
    unsigned int vao, vbo;
} Emitter3D;

/* ── シーングラフ ─*/
typedef struct SceneNode {
    float       lpos[3], lrot[3], lscale[3];
    ENG_3D_MeshID mesh;
    int          parent;   /* -1=なし */
    bool         active, used;
} SceneNode;

/* ── アニメーションキー ─*/
typedef struct {
    float t;
    float v[3];
} AnimKey;

typedef struct {
    AnimKey pos_keys[MAX_ANIM_KEYS];   int n_pos;
    AnimKey rot_keys[MAX_ANIM_KEYS];   int n_rot;
    AnimKey scale_keys[MAX_ANIM_KEYS]; int n_scale;
    float   time, duration;
    bool    playing, loop, used;
    float   cur_pos[3], cur_rot[3], cur_scale[3];
} Anim3D;

struct ENG_3D {
    SDL_Window*   window;
    SDL_GLContext gl_ctx;
    int           w, h;

    /* カメラ */
    vec3  cam_pos, cam_target;
    float fov, near_z, far_z;
    mat4  mat_proj, mat_view;
    /* 前frame のキー状態 (KeyDown/Up 判定用) */
    uint8_t prev_keys[SDL_NUM_SCANCODES];

    /* シェーダー (Phong + 法線マップ + シャドウ) */
    unsigned int shader_main;
    /* シャドウパス */
    unsigned int shader_shadow;
    unsigned int shadow_fbo, shadow_depth_tex;
    bool         shadow_on;
    float        shadow_bias, shadow_ortho;
    mat4         mat_light_space;

    /* ブルーム FBO */
    unsigned int bloom_fbo, bloom_color_tex, bloom_depth_rbo;
    unsigned int bloom_fbo2, bloom_color_tex2;
    unsigned int shader_blur, shader_combine;
    unsigned int quad_vao, quad_vbo;
    bool         bloom_on;
    float        bloom_threshold, bloom_intensity;

    /* スカイボックス */
    unsigned int skybox_vao, skybox_vbo;
    unsigned int skybox_cubemap;
    unsigned int shader_skybox;
    bool         skybox_on;

    /* ライティング */
    float        ambient[3];
    float        dir_dir[3], dir_col[3];
    PointLight3D pt_lights[ENG_3D_MAX_LIGHTS];
    SpotLight3D  sp_lights[ENG_3D_MAX_SPOTS];

    /* フォグ */
    float fog_color[3];
    float fog_start, fog_end, fog_density;
    int   fog_mode;   /* 0=linear 1=exp 2=exp2 */
    bool  fog_on;

    /* メッシュ / テクスチャ */
    Mesh3D       meshes  [ENG_3D_MAX_MESHES];
    unsigned int textures[ENG_3D_MAX_TEXTURES];
    bool         tex_used[ENG_3D_MAX_TEXTURES];

    /* パーティクル */
    Emitter3D    emitters[ENG_3D_MAX_EMITTERS];
    unsigned int shader_particle;

    /* シーングラフ */
    SceneNode    nodes[ENG_3D_MAX_NODES];

    /* アニメーション */
    Anim3D       anims[ENG_3D_MAX_ANIMS];

    /* 入力 */
    const uint8_t* keys;
    float  mx, my, mdx, mdy, scroll;
    uint32_t mouse_btn, prev_mouse_btn;

    /* 時間 */
    float    delta;
    uint64_t last_tick;
    int      fps, fps_ctr;
    uint64_t fps_tick;
    bool     quit;
};

/* ══════════════════════════════════════════════════════
 * シェーダーソース
 * ══════════════════════════════════════════════════════*/

/* ── メインシェーダー 頂点 ─*/
static const char* VERT_MAIN =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"layout(location=2) in vec2 aUV;\n"
"layout(location=3) in vec3 aTangent;\n"
"uniform mat4 uMVP;\n"
"uniform mat4 uModel;\n"
"uniform mat3 uNM;\n"       /* 法線行列 */
"uniform mat4 uLightSpace;\n"
"out vec3 vFragPos;\n"
"out vec2 vUV;\n"
"out vec4 vFragPosLS;\n"    /* ライト空間 */
"out mat3 vTBN;\n"
"void main(){\n"
"  vec4 wPos=uModel*vec4(aPos,1.0);\n"
"  vFragPos=wPos.xyz;\n"
"  vUV=aUV;\n"
"  vFragPosLS=uLightSpace*wPos;\n"
"  vec3 T=normalize(uNM*aTangent);\n"
"  vec3 N=normalize(uNM*aNormal);\n"
"  T=normalize(T-dot(T,N)*N);\n"
"  vec3 B=cross(N,T);\n"
"  vTBN=mat3(T,B,N);\n"
"  gl_Position=uMVP*vec4(aPos,1.0);\n"
"}\n";

/* ── メインシェーダー フラグメント ─*/
static const char* FRAG_MAIN =
"#version 330 core\n"
"in vec3 vFragPos;\n"
"in vec2 vUV;\n"
"in vec4 vFragPosLS;\n"
"in mat3 vTBN;\n"
"out vec4 FragColor;\n"
"uniform vec4  uColor;\n"
"uniform vec3  uEmissive;\n"
"uniform float uEmissiveInt;\n"
"uniform float uSpecInt;\n"
"uniform float uShininess;\n"
"uniform sampler2D uAlbedo;\n"
"uniform sampler2D uNormalMap;\n"
"uniform sampler2D uShadowMap;\n"
"uniform int   uHasTex;\n"
"uniform int   uHasNM;\n"
"uniform int   uHasShadow;\n"
"uniform float uShadowBias;\n"
"uniform vec3  uCamPos;\n"
"uniform vec3  uAmbient;\n"
"uniform vec3  uDirDir;\n"
"uniform vec3  uDirCol;\n"
"/* point lights */\n"
"uniform vec3  uPtPos[8];\n"
"uniform vec3  uPtCol[8];\n"
"uniform float uPtRad[8];\n"
"uniform int   uPtCount;\n"
"/* spot lights */\n"
"uniform vec3  uSpPos[4];\n"
"uniform vec3  uSpDir[4];\n"
"uniform vec3  uSpCol[4];\n"
"uniform float uSpRad[4];\n"
"uniform float uSpCut[4];\n"
"uniform float uSpOut[4];\n"
"uniform int   uSpCount;\n"
"/* fog */\n"
"uniform int   uFogMode;\n"
"uniform vec3  uFogColor;\n"
"uniform float uFogStart;\n"
"uniform float uFogEnd;\n"
"uniform float uFogDensity;\n"
"\n"
"float shadow(vec4 ls, vec3 N, vec3 L){\n"
"  if(uHasShadow==0) return 0.0;\n"
"  vec3 pc=ls.xyz/ls.w*0.5+0.5;\n"
"  if(pc.z>1.0) return 0.0;\n"
"  float bias=max(uShadowBias*8.0*(1.0-dot(N,L)),uShadowBias);\n"
"  float shadow=0.0;\n"
"  vec2 texelSize=1.0/vec2(textureSize(uShadowMap,0));\n"
"  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++){\n"
"    float d=texture(uShadowMap,pc.xy+vec2(x,y)*texelSize).r;\n"
"    shadow+=(pc.z-bias>d)?1.0:0.0;\n"
"  }\n"
"  return shadow/9.0;\n"
"}\n"
"\n"
"void main(){\n"
"  vec3 base=(uHasTex!=0)?texture(uAlbedo,vUV).rgb:uColor.rgb;\n"
"  vec3 N;\n"
"  if(uHasNM!=0){\n"
"    N=texture(uNormalMap,vUV).rgb*2.0-1.0;\n"
"    N=normalize(vTBN*N);\n"
"  } else { N=normalize(vTBN[2]); }\n"
"  vec3 V=normalize(uCamPos-vFragPos);\n"
"\n"
"  /* 方向ライト */\n"
"  vec3 L=normalize(-uDirDir);\n"
"  float diff=max(dot(N,L),0.0);\n"
"  vec3 H=normalize(L+V);\n"
"  float spec=pow(max(dot(N,H),0.0),uShininess)*uSpecInt;\n"
"  float sh=shadow(vFragPosLS,N,L);\n"
"  vec3 lighting=uAmbient+(diff*uDirCol+spec*uDirCol)*(1.0-sh*0.8);\n"
"\n"
"  /* ポイントライト */\n"
"  for(int i=0;i<uPtCount;i++){\n"
"    vec3 PL=uPtPos[i]-vFragPos;\n"
"    float dist=length(PL);\n"
"    if(dist<uPtRad[i]){\n"
"      float att=clamp(1.0-dist/uPtRad[i],0.0,1.0);\n"
"      att*=att;\n"
"      vec3 PL_n=normalize(PL);\n"
"      float pd=max(dot(N,PL_n),0.0)*att;\n"
"      vec3 PH=normalize(PL_n+V);\n"
"      float ps=pow(max(dot(N,PH),0.0),uShininess)*uSpecInt*att;\n"
"      lighting+=pd*uPtCol[i]+ps*uPtCol[i];\n"
"    }\n"
"  }\n"
"\n"
"  /* スポットライト */\n"
"  for(int i=0;i<uSpCount;i++){\n"
"    vec3 SL=uSpPos[i]-vFragPos;\n"
"    float dist=length(SL);\n"
"    if(dist<uSpRad[i]){\n"
"      vec3 SL_n=normalize(SL);\n"
"      float theta=dot(SL_n,normalize(-uSpDir[i]));\n"
"      float eps=uSpCut[i]-uSpOut[i];\n"
"      float intensity=clamp((theta-uSpOut[i])/eps,0.0,1.0);\n"
"      float att=clamp(1.0-dist/uSpRad[i],0.0,1.0); att*=att;\n"
"      float sd=max(dot(N,SL_n),0.0)*att*intensity;\n"
"      vec3 SH=normalize(SL_n+V);\n"
"      float ss=pow(max(dot(N,SH),0.0),uShininess)*uSpecInt*att*intensity;\n"
"      lighting+=sd*uSpCol[i]+ss*uSpCol[i];\n"
"    }\n"
"  }\n"
"\n"
"  vec3 emissive=uEmissive*uEmissiveInt;\n"
"  vec3 final_color=clamp(lighting,0.0,1.0)*base+emissive;\n"
"\n"
"  /* フォグ */\n"
"  if(uFogMode>0){\n"
"    float dist=length(uCamPos-vFragPos);\n"
"    float factor=0.0;\n"
"    if(uFogMode==1) factor=1.0-exp(-uFogDensity*dist);\n"
"    else            factor=1.0-exp(-uFogDensity*uFogDensity*dist*dist);\n"
"    factor=clamp(factor,0.0,1.0);\n"
"    final_color=mix(final_color,uFogColor,factor);\n"
"  } else if(uFogMode==0 && uFogEnd>uFogStart){\n"
"    /* linear fog は fogMode==-1 を使わず別扱いにするため ここには来ない */\n"
"  }\n"
"  FragColor=vec4(final_color,uColor.a);\n"
"}\n";

/* ── シャドウパス 頂点 ─*/
static const char* VERT_SHADOW =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"uniform mat4 uLightSpace;\n"
"uniform mat4 uModel;\n"
"void main(){ gl_Position=uLightSpace*uModel*vec4(aPos,1.0); }\n";

static const char* FRAG_SHADOW =
"#version 330 core\n"
"void main(){}\n";

/* ── スカイボックス ─*/
static const char* VERT_SKY =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"out vec3 vTexCoord;\n"
"uniform mat4 uView;\n"
"uniform mat4 uProj;\n"
"void main(){\n"
"  vTexCoord=aPos;\n"
"  vec4 pos=uProj*uView*vec4(aPos,1.0);\n"
"  gl_Position=pos.xyww;\n"
"}\n";

static const char* FRAG_SKY =
"#version 330 core\n"
"in vec3 vTexCoord;\n"
"out vec4 FragColor;\n"
"uniform samplerCube uSkybox;\n"
"uniform vec3 uFogColor;\n"
"uniform float uFogDensity;\n"
"uniform int uHasFog;\n"
"void main(){\n"
"  vec3 c=texture(uSkybox,vTexCoord).rgb;\n"
"  FragColor=vec4(c,1.0);\n"
"}\n";

/* ── ガウスブラー (縦横共用、方向は uniform で切替) ─*/
static const char* VERT_QUAD =
"#version 330 core\n"
"layout(location=0) in vec2 aPos;\n"
"layout(location=1) in vec2 aUV;\n"
"out vec2 vUV;\n"
"void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); }\n";

static const char* FRAG_BLUR =
"#version 330 core\n"
"in vec2 vUV;\n"
"out vec4 FragColor;\n"
"uniform sampler2D uImage;\n"
"uniform bool uHorizontal;\n"
"const float w[5]=float[](0.227027,0.194595,0.121622,0.054054,0.016216);\n"
"void main(){\n"
"  vec2 texel=1.0/textureSize(uImage,0);\n"
"  vec3 result=texture(uImage,vUV).rgb*w[0];\n"
"  if(uHorizontal){\n"
"    for(int i=1;i<5;i++)\n"
"      result+=(texture(uImage,vUV+vec2(texel.x*i,0)).rgb\n"
"              +texture(uImage,vUV-vec2(texel.x*i,0)).rgb)*w[i];\n"
"  } else {\n"
"    for(int i=1;i<5;i++)\n"
"      result+=(texture(uImage,vUV+vec2(0,texel.y*i)).rgb\n"
"              +texture(uImage,vUV-vec2(0,texel.y*i)).rgb)*w[i];\n"
"  }\n"
"  FragColor=vec4(result,1.0);\n"
"}\n";

static const char* FRAG_COMBINE =
"#version 330 core\n"
"in vec2 vUV;\n"
"out vec4 FragColor;\n"
"uniform sampler2D uScene;\n"
"uniform sampler2D uBloom;\n"
"uniform float uThreshold;\n"
"uniform float uIntensity;\n"
"void main(){\n"
"  vec3 scene=texture(uScene,vUV).rgb;\n"
"  vec3 bloom=texture(uBloom,vUV).rgb;\n"
"  /* tone mapping (Reinhard) */\n"
"  vec3 hdr=scene+bloom*uIntensity;\n"
"  vec3 mapped=hdr/(hdr+vec3(1.0));\n"
"  FragColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);\n"
"}\n";

/* ── パーティクルシェーダー ─*/
static const char* VERT_PARTICLE =
"#version 330 core\n"
"layout(location=0) in vec2 aQuad;\n"
"layout(location=1) in vec3 aPos;\n"    /* per-instance */
"layout(location=2) in vec4 aColor;\n"
"layout(location=3) in float aSize;\n"
"out vec2 vUV;\n"
"out vec4 vColor;\n"
"uniform mat4 uView;\n"
"uniform mat4 uProj;\n"
"void main(){\n"
"  vec3 camR=vec3(uView[0][0],uView[1][0],uView[2][0]);\n"
"  vec3 camU=vec3(uView[0][1],uView[1][1],uView[2][1]);\n"
"  vec3 wPos=aPos+(camR*aQuad.x+camU*aQuad.y)*aSize;\n"
"  vUV=aQuad*0.5+0.5;\n"
"  vColor=aColor;\n"
"  gl_Position=uProj*uView*vec4(wPos,1.0);\n"
"}\n";

static const char* FRAG_PARTICLE =
"#version 330 core\n"
"in vec2 vUV;\n"
"in vec4 vColor;\n"
"out vec4 FragColor;\n"
"uniform sampler2D uTex;\n"
"uniform int uHasTex;\n"
"void main(){\n"
"  vec4 c=vColor;\n"
"  if(uHasTex!=0) c*=texture(uTex,vUV);\n"
"  if(c.a<0.01) discard;\n"
"  FragColor=c;\n"
"}\n";

/* ══════════════════════════════════════════════════════
 * シェーダーコンパイルユーティリティ
 * ══════════════════════════════════════════════════════*/
static unsigned int compile_shader(const char* src, GLenum type) {
    unsigned int s=glCreateShader(type);
    glShaderSource(s,1,&src,NULL);
    glCompileShader(s);
    int ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char buf[512]; glGetShaderInfoLog(s,512,NULL,buf);
             fprintf(stderr,"[3D] シェーダーエラー: %s\n",buf); }
    return s;
}
static unsigned int build_program2(const char* vs_src, const char* fs_src) {
    unsigned int vs=compile_shader(vs_src,GL_VERTEX_SHADER);
    unsigned int fs=compile_shader(fs_src,GL_FRAGMENT_SHADER);
    unsigned int p=glCreateProgram();
    glAttachShader(p,vs); glAttachShader(p,fs);
    glLinkProgram(p);
    int ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ char buf[512]; glGetProgramInfoLog(p,512,NULL,buf);
             fprintf(stderr,"[3D] リンクエラー: %s\n",buf); }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

/* uniform ヘルパー */
#define UL(prog,name) glGetUniformLocation((prog),(name))
#define U1I(p,n,v)    glUniform1i(UL(p,n),(v))
#define U1F(p,n,v)    glUniform1f(UL(p,n),(v))
#define U3F(p,n,a)    glUniform3fv(UL(p,n),1,(a))
#define U4F(p,n,a)    glUniform4fv(UL(p,n),1,(a))
#define UM4(p,n,a)    glUniformMatrix4fv(UL(p,n),1,GL_FALSE,(a))
#define UM3(p,n,a)    glUniformMatrix3fv(UL(p,n),1,GL_FALSE,(a))

/* ══════════════════════════════════════════════════════
 * メッシュ内部ユーティリティ
 * ══════════════════════════════════════════════════════*/
static void upload_mesh(Mesh3D* m, Vertex3D* verts, uint32_t vcnt,
                         uint32_t* idx, uint32_t icnt)
{
    glGenVertexArrays(1,&m->vao);
    glGenBuffers(1,&m->vbo);
    glGenBuffers(1,&m->ebo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER,m->vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(vcnt*sizeof(Vertex3D)),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)(icnt*sizeof(uint32_t)),idx,GL_STATIC_DRAW);
    /* pos */ glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,p));
    /* nor */ glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,n));
    /* uv  */ glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,uv));
    /* tan */ glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,t));
    glBindVertexArray(0);
    m->vertex_count=vcnt;
    m->index_count =icnt;
}

static void compute_tangents(Vertex3D* verts, uint32_t vcnt,
                               uint32_t* idx, uint32_t icnt)
{
    for(uint32_t i=0;i<vcnt;i++){ verts[i].t[0]=verts[i].t[1]=verts[i].t[2]=0.f; }
    for(uint32_t i=0;i<icnt;i+=3){
        Vertex3D *v0=&verts[idx[i]],*v1=&verts[idx[i+1]],*v2=&verts[idx[i+2]];
        float e1[3]={v1->p[0]-v0->p[0],v1->p[1]-v0->p[1],v1->p[2]-v0->p[2]};
        float e2[3]={v2->p[0]-v0->p[0],v2->p[1]-v0->p[1],v2->p[2]-v0->p[2]};
        float du1=v1->uv[0]-v0->uv[0], dv1=v1->uv[1]-v0->uv[1];
        float du2=v2->uv[0]-v0->uv[0], dv2=v2->uv[1]-v0->uv[1];
        float f=1.f/(du1*dv2-du2*dv1+1e-8f);
        float tx=f*(dv2*e1[0]-dv1*e2[0]);
        float ty=f*(dv2*e1[1]-dv1*e2[1]);
        float tz=f*(dv2*e1[2]-dv1*e2[2]);
        for(int k=0;k<3;k++){
            verts[idx[i+k]].t[0]+=tx;
            verts[idx[i+k]].t[1]+=ty;
            verts[idx[i+k]].t[2]+=tz;
        }
    }
    for(uint32_t i=0;i<vcnt;i++){
        float* t=verts[i].t, len=sqrtf(t[0]*t[0]+t[1]*t[1]+t[2]*t[2])+1e-8f;
        t[0]/=len; t[1]/=len; t[2]/=len;
    }
}

static void compute_bounds(Mesh3D* m, Vertex3D* verts, uint32_t vcnt){
    if(!vcnt)return;
    for(int k=0;k<3;k++) m->bounds.min[k]=m->bounds.max[k]=verts[0].p[k];
    for(uint32_t i=1;i<vcnt;i++) for(int k=0;k<3;k++){
        if(verts[i].p[k]<m->bounds.min[k]) m->bounds.min[k]=verts[i].p[k];
        if(verts[i].p[k]>m->bounds.max[k]) m->bounds.max[k]=verts[i].p[k];
    }
}

static int alloc_mesh_slot(ENG_3D* ctx){
    for(int i=0;i<ENG_3D_MAX_MESHES;i++) if(!ctx->meshes[i].used) return i;
    return -1;
}

static void mesh_default(Mesh3D* m){
    m->color[0]=m->color[1]=m->color[2]=m->color[3]=1.f;
    m->spec_intensity=0.5f; m->shininess=32.f;
    m->cast_shadow=true; m->receive_shadow=true; m->used=true;
}

/* ══════════════════════════════════════════════════════
 * メッシュ生成 プリミティブ
 * ══════════════════════════════════════════════════════*/
ENG_3D_MeshID eng3d_mesh_cube(ENG_3D* ctx, float w, float h, float d){
    int slot=alloc_mesh_slot(ctx); if(slot<0) return 0;
    float hx=w*0.5f,hy=h*0.5f,hz=d*0.5f;
    /* 面ごとに 4 頂点、法線を持つ */
    Vertex3D verts[24]; uint32_t idx[36]; memset(verts,0,sizeof(verts));
    /* front */
    float faces[6][3]={{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
    /* 簡略: 一面ずつ手打ち */
    int vi=0,ii=0;
    /* +Z */
    float nx=0,ny=0,nz=1;
    verts[vi]=(Vertex3D){{-hx,-hy, hz},{nx,ny,nz},{0,0}};vi++;
    verts[vi]=(Vertex3D){{ hx,-hy, hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy, hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{-hx, hy, hz},{nx,ny,nz},{0,1}};vi++;
    for(int f=0;f<6;f++) idx[ii+f]=((uint32_t[]){0,1,2,0,2,3}[f])+(uint32_t)(vi-4); ii+=6;
    /* -Z */
    nx=0;ny=0;nz=-1;
    verts[vi]=(Vertex3D){{ hx,-hy,-hz},{nx,ny,nz},{0,0}};vi++;
    verts[vi]=(Vertex3D){{-hx,-hy,-hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{-hx, hy,-hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy,-hz},{nx,ny,nz},{0,1}};vi++;
    { uint32_t base=(uint32_t)(vi-4); for(int f=0;f<6;f++) idx[ii+f]=base+((uint32_t[]){0,1,2,0,2,3})[f]; ii+=6; }
    /* +Y */
    nx=0;ny=1;nz=0;
    verts[vi]=(Vertex3D){{-hx, hy, hz},{nx,ny,nz},{0,1}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy, hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy,-hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{-hx, hy,-hz},{nx,ny,nz},{0,0}};vi++;
    { uint32_t base=(uint32_t)(vi-4); for(int f=0;f<6;f++) idx[ii+f]=base+((uint32_t[]){0,1,2,0,2,3})[f]; ii+=6; }
    /* -Y */
    nx=0;ny=-1;nz=0;
    verts[vi]=(Vertex3D){{-hx,-hy,-hz},{nx,ny,nz},{0,0}};vi++;
    verts[vi]=(Vertex3D){{ hx,-hy,-hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{ hx,-hy, hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{-hx,-hy, hz},{nx,ny,nz},{0,1}};vi++;
    { uint32_t base=(uint32_t)(vi-4); for(int f=0;f<6;f++) idx[ii+f]=base+((uint32_t[]){0,1,2,0,2,3})[f]; ii+=6; }
    /* +X */
    nx=1;ny=0;nz=0;
    verts[vi]=(Vertex3D){{ hx,-hy, hz},{nx,ny,nz},{0,0}};vi++;
    verts[vi]=(Vertex3D){{ hx,-hy,-hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy,-hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{ hx, hy, hz},{nx,ny,nz},{0,1}};vi++;
    { uint32_t base=(uint32_t)(vi-4); for(int f=0;f<6;f++) idx[ii+f]=base+((uint32_t[]){0,1,2,0,2,3})[f]; ii+=6; }
    /* -X */
    nx=-1;ny=0;nz=0;
    verts[vi]=(Vertex3D){{-hx,-hy,-hz},{nx,ny,nz},{0,0}};vi++;
    verts[vi]=(Vertex3D){{-hx,-hy, hz},{nx,ny,nz},{1,0}};vi++;
    verts[vi]=(Vertex3D){{-hx, hy, hz},{nx,ny,nz},{1,1}};vi++;
    verts[vi]=(Vertex3D){{-hx, hy,-hz},{nx,ny,nz},{0,1}};vi++;
    { uint32_t base=(uint32_t)(vi-4); for(int f=0;f<6;f++) idx[ii+f]=base+((uint32_t[]){0,1,2,0,2,3})[f]; ii+=6; }

    (void)faces;
    compute_tangents(verts,24,idx,36);
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,24);
    upload_mesh(m,verts,24,idx,36);
    return slot+1;
}

ENG_3D_MeshID eng3d_mesh_sphere(ENG_3D* ctx, float r, int slices, int stacks){
    if(slices<3) slices=16; if(stacks<2) stacks=8;
    int vcnt=(slices+1)*(stacks+1);
    int icnt=slices*stacks*6;
    Vertex3D* verts=(Vertex3D*)malloc((size_t)vcnt*sizeof(Vertex3D));
    uint32_t* idx=(uint32_t*)malloc((size_t)icnt*sizeof(uint32_t));
    int vi=0;
    for(int j=0;j<=stacks;j++){
        float phi=(float)j/(float)stacks*(float)M_PI;
        for(int i=0;i<=slices;i++){
            float theta=(float)i/(float)slices*2.f*(float)M_PI;
            float nx=sinf(phi)*cosf(theta), ny=cosf(phi), nz=sinf(phi)*sinf(theta);
            verts[vi].p[0]=r*nx; verts[vi].p[1]=r*ny; verts[vi].p[2]=r*nz;
            verts[vi].n[0]=nx;   verts[vi].n[1]=ny;   verts[vi].n[2]=nz;
            verts[vi].uv[0]=(float)i/(float)slices;
            verts[vi].uv[1]=(float)j/(float)stacks;
            vi++;
        }
    }
    int ii=0;
    for(int j=0;j<stacks;j++) for(int i=0;i<slices;i++){
        uint32_t a=(uint32_t)(j*(slices+1)+i);
        uint32_t b=a+(uint32_t)(slices+1);
        idx[ii++]=a; idx[ii++]=b; idx[ii++]=a+1;
        idx[ii++]=a+1; idx[ii++]=b; idx[ii++]=b+1;
    }
    compute_tangents(verts,(uint32_t)vcnt,idx,(uint32_t)icnt);
    int slot=alloc_mesh_slot(ctx);
    if(slot<0){free(verts);free(idx);return 0;}
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,(uint32_t)vcnt);
    upload_mesh(m,verts,(uint32_t)vcnt,idx,(uint32_t)icnt);
    free(verts); free(idx);
    return slot+1;
}

ENG_3D_MeshID eng3d_mesh_plane(ENG_3D* ctx, float w, float d){
    float hw=w*0.5f, hd=d*0.5f;
    Vertex3D verts[4]={
        {{-hw,0, hd},{0,1,0},{0,1}},
        {{ hw,0, hd},{0,1,0},{1,1}},
        {{ hw,0,-hd},{0,1,0},{1,0}},
        {{-hw,0,-hd},{0,1,0},{0,0}}
    };
    uint32_t idx[6]={0,1,2,0,2,3};
    compute_tangents(verts,4,idx,6);
    int slot=alloc_mesh_slot(ctx); if(slot<0) return 0;
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,4);
    upload_mesh(m,verts,4,idx,6);
    return slot+1;
}

ENG_3D_MeshID eng3d_mesh_cylinder(ENG_3D* ctx, float r, float h, int segs){
    if(segs<3) segs=16;
    int vcnt=(segs+1)*4+2; /* side top+bot caps */
    int icnt=segs*6 + segs*3*2;
    Vertex3D* verts=(Vertex3D*)calloc((size_t)vcnt,sizeof(Vertex3D));
    uint32_t* idx=(uint32_t*)malloc((size_t)icnt*sizeof(uint32_t));
    float hh=h*0.5f; int vi=0,ii=0;
    /* side */
    for(int i=0;i<=segs;i++){
        float th=(float)i/(float)segs*2.f*(float)M_PI;
        float cx=cosf(th), cz=sinf(th);
        verts[vi].p[0]=r*cx; verts[vi].p[1]=-hh; verts[vi].p[2]=r*cz;
        verts[vi].n[0]=cx; verts[vi].n[2]=cz; verts[vi].uv[0]=(float)i/(float)segs; verts[vi].uv[1]=0; vi++;
        verts[vi].p[0]=r*cx; verts[vi].p[1]= hh; verts[vi].p[2]=r*cz;
        verts[vi].n[0]=cx; verts[vi].n[2]=cz; verts[vi].uv[0]=(float)i/(float)segs; verts[vi].uv[1]=1; vi++;
    }
    for(int i=0;i<segs;i++){
        uint32_t b=(uint32_t)(i*2);
        idx[ii++]=b; idx[ii++]=b+2; idx[ii++]=b+1;
        idx[ii++]=b+1; idx[ii++]=b+2; idx[ii++]=b+3;
    }
    /* top cap center */
    uint32_t tc=(uint32_t)vi;
    verts[vi].p[1]=hh; verts[vi].n[1]=1.f; verts[vi].uv[0]=verts[vi].uv[1]=0.5f; vi++;
    for(int i=0;i<=segs;i++){
        float th=(float)i/(float)segs*2.f*(float)M_PI;
        verts[vi].p[0]=r*cosf(th); verts[vi].p[1]=hh; verts[vi].p[2]=r*sinf(th);
        verts[vi].n[1]=1.f;
        verts[vi].uv[0]=cosf(th)*0.5f+0.5f; verts[vi].uv[1]=sinf(th)*0.5f+0.5f; vi++;
    }
    for(int i=0;i<segs;i++){
        idx[ii++]=tc; idx[ii++]=tc+1+(uint32_t)i+1; idx[ii++]=tc+1+(uint32_t)i;
    }
    /* bot cap */
    uint32_t bc=(uint32_t)vi;
    verts[vi].p[1]=-hh; verts[vi].n[1]=-1.f; verts[vi].uv[0]=verts[vi].uv[1]=0.5f; vi++;
    for(int i=0;i<=segs;i++){
        float th=(float)i/(float)segs*2.f*(float)M_PI;
        verts[vi].p[0]=r*cosf(th); verts[vi].p[1]=-hh; verts[vi].p[2]=r*sinf(th);
        verts[vi].n[1]=-1.f;
        verts[vi].uv[0]=cosf(th)*0.5f+0.5f; verts[vi].uv[1]=sinf(th)*0.5f+0.5f; vi++;
    }
    for(int i=0;i<segs;i++){
        idx[ii++]=bc; idx[ii++]=bc+1+(uint32_t)i; idx[ii++]=bc+1+(uint32_t)i+1;
    }
    compute_tangents(verts,(uint32_t)vi,idx,(uint32_t)ii);
    int slot=alloc_mesh_slot(ctx);
    if(slot<0){free(verts);free(idx);return 0;}
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,(uint32_t)vi);
    upload_mesh(m,verts,(uint32_t)vi,idx,(uint32_t)ii);
    free(verts); free(idx);
    return slot+1;
}

ENG_3D_MeshID eng3d_mesh_capsule(ENG_3D* ctx, float r, float h, int segs){
    /* カプセル = 2 球端 + 円柱中間 で UV スフィア風に生成 */
    if(segs<4) segs=16;
    int hemi=segs/2;
    int vcnt=(segs+1)*(hemi*2+1+1);
    int icnt= segs*(hemi*2)*6;
    Vertex3D* verts=(Vertex3D*)calloc((size_t)vcnt,sizeof(Vertex3D));
    uint32_t* idx  =(uint32_t*)malloc((size_t)icnt*sizeof(uint32_t));
    float hh=h*0.5f; int vi=0,ii=0,tot_stacks=hemi*2;
    for(int j=0;j<=tot_stacks;j++){
        float phi;
        float yoffset=0;
        if(j<=hemi){
            phi=(float)j/(float)hemi*(float)M_PI_2;
            yoffset=-hh;
        } else {
            phi=(float)M_PI_2+(float)(j-hemi)/(float)hemi*(float)M_PI_2;
            yoffset= hh;
        }
        float sp=sinf(phi), cp=cosf(phi);
        for(int i=0;i<=segs;i++){
            float th=(float)i/(float)segs*2.f*(float)M_PI;
            float nx=sp*cosf(th), ny=cp, nz=sp*sinf(th);
            verts[vi].p[0]=r*nx; verts[vi].p[1]=r*ny+yoffset; verts[vi].p[2]=r*nz;
            verts[vi].n[0]=nx; verts[vi].n[1]=ny; verts[vi].n[2]=nz;
            verts[vi].uv[0]=(float)i/(float)segs;
            verts[vi].uv[1]=(float)j/(float)tot_stacks;
            vi++;
        }
    }
    for(int j=0;j<tot_stacks;j++) for(int i=0;i<segs;i++){
        uint32_t a=(uint32_t)(j*(segs+1)+i), b=a+(uint32_t)(segs+1);
        idx[ii++]=a; idx[ii++]=b; idx[ii++]=a+1;
        idx[ii++]=a+1; idx[ii++]=b; idx[ii++]=b+1;
    }
    compute_tangents(verts,(uint32_t)vi,idx,(uint32_t)ii);
    int slot=alloc_mesh_slot(ctx);
    if(slot<0){free(verts);free(idx);return 0;}
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,(uint32_t)vi);
    upload_mesh(m,verts,(uint32_t)vi,idx,(uint32_t)ii);
    free(verts); free(idx);
    return slot+1;
}

ENG_3D_MeshID eng3d_mesh_torus(ENG_3D* ctx, float R, float r, int segsR, int segsr){
    if(segsR<4) segsR=32; if(segsr<4) segsr=16;
    int vcnt=(segsR+1)*(segsr+1);
    int icnt=segsR*segsr*6;
    Vertex3D* verts=(Vertex3D*)calloc((size_t)vcnt,sizeof(Vertex3D));
    uint32_t* idx  =(uint32_t*)malloc((size_t)icnt*sizeof(uint32_t));
    int vi=0,ii=0;
    for(int i=0;i<=segsR;i++){
        float u=(float)i/(float)segsR*2.f*(float)M_PI;
        for(int j=0;j<=segsr;j++){
            float v=(float)j/(float)segsr*2.f*(float)M_PI;
            float cx=(R+r*cosf(v))*cosf(u);
            float cy=r*sinf(v);
            float cz=(R+r*cosf(v))*sinf(u);
            float nx=cosf(v)*cosf(u), ny=sinf(v), nz=cosf(v)*sinf(u);
            verts[vi].p[0]=cx; verts[vi].p[1]=cy; verts[vi].p[2]=cz;
            verts[vi].n[0]=nx; verts[vi].n[1]=ny; verts[vi].n[2]=nz;
            verts[vi].uv[0]=(float)i/(float)segsR;
            verts[vi].uv[1]=(float)j/(float)segsr;
            vi++;
        }
    }
    for(int i=0;i<segsR;i++) for(int j=0;j<segsr;j++){
        uint32_t a=(uint32_t)(i*(segsr+1)+j), b=a+(uint32_t)(segsr+1);
        idx[ii++]=a; idx[ii++]=b; idx[ii++]=a+1;
        idx[ii++]=a+1; idx[ii++]=b; idx[ii++]=b+1;
    }
    compute_tangents(verts,(uint32_t)vi,idx,(uint32_t)ii);
    int slot=alloc_mesh_slot(ctx);
    if(slot<0){free(verts);free(idx);return 0;}
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,(uint32_t)vi);
    upload_mesh(m,verts,(uint32_t)vi,idx,(uint32_t)ii);
    free(verts); free(idx);
    return slot+1;
}

/* ══════════════════════════════════════════════════════
 * OBJ ローダー (簡易)
 * ══════════════════════════════════════════════════════*/
#define OBJ_MAX 131072
ENG_3D_MeshID eng3d_mesh_load_obj(ENG_3D* ctx, const char* path){
    FILE* fp=fopen(path,"r"); if(!fp){ fprintf(stderr,"[3D] OBJ: %s\n",path); return 0; }
    float* pos=(float*)malloc(OBJ_MAX*3*sizeof(float));
    float* nor=(float*)malloc(OBJ_MAX*3*sizeof(float));
    float* uv =(float*)malloc(OBJ_MAX*2*sizeof(float));
    Vertex3D* verts=(Vertex3D*)malloc(OBJ_MAX*3*sizeof(Vertex3D));
    uint32_t* idx  =(uint32_t*)malloc(OBJ_MAX*3*sizeof(uint32_t));
    int np=0,nn=0,nu=0,vi=0,ii=0;
    char line[512];
    while(fgets(line,sizeof(line),fp)){
        if(line[0]=='v'&&line[1]==' '){ sscanf(line+2,"%f%f%f",&pos[np*3],&pos[np*3+1],&pos[np*3+2]); np++; }
        else if(line[0]=='v'&&line[1]=='n'){ sscanf(line+3,"%f%f%f",&nor[nn*3],&nor[nn*3+1],&nor[nn*3+2]); nn++; }
        else if(line[0]=='v'&&line[1]=='t'){ sscanf(line+3,"%f%f",&uv[nu*2],&uv[nu*2+1]); nu++; }
        else if(line[0]=='f'&&line[1]==' '){
            int pi[4]={0},ti[4]={0},ni[4]={0},fc=0;
            char* p=line+2;
            while(*p&&fc<4){
                int a=0,b=0,c=0;
                if(sscanf(p,"%d/%d/%d",&a,&b,&c)==3){ pi[fc]=a-1; ti[fc]=b-1; ni[fc]=c-1; }
                else if(sscanf(p,"%d//%d",&a,&c)==2){ pi[fc]=a-1; ni[fc]=c-1; }
                else if(sscanf(p,"%d",&a)==1){ pi[fc]=a-1; }
                fc++;
                while(*p&&*p!=' '&&*p!='\n') p++;
                while(*p==' ') p++;
            }
            int faces=fc-2;
            for(int f=0;f<faces;f++){
                int tris[3]={0,f+1,f+2};
                for(int k=0;k<3;k++){
                    int ti2=tris[k];
                    Vertex3D v; memset(&v,0,sizeof(v));
                    int pi2=pi[ti2];
                    v.p[0]=pos[pi2*3]; v.p[1]=pos[pi2*3+1]; v.p[2]=pos[pi2*3+2];
                    if(nn>0){ int ni2=ni[ti2]; v.n[0]=nor[ni2*3]; v.n[1]=nor[ni2*3+1]; v.n[2]=nor[ni2*3+2]; }
                    if(nu>0){ int ui=ti[ti2]; v.uv[0]=uv[ui*2]; v.uv[1]=uv[ui*2+1]; }
                    verts[vi]=v; idx[ii++]=(uint32_t)vi; vi++;
                }
            }
        }
    }
    fclose(fp);
    compute_tangents(verts,(uint32_t)vi,idx,(uint32_t)ii);
    int slot=alloc_mesh_slot(ctx);
    if(slot<0){ free(pos);free(nor);free(uv);free(verts);free(idx); return 0; }
    Mesh3D* m=&ctx->meshes[slot]; mesh_default(m);
    compute_bounds(m,verts,(uint32_t)vi);
    upload_mesh(m,verts,(uint32_t)vi,idx,(uint32_t)ii);
    free(pos);free(nor);free(uv);free(verts);free(idx);
    return slot+1;
}

void eng3d_mesh_destroy(ENG_3D* ctx, ENG_3D_MeshID id){
    if(id<1||id>ENG_3D_MAX_MESHES||!ctx->meshes[id-1].used) return;
    Mesh3D* m=&ctx->meshes[id-1];
    glDeleteVertexArrays(1,&m->vao);
    glDeleteBuffers(1,&m->vbo);
    glDeleteBuffers(1,&m->ebo);
    memset(m,0,sizeof(*m));
}

int          eng3d_mesh_vertex_count(ENG_3D* ctx,ENG_3D_MeshID id){ return (id>=1&&id<=ENG_3D_MAX_MESHES)?ctx->meshes[id-1].vertex_count:0; }
ENG_3D_AABB  eng3d_mesh_bounds     (ENG_3D* ctx,ENG_3D_MeshID id){
    if(id>=1&&id<=ENG_3D_MAX_MESHES) return ctx->meshes[id-1].bounds;
    ENG_3D_AABB a; memset(&a,0,sizeof(a)); return a;
}

/* ══════════════════════════════════════════════════════
 * テクスチャ
 * ══════════════════════════════════════════════════════*/
ENG_3D_TexID eng3d_tex_load(ENG_3D* ctx, const char* path){
    for(int i=0;i<ENG_3D_MAX_TEXTURES;i++) if(!ctx->tex_used[i]){
        stbi_set_flip_vertically_on_load(1);
        int w,h,ch; unsigned char* data=stbi_load(path,&w,&h,&ch,0);
        if(!data){ fprintf(stderr,"[3D] tex: %s\n",path); return 0; }
        GLenum fmt=(ch==4)?GL_RGBA:(ch==3)?GL_RGB:GL_RED;
        glGenTextures(1,&ctx->textures[i]);
        glBindTexture(GL_TEXTURE_2D,ctx->textures[i]);
        glTexImage2D(GL_TEXTURE_2D,0,(GLint)fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        stbi_image_free(data);
        ctx->tex_used[i]=true;
        return i+1;
    }
    return 0;
}

void eng3d_tex_destroy(ENG_3D* ctx, ENG_3D_TexID id){
    if(id<1||id>ENG_3D_MAX_TEXTURES||!ctx->tex_used[id-1]) return;
    glDeleteTextures(1,&ctx->textures[id-1]);
    ctx->tex_used[id-1]=false;
    ctx->textures[id-1]=0;
}

/* ══════════════════════════════════════════════════════
 * マテリアルセッター
 * ══════════════════════════════════════════════════════*/
#define MESH_OK(ctx,id) ((id)>=1&&(id)<=ENG_3D_MAX_MESHES&&(ctx)->meshes[(id)-1].used)
void eng3d_mesh_color      (ENG_3D* ctx,ENG_3D_MeshID id,float r,float g,float b,float a){if(!MESH_OK(ctx,id))return;float*c=ctx->meshes[id-1].color;c[0]=r;c[1]=g;c[2]=b;c[3]=a;}
void eng3d_mesh_texture    (ENG_3D* ctx,ENG_3D_MeshID id,ENG_3D_TexID t){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].tex_id=t;}
void eng3d_mesh_normal_map (ENG_3D* ctx,ENG_3D_MeshID id,ENG_3D_TexID t){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].normal_map_id=t;}
void eng3d_mesh_specular   (ENG_3D* ctx,ENG_3D_MeshID id,float in,float sh){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].spec_intensity=in;ctx->meshes[id-1].shininess=sh;}
void eng3d_mesh_emissive   (ENG_3D* ctx,ENG_3D_MeshID id,float r,float g,float b,float in){if(!MESH_OK(ctx,id))return;float*e=ctx->meshes[id-1].emissive;e[0]=r;e[1]=g;e[2]=b;ctx->meshes[id-1].emissive_int=in;}
void eng3d_mesh_wireframe  (ENG_3D* ctx,ENG_3D_MeshID id,bool on){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].wireframe=on;}
void eng3d_mesh_cast_shadow(ENG_3D* ctx,ENG_3D_MeshID id,bool on){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].cast_shadow=on;}
void eng3d_mesh_receive_shadow(ENG_3D* ctx,ENG_3D_MeshID id,bool on){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].receive_shadow=on;}
void eng3d_mesh_transparent(ENG_3D* ctx,ENG_3D_MeshID id,bool on){if(!MESH_OK(ctx,id))return;ctx->meshes[id-1].transparent=on;}

/* ══════════════════════════════════════════════════════
 * FBO / ブルーム / シャドウセットアップ
 * ══════════════════════════════════════════════════════*/
static void setup_quad(ENG_3D* ctx) {
    float quad[]={
        -1,1,0,1, -1,-1,0,0, 1,-1,1,0,
        -1,1,0,1,  1,-1,1,0, 1, 1,1,1};
    glGenVertexArrays(1,&ctx->quad_vao);
    glGenBuffers(1,&ctx->quad_vbo);
    glBindVertexArray(ctx->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER,ctx->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

static void setup_bloom_fbo(ENG_3D* ctx) {
    glGenFramebuffers(1,&ctx->bloom_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo);
    glGenTextures(1,&ctx->bloom_color_tex);
    glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,ctx->w,ctx->h,0,GL_RGB,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ctx->bloom_color_tex,0);
    glGenRenderbuffers(1,&ctx->bloom_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER,ctx->bloom_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,ctx->w,ctx->h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,ctx->bloom_depth_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glGenFramebuffers(1,&ctx->bloom_fbo2);
    glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo2);
    glGenTextures(1,&ctx->bloom_color_tex2);
    glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex2);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,ctx->w,ctx->h,0,GL_RGB,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ctx->bloom_color_tex2,0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static void setup_shadow_fbo(ENG_3D* ctx) {
    glGenFramebuffers(1,&ctx->shadow_fbo);
    glGenTextures(1,&ctx->shadow_depth_tex);
    glBindTexture(GL_TEXTURE_2D,ctx->shadow_depth_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,SHADOW_MAP_W,SHADOW_MAP_H,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float border[]={1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glBindFramebuffer(GL_FRAMEBUFFER,ctx->shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,ctx->shadow_depth_tex,0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static float skybox_verts[] = {
    -1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,
    -1,-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,
    1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,
    -1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,1,
    -1,1,-1,1,1,-1,1,1,1,1,1,1,-1,1,1,-1,1,-1,
    -1,-1,-1,-1,-1,1,1,-1,-1,1,-1,-1,-1,-1,1,1,-1,1};

static void setup_skybox_vao(ENG_3D* ctx) {
    glGenVertexArrays(1,&ctx->skybox_vao);
    glGenBuffers(1,&ctx->skybox_vbo);
    glBindVertexArray(ctx->skybox_vao);
    glBindBuffer(GL_ARRAY_BUFFER,ctx->skybox_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(skybox_verts),skybox_verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

static void emitter_init_vbo(Emitter3D* e) {
    glGenVertexArrays(1,&e->vao);
    glGenBuffers(1,&e->vbo);
    glBindVertexArray(e->vao);
    glBindBuffer(GL_ARRAY_BUFFER,e->vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(e->max_parts*8*sizeof(float)),NULL,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glVertexAttribDivisor(1,1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glVertexAttribDivisor(2,1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(7*sizeof(float)));
    glVertexAttribDivisor(3,1);
    glBindVertexArray(0);
}

/* ══════════════════════════════════════════════════════
 * ライフサイクル
 * ══════════════════════════════════════════════════════*/
ENG_3D* eng3d_create(const char* title,int w,int h){
    if(SDL_WasInit(SDL_INIT_VIDEO)==0) SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* win=SDL_CreateWindow(title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        w,h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win){fprintf(stderr,"[3D] SDL_CreateWindow: %s\n",SDL_GetError());return NULL;}
    SDL_GLContext gl=SDL_GL_CreateContext(win);
    if(!gl){fprintf(stderr,"[3D] GL: %s\n",SDL_GetError());SDL_DestroyWindow(win);return NULL;}
#ifdef _WIN32
    win_gl_load();
#endif
    SDL_GL_SetSwapInterval(1);
    ENG_3D* ctx=(ENG_3D*)calloc(1,sizeof(ENG_3D));
    ctx->window=win; ctx->gl_ctx=gl; ctx->w=w; ctx->h=h;
    ctx->cam_pos[0]=0; ctx->cam_pos[1]=3; ctx->cam_pos[2]=5;
    ctx->fov=60.f; ctx->near_z=0.1f; ctx->far_z=500.f;
    ctx->ambient[0]=ctx->ambient[1]=ctx->ambient[2]=0.2f;
    ctx->dir_dir[0]=0.5f; ctx->dir_dir[1]=-1.f; ctx->dir_dir[2]=0.3f;
    ctx->dir_col[0]=ctx->dir_col[1]=ctx->dir_col[2]=0.8f;
    ctx->shadow_bias=0.005f; ctx->shadow_ortho=20.f;
    ctx->bloom_threshold=1.f; ctx->bloom_intensity=0.5f;
    ctx->fog_color[0]=ctx->fog_color[1]=ctx->fog_color[2]=0.5f;
    ctx->fog_start=50.f; ctx->fog_end=200.f; ctx->fog_density=0.01f;
    for(int i=0;i<ENG_3D_MAX_NODES;i++){
        ctx->nodes[i].lscale[0]=ctx->nodes[i].lscale[1]=ctx->nodes[i].lscale[2]=1.f;
        ctx->nodes[i].parent=-1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    ctx->shader_main    =build_program2(VERT_MAIN,   FRAG_MAIN);
    ctx->shader_shadow  =build_program2(VERT_SHADOW, FRAG_SHADOW);
    ctx->shader_skybox  =build_program2(VERT_SKY,    FRAG_SKY);
    ctx->shader_blur    =build_program2(VERT_QUAD,   FRAG_BLUR);
    ctx->shader_combine =build_program2(VERT_QUAD,   FRAG_COMBINE);
    ctx->shader_particle=build_program2(VERT_PARTICLE,FRAG_PARTICLE);
    setup_quad(ctx);
    setup_bloom_fbo(ctx);
    setup_shadow_fbo(ctx);
    setup_skybox_vao(ctx);
    ctx->last_tick=SDL_GetPerformanceCounter();
    ctx->fps_tick =ctx->last_tick;
    ctx->keys=SDL_GetKeyboardState(NULL);
    return ctx;
}

void eng3d_destroy(ENG_3D* ctx){
    if(!ctx) return;
    for(int i=0;i<ENG_3D_MAX_MESHES;i++) if(ctx->meshes[i].used) eng3d_mesh_destroy(ctx,i+1);
    for(int i=0;i<ENG_3D_MAX_TEXTURES;i++) if(ctx->tex_used[i]) eng3d_tex_destroy(ctx,i+1);
    for(int i=0;i<ENG_3D_MAX_EMITTERS;i++) if(ctx->emitters[i].used) eng3d_emitter_destroy(ctx,i+1);
    glDeleteProgram(ctx->shader_main); glDeleteProgram(ctx->shader_shadow);
    glDeleteProgram(ctx->shader_skybox); glDeleteProgram(ctx->shader_blur);
    glDeleteProgram(ctx->shader_combine); glDeleteProgram(ctx->shader_particle);
    glDeleteFramebuffers(1,&ctx->bloom_fbo); glDeleteFramebuffers(1,&ctx->bloom_fbo2);
    glDeleteFramebuffers(1,&ctx->shadow_fbo);
    glDeleteTextures(1,&ctx->bloom_color_tex); glDeleteTextures(1,&ctx->bloom_color_tex2);
    glDeleteTextures(1,&ctx->shadow_depth_tex);
    glDeleteRenderbuffers(1,&ctx->bloom_depth_rbo);
    glDeleteVertexArrays(1,&ctx->quad_vao); glDeleteBuffers(1,&ctx->quad_vbo);
    glDeleteVertexArrays(1,&ctx->skybox_vao); glDeleteBuffers(1,&ctx->skybox_vbo);
    if(ctx->skybox_on) glDeleteTextures(1,&ctx->skybox_cubemap);
    SDL_GL_DeleteContext(ctx->gl_ctx); SDL_DestroyWindow(ctx->window);
    free(ctx);
}

bool eng3d_update(ENG_3D* ctx){
    uint64_t now=SDL_GetPerformanceCounter();
    uint64_t freq=SDL_GetPerformanceFrequency();
    ctx->delta=(float)(now-ctx->last_tick)/(float)freq;
    ctx->last_tick=now;
    ctx->fps_ctr++;
    if(now-ctx->fps_tick>=freq){ctx->fps=ctx->fps_ctr;ctx->fps_ctr=0;ctx->fps_tick=now;}
    memcpy(ctx->prev_keys,ctx->keys,sizeof(ctx->prev_keys));
    ctx->prev_mouse_btn=ctx->mouse_btn;
    ctx->mdx=ctx->mdy=ctx->scroll=0;
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT) ctx->quit=true;
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_RESIZED){
            ctx->w=e.window.data1; ctx->h=e.window.data2;
            glViewport(0,0,ctx->w,ctx->h);
        }
        if(e.type==SDL_MOUSEMOTION){ctx->mdx=(float)e.motion.xrel;ctx->mdy=(float)e.motion.yrel;}
        if(e.type==SDL_MOUSEWHEEL) ctx->scroll+=(float)e.wheel.y;
    }
    int mx,my; ctx->mouse_btn=SDL_GetMouseState(&mx,&my);
    ctx->mx=(float)mx; ctx->my=(float)my;
    return !ctx->quit;
}

float eng3d_delta (ENG_3D* ctx){ return ctx->delta; }
int   eng3d_fps   (ENG_3D* ctx){ return ctx->fps; }
int   eng3d_width (ENG_3D* ctx){ return ctx->w; }
int   eng3d_height(ENG_3D* ctx){ return ctx->h; }

/* ══════════════════════════════════════════════════════
 * カメラ
 * ══════════════════════════════════════════════════════*/
void eng3d_cam_perspective(ENG_3D* ctx,float fov,float n,float f){ctx->fov=fov;ctx->near_z=n;ctx->far_z=f;}
void eng3d_cam_pos   (ENG_3D* ctx,float x,float y,float z){ctx->cam_pos[0]=x;ctx->cam_pos[1]=y;ctx->cam_pos[2]=z;}
void eng3d_cam_target(ENG_3D* ctx,float tx,float ty,float tz){ctx->cam_target[0]=tx;ctx->cam_target[1]=ty;ctx->cam_target[2]=tz;}
void eng3d_cam_lookat(ENG_3D* ctx,float ex,float ey,float ez,float tx,float ty,float tz){eng3d_cam_pos(ctx,ex,ey,ez);eng3d_cam_target(ctx,tx,ty,tz);}
void eng3d_cam_vectors(ENG_3D* ctx,
    float* fx,float* fy,float* fz,float* rx,float* ry,float* rz,float* ux,float* uy,float* uz)
{
    vec3 f,s,u,up_hint={0,1,0};
    v3_sub(f,ctx->cam_target,ctx->cam_pos); v3_norm(f);
    v3_cross(s,f,up_hint); v3_norm(s);
    v3_cross(u,s,f);
    if(fx)*fx=f[0]; if(fy)*fy=f[1]; if(fz)*fz=f[2];
    if(rx)*rx=s[0]; if(ry)*ry=s[1]; if(rz)*rz=s[2];
    if(ux)*ux=u[0]; if(uy)*uy=u[1]; if(uz)*uz=u[2];
}

/* ══════════════════════════════════════════════════════
 * ライティング / シャドウ / フォグ / ブルーム
 * ══════════════════════════════════════════════════════*/
void eng3d_ambient(ENG_3D* ctx,float r,float g,float b){ctx->ambient[0]=r;ctx->ambient[1]=g;ctx->ambient[2]=b;}
void eng3d_dir_light(ENG_3D* ctx,float dx,float dy,float dz,float r,float g,float b){
    ctx->dir_dir[0]=dx;ctx->dir_dir[1]=dy;ctx->dir_dir[2]=dz;
    ctx->dir_col[0]=r; ctx->dir_col[1]=g; ctx->dir_col[2]=b;
}
void eng3d_point_light(ENG_3D* ctx,int slot,float x,float y,float z,float r,float g,float b,float radius){
    if(slot<0||slot>=ENG_3D_MAX_LIGHTS)return;
    PointLight3D* p=&ctx->pt_lights[slot];
    p->x=x;p->y=y;p->z=z;p->r=r;p->g=g;p->b=b;p->radius=radius;p->active=(radius>0.f);
}
void eng3d_spot_light(ENG_3D* ctx,int slot,
    float x,float y,float z,float dx,float dy,float dz,
    float r,float g,float b,float radius,float cutoff,float outer)
{
    if(slot<0||slot>=ENG_3D_MAX_SPOTS)return;
    SpotLight3D* s=&ctx->sp_lights[slot];
    s->x=x;s->y=y;s->z=z;s->dx=dx;s->dy=dy;s->dz=dz;
    s->r=r;s->g=g;s->b=b;s->radius=radius;
    s->cutoff_cos=cosf(DEG2RAD(cutoff));
    s->outer_cos =cosf(DEG2RAD(outer));
    s->active=(radius>0.f);
}
void eng3d_spot_light_off(ENG_3D* ctx,int slot){if(slot>=0&&slot<ENG_3D_MAX_SPOTS)ctx->sp_lights[slot].active=false;}
void eng3d_shadow_enable(ENG_3D* ctx,bool on){ctx->shadow_on=on;}
void eng3d_shadow_bias  (ENG_3D* ctx,float b){ctx->shadow_bias=b;}
void eng3d_shadow_size  (ENG_3D* ctx,float s){ctx->shadow_ortho=s;}
void eng3d_fog_enable   (ENG_3D* ctx,bool on){ctx->fog_on=on;}
void eng3d_fog(ENG_3D* ctx,float r,float g,float b,int mode,float st,float en,float den){
    ctx->fog_color[0]=r;ctx->fog_color[1]=g;ctx->fog_color[2]=b;
    ctx->fog_mode=mode;ctx->fog_start=st;ctx->fog_end=en;ctx->fog_density=den;
}
void eng3d_bloom_enable   (ENG_3D* ctx,bool on){ctx->bloom_on=on;}
void eng3d_bloom_threshold(ENG_3D* ctx,float t){ctx->bloom_threshold=t;}
void eng3d_bloom_intensity(ENG_3D* ctx,float v){ctx->bloom_intensity=v;}

/* ══════════════════════════════════════════════════════
 * スカイボックス
 * ══════════════════════════════════════════════════════*/
bool eng3d_skybox_load(ENG_3D* ctx,
    const char* px,const char* nx,
    const char* py,const char* ny,
    const char* pz,const char* nz)
{
    if(ctx->skybox_on) glDeleteTextures(1,&ctx->skybox_cubemap);
    glGenTextures(1,&ctx->skybox_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP,ctx->skybox_cubemap);
    const char* faces[6]={px,nx,py,ny,pz,nz};
    stbi_set_flip_vertically_on_load(0);
    for(int i=0;i<6;i++){
        int w,h,ch; unsigned char* data=stbi_load(faces[i],&w,&h,&ch,3);
        if(!data){fprintf(stderr,"[3D] skybox: %s\n",faces[i]);return false;}
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,data);
        stbi_image_free(data);
    }
    stbi_set_flip_vertically_on_load(1);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    ctx->skybox_on=true; return true;
}
void eng3d_skybox_draw(ENG_3D* ctx){
    if(!ctx->skybox_on)return;
    glDepthFunc(GL_LEQUAL);
    glUseProgram(ctx->shader_skybox);
    mat4 view_no_trans; m4_copy(view_no_trans,ctx->mat_view);
    view_no_trans[12]=view_no_trans[13]=view_no_trans[14]=0;
    UM4(ctx->shader_skybox,"uView",view_no_trans);
    UM4(ctx->shader_skybox,"uProj",ctx->mat_proj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP,ctx->skybox_cubemap);
    U1I(ctx->shader_skybox,"uSkybox",0);
    glBindVertexArray(ctx->skybox_vao);
    glDrawArrays(GL_TRIANGLES,0,36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}
void eng3d_skybox_unload(ENG_3D* ctx){
    if(!ctx->skybox_on)return;
    glDeleteTextures(1,&ctx->skybox_cubemap);
    ctx->skybox_on=false;
}

/* ══════════════════════════════════════════════════════
 * 内部: uniform 一括設定
 * ══════════════════════════════════════════════════════*/
static void set_lighting_uniforms(ENG_3D* ctx,unsigned int prog){
    U3F(prog,"uAmbient",ctx->ambient);
    U3F(prog,"uDirDir",ctx->dir_dir);
    U3F(prog,"uDirCol",ctx->dir_col);
    U3F(prog,"uCamPos",ctx->cam_pos);
    float pt_pos[ENG_3D_MAX_LIGHTS*3]={0},pt_col[ENG_3D_MAX_LIGHTS*3]={0},pt_rad[ENG_3D_MAX_LIGHTS]={0};
    int npt=0;
    for(int i=0;i<ENG_3D_MAX_LIGHTS;i++) if(ctx->pt_lights[i].active){
        pt_pos[npt*3]=ctx->pt_lights[i].x;pt_pos[npt*3+1]=ctx->pt_lights[i].y;pt_pos[npt*3+2]=ctx->pt_lights[i].z;
        pt_col[npt*3]=ctx->pt_lights[i].r;pt_col[npt*3+1]=ctx->pt_lights[i].g;pt_col[npt*3+2]=ctx->pt_lights[i].b;
        pt_rad[npt]=ctx->pt_lights[i].radius; npt++;
    }
    glUniform3fv(UL(prog,"uPtPos"),ENG_3D_MAX_LIGHTS,pt_pos);
    glUniform3fv(UL(prog,"uPtCol"),ENG_3D_MAX_LIGHTS,pt_col);
    glUniform1fv(UL(prog,"uPtRad"),ENG_3D_MAX_LIGHTS,pt_rad);
    U1I(prog,"uPtCount",npt);
    float sp_pos[ENG_3D_MAX_SPOTS*3]={0},sp_dir[ENG_3D_MAX_SPOTS*3]={0},sp_col[ENG_3D_MAX_SPOTS*3]={0};
    float sp_rad[ENG_3D_MAX_SPOTS]={0},sp_cut[ENG_3D_MAX_SPOTS]={0},sp_out[ENG_3D_MAX_SPOTS]={0};
    int nsp=0;
    for(int i=0;i<ENG_3D_MAX_SPOTS;i++) if(ctx->sp_lights[i].active){
        sp_pos[nsp*3]=ctx->sp_lights[i].x;sp_pos[nsp*3+1]=ctx->sp_lights[i].y;sp_pos[nsp*3+2]=ctx->sp_lights[i].z;
        sp_dir[nsp*3]=ctx->sp_lights[i].dx;sp_dir[nsp*3+1]=ctx->sp_lights[i].dy;sp_dir[nsp*3+2]=ctx->sp_lights[i].dz;
        sp_col[nsp*3]=ctx->sp_lights[i].r;sp_col[nsp*3+1]=ctx->sp_lights[i].g;sp_col[nsp*3+2]=ctx->sp_lights[i].b;
        sp_rad[nsp]=ctx->sp_lights[i].radius;
        sp_cut[nsp]=ctx->sp_lights[i].cutoff_cos;
        sp_out[nsp]=ctx->sp_lights[i].outer_cos;
        nsp++;
    }
    glUniform3fv(UL(prog,"uSpPos"),ENG_3D_MAX_SPOTS,sp_pos);
    glUniform3fv(UL(prog,"uSpDir"),ENG_3D_MAX_SPOTS,sp_dir);
    glUniform3fv(UL(prog,"uSpCol"),ENG_3D_MAX_SPOTS,sp_col);
    glUniform1fv(UL(prog,"uSpRad"),ENG_3D_MAX_SPOTS,sp_rad);
    glUniform1fv(UL(prog,"uSpCut"),ENG_3D_MAX_SPOTS,sp_cut);
    glUniform1fv(UL(prog,"uSpOut"),ENG_3D_MAX_SPOTS,sp_out);
    U1I(prog,"uSpCount",nsp);
    int fm=ctx->fog_on?ctx->fog_mode:-1;
    U1I(prog,"uFogMode",fm);
    U3F(prog,"uFogColor",ctx->fog_color);
    U1F(prog,"uFogStart",ctx->fog_start);
    U1F(prog,"uFogEnd",ctx->fog_end);
    U1F(prog,"uFogDensity",ctx->fog_density);
}

/* ══════════════════════════════════════════════════════
 * 内部: メッシュ描画
 * ══════════════════════════════════════════════════════*/
static void draw_mesh_internal(ENG_3D* ctx,unsigned int prog,
    ENG_3D_MeshID mesh_id,
    float px,float py,float pz,
    float rx,float ry,float rz,
    float sx,float sy,float sz)
{
    if(mesh_id<1||mesh_id>ENG_3D_MAX_MESHES||!ctx->meshes[mesh_id-1].used)return;
    Mesh3D* m=&ctx->meshes[mesh_id-1];
    mat4 model,mvp,tmp;
    m4_trs(model,px,py,pz,rx,ry,rz,sx,sy,sz);
    m4_mul(tmp,ctx->mat_view,model); m4_mul(mvp,ctx->mat_proj,tmp);
    float nm[9]; m4_normal(nm,model);
    UM4(prog,"uMVP",mvp); UM4(prog,"uModel",model);
    if(UL(prog,"uNM")>=0) UM3(prog,"uNM",nm);
    UM4(prog,"uLightSpace",ctx->mat_light_space);
    U4F(prog,"uColor",m->color);
    U3F(prog,"uEmissive",m->emissive);
    U1F(prog,"uEmissiveInt",m->emissive_int);
    U1F(prog,"uSpecInt",m->spec_intensity);
    U1F(prog,"uShininess",m->shininess);
    int hasTex=0;
    if(m->tex_id>=1&&m->tex_id<=ENG_3D_MAX_TEXTURES&&ctx->tex_used[m->tex_id-1]){
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,ctx->textures[m->tex_id-1]);
        U1I(prog,"uAlbedo",0); hasTex=1;
    }
    U1I(prog,"uHasTex",hasTex);
    int hasNM=0;
    if(m->normal_map_id>=1&&m->normal_map_id<=ENG_3D_MAX_TEXTURES&&ctx->tex_used[m->normal_map_id-1]){
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,ctx->textures[m->normal_map_id-1]);
        U1I(prog,"uNormalMap",1); hasNM=1;
    }
    U1I(prog,"uHasNM",hasNM);
    int hasShadow=ctx->shadow_on&&m->receive_shadow?1:0;
    if(hasShadow){
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,ctx->shadow_depth_tex);
        U1I(prog,"uShadowMap",2);
    }
    U1I(prog,"uHasShadow",hasShadow);
    U1F(prog,"uShadowBias",ctx->shadow_bias);
    if(m->wireframe) glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    if(m->transparent){glEnable(GL_BLEND);glDepthMask(GL_FALSE);}
    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES,(GLsizei)m->index_count,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
    if(m->wireframe) glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    if(m->transparent) glDepthMask(GL_TRUE);
}

/* ══════════════════════════════════════════════════════
 * 描画 パブリック API
 * ══════════════════════════════════════════════════════*/
void eng3d_begin(ENG_3D* ctx,float r,float g,float b){
    float aspect=(ctx->h>0)?(float)ctx->w/(float)ctx->h:1.f;
    m4_perspective(ctx->mat_proj,DEG2RAD(ctx->fov),aspect,ctx->near_z,ctx->far_z);
    vec3 up={0,1,0};
    m4_lookat(ctx->mat_view,ctx->cam_pos,ctx->cam_target,up);
    /* シャドウ行列だけ事前計算 */
    if(ctx->shadow_on){
        float os=ctx->shadow_ortho;
        mat4 lp,lv;
        vec3 ld={ctx->dir_dir[0],ctx->dir_dir[1],ctx->dir_dir[2]};
        vec3 le={-ld[0]*os,-ld[1]*os,-ld[2]*os};
        vec3 lt={0,0,0};
        m4_ortho(lp,-os,os,-os,os,-os*2.f,os*2.f);
        m4_lookat(lv,le,lt,up);
        m4_mul(ctx->mat_light_space,lp,lv);
    } else {
        m4_id(ctx->mat_light_space);
    }
    if(ctx->bloom_on) glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo);
    else              glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,ctx->w,ctx->h);
    glClearColor(r,g,b,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(ctx->shader_main);
    set_lighting_uniforms(ctx,ctx->shader_main);
}

void eng3d_draw(ENG_3D* ctx,ENG_3D_MeshID mesh_id,
    float px,float py,float pz,
    float rx,float ry,float rz,
    float sx,float sy,float sz)
{
    if(!ctx)return;
    if(sx==0&&sy==0&&sz==0){sx=sy=sz=1.f;}
    /* シャドウパス */
    if(ctx->shadow_on&&mesh_id>=1&&mesh_id<=ENG_3D_MAX_MESHES
       &&ctx->meshes[mesh_id-1].used&&ctx->meshes[mesh_id-1].cast_shadow)
    {
        mat4 model; m4_trs(model,px,py,pz,rx,ry,rz,sx,sy,sz);
        glBindFramebuffer(GL_FRAMEBUFFER,ctx->shadow_fbo);
        glViewport(0,0,SHADOW_MAP_W,SHADOW_MAP_H);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(ctx->shader_shadow);
        glCullFace(GL_FRONT);
        glUniformMatrix4fv(UL(ctx->shader_shadow,"uModel"),1,GL_FALSE,model);
        glUniformMatrix4fv(UL(ctx->shader_shadow,"uLightSpace"),1,GL_FALSE,ctx->mat_light_space);
        glBindVertexArray(ctx->meshes[mesh_id-1].vao);
        glDrawElements(GL_TRIANGLES,(GLsizei)ctx->meshes[mesh_id-1].index_count,GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
        glCullFace(GL_BACK);
        if(ctx->bloom_on) glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo);
        else              glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,ctx->w,ctx->h);
        glUseProgram(ctx->shader_main);
    }
    draw_mesh_internal(ctx,ctx->shader_main,mesh_id,px,py,pz,rx,ry,rz,sx,sy,sz);
}

void eng3d_end(ENG_3D* ctx){
    if(ctx->bloom_on){
        /* 水平ブラー */
        glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo2);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(ctx->shader_blur);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex);
        U1I(ctx->shader_blur,"uImage",0);
        glUniform1i(UL(ctx->shader_blur,"uHorizontal"),1);
        glBindVertexArray(ctx->quad_vao); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0);
        /* 垂直ブラー */
        glBindFramebuffer(GL_FRAMEBUFFER,ctx->bloom_fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex2);
        glUniform1i(UL(ctx->shader_blur,"uHorizontal"),0);
        glBindVertexArray(ctx->quad_vao); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0);
        /* 合成 */
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,ctx->w,ctx->h);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(ctx->shader_combine);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,ctx->bloom_color_tex2);
        U1I(ctx->shader_combine,"uScene",0);
        U1I(ctx->shader_combine,"uBloom",1);
        U1F(ctx->shader_combine,"uThreshold",ctx->bloom_threshold);
        U1F(ctx->shader_combine,"uIntensity",ctx->bloom_intensity);
        glBindVertexArray(ctx->quad_vao); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0);
    }
    SDL_GL_SwapWindow(ctx->window);
}

/* ══════════════════════════════════════════════════════
 * パーティクルシステム
 * ══════════════════════════════════════════════════════*/
ENG_3D_EmitterID eng3d_emitter_create(ENG_3D* ctx,int max_p){
    for(int i=0;i<ENG_3D_MAX_EMITTERS;i++) if(!ctx->emitters[i].used){
        Emitter3D* e=&ctx->emitters[i]; memset(e,0,sizeof(*e));
        if(max_p<1)max_p=100;
        e->max_parts=(max_p>MAX_PARTICLES)?MAX_PARTICLES:max_p;
        e->rate=10.f; e->life_min=1.f; e->life_max=2.f;
        e->vel[1]=1.f; e->spread=0.5f;
        e->size_s=0.1f;
        e->color_s[0]=e->color_s[1]=e->color_s[2]=e->color_s[3]=1.f;
        e->active=true; e->used=true;
        emitter_init_vbo(e);
        return i+1;
    }
    return 0;
}
void eng3d_emitter_destroy(ENG_3D* ctx,ENG_3D_EmitterID id){
    if(id<1||id>ENG_3D_MAX_EMITTERS||!ctx->emitters[id-1].used)return;
    Emitter3D* e=&ctx->emitters[id-1];
    glDeleteVertexArrays(1,&e->vao); glDeleteBuffers(1,&e->vbo);
    memset(e,0,sizeof(*e));
}
void eng3d_emitter_pos    (ENG_3D* c,ENG_3D_EmitterID id,float x,float y,float z){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].pos[0]=x;c->emitters[id-1].pos[1]=y;c->emitters[id-1].pos[2]=z;}
void eng3d_emitter_rate   (ENG_3D* c,ENG_3D_EmitterID id,float r){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].rate=r;}
void eng3d_emitter_life   (ENG_3D* c,ENG_3D_EmitterID id,float mn,float mx){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].life_min=mn;c->emitters[id-1].life_max=mx;}
void eng3d_emitter_velocity(ENG_3D* c,ENG_3D_EmitterID id,float vx,float vy,float vz,float sp){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;Emitter3D*e=&c->emitters[id-1];e->vel[0]=vx;e->vel[1]=vy;e->vel[2]=vz;e->spread=sp;}
void eng3d_emitter_gravity (ENG_3D* c,ENG_3D_EmitterID id,float gx,float gy,float gz){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;Emitter3D*e=&c->emitters[id-1];e->grav[0]=gx;e->grav[1]=gy;e->grav[2]=gz;}
void eng3d_emitter_color  (ENG_3D* c,ENG_3D_EmitterID id,float r,float g,float b,float a){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;float*cs=c->emitters[id-1].color_s;cs[0]=r;cs[1]=g;cs[2]=b;cs[3]=a;}
void eng3d_emitter_color_end(ENG_3D* c,ENG_3D_EmitterID id,float r,float g,float b,float a){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;float*ce=c->emitters[id-1].color_e;ce[0]=r;ce[1]=g;ce[2]=b;ce[3]=a;}
void eng3d_emitter_size   (ENG_3D* c,ENG_3D_EmitterID id,float s,float e){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].size_s=s;c->emitters[id-1].size_e=e;}
void eng3d_emitter_texture(ENG_3D* c,ENG_3D_EmitterID id,ENG_3D_TexID t){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].tex_id=t;}
void eng3d_emitter_active (ENG_3D* c,ENG_3D_EmitterID id,bool on){if(id<1||id>ENG_3D_MAX_EMITTERS||!c->emitters[id-1].used)return;c->emitters[id-1].active=on;}

static float randf(void){ return (float)rand()/(float)RAND_MAX; }

void eng3d_emitter_burst(ENG_3D* ctx,ENG_3D_EmitterID id,int count){
    if(id<1||id>ENG_3D_MAX_EMITTERS||!ctx->emitters[id-1].used)return;
    Emitter3D* e=&ctx->emitters[id-1];
    for(int i=0;i<count;i++){
        for(int j=0;j<e->max_parts;j++){
            if(!e->parts[j].alive){
                Particle* p=&e->parts[j];
                for(int k=0;k<3;k++) p->pos[k]=e->pos[k];
                p->vel[0]=e->vel[0]+(randf()-0.5f)*2.f*e->spread;
                p->vel[1]=e->vel[1]+(randf()-0.5f)*2.f*e->spread;
                p->vel[2]=e->vel[2]+(randf()-0.5f)*2.f*e->spread;
                p->life=e->life_min+(e->life_max-e->life_min)*randf();
                p->life_max=p->life;
                for(int k=0;k<4;k++){p->color[k]=e->color_s[k];p->color_end[k]=e->color_e[k];}
                p->size=e->size_s; p->size_end=e->size_e;
                p->alive=true; break;
            }
        }
    }
}

void eng3d_emitter_update_draw(ENG_3D* ctx,ENG_3D_EmitterID id){
    if(id<1||id>ENG_3D_MAX_EMITTERS||!ctx->emitters[id-1].used)return;
    Emitter3D* e=&ctx->emitters[id-1];
    float dt=ctx->delta;
    if(e->active){
        e->accum+=e->rate*dt;
        while(e->accum>=1.f){eng3d_emitter_burst(ctx,id,1);e->accum-=1.f;}
    }
    float* inst=(float*)malloc((size_t)e->max_parts*8*sizeof(float));
    int alive=0;
    for(int i=0;i<e->max_parts;i++){
        Particle* p=&e->parts[i]; if(!p->alive)continue;
        p->life-=dt;
        if(p->life<=0.f){p->alive=false;continue;}
        float t=1.f-(p->life/p->life_max);
        for(int k=0;k<3;k++) p->vel[k]+=e->grav[k]*dt;
        for(int k=0;k<3;k++) p->pos[k]+=p->vel[k]*dt;
        for(int k=0;k<4;k++) p->color[k]=e->color_s[k]+(e->color_e[k]-e->color_s[k])*t;
        p->size=e->size_s+(e->size_e-e->size_s)*t;
        inst[alive*8+0]=p->pos[0]; inst[alive*8+1]=p->pos[1]; inst[alive*8+2]=p->pos[2];
        inst[alive*8+3]=p->color[0]; inst[alive*8+4]=p->color[1];
        inst[alive*8+5]=p->color[2]; inst[alive*8+6]=p->color[3];
        inst[alive*8+7]=p->size;
        alive++;
    }
    if(alive>0){
        glUseProgram(ctx->shader_particle);
        UM4(ctx->shader_particle,"uView",ctx->mat_view);
        UM4(ctx->shader_particle,"uProj",ctx->mat_proj);
        int hasTex=0;
        if(e->tex_id>=1&&e->tex_id<=ENG_3D_MAX_TEXTURES&&ctx->tex_used[e->tex_id-1]){
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,ctx->textures[e->tex_id-1]);
            U1I(ctx->shader_particle,"uTex",0); hasTex=1;
        }
        U1I(ctx->shader_particle,"uHasTex",hasTex);
        glBindVertexArray(e->vao);
        glBindBuffer(GL_ARRAY_BUFFER,e->vbo);
        glBufferSubData(GL_ARRAY_BUFFER,0,(GLsizeiptr)(alive*8*sizeof(float)),inst);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glDepthMask(GL_FALSE);
        glDrawArraysInstanced(GL_TRIANGLE_FAN,0,4,alive);
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glBindVertexArray(0);
    }
    free(inst);
}

/* ══════════════════════════════════════════════════════
 * シーングラフ
 * ══════════════════════════════════════════════════════*/
ENG_3D_NodeID eng3d_node_create(ENG_3D* ctx){
    for(int i=0;i<ENG_3D_MAX_NODES;i++) if(!ctx->nodes[i].used){
        SceneNode* n=&ctx->nodes[i]; memset(n,0,sizeof(*n));
        n->lscale[0]=n->lscale[1]=n->lscale[2]=1.f;
        n->parent=-1; n->active=true; n->used=true; return i+1;
    }
    return 0;
}
void eng3d_node_destroy(ENG_3D* ctx,ENG_3D_NodeID id){if(id<1||id>ENG_3D_MAX_NODES)return;memset(&ctx->nodes[id-1],0,sizeof(SceneNode));}
void eng3d_node_parent(ENG_3D* ctx,ENG_3D_NodeID child,ENG_3D_NodeID parent){if(child<1||child>ENG_3D_MAX_NODES)return;ctx->nodes[child-1].parent=(parent>=1&&parent<=ENG_3D_MAX_NODES)?(int)(parent-1):-1;}
void eng3d_node_mesh  (ENG_3D* ctx,ENG_3D_NodeID id,ENG_3D_MeshID mesh){if(id<1||id>ENG_3D_MAX_NODES)return;ctx->nodes[id-1].mesh=mesh;}
void eng3d_node_pos   (ENG_3D* ctx,ENG_3D_NodeID id,float x,float y,float z){if(id<1||id>ENG_3D_MAX_NODES)return;SceneNode*n=&ctx->nodes[id-1];n->lpos[0]=x;n->lpos[1]=y;n->lpos[2]=z;}
void eng3d_node_rot   (ENG_3D* ctx,ENG_3D_NodeID id,float x,float y,float z){if(id<1||id>ENG_3D_MAX_NODES)return;SceneNode*n=&ctx->nodes[id-1];n->lrot[0]=x;n->lrot[1]=y;n->lrot[2]=z;}
void eng3d_node_scale (ENG_3D* ctx,ENG_3D_NodeID id,float x,float y,float z){if(id<1||id>ENG_3D_MAX_NODES)return;SceneNode*n=&ctx->nodes[id-1];n->lscale[0]=x;n->lscale[1]=y;n->lscale[2]=z;}
void eng3d_node_active(ENG_3D* ctx,ENG_3D_NodeID id,bool on){if(id<1||id>ENG_3D_MAX_NODES)return;ctx->nodes[id-1].active=on;}

static void node_world_mat(ENG_3D* ctx,int idx,mat4 out){
    SceneNode* n=&ctx->nodes[idx];
    mat4 local;
    m4_trs(local,n->lpos[0],n->lpos[1],n->lpos[2],
                 n->lrot[0],n->lrot[1],n->lrot[2],
                 n->lscale[0],n->lscale[1],n->lscale[2]);
    if(n->parent>=0){
        mat4 pmat; node_world_mat(ctx,n->parent,pmat);
        m4_mul(out,pmat,local);
    } else {
        m4_copy(out,local);
    }
}

void eng3d_node_draw(ENG_3D* ctx,ENG_3D_NodeID id){
    if(id<1||id>ENG_3D_MAX_NODES||!ctx->nodes[id-1].used||!ctx->nodes[id-1].active)return;
    mat4 wm; node_world_mat(ctx,id-1,wm);
    SceneNode* n=&ctx->nodes[id-1];
    eng3d_draw(ctx,n->mesh,wm[12],wm[13],wm[14],
               n->lrot[0],n->lrot[1],n->lrot[2],
               n->lscale[0],n->lscale[1],n->lscale[2]);
}
void eng3d_node_world_pos(ENG_3D* ctx,ENG_3D_NodeID id,float* x,float* y,float* z){
    if(id<1||id>ENG_3D_MAX_NODES){if(x)*x=0;if(y)*y=0;if(z)*z=0;return;}
    mat4 wm; node_world_mat(ctx,id-1,wm);
    if(x)*x=wm[12]; if(y)*y=wm[13]; if(z)*z=wm[14];
}

/* ══════════════════════════════════════════════════════
 * キーフレームアニメーション
 * ══════════════════════════════════════════════════════*/
ENG_3D_AnimID eng3d_anim_create(ENG_3D* ctx){
    for(int i=0;i<ENG_3D_MAX_ANIMS;i++) if(!ctx->anims[i].used){
        Anim3D* a=&ctx->anims[i]; memset(a,0,sizeof(*a));
        a->cur_scale[0]=a->cur_scale[1]=a->cur_scale[2]=1.f;
        a->used=true; return i+1;
    }
    return 0;
}
void eng3d_anim_destroy(ENG_3D* ctx,ENG_3D_AnimID id){if(id<1||id>ENG_3D_MAX_ANIMS)return;memset(&ctx->anims[id-1],0,sizeof(Anim3D));}

static void anim_add_key(AnimKey* keys,int* n,float t,float x,float y,float z){
    if(*n>=MAX_ANIM_KEYS)return;
    keys[*n].t=t;keys[*n].v[0]=x;keys[*n].v[1]=y;keys[*n].v[2]=z;(*n)++;
}
void eng3d_anim_key_pos  (ENG_3D* ctx,ENG_3D_AnimID id,float t,float x,float y,float z){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;Anim3D*a=&ctx->anims[id-1];anim_add_key(a->pos_keys,&a->n_pos,t,x,y,z);if(t>a->duration)a->duration=t;}
void eng3d_anim_key_rot  (ENG_3D* ctx,ENG_3D_AnimID id,float t,float x,float y,float z){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;Anim3D*a=&ctx->anims[id-1];anim_add_key(a->rot_keys,&a->n_rot,t,x,y,z);if(t>a->duration)a->duration=t;}
void eng3d_anim_key_scale(ENG_3D* ctx,ENG_3D_AnimID id,float t,float x,float y,float z){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;Anim3D*a=&ctx->anims[id-1];anim_add_key(a->scale_keys,&a->n_scale,t,x,y,z);if(t>a->duration)a->duration=t;}
void eng3d_anim_play(ENG_3D* ctx,ENG_3D_AnimID id){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;ctx->anims[id-1].playing=true;}
void eng3d_anim_stop(ENG_3D* ctx,ENG_3D_AnimID id){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;ctx->anims[id-1].playing=false;}
void eng3d_anim_loop(ENG_3D* ctx,ENG_3D_AnimID id,bool on){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;ctx->anims[id-1].loop=on;}
void eng3d_anim_seek(ENG_3D* ctx,ENG_3D_AnimID id,float t){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;ctx->anims[id-1].time=t;}
bool eng3d_anim_is_playing(ENG_3D* ctx,ENG_3D_AnimID id){if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return false;return ctx->anims[id-1].playing;}

static void anim_eval(AnimKey* keys,int n,float t,float* out){
    if(n==0){out[0]=out[1]=out[2]=0.f;return;}
    if(n==1||t<=keys[0].t){memcpy(out,keys[0].v,12);return;}
    if(t>=keys[n-1].t){memcpy(out,keys[n-1].v,12);return;}
    for(int i=0;i<n-1;i++){
        if(t>=keys[i].t&&t<=keys[i+1].t){
            float alpha=(t-keys[i].t)/(keys[i+1].t-keys[i].t);
            for(int j=0;j<3;j++) out[j]=keys[i].v[j]+(keys[i+1].v[j]-keys[i].v[j])*alpha;
            return;
        }
    }
    memcpy(out,keys[n-1].v,12);
}

void eng3d_anim_update(ENG_3D* ctx,ENG_3D_AnimID id,float delta){
    if(id<1||id>ENG_3D_MAX_ANIMS||!ctx->anims[id-1].used)return;
    Anim3D* a=&ctx->anims[id-1]; if(!a->playing)return;
    a->time+=delta;
    if(a->duration>0&&a->time>a->duration){
        if(a->loop) a->time=fmodf(a->time,a->duration);
        else {a->time=a->duration;a->playing=false;}
    }
    anim_eval(a->pos_keys,a->n_pos,a->time,a->cur_pos);
    anim_eval(a->rot_keys,a->n_rot,a->time,a->cur_rot);
    if(a->n_scale>0) anim_eval(a->scale_keys,a->n_scale,a->time,a->cur_scale);
    else {a->cur_scale[0]=a->cur_scale[1]=a->cur_scale[2]=1.f;}
}
void eng3d_anim_get_pos  (ENG_3D* ctx,ENG_3D_AnimID id,float*x,float*y,float*z){Anim3D*a=(id>=1&&id<=ENG_3D_MAX_ANIMS&&ctx->anims[id-1].used)?&ctx->anims[id-1]:NULL;if(x)*x=a?a->cur_pos[0]:0;if(y)*y=a?a->cur_pos[1]:0;if(z)*z=a?a->cur_pos[2]:0;}
void eng3d_anim_get_rot  (ENG_3D* ctx,ENG_3D_AnimID id,float*x,float*y,float*z){Anim3D*a=(id>=1&&id<=ENG_3D_MAX_ANIMS&&ctx->anims[id-1].used)?&ctx->anims[id-1]:NULL;if(x)*x=a?a->cur_rot[0]:0;if(y)*y=a?a->cur_rot[1]:0;if(z)*z=a?a->cur_rot[2]:0;}
void eng3d_anim_get_scale(ENG_3D* ctx,ENG_3D_AnimID id,float*x,float*y,float*z){Anim3D*a=(id>=1&&id<=ENG_3D_MAX_ANIMS&&ctx->anims[id-1].used)?&ctx->anims[id-1]:NULL;if(x)*x=a?a->cur_scale[0]:1;if(y)*y=a?a->cur_scale[1]:1;if(z)*z=a?a->cur_scale[2]:1;}

/* ══════════════════════════════════════════════════════
 * レイキャスト
 * ══════════════════════════════════════════════════════*/
static bool ray_aabb(const float* ro,const float* rd,const ENG_3D_AABB* aabb,float* t_out){
    float tmin=-FLT_MAX,tmax=FLT_MAX;
    for(int i=0;i<3;i++){
        if(fabsf(rd[i])<1e-8f){
            if(ro[i]<aabb->min[i]||ro[i]>aabb->max[i])return false;
        } else {
            float t1=(aabb->min[i]-ro[i])/rd[i], t2=(aabb->max[i]-ro[i])/rd[i];
            if(t1>t2){float tmp=t1;t1=t2;t2=tmp;}
            if(t1>tmin)tmin=t1; if(t2<tmax)tmax=t2;
            if(tmin>tmax)return false;
        }
    }
    if(tmax<0)return false;
    *t_out=tmin>=0?tmin:tmax;
    return true;
}

ENG_3D_RayHit eng3d_raycast(ENG_3D* ctx,float ox,float oy,float oz,float dx,float dy,float dz){
    ENG_3D_RayHit hit={0};
    float ro[3]={ox,oy,oz},rd[3]={dx,dy,dz};
    float len=sqrtf(rd[0]*rd[0]+rd[1]*rd[1]+rd[2]*rd[2])+1e-8f;
    rd[0]/=len;rd[1]/=len;rd[2]/=len;
    float best=FLT_MAX;
    for(int i=0;i<ENG_3D_MAX_MESHES;i++){
        Mesh3D* m=&ctx->meshes[i]; if(!m->used)continue;
        float t;
        if(ray_aabb(ro,rd,&m->bounds,&t)&&t<best){
            best=t; hit.hit=true; hit.dist=t;
            hit.x=ro[0]+rd[0]*t; hit.y=ro[1]+rd[1]*t; hit.z=ro[2]+rd[2]*t;
            hit.mesh_id=i+1;
        }
    }
    return hit;
}

ENG_3D_RayHit eng3d_raycast_screen(ENG_3D* ctx,float sx,float sy){
    float ndcx=(2.f*sx/(float)ctx->w)-1.f;
    float ndcy=1.f-(2.f*sy/(float)ctx->h);
    float aspect=(ctx->h>0)?(float)ctx->w/(float)ctx->h:1.f;
    float tanf_fov=tanf(DEG2RAD(ctx->fov)*0.5f);
    float vx=ndcx*tanf_fov*aspect, vy=ndcy*tanf_fov;
    vec3 front,s,u,up={0,1,0};
    v3_sub(front,ctx->cam_target,ctx->cam_pos); v3_norm(front);
    v3_cross(s,front,up); v3_norm(s);
    v3_cross(u,s,front);
    float dx=s[0]*vx+u[0]*vy+front[0];
    float dy=s[1]*vx+u[1]*vy+front[1];
    float dz=s[2]*vx+u[2]*vy+front[2];
    return eng3d_raycast(ctx,ctx->cam_pos[0],ctx->cam_pos[1],ctx->cam_pos[2],dx,dy,dz);
}

bool eng3d_aabb_overlap(ENG_3D_AABB a,ENG_3D_AABB b){
    return(a.max[0]>=b.min[0]&&a.min[0]<=b.max[0])&&
          (a.max[1]>=b.min[1]&&a.min[1]<=b.max[1])&&
          (a.max[2]>=b.min[2]&&a.min[2]<=b.max[2]);
}
ENG_3D_AABB eng3d_aabb_transform(ENG_3D_AABB aabb,
    float px,float py,float pz,float rx,float ry,float rz,float sx,float sy,float sz)
{
    return aabb_transform_internal(aabb,px,py,pz,rx,ry,rz,sx,sy,sz);
}

/* ══════════════════════════════════════════════════════
 * 入力
 * ══════════════════════════════════════════════════════*/
bool  eng3d_key        (ENG_3D* ctx,int sc){ return ctx->keys[sc]!=0; }
bool  eng3d_key_down   (ENG_3D* ctx,int sc){ return ctx->keys[sc]&&!ctx->prev_keys[sc]; }
bool  eng3d_key_up     (ENG_3D* ctx,int sc){ return !ctx->keys[sc]&&ctx->prev_keys[sc]; }
float eng3d_mouse_x    (ENG_3D* ctx){ return ctx->mx; }
float eng3d_mouse_y    (ENG_3D* ctx){ return ctx->my; }
float eng3d_mouse_dx   (ENG_3D* ctx){ return ctx->mdx; }
float eng3d_mouse_dy   (ENG_3D* ctx){ return ctx->mdy; }
float eng3d_scroll     (ENG_3D* ctx){ return ctx->scroll; }
bool  eng3d_mouse_btn  (ENG_3D* ctx,int btn){
    if(btn==1)return(ctx->mouse_btn&SDL_BUTTON_LMASK)!=0;
    if(btn==2)return(ctx->mouse_btn&SDL_BUTTON_MMASK)!=0;
    if(btn==3)return(ctx->mouse_btn&SDL_BUTTON_RMASK)!=0;
    return false;
}
bool  eng3d_mouse_btn_down(ENG_3D* ctx,int btn){
    uint32_t mask=(btn==1)?SDL_BUTTON_LMASK:(btn==3)?SDL_BUTTON_RMASK:SDL_BUTTON_MMASK;
    return (ctx->mouse_btn&mask)&&!(ctx->prev_mouse_btn&mask);
}
void  eng3d_mouse_relative(ENG_3D* ctx,bool on){(void)ctx;SDL_SetRelativeMouseMode(on?SDL_TRUE:SDL_FALSE);}
