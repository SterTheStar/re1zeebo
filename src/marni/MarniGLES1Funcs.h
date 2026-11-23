// MarniGLES1Funcs.h - OpenGL ES 1.0/1.1 constants and BREW IGL bridge.
//
// Isolated from MarniDX.h so GLES1 types never leak into src/game/.
#pragma once

#include <stdint.h>

// Standard OpenGL ES 1.0/1.1 constants
#define GLES1_FALSE                   0
#define GLES1_TRUE                    1
#define GLES1_ZERO                    0
#define GLES1_ONE                     1
#define GLES1_SRC_ALPHA               0x0302
#define GLES1_ONE_MINUS_SRC_ALPHA     0x0303

#define GLES1_COLOR_BUFFER_BIT        0x00004000
#define GLES1_DEPTH_BUFFER_BIT        0x00000100

#define GLES1_TRIANGLES               0x0004
#define GLES1_TRIANGLE_STRIP          0x0005

#define GLES1_DEPTH_TEST              0x0B71
#define GLES1_BLEND                   0x0BE2
#define GLES1_TEXTURE_2D              0x0DE1
#define GLES1_CULL_FACE               0x0B44
#define GLES1_SCISSOR_TEST            0x0C11

#define GLES1_LEQUAL                  0x0203

#define GLES1_UNSIGNED_BYTE           0x1401
// 0x8033 e' GL_UNSIGNED_SHORT_4_4_4_4; 5_5_5_1 e' 0x8034 (gl.h do BREW).
#define GLES1_UNSIGNED_SHORT_5_5_5_1  0x8034
#define GLES1_FLOAT                   0x1406

#define GLES1_RGBA                    0x1908

#define GLES1_NEAREST                 0x2600
#define GLES1_LINEAR                  0x2601
#define GLES1_TEXTURE_MAG_FILTER      0x2800
#define GLES1_TEXTURE_MIN_FILTER      0x2801
#define GLES1_TEXTURE_WRAP_S          0x2802
#define GLES1_TEXTURE_WRAP_T          0x2803
#define GLES1_CLAMP_TO_EDGE           0x812F
#define GLES1_REPEAT                  0x2901

#define GLES1_MODELVIEW               0x1700
#define GLES1_PROJECTION              0x1701

#define GLES1_VERTEX_ARRAY            0x8074
#define GLES1_COLOR_ARRAY             0x8076
#define GLES1_TEXTURE_COORD_ARRAY     0x8078

// EGL constants
#define ZEEBO_EGL_DEFAULT_DISPLAY     0
#define ZEEBO_EGL_RED_SIZE            0x3024
#define ZEEBO_EGL_GREEN_SIZE          0x3023
#define ZEEBO_EGL_BLUE_SIZE           0x3022
#define ZEEBO_EGL_ALPHA_SIZE          0x3021
#define ZEEBO_EGL_DEPTH_SIZE          0x3025
#define ZEEBO_EGL_NONE                0x3038
#define ZEEBO_EGL_NO_CONTEXT          ((void*)0)
#define ZEEBO_EGL_NO_SURFACE          ((void*)0)
#define ZEEBO_EGL_NO_DISPLAY          ((void*)0)

// ---------------------------------------------------------------------------
// BREW IGL (GL_LEGACY) vtable matching zeebx-core aee_slots::GL_LEGACY (0..79)
// Note: methods 3..79 omit `this`, matching AEEGL.h / zeebx-emu.
// ---------------------------------------------------------------------------
typedef int32_t GLfixed;

