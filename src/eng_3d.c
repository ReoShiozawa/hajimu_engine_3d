/**
 * src/eng_3d.c — 3D エンジン実装
 *
 * SDL2 + OpenGL 3.3 Core Profile
 * Blinn-Phong 照明, VAO/VBO 管理, stb_image テクスチャ,
 * 簡易 OBJ ローダー, コラム優先 mat4 演算
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

#include "eng_3d.h"

#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#else
#  define GL_GLEXT_PROTOTYPES 1
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ────────────────────────────────────────────────────────
 * 定数
 * ────────────────────────────────────────────────────────*/
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#define DEG2RAD(d)  ((float)((d) * M_PI / 180.0))

/* ────────────────────────────────────────────────────────
 * mat4 ユーティリティ (列優先)
 * ────────────────────────────────────────────────────────*/
typedef float mat4[16];
typedef float vec3[3];

static void m4_id(mat4 m) {
    memset(m, 0, sizeof(mat4));
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

static void m4_mul(mat4 dst, const mat4 a, const mat4 b) {
    mat4 t;
    for (int c=0;c<4;c++)
        for (int r=0;r<4;r++) {
            t[c*4+r]=0;
            for (int k=0;k<4;k++)
                t[c*4+r] += a[k*4+r] * b[c*4+k];
        }
    memcpy(dst, t, sizeof(mat4));
}

static void m4_perspective(mat4 m, float fov_rad, float aspect, float n, float f) {
    float t = tanf(fov_rad * 0.5f);
    memset(m, 0, sizeof(mat4));
    m[0]  =  1.0f / (aspect * t);
    m[5]  =  1.0f / t;
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -(2.0f * f * n) / (f - n);
}

static float v3_dot(const vec3 a, const vec3 b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void  v3_cross(vec3 r, const vec3 a, const vec3 b) {
    r[0]=a[1]*b[2]-a[2]*b[1];
    r[1]=a[2]*b[0]-a[0]*b[2];
    r[2]=a[0]*b[1]-a[1]*b[0];
}
static void v3_norm(vec3 v) {
    float l = sqrtf(v3_dot(v,v));
    if (l<1e-8f) return;
    v[0]/=l; v[1]/=l; v[2]/=l;
}
static void v3_sub(vec3 r, const vec3 a, const vec3 b) {
    r[0]=a[0]-b[0]; r[1]=a[1]-b[1]; r[2]=a[2]-b[2];
}

static void m4_lookat(mat4 m, const vec3 eye, const vec3 at, const vec3 up) {
    vec3 f,s,u;
    v3_sub(f, at, eye); v3_norm(f);
    v3_cross(s, f, up); v3_norm(s);
    v3_cross(u, s, f);
    memset(m,0,sizeof(mat4));
    m[0] = s[0]; m[4] = s[1]; m[8]  = s[2];
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2];
    m[2] =-f[0]; m[6] =-f[1]; m[10] =-f[2];
    m[12] = -v3_dot(s, eye);
    m[13] = -v3_dot(u, eye);
    m[14] =  v3_dot(f, eye);
    m[15] = 1.0f;
}

/* 3x3 法線行列 (モデル行列の左上 3×3 の逆転置) — 直交変換なら転置=逆 */
static void m4_normal_mat(float nm[9], const mat4 m) {
    /* 簡易: スケールが均一なら m[0..2][0..2] で十分 */
    nm[0]=m[0]; nm[1]=m[1]; nm[2]=m[2];
    nm[3]=m[4]; nm[4]=m[5]; nm[5]=m[6];
    nm[6]=m[8]; nm[7]=m[9]; nm[8]=m[10];
}

/* TRS モデル行列 */
static void m4_trs(mat4 out,
                   float px,float py,float pz,
                   float rx,float ry,float rz,
                   float sx,float sy,float sz)
{
    float cx=cosf(DEG2RAD(rx)), sx_=sinf(DEG2RAD(rx));
    float cy=cosf(DEG2RAD(ry)), sy_=sinf(DEG2RAD(ry));
    float cz=cosf(DEG2RAD(rz)), sz_=sinf(DEG2RAD(rz));

    /* Ry * Rx * Rz (YXZ オイラー) */
    mat4 T,S,Rx,Ry,Rz,R,tmp;
    m4_id(T); T[12]=px; T[13]=py; T[14]=pz;
    m4_id(S); S[0]=sx; S[5]=sy; S[10]=sz;
    m4_id(Rx); Rx[5]=cx; Rx[6]=-sx_; Rx[9]=sx_; Rx[10]=cx;
    m4_id(Ry); Ry[0]=cy; Ry[2]=sy_; Ry[8]=-sy_; Ry[10]=cy;
    m4_id(Rz); Rz[0]=cz; Rz[1]=sz_; Rz[4]=-sz_; Rz[5]=cz;
    m4_mul(tmp, Ry, Rx);
    m4_mul(R,   tmp, Rz);
    m4_mul(tmp, R, S);
    m4_mul(out, T, tmp);
}

/* ────────────────────────────────────────────────────────
 * 内部構造体
 * ────────────────────────────────────────────────────────*/
typedef struct {
    unsigned int vao, vbo, ebo;
    int          index_count;
    int          vertex_count;
    float        color[4];
    float        spec_intensity;
    float        shininess;
    int          tex_id;  /* 0=なし */
    bool         used;
} Mesh3D;

typedef struct {
    float x,y,z,r,g,b,radius;
    bool  active;
} PointLight;

struct ENG_3D {
    SDL_Window*   window;
    SDL_GLContext gl_ctx;
    int           w, h;

    /* カメラ */
    vec3  cam_pos, cam_target;
    float fov, near_z, far_z;
    mat4  mat_proj, mat_view;

    /* シェーダー */
    unsigned int shader;

    /* ライティング */
    float ambient[3];
    float dir_dir[3],  dir_col[3];
    PointLight pt_lights[ENG_3D_MAX_LIGHTS];

    /* メッシュ / テクスチャ */
    Mesh3D   meshes[ENG_3D_MAX_MESHES];
    unsigned int textures[ENG_3D_MAX_TEXTURES];
    bool         tex_used[ENG_3D_MAX_TEXTURES];

    /* 入力 */
    const uint8_t* keys;
    float  mx, my, mdx, mdy;
    uint32_t mouse_btn;

    /* 時間 */
    float  delta;
    uint64_t last_tick;
    int    fps, fps_ctr;
    uint64_t fps_tick;
    bool   quit;
};

/* ────────────────────────────────────────────────────────
 * GLSL シェーダー
 * ────────────────────────────────────────────────────────*/
static const char* VERT_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNormal;\n"
    "layout(location=2) in vec2 aUV;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "uniform mat3 uNormalMat;\n"
    "out vec3 vNormal;\n"
    "out vec3 vFragPos;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vec4 wPos = uModel * vec4(aPos, 1.0);\n"
    "    vFragPos  = wPos.xyz;\n"
    "    vNormal   = uNormalMat * aNormal;\n"
    "    vUV       = aUV;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec3 vFragPos;\n"
    "in vec2 vUV;\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uAmbient;\n"
    "uniform vec3 uDirDir;\n"
    "uniform vec3 uDirCol;\n"
    "uniform vec4 uColor;\n"
    "uniform vec3 uCamPos;\n"
    "uniform float uSpecInt;\n"
    "uniform float uShininess;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uHasTex;\n"
    "/* point lights */\n"
    "uniform vec3  uPtPos[4];\n"
    "uniform vec3  uPtCol[4];\n"
    "uniform float uPtRad[4];\n"
    "uniform int   uPtCount;\n"
    "void main() {\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 V = normalize(uCamPos - vFragPos);\n"
    "    vec3 base = (uHasTex != 0) ? texture(uTex, vUV).rgb : uColor.rgb;\n"
    "    /* 方向ライト */\n"
    "    vec3 L = normalize(-uDirDir);\n"
    "    float diff = max(dot(N, L), 0.0);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float spec = pow(max(dot(N, H), 0.0), uShininess) * uSpecInt;\n"
    "    vec3 lighting = uAmbient + diff*uDirCol + spec*uDirCol;\n"
    "    /* ポイントライト */\n"
    "    for (int i=0; i<uPtCount; i++) {\n"
    "        vec3 PL = uPtPos[i] - vFragPos;\n"
    "        float dist = length(PL);\n"
    "        if (dist < uPtRad[i]) {\n"
    "            float att = 1.0 - dist / uPtRad[i];\n"
    "            vec3 PL_n = normalize(PL);\n"
    "            float pd = max(dot(N, PL_n), 0.0) * att;\n"
    "            vec3 PH = normalize(PL_n + V);\n"
    "            float ps = pow(max(dot(N, PH), 0.0), uShininess) * uSpecInt * att;\n"
    "            lighting += pd*uPtCol[i] + ps*uPtCol[i];\n"
    "        }\n"
    "    }\n"
    "    FragColor = vec4(clamp(lighting, 0.0, 1.0) * base, uColor.a);\n"
    "}\n";

/* ────────────────────────────────────────────────────────
 * シェーダーコンパイル補助
 * ────────────────────────────────────────────────────────*/
static unsigned int compile_shader(const char* src, GLenum type) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, 512, NULL, buf);
        fprintf(stderr, "[3D] シェーダーエラー: %s\n", buf);
    }
    return s;
}

