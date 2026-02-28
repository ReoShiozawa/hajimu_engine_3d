/**
 * win_gl.c — Windows OpenGL 2.0+ 関数ローダー実装
 *
 * SDL_GL_GetProcAddress() を使って GL 拡張関数を動的にロードする。
 * SDL_GL_CreateContext() 後に win_gl_load() を呼ぶこと。
 */
#ifdef _WIN32

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "win_gl.h"

/* ── 関数ポインタ実体 ────────────────────────────────*/
PFNGLACTIVETEXTUREPROC             pfn_glActiveTexture;
PFNGLATTACHSHADERPROC              pfn_glAttachShader;
PFNGLBINDBUFFERPROC                pfn_glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC           pfn_glBindFramebuffer;
PFNGLBINDRENDERBUFFERPROC          pfn_glBindRenderbuffer;
PFNGLBINDVERTEXARRAYPROC           pfn_glBindVertexArray;
PFNGLBUFFERDATAPROC                pfn_glBufferData;
PFNGLBUFFERSUBDATAPROC             pfn_glBufferSubData;
PFNGLCOMPILESHADERPROC             pfn_glCompileShader;
PFNGLCREATEPROGRAMPROC             pfn_glCreateProgram;
PFNGLCREATESHADERPROC              pfn_glCreateShader;
PFNGLDELETEBUFFERSPROC             pfn_glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC        pfn_glDeleteFramebuffers;
PFNGLDELETEPROGRAMPROC             pfn_glDeleteProgram;
PFNGLDELETERENDERBUFFERSPROC       pfn_glDeleteRenderbuffers;
PFNGLDELETESHADERPROC              pfn_glDeleteShader;
PFNGLDELETEVERTEXARRAYSPROC        pfn_glDeleteVertexArrays;
PFNGLDRAWARRAYSINSTANCEDPROC       pfn_glDrawArraysInstanced;
PFNGLENABLEVERTEXATTRIBARRAYPROC   pfn_glEnableVertexAttribArray;
PFNGLFRAMEBUFFERRENDERBUFFERPROC   pfn_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC      pfn_glFramebufferTexture2D;
PFNGLGENBUFFERSPROC                pfn_glGenBuffers;
PFNGLGENERATEMIPMAPPROC            pfn_glGenerateMipmap;
PFNGLGENFRAMEBUFFERSPROC           pfn_glGenFramebuffers;
PFNGLGENRENDERBUFFERSPROC          pfn_glGenRenderbuffers;
PFNGLGENVERTEXARRAYSPROC           pfn_glGenVertexArrays;
PFNGLGETPROGRAMINFOLOGPROC         pfn_glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC              pfn_glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC          pfn_glGetShaderInfoLog;
PFNGLGETSHADERIVPROC               pfn_glGetShaderiv;
PFNGLGETUNIFORMLOCATIONPROC        pfn_glGetUniformLocation;
PFNGLLINKPROGRAMPROC               pfn_glLinkProgram;
PFNGLRENDERBUFFERSTORAGEPROC       pfn_glRenderbufferStorage;
PFNGLSHADERSOURCEPROC              pfn_glShaderSource;
PFNGLUNIFORM1FPROC                 pfn_glUniform1f;
PFNGLUNIFORM1FVPROC                pfn_glUniform1fv;
PFNGLUNIFORM1IPROC                 pfn_glUniform1i;
PFNGLUNIFORM3FVPROC                pfn_glUniform3fv;
PFNGLUNIFORM4FVPROC                pfn_glUniform4fv;
PFNGLUNIFORMMATRIX3FVPROC          pfn_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC          pfn_glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC                pfn_glUseProgram;
PFNGLVERTEXATTRIBDIVISORPROC       pfn_glVertexAttribDivisor;
PFNGLVERTEXATTRIBPOINTERPROC       pfn_glVertexAttribPointer;

