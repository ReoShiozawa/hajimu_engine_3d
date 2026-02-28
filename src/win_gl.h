/**
 * win_gl.h — Windows OpenGL 2.0+ 関数ローダー
 *
 * SDL2/SDL_opengl.h と SDL2/SDL_opengl_glext.h のインクルード後に
 * このヘッダーをインクルードすること。
 * win_gl_load() は SDL_GL_CreateContext() 後に呼ぶこと。
 */
#pragma once

#ifdef _WIN32

/* ── 関数ポインタ extern 宣言 ──────────────────────────*/
extern PFNGLACTIVETEXTUREPROC             pfn_glActiveTexture;
extern PFNGLATTACHSHADERPROC              pfn_glAttachShader;
extern PFNGLBINDBUFFERPROC                pfn_glBindBuffer;
extern PFNGLBINDFRAMEBUFFERPROC           pfn_glBindFramebuffer;
extern PFNGLBINDRENDERBUFFERPROC          pfn_glBindRenderbuffer;
extern PFNGLBINDVERTEXARRAYPROC           pfn_glBindVertexArray;
extern PFNGLBUFFERDATAPROC                pfn_glBufferData;
extern PFNGLBUFFERSUBDATAPROC             pfn_glBufferSubData;
extern PFNGLCOMPILESHADERPROC             pfn_glCompileShader;
extern PFNGLCREATEPROGRAMPROC             pfn_glCreateProgram;
extern PFNGLCREATESHADERPROC              pfn_glCreateShader;
extern PFNGLDELETEBUFFERSPROC             pfn_glDeleteBuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC        pfn_glDeleteFramebuffers;
extern PFNGLDELETEPROGRAMPROC             pfn_glDeleteProgram;
extern PFNGLDELETERENDERBUFFERSPROC       pfn_glDeleteRenderbuffers;
extern PFNGLDELETESHADERPROC              pfn_glDeleteShader;
extern PFNGLDELETEVERTEXARRAYSPROC        pfn_glDeleteVertexArrays;
extern PFNGLDRAWARRAYSINSTANCEDPROC       pfn_glDrawArraysInstanced;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC   pfn_glEnableVertexAttribArray;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC   pfn_glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC      pfn_glFramebufferTexture2D;
extern PFNGLGENBUFFERSPROC                pfn_glGenBuffers;
extern PFNGLGENERATEMIPMAPPROC            pfn_glGenerateMipmap;
extern PFNGLGENFRAMEBUFFERSPROC           pfn_glGenFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC          pfn_glGenRenderbuffers;
extern PFNGLGENVERTEXARRAYSPROC           pfn_glGenVertexArrays;
extern PFNGLGETPROGRAMINFOLOGPROC         pfn_glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC              pfn_glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC          pfn_glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC               pfn_glGetShaderiv;
extern PFNGLGETUNIFORMLOCATIONPROC        pfn_glGetUniformLocation;
extern PFNGLLINKPROGRAMPROC               pfn_glLinkProgram;
extern PFNGLRENDERBUFFERSTORAGEPROC       pfn_glRenderbufferStorage;
extern PFNGLSHADERSOURCEPROC              pfn_glShaderSource;
extern PFNGLUNIFORM1FPROC                 pfn_glUniform1f;
extern PFNGLUNIFORM1FVPROC                pfn_glUniform1fv;
extern PFNGLUNIFORM1IPROC                 pfn_glUniform1i;
extern PFNGLUNIFORM3FVPROC                pfn_glUniform3fv;
extern PFNGLUNIFORM4FVPROC                pfn_glUniform4fv;
extern PFNGLUNIFORMMATRIX3FVPROC          pfn_glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FVPROC          pfn_glUniformMatrix4fv;
extern PFNGLUSEPROGRAMPROC                pfn_glUseProgram;
extern PFNGLVERTEXATTRIBDIVISORPROC       pfn_glVertexAttribDivisor;
extern PFNGLVERTEXATTRIBPOINTERPROC       pfn_glVertexAttribPointer;

/* ── gl* → pfn_gl* マクロ置換 ────────────────────────*/
#define glActiveTexture            pfn_glActiveTexture
#define glAttachShader             pfn_glAttachShader
#define glBindBuffer               pfn_glBindBuffer
#define glBindFramebuffer          pfn_glBindFramebuffer
#define glBindRenderbuffer         pfn_glBindRenderbuffer
#define glBindVertexArray          pfn_glBindVertexArray
#define glBufferData               pfn_glBufferData
#define glBufferSubData            pfn_glBufferSubData
#define glCompileShader            pfn_glCompileShader
#define glCreateProgram            pfn_glCreateProgram
#define glCreateShader             pfn_glCreateShader
#define glDeleteBuffers            pfn_glDeleteBuffers
#define glDeleteFramebuffers       pfn_glDeleteFramebuffers
#define glDeleteProgram            pfn_glDeleteProgram
#define glDeleteRenderbuffers      pfn_glDeleteRenderbuffers
#define glDeleteShader             pfn_glDeleteShader
#define glDeleteVertexArrays       pfn_glDeleteVertexArrays
#define glDrawArraysInstanced      pfn_glDrawArraysInstanced
#define glEnableVertexAttribArray  pfn_glEnableVertexAttribArray
#define glFramebufferRenderbuffer  pfn_glFramebufferRenderbuffer
#define glFramebufferTexture2D     pfn_glFramebufferTexture2D
#define glGenBuffers               pfn_glGenBuffers
#define glGenerateMipmap           pfn_glGenerateMipmap
#define glGenFramebuffers          pfn_glGenFramebuffers
#define glGenRenderbuffers         pfn_glGenRenderbuffers
#define glGenVertexArrays          pfn_glGenVertexArrays
#define glGetProgramInfoLog        pfn_glGetProgramInfoLog
#define glGetProgramiv             pfn_glGetProgramiv
#define glGetShaderInfoLog         pfn_glGetShaderInfoLog
#define glGetShaderiv              pfn_glGetShaderiv
#define glGetUniformLocation       pfn_glGetUniformLocation
#define glLinkProgram              pfn_glLinkProgram
#define glRenderbufferStorage      pfn_glRenderbufferStorage
#define glShaderSource             pfn_glShaderSource
#define glUniform1f                pfn_glUniform1f
#define glUniform1fv               pfn_glUniform1fv
#define glUniform1i                pfn_glUniform1i
#define glUniform3fv               pfn_glUniform3fv
#define glUniform4fv               pfn_glUniform4fv
#define glUniformMatrix3fv         pfn_glUniformMatrix3fv
#define glUniformMatrix4fv         pfn_glUniformMatrix4fv
#define glUseProgram               pfn_glUseProgram
#define glVertexAttribDivisor      pfn_glVertexAttribDivisor
#define glVertexAttribPointer      pfn_glVertexAttribPointer

/* ── ローダー関数 ─────────────────────────────────────*/
/** SDL_GL_CreateContext() 後に必ず呼ぶこと。
 *  戻り値: 成功=1, 失敗=0 */
int win_gl_load(void);

#endif /* _WIN32 */