static unsigned int build_program(void) {
    unsigned int vs = compile_shader(VERT_SRC, GL_VERTEX_SHADER);
    unsigned int fs = compile_shader(FRAG_SRC, GL_FRAGMENT_SHADER);
    unsigned int p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetProgramInfoLog(p, 512, NULL, buf);
        fprintf(stderr, "[3D] リンクエラー: %s\n", buf);
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

/* ────────────────────────────────────────────────────────
 * 頂点バッファ構造: float pos[3] + float normal[3] + float uv[2]
 * ────────────────────────────────────────────────────────*/
#define VFLOATS 8
typedef struct { float p[3], n[3], uv[2]; } Vertex3D;

static void upload_mesh(Mesh3D* m, const Vertex3D* verts, int nv,
                         const unsigned int* idx, int ni)
{
    glGenVertexArrays(1, &m->vao);
    glGenBuffers(1, &m->vbo);
    glGenBuffers(1, &m->ebo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(nv*sizeof(Vertex3D)), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(ni*sizeof(unsigned int)), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,n));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex3D),(void*)offsetof(Vertex3D,uv));
    glBindVertexArray(0);
    m->index_count  = ni;
    m->vertex_count = nv;
    m->used = true;
}

static int alloc_mesh_slot(ENG_3D* ctx) {
    for (int i=0;i<ENG_3D_MAX_MESHES;i++)
        if (!ctx->meshes[i].used) return i+1;
    return 0;
}