/* ── ローダー ──────────────────────────────────────*/
#define LOAD(var, name) \
    var = (typeof(var)) SDL_GL_GetProcAddress(name); \
    if (!var) { \
        SDL_Log("[win_gl] WARNING: %s not found\n", name); \
    }

int win_gl_load(void) {
    LOAD(pfn_glActiveTexture,           "glActiveTexture")
    LOAD(pfn_glAttachShader,            "glAttachShader")
    LOAD(pfn_glBindBuffer,              "glBindBuffer")
    LOAD(pfn_glBindFramebuffer,         "glBindFramebuffer")
    LOAD(pfn_glBindRenderbuffer,        "glBindRenderbuffer")
    LOAD(pfn_glBindVertexArray,         "glBindVertexArray")
    LOAD(pfn_glBufferData,              "glBufferData")
    LOAD(pfn_glBufferSubData,           "glBufferSubData")
    LOAD(pfn_glCompileShader,           "glCompileShader")
    LOAD(pfn_glCreateProgram,           "glCreateProgram")
    LOAD(pfn_glCreateShader,            "glCreateShader")
    LOAD(pfn_glDeleteBuffers,           "glDeleteBuffers")
    LOAD(pfn_glDeleteFramebuffers,      "glDeleteFramebuffers")
    LOAD(pfn_glDeleteProgram,           "glDeleteProgram")
    LOAD(pfn_glDeleteRenderbuffers,     "glDeleteRenderbuffers")
    LOAD(pfn_glDeleteShader,            "glDeleteShader")
    LOAD(pfn_glDeleteVertexArrays,      "glDeleteVertexArrays")
    LOAD(pfn_glDrawArraysInstanced,     "glDrawArraysInstanced")
    LOAD(pfn_glEnableVertexAttribArray, "glEnableVertexAttribArray")
    LOAD(pfn_glFramebufferRenderbuffer, "glFramebufferRenderbuffer")
    LOAD(pfn_glFramebufferTexture2D,    "glFramebufferTexture2D")
    LOAD(pfn_glGenBuffers,              "glGenBuffers")
    LOAD(pfn_glGenerateMipmap,          "glGenerateMipmap")
    LOAD(pfn_glGenFramebuffers,         "glGenFramebuffers")
    LOAD(pfn_glGenRenderbuffers,        "glGenRenderbuffers")
    LOAD(pfn_glGenVertexArrays,         "glGenVertexArrays")
    LOAD(pfn_glGetProgramInfoLog,       "glGetProgramInfoLog")
    LOAD(pfn_glGetProgramiv,            "glGetProgramiv")
    LOAD(pfn_glGetShaderInfoLog,        "glGetShaderInfoLog")
    LOAD(pfn_glGetShaderiv,             "glGetShaderiv")
    LOAD(pfn_glGetUniformLocation,      "glGetUniformLocation")
    LOAD(pfn_glLinkProgram,             "glLinkProgram")
    LOAD(pfn_glRenderbufferStorage,     "glRenderbufferStorage")
    LOAD(pfn_glShaderSource,            "glShaderSource")
    LOAD(pfn_glUniform1f,               "glUniform1f")
    LOAD(pfn_glUniform1fv,              "glUniform1fv")
    LOAD(pfn_glUniform1i,               "glUniform1i")
    LOAD(pfn_glUniform3fv,              "glUniform3fv")
    LOAD(pfn_glUniform4fv,              "glUniform4fv")
    LOAD(pfn_glUniformMatrix3fv,        "glUniformMatrix3fv")
    LOAD(pfn_glUniformMatrix4fv,        "glUniformMatrix4fv")
    LOAD(pfn_glUseProgram,              "glUseProgram")
    LOAD(pfn_glVertexAttribDivisor,     "glVertexAttribDivisor")
    LOAD(pfn_glVertexAttribPointer,     "glVertexAttribPointer")
    return 1;
}

#endif /* _WIN32 */