typedef struct IGLVtbl {
    uint32_t (*AddRef)(void* po);
    uint32_t (*Release)(void* po);
    int      (*QueryInterface)(void* po, uint32_t id, void** ppNew);
    void (*glActiveTexture)(uint32_t texture);
    void (*glAlphaFuncx)(uint32_t func, GLfixed ref);
    void (*glBindTexture)(uint32_t target, uint32_t texture);
    void (*glBlendFunc)(uint32_t sfactor, uint32_t dfactor);
    void (*glClear)(uint32_t mask);
    void (*glClearColorx)(GLfixed r, GLfixed g, GLfixed b, GLfixed a);
    void (*glClearDepthx)(GLfixed depth);
    void (*glClearStencil)(int s);
    void (*glClientActiveTexture)(uint32_t texture);
    void (*glColor4x)(GLfixed r, GLfixed g, GLfixed b, GLfixed a);
    void (*glColorMask)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void (*glColorPointer)(int size, uint32_t type, int stride, const void* ptr);
    void (*glCompressedTexImage2D)(uint32_t, int, uint32_t, int, int, int, int, const void*);
    void (*glCompressedTexSubImage2D)(uint32_t, int, int, int, int, int, uint32_t, int, const void*);
    void (*glCopyTexImage2D)(uint32_t, int, uint32_t, int, int, int, int, int);
    void (*glCopyTexSubImage2D)(uint32_t, int, int, int, int, int, int, int);
    void (*glCullFace)(uint32_t mode);
    void (*glDeleteTextures)(int n, const uint32_t* textures);
    void (*glDepthFunc)(uint32_t func);
    void (*glDepthMask)(uint8_t flag);
    void (*glDepthRangex)(GLfixed zNear, GLfixed zFar);
    void (*glDisable)(uint32_t cap);
    void (*glDisableClientState)(uint32_t array);
    void (*glDrawArrays)(uint32_t mode, int first, int count);
    void (*glDrawElements)(uint32_t mode, int count, uint32_t type, const void* indices);
    void (*glEnable)(uint32_t cap);
    void (*glEnableClientState)(uint32_t array);
    void (*glFinish)(void);
    void (*glFlush)(void);
    void (*glFogx)(uint32_t pname, GLfixed param);
    void (*glFogxv)(uint32_t pname, const GLfixed* params);
    void (*glFrontFace)(uint32_t mode);
    void (*glFrustumx)(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f);
    void (*glGenTextures)(int n, uint32_t* textures);
    uint32_t (*glGetError)(void);
    void (*glGetIntegerv)(uint32_t pname, int* params);
    const uint8_t* (*glGetString)(uint32_t name);
    void (*glHint)(uint32_t target, uint32_t mode);
    void (*glLightModelx)(uint32_t pname, GLfixed param);
    void (*glLightModelxv)(uint32_t pname, const GLfixed* params);
    void (*glLightx)(uint32_t light, uint32_t pname, GLfixed param);
    void (*glLightxv)(uint32_t light, uint32_t pname, const GLfixed* params);
    void (*glLineWidthx)(GLfixed width);
    void (*glLoadIdentity)(void);
    void (*glLoadMatrixx)(const GLfixed* m);
    void (*glLogicOp)(uint32_t opcode);
    void (*glMaterialx)(uint32_t face, uint32_t pname, GLfixed param);
    void (*glMaterialxv)(uint32_t face, uint32_t pname, const GLfixed* params);
    void (*glMatrixMode)(uint32_t mode);
    void (*glMultMatrixx)(const GLfixed* m);
    void (*glMultiTexCoord4x)(uint32_t target, GLfixed s, GLfixed t, GLfixed r, GLfixed q);
    void (*glNormal3x)(GLfixed nx, GLfixed ny, GLfixed nz);
    void (*glNormalPointer)(uint32_t type, int stride, const void* ptr);
    void (*glOrthox)(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f);
    void (*glPixelStorei)(uint32_t pname, int param);
    void (*glPointSizex)(GLfixed size);
    void (*glPolygonOffsetx)(GLfixed factor, GLfixed units);
    void (*glPopMatrix)(void);
    void (*glPushMatrix)(void);
    void (*glReadPixels)(int x, int y, int width, int height, uint32_t format, uint32_t type, void* pixels);
    void (*glRotatex)(GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
    void (*glSampleCoveragex)(GLfixed value, uint8_t invert);
    void (*glScalex)(GLfixed x, GLfixed y, GLfixed z);
    void (*glScissor)(int x, int y, int width, int height);
    void (*glShadeModel)(uint32_t mode);
    void (*glStencilFunc)(uint32_t func, int ref, uint32_t mask);
    void (*glStencilMask)(uint32_t mask);
    void (*glStencilOp)(uint32_t fail, uint32_t zfail, uint32_t zpass);
    void (*glTexCoordPointer)(int size, uint32_t type, int stride, const void* ptr);
    void (*glTexEnvx)(uint32_t target, uint32_t pname, GLfixed param);
    void (*glTexEnvxv)(uint32_t target, uint32_t pname, const GLfixed* params);
    void (*glTexImage2D)(uint32_t target, int level, int internalformat, int width, int height, int border, uint32_t format, uint32_t type, const void* pixels);
    void (*glTexParameterx)(uint32_t target, uint32_t pname, GLfixed param);
    void (*glTexSubImage2D)(uint32_t target, int level, int xoffset, int yoffset, int width, int height, uint32_t format, uint32_t type, const void* pixels);
    void (*glTranslatex)(GLfixed x, GLfixed y, GLfixed z);
    void (*glVertexPointer)(int size, uint32_t type, int stride, const void* ptr);
    void (*glViewport)(int x, int y, int width, int height);
} IGLVtbl;

typedef struct IGL {
    const IGLVtbl* pvt;
} IGL;