/* ────────────────────────────────────────────────────────
 * プリミティブ生成
 * ────────────────────────────────────────────────────────*/
ENG_3D_MeshID eng3d_mesh_cube(ENG_3D* ctx, float w, float h, float d) {
    float hx=w*0.5f, hy=h*0.5f, hz=d*0.5f;
    /* 面 ×6、各面 4 頂点 */
    static const float nrm[6][3] = {
        { 0, 0, 1},{ 0, 0,-1},{ 0, 1, 0},
        { 0,-1, 0},{ 1, 0, 0},{-1, 0, 0}
    };
    /* 各面の i0,i1,i2,i3 (CCW から見て正面) */
    static const float pos[6][4][3] = {
        /* front */  {{-1,+1,+1},{-1,-1,+1},{+1,-1,+1},{+1,+1,+1}},
        /* back  */  {{+1,+1,-1},{+1,-1,-1},{-1,-1,-1},{-1,+1,-1}},
        /* top   */  {{-1,+1,-1},{-1,+1,+1},{+1,+1,+1},{+1,+1,-1}},
        /* bot   */  {{-1,-1,+1},{-1,-1,-1},{+1,-1,-1},{+1,-1,+1}},
        /* right */  {{+1,+1,+1},{+1,-1,+1},{+1,-1,-1},{+1,+1,-1}},
        /* left  */  {{-1,+1,-1},{-1,-1,-1},{-1,-1,+1},{-1,+1,+1}},
    };
    static const float uvs[4][2]={{0,1},{0,0},{1,0},{1,1}};

    Vertex3D verts[24]; unsigned int idx[36];
    for (int f=0;f<6;f++) {
        for (int v=0;v<4;v++) {
            verts[f*4+v].p[0]=pos[f][v][0]*hx;
            verts[f*4+v].p[1]=pos[f][v][1]*hy;
            verts[f*4+v].p[2]=pos[f][v][2]*hz;
            verts[f*4+v].n[0]=nrm[f][0];
            verts[f*4+v].n[1]=nrm[f][1];
            verts[f*4+v].n[2]=nrm[f][2];
            verts[f*4+v].uv[0]=uvs[v][0];
            verts[f*4+v].uv[1]=uvs[v][1];
        }
        unsigned int b=f*4;
        idx[f*6+0]=b; idx[f*6+1]=b+1; idx[f*6+2]=b+2;
        idx[f*6+3]=b; idx[f*6+4]=b+2; idx[f*6+5]=b+3;
    }
    int slot = alloc_mesh_slot(ctx); if (!slot) return 0;
    Mesh3D* m = &ctx->meshes[slot-1];
    memset(m,0,sizeof(*m));
    m->color[0]=m->color[1]=m->color[2]=m->color[3]=1.0f;
    m->spec_intensity=0.5f; m->shininess=32.0f;
    m->tex_id=0;
    upload_mesh(m, verts, 24, idx, 36);
    return slot;
}

ENG_3D_MeshID eng3d_mesh_sphere(ENG_3D* ctx, float r, int slices, int stacks) {
    if (slices<3) slices=3;
    if (stacks<2) stacks=2;
    int nv = (slices+1)*(stacks+1);
    int ni = slices*stacks*6;
    Vertex3D* verts = malloc((size_t)nv*sizeof(Vertex3D));
    unsigned int* idx = malloc((size_t)ni*sizeof(unsigned int));
    int vi=0;
    for (int j=0;j<=stacks;j++) {
        float phi = (float)M_PI * j / stacks;
        float sp=sinf(phi), cp=cosf(phi);
        for (int i=0;i<=slices;i++) {
            float theta = 2.0f*(float)M_PI * i / slices;
            float st=sinf(theta), ct=cosf(theta);
            verts[vi].p[0]=r*sp*ct;
            verts[vi].p[1]=r*cp;
            verts[vi].p[2]=r*sp*st;
            verts[vi].n[0]=sp*ct;
            verts[vi].n[1]=cp;
            verts[vi].n[2]=sp*st;
            verts[vi].uv[0]=(float)i/slices;
            verts[vi].uv[1]=(float)j/stacks;
            vi++;
        }
    }
    int ii=0;
    for (int j=0;j<stacks;j++) {
        for (int i=0;i<slices;i++) {
            unsigned int a=(unsigned int)(j*(slices+1)+i);
            unsigned int b=a+1, c=a+(unsigned int)(slices+1), d=c+1;
            idx[ii++]=a; idx[ii++]=c; idx[ii++]=b;
            idx[ii++]=b; idx[ii++]=c; idx[ii++]=d;
        }
    }
    int slot = alloc_mesh_slot(ctx);
    if (!slot) { free(verts); free(idx); return 0; }
    Mesh3D* m = &ctx->meshes[slot-1];
    memset(m,0,sizeof(*m));
    m->color[0]=m->color[1]=m->color[2]=m->color[3]=1.0f;
    m->spec_intensity=0.5f; m->shininess=32.0f;
    m->tex_id=0;
    upload_mesh(m, verts, nv, idx, ni);
    free(verts); free(idx);
    return slot;
}

ENG_3D_MeshID eng3d_mesh_plane(ENG_3D* ctx, float w, float d) {
    float hw=w*0.5f, hd=d*0.5f;
    Vertex3D v[4]={
        {{-hw,0,-hd},{0,1,0},{0,0}},
        {{ hw,0,-hd},{0,1,0},{1,0}},
        {{ hw,0, hd},{0,1,0},{1,1}},
        {{-hw,0, hd},{0,1,0},{0,1}},
    };
    unsigned int idx[6]={0,1,2,0,2,3};
    int slot = alloc_mesh_slot(ctx); if (!slot) return 0;
    Mesh3D* m = &ctx->meshes[slot-1];
    memset(m,0,sizeof(*m));
    m->color[0]=m->color[1]=m->color[2]=m->color[3]=1.0f;
    m->spec_intensity=0.5f; m->shininess=32.0f;
    m->tex_id=0;
    upload_mesh(m, v, 4, idx, 6);
    return slot;
}

/* 簡易 OBJ ローダー (v/vt/vn + f 構文サポート) */
ENG_3D_MeshID eng3d_mesh_load_obj(ENG_3D* ctx, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[3D] OBJ 読込失敗: %s\n", path); return 0; }

#define OBJ_MAX 65536
    float (*pv)[3] = malloc(OBJ_MAX*3*sizeof(float));
    float (*puv)[2]= malloc(OBJ_MAX*2*sizeof(float));
    float (*pn)[3] = malloc(OBJ_MAX*3*sizeof(float));
    Vertex3D* verts = malloc(OBJ_MAX*sizeof(Vertex3D));
    unsigned int* idx = malloc(OBJ_MAX*sizeof(unsigned int));
    int np=0,nuv=0,nn=0,nv=0,ni=0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0]=='v' && line[1]==' ') {
            sscanf(line+2,"%f %f %f",&pv[np][0],&pv[np][1],&pv[np][2]); np++;
        } else if (line[0]=='v' && line[1]=='t') {
            sscanf(line+3,"%f %f",&puv[nuv][0],&puv[nuv][1]); nuv++;
        } else if (line[0]=='v' && line[1]=='n') {
            sscanf(line+3,"%f %f %f",&pn[nn][0],&pn[nn][1],&pn[nn][2]); nn++;
        } else if (line[0]=='f') {
            int pi[4]={-1,-1,-1,-1}, ti[4]={-1,-1,-1,-1}, ni_[4]={-1,-1,-1,-1};
            int cnt=0;
            char* p=line+2;
            while(*p && cnt<4) {
                int a=-1,b=-1,c=-1;
                if (sscanf(p,"%d/%d/%d",&a,&b,&c)==3)       { pi[cnt]=a-1; ti[cnt]=b-1; ni_[cnt]=c-1; }
                else if (sscanf(p,"%d//%d",&a,&c)==2)        { pi[cnt]=a-1; ni_[cnt]=c-1; }
                else if (sscanf(p,"%d/%d",&a,&b)==2)         { pi[cnt]=a-1; ti[cnt]=b-1; }
                else if (sscanf(p,"%d",&a)==1)               { pi[cnt]=a-1; }
                cnt++;
                while(*p && *p!=' ' && *p!='\n') p++;
                while(*p==' ') p++;
            }
            /* 三角形に分割 (3 or 4 頂点対応) */
            int tris = cnt-2;
            for (int t=0;t<tris;t++) {
                int face[3]={0, t+1, t+2};
                for (int j=0;j<3;j++) {
                    Vertex3D vtx={0};
                    int fi=face[j];
                    if (pi[fi]>=0) memcpy(vtx.p, pv[pi[fi]], 12);
                    if (ti[fi]>=0) memcpy(vtx.uv, puv[ti[fi]], 8);
                    if (ni_[fi]>=0) memcpy(vtx.n, pn[ni_[fi]], 12);
                    verts[nv]=vtx;
                    idx[ni++]=(unsigned int)nv++;
                }
            }
        }
    }
    fclose(f);
    int slot=0;
    if (nv>0) {
        slot = alloc_mesh_slot(ctx);
        if (slot) {
            Mesh3D* m=&ctx->meshes[slot-1];
            memset(m,0,sizeof(*m));
            m->color[0]=m->color[1]=m->color[2]=m->color[3]=1.0f;
            m->spec_intensity=0.5f; m->shininess=32.0f;
            m->tex_id=0;
            upload_mesh(m, verts, nv, idx, ni);
        }
    }
    free(pv); free(puv); free(pn); free(verts); free(idx);
#undef OBJ_MAX
    return slot;
}

void eng3d_mesh_destroy(ENG_3D* ctx, ENG_3D_MeshID id) {
    if (id<1||id>ENG_3D_MAX_MESHES) return;
    Mesh3D* m=&ctx->meshes[id-1];
    if (!m->used) return;
    glDeleteVertexArrays(1,&m->vao);
    glDeleteBuffers(1,&m->vbo);
    glDeleteBuffers(1,&m->ebo);
    memset(m,0,sizeof(*m));
}

int eng3d_mesh_vertex_count(ENG_3D* ctx, ENG_3D_MeshID id) {
    if (id<1||id>ENG_3D_MAX_MESHES||!ctx->meshes[id-1].used) return 0;
    return ctx->meshes[id-1].vertex_count;
}

/* ────────────────────────────────────────────────────────
 * テクスチャ
 * ────────────────────────────────────────────────────────*/
ENG_3D_TexID eng3d_tex_load(ENG_3D* ctx, const char* path) {
    /* 空きスロット */
    int slot=-1;
    for (int i=0;i<ENG_3D_MAX_TEXTURES;i++) if (!ctx->tex_used[i]){slot=i;break;}
    if (slot<0) return 0;

    stbi_set_flip_vertically_on_load(1);
    int w,h,ch;
    unsigned char* data=stbi_load(path,&w,&h,&ch,4);
    if (!data) { fprintf(stderr,"[3D] テクスチャ読込失敗: %s\n",path); return 0; }

    glGenTextures(1,&ctx->textures[slot]);
    glBindTexture(GL_TEXTURE_2D,ctx->textures[slot]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,0);
    stbi_image_free(data);
    ctx->tex_used[slot]=true;
    return slot+1;
}

void eng3d_tex_destroy(ENG_3D* ctx, ENG_3D_TexID id) {
    if (id<1||id>ENG_3D_MAX_TEXTURES||!ctx->tex_used[id-1]) return;
    glDeleteTextures(1,&ctx->textures[id-1]);
    ctx->tex_used[id-1]=false;
    ctx->textures[id-1]=0;
}

/* ────────────────────────────────────────────────────────
 * マテリアル
 * ────────────────────────────────────────────────────────*/
void eng3d_mesh_color(ENG_3D* ctx, ENG_3D_MeshID id, float r, float g, float b, float a) {
    if (id<1||id>ENG_3D_MAX_MESHES||!ctx->meshes[id-1].used) return;
    float* c=ctx->meshes[id-1].color;
    c[0]=r; c[1]=g; c[2]=b; c[3]=a;
}

void eng3d_mesh_texture(ENG_3D* ctx, ENG_3D_MeshID id, ENG_3D_TexID tex_id) {
    if (id<1||id>ENG_3D_MAX_MESHES||!ctx->meshes[id-1].used) return;
    ctx->meshes[id-1].tex_id=tex_id;
}

void eng3d_mesh_specular(ENG_3D* ctx, ENG_3D_MeshID id, float intensity, float shininess) {
    if (id<1||id>ENG_3D_MAX_MESHES||!ctx->meshes[id-1].used) return;
    ctx->meshes[id-1].spec_intensity=intensity;
    ctx->meshes[id-1].shininess=shininess;
}

/* ────────────────────────────────────────────────────────
 * ライフサイクル
 * ────────────────────────────────────────────────────────*/
ENG_3D* eng3d_create(const char* title, int w, int h) {
    if (SDL_WasInit(SDL_INIT_VIDEO)==0)
        SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr,"[3D] SDL_CreateWindow: %s\n",SDL_GetError()); return NULL; }

    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) { fprintf(stderr,"[3D] GL コンテキスト失敗: %s\n",SDL_GetError()); SDL_DestroyWindow(win); return NULL; }
    SDL_GL_SetSwapInterval(1);

    ENG_3D* ctx = calloc(1, sizeof(ENG_3D));
    ctx->window = win;
    ctx->gl_ctx = gl;
    ctx->w = w; ctx->h = h;

    /* デフォルト設定 */
    ctx->cam_pos[0]=0; ctx->cam_pos[1]=3; ctx->cam_pos[2]=5;
    ctx->cam_target[0]=ctx->cam_target[1]=ctx->cam_target[2]=0;
    ctx->fov=60.0f; ctx->near_z=0.1f; ctx->far_z=500.0f;
    ctx->ambient[0]=ctx->ambient[1]=ctx->ambient[2]=0.2f;
    ctx->dir_dir[0]=0.5f; ctx->dir_dir[1]=-1.0f; ctx->dir_dir[2]=0.3f;
    ctx->dir_col[0]=ctx->dir_col[1]=ctx->dir_col[2]=0.8f;

    /* OpenGL 初期設定 */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    ctx->shader = build_program();
    ctx->last_tick = SDL_GetPerformanceCounter();
    ctx->fps_tick  = ctx->last_tick;
    ctx->keys = SDL_GetKeyboardState(NULL);
    return ctx;
}

void eng3d_destroy(ENG_3D* ctx) {
    if (!ctx) return;
    for (int i=0;i<ENG_3D_MAX_MESHES;i++)
        if (ctx->meshes[i].used) eng3d_mesh_destroy(ctx, i+1);
    for (int i=0;i<ENG_3D_MAX_TEXTURES;i++)
        if (ctx->tex_used[i]) eng3d_tex_destroy(ctx, i+1);
    glDeleteProgram(ctx->shader);
    SDL_GL_DeleteContext(ctx->gl_ctx);
    SDL_DestroyWindow(ctx->window);
    free(ctx);
}

bool eng3d_update(ENG_3D* ctx) {
    /* delta */
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    ctx->delta = (float)(now - ctx->last_tick) / (float)freq;
    ctx->last_tick = now;

    /* FPS */
    ctx->fps_ctr++;
    if (now - ctx->fps_tick >= freq) {
        ctx->fps = ctx->fps_ctr;
        ctx->fps_ctr = 0;
        ctx->fps_tick = now;
    }

    /* マウスデルタリセット */
    ctx->mdx = ctx->mdy = 0;

    /* イベント */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type==SDL_QUIT) ctx->quit=true;
        if (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_RESIZED) {
            ctx->w=e.window.data1; ctx->h=e.window.data2;
            glViewport(0,0,ctx->w,ctx->h);
        }
        if (e.type==SDL_MOUSEMOTION) {
            ctx->mdx=(float)e.motion.xrel;
            ctx->mdy=(float)e.motion.yrel;
        }
    }
    int mx,my; ctx->mouse_btn=SDL_GetMouseState(&mx,&my);
    ctx->mx=(float)mx; ctx->my=(float)my;
    return !ctx->quit;
}

float eng3d_delta(ENG_3D* ctx) { return ctx->delta; }
int   eng3d_fps  (ENG_3D* ctx) { return ctx->fps;   }

/* ────────────────────────────────────────────────────────
 * カメラ / ライト
 * ────────────────────────────────────────────────────────*/
void eng3d_cam_perspective(ENG_3D* ctx, float fov_deg, float near_z, float far_z) {
    ctx->fov=fov_deg; ctx->near_z=near_z; ctx->far_z=far_z;
}
void eng3d_cam_pos(ENG_3D* ctx, float x, float y, float z) {
    ctx->cam_pos[0]=x; ctx->cam_pos[1]=y; ctx->cam_pos[2]=z;
}
void eng3d_cam_target(ENG_3D* ctx, float tx, float ty, float tz) {
    ctx->cam_target[0]=tx; ctx->cam_target[1]=ty; ctx->cam_target[2]=tz;
}
void eng3d_cam_lookat(ENG_3D* ctx, float ex, float ey, float ez, float tx, float ty, float tz) {
    eng3d_cam_pos(ctx,ex,ey,ez);
    eng3d_cam_target(ctx,tx,ty,tz);
}
void eng3d_ambient(ENG_3D* ctx, float r, float g, float b) {
    ctx->ambient[0]=r; ctx->ambient[1]=g; ctx->ambient[2]=b;
}
void eng3d_dir_light(ENG_3D* ctx, float dx, float dy, float dz, float r, float g, float b) {
    ctx->dir_dir[0]=dx; ctx->dir_dir[1]=dy; ctx->dir_dir[2]=dz;
    ctx->dir_col[0]=r;  ctx->dir_col[1]=g;  ctx->dir_col[2]=b;
}
void eng3d_point_light(ENG_3D* ctx, int slot, float x, float y, float z, float r, float g, float b, float radius) {
    if (slot<0||slot>=ENG_3D_MAX_LIGHTS) return;
    ctx->pt_lights[slot].x=x; ctx->pt_lights[slot].y=y; ctx->pt_lights[slot].z=z;
    ctx->pt_lights[slot].r=r; ctx->pt_lights[slot].g=g; ctx->pt_lights[slot].b=b;
    ctx->pt_lights[slot].radius=radius;
    ctx->pt_lights[slot].active=(radius>0.0f);
}

/* ────────────────────────────────────────────────────────
 * 描画
 * ────────────────────────────────────────────────────────*/
void eng3d_begin(ENG_3D* ctx, float r, float g, float b) {
    glViewport(0,0,ctx->w,ctx->h);
    glClearColor(r,g,b,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    float aspect = (ctx->h>0) ? (float)ctx->w / (float)ctx->h : 1.0f;
    m4_perspective(ctx->mat_proj, DEG2RAD(ctx->fov), aspect, ctx->near_z, ctx->far_z);

    vec3 up={0,1,0};
    m4_lookat(ctx->mat_view, ctx->cam_pos, ctx->cam_target, up);

    /* シェーダーへ定数設定 */
    glUseProgram(ctx->shader);

    /* ライト */
    glUniform3fv(glGetUniformLocation(ctx->shader,"uAmbient"),1,ctx->ambient);
    glUniform3fv(glGetUniformLocation(ctx->shader,"uDirDir"),1,ctx->dir_dir);
    glUniform3fv(glGetUniformLocation(ctx->shader,"uDirCol"),1,ctx->dir_col);
    glUniform3fv(glGetUniformLocation(ctx->shader,"uCamPos"),1,ctx->cam_pos);

    /* ポイントライト */
    float pt_pos[12]={0},pt_col[12]={0},pt_rad[4]={0};
    int npt=0;
    for (int i=0;i<ENG_3D_MAX_LIGHTS;i++) {
        if (!ctx->pt_lights[i].active) continue;
        pt_pos[npt*3]=ctx->pt_lights[i].x;
        pt_pos[npt*3+1]=ctx->pt_lights[i].y;
        pt_pos[npt*3+2]=ctx->pt_lights[i].z;
        pt_col[npt*3]=ctx->pt_lights[i].r;
        pt_col[npt*3+1]=ctx->pt_lights[i].g;
        pt_col[npt*3+2]=ctx->pt_lights[i].b;
        pt_rad[npt]=ctx->pt_lights[i].radius;
        npt++;
    }
    glUniform3fv(glGetUniformLocation(ctx->shader,"uPtPos"),4,pt_pos);
    glUniform3fv(glGetUniformLocation(ctx->shader,"uPtCol"),4,pt_col);
    glUniform1fv(glGetUniformLocation(ctx->shader,"uPtRad"),4,pt_rad);
    glUniform1i (glGetUniformLocation(ctx->shader,"uPtCount"),npt);
}

void eng3d_draw(ENG_3D* ctx, ENG_3D_MeshID mesh_id,
                 float px, float py, float pz,
                 float rx, float ry, float rz,
                 float sx, float sy, float sz)
{
    if (mesh_id<1||mesh_id>ENG_3D_MAX_MESHES||!ctx->meshes[mesh_id-1].used) return;
    Mesh3D* m=&ctx->meshes[mesh_id-1];

    mat4 model, mvp, tmp;
    m4_trs(model, px,py,pz, rx,ry,rz, sx,sy,sz);
    m4_mul(tmp, ctx->mat_view, model);
    m4_mul(mvp, ctx->mat_proj, tmp);

    float nm[9]; m4_normal_mat(nm, model);

    glUniformMatrix4fv(glGetUniformLocation(ctx->shader,"uMVP"),  1,GL_FALSE,mvp);
    glUniformMatrix4fv(glGetUniformLocation(ctx->shader,"uModel"),1,GL_FALSE,model);
    glUniformMatrix3fv(glGetUniformLocation(ctx->shader,"uNormalMat"),1,GL_FALSE,nm);
    glUniform4fv(glGetUniformLocation(ctx->shader,"uColor"),1,m->color);
    glUniform1f (glGetUniformLocation(ctx->shader,"uSpecInt"),m->spec_intensity);
    glUniform1f (glGetUniformLocation(ctx->shader,"uShininess"),m->shininess);

    /* テクスチャ */
    int hasTex=0;
    if (m->tex_id>=1&&m->tex_id<=ENG_3D_MAX_TEXTURES&&ctx->tex_used[m->tex_id-1]) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,ctx->textures[m->tex_id-1]);
        glUniform1i(glGetUniformLocation(ctx->shader,"uTex"),0);
        hasTex=1;
    }
    glUniform1i(glGetUniformLocation(ctx->shader,"uHasTex"),hasTex);

    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES,(GLsizei)m->index_count,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
    if (hasTex) glBindTexture(GL_TEXTURE_2D,0);
}

void eng3d_end(ENG_3D* ctx) {
    SDL_GL_SwapWindow(ctx->window);
}

/* ────────────────────────────────────────────────────────
 * 入力
 * ────────────────────────────────────────────────────────*/
bool  eng3d_key(ENG_3D* ctx, int scancode) { return ctx->keys[scancode] != 0; }
float eng3d_mouse_x(ENG_3D* ctx)  { return ctx->mx; }
float eng3d_mouse_y(ENG_3D* ctx)  { return ctx->my; }
float eng3d_mouse_dx(ENG_3D* ctx) { return ctx->mdx; }
float eng3d_mouse_dy(ENG_3D* ctx) { return ctx->mdy; }
bool  eng3d_mouse_btn(ENG_3D* ctx, int btn) {
    if (btn==1) return (ctx->mouse_btn & SDL_BUTTON_LMASK)!=0;
    if (btn==2) return (ctx->mouse_btn & SDL_BUTTON_MMASK)!=0;
    if (btn==3) return (ctx->mouse_btn & SDL_BUTTON_RMASK)!=0;
    return false;
}
