// MarniDX_GLES1.cpp - OpenGL ES 1.0/1.1 fixed-function backend for Zeebo.
//
// Counterpart of MarniDX.cpp (D3D11) and MarniDX_GL.cpp (GL 3.3 Core).
// Targets Zeebo's Adreno 130 GPU (OpenGL ES 1.0/1.1, no shaders, no VAOs)
// through BREW's IEGL and standard GLES1 entry points.
#include "MarniDX.h"
#include "MarniGLES1Funcs.h"
#include "../platform/zeebo/zeebo_brew.h"
#include "../game/Types.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARNI_MAX_TEXTURES 2048

// ---------------------------------------------------------------------------
// BREW IGL dispatchers (forward GLES1 calls through g_Zeebo.pGL vtable)
// ---------------------------------------------------------------------------
static inline const IGLVtbl* ZeeboGL(void)
{
    return g_Zeebo.pGL ? ((IGL*)g_Zeebo.pGL)->pvt : NULL;
}

static inline GLfixed ToFixed16(float f)
{
    return (GLfixed)(f * 65536.0f);
}

static void glBindTexture(uint32_t target, uint32_t texture) {
    if (auto* gl = ZeeboGL()) gl->glBindTexture(target, texture);
}
static void glBlendFunc(uint32_t sfactor, uint32_t dfactor) {
    if (auto* gl = ZeeboGL()) gl->glBlendFunc(sfactor, dfactor);
}
static void glClear(uint32_t mask) {
    if (auto* gl = ZeeboGL()) gl->glClear(mask);
}
static void glClearColor(float r, float g, float b, float a) {
    if (auto* gl = ZeeboGL()) gl->glClearColorx(ToFixed16(r), ToFixed16(g), ToFixed16(b), ToFixed16(a));
}
static void glClearDepthf(float depth) {
    if (auto* gl = ZeeboGL()) gl->glClearDepthx(ToFixed16(depth));
}
static void glColorPointer(int size, uint32_t type, int stride, const void* ptr) {
    if (auto* gl = ZeeboGL()) gl->glColorPointer(size, type, stride, ptr);
}
static void glDeleteTextures(int n, const uint32_t* textures) {
    if (auto* gl = ZeeboGL()) gl->glDeleteTextures(n, textures);
}
static void glDepthFunc(uint32_t func) {
    if (auto* gl = ZeeboGL()) gl->glDepthFunc(func);
}
static void glDepthMask(uint8_t flag) {
    if (auto* gl = ZeeboGL()) gl->glDepthMask(flag);
}
static void glDisable(uint32_t cap) {
    if (auto* gl = ZeeboGL()) gl->glDisable(cap);
}
static void glDisableClientState(uint32_t array) {
    if (auto* gl = ZeeboGL()) gl->glDisableClientState(array);
}
static void glDrawArrays(uint32_t mode, int first, int count) {
    if (auto* gl = ZeeboGL()) gl->glDrawArrays(mode, first, count);
}
static void glEnable(uint32_t cap) {
    if (auto* gl = ZeeboGL()) gl->glEnable(cap);
}
static void glEnableClientState(uint32_t array) {
    if (auto* gl = ZeeboGL()) gl->glEnableClientState(array);
}
static void glGenTextures(int n, uint32_t* textures) {
    if (auto* gl = ZeeboGL()) gl->glGenTextures(n, textures);
}
static void glLoadIdentity(void) {
    if (auto* gl = ZeeboGL()) gl->glLoadIdentity();
}
static void glMatrixMode(uint32_t mode) {
    if (auto* gl = ZeeboGL()) gl->glMatrixMode(mode);
}
static void glOrthof(float left, float right, float bottom, float top, float zNear, float zFar) {
    if (auto* gl = ZeeboGL()) gl->glOrthox(ToFixed16(left), ToFixed16(right), ToFixed16(bottom), ToFixed16(top), ToFixed16(zNear), ToFixed16(zFar));
}
static void glReadPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void* pixels) {
    if (auto* gl = ZeeboGL()) gl->glReadPixels(x, y, width, height, format, type, pixels);
}
static void glTexCoordPointer(int size, uint32_t type, int stride, const void* ptr) {
    if (auto* gl = ZeeboGL()) gl->glTexCoordPointer(size, type, stride, ptr);
}
static void glTexImage2D(uint32_t target, int level, int internalformat, int width, int height, int border, uint32_t format, uint32_t type, const void* pixels) {
    if (auto* gl = ZeeboGL()) gl->glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
static void glTexParameteri(uint32_t target, uint32_t pname, int param) {
    if (auto* gl = ZeeboGL()) gl->glTexParameterx(target, pname, (GLfixed)param);
}
static void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height, uint32_t format, uint32_t type, const void* pixels) {
    if (auto* gl = ZeeboGL()) gl->glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
static void glVertexPointer(int size, uint32_t type, int stride, const void* ptr) {
    if (auto* gl = ZeeboGL()) gl->glVertexPointer(size, type, stride, ptr);
}
static void glViewport(int x, int y, int width, int height) {
    if (auto* gl = ZeeboGL()) gl->glViewport(x, y, width, height);
}

namespace {

MarniDX* s_instance = NULL;

struct Gles1Vertex {
    float x, y, z, w;
    float u, v;
    float r, g, b, a;
};

static Gles1Vertex s_scratchVerts[1024 * 3];

void UnpackColor(DWORD color, float* r, float* g, float* b, float* a)
{
    *a = (float)((color >> 24) & 0xFF) / 255.0f;
    *r = (float)((color >> 16) & 0xFF) / 255.0f;
    *g = (float)((color >>  8) & 0xFF) / 255.0f;
    *b = (float)((color >>  0) & 0xFF) / 255.0f;
}

uint8_t* ConvertToRGBA8888(int width, int height, int bpp, const void* src)
{
    if (!src || width <= 0 || height <= 0) {
        return NULL;
    }
    size_t count = (size_t)width * (size_t)height;
    uint8_t* dst = (uint8_t*)malloc(count * 4);
    if (!dst) {
        return NULL;
    }

    const uint8_t* in8 = (const uint8_t*)src;
    if (bpp == 32) {
        // 32-bit input is already RGBA (R in lowest byte), matching MarniDX_GL.cpp.
        memcpy(dst, in8, count * 4);
    } else if (bpp == 24) {
        for (size_t i = 0; i < count; ++i) {
            dst[i * 4 + 0] = in8[i * 3 + 2];
            dst[i * 4 + 1] = in8[i * 3 + 1];
            dst[i * 4 + 2] = in8[i * 3 + 0];
            dst[i * 4 + 3] = 255;
        }
    } else if (bpp == 16) {
        const uint16_t* in16 = (const uint16_t*)src;
        for (size_t i = 0; i < count; ++i) {
            uint16_t p = in16[i];
            uint8_t r = (uint8_t)(((p >> 10) & 0x1F) << 3);
            uint8_t g = (uint8_t)(((p >>  5) & 0x1F) << 3);
            uint8_t b = (uint8_t)(((p >>  0) & 0x1F) << 3);
            uint8_t a = (p == 0) ? 0 : 255;
            dst[i * 4 + 0] = r;
            dst[i * 4 + 1] = g;
            dst[i * 4 + 2] = b;
            dst[i * 4 + 3] = a;
        }
    } else {
        memset(dst, 255, count * 4);
    }
    return dst;
}

} // namespace

struct MarniDX::Impl {
    AEEEGLDisplay display = ZEEBO_EGL_NO_DISPLAY;
    AEEEGLSurface surface = ZEEBO_EGL_NO_SURFACE;
    AEEEGLContext context = ZEEBO_EGL_NO_CONTEXT;
    int  width  = 640;
    int  height = 480;
    bool ready  = false;
    bool aspectLock = true;
    int  viewX = 0, viewY = 0, viewW = 640, viewH = 480;

    MarniHandle whiteHandle = MARNI_NULL_HANDLE;
    MarniHandle fontHandle  = MARNI_NULL_HANDLE;
    int fontW = 0, fontH = 0;

    struct TexSlot {
        uint32_t tex = 0;
        int w = 0, h = 0;
        unsigned int serial = 0;
    } slots[MARNI_MAX_TEXTURES];

    void UpdateView() {
        viewX = 0; viewY = 0; viewW = width; viewH = height;
        if (aspectLock && width > 0 && height > 0) {
            if (width * 3 > height * 4) {
                viewW = height * 4 / 3;
                viewX = (width - viewW) / 2;
            } else if (width * 3 < height * 4) {
                viewH = width * 3 / 4;
                viewY = (height - viewH) / 2;
            }
        }
    }

    void ApplyBlendAndSampler(MarniHandle tex, MarniSampler sampler, MarniBlend blend) {
        if (tex != MARNI_NULL_HANDLE && tex < MARNI_MAX_TEXTURES && slots[tex].tex != 0) {
            glEnable(GLES1_TEXTURE_2D);
            glBindTexture(GLES1_TEXTURE_2D, slots[tex].tex);
        } else if (whiteHandle != MARNI_NULL_HANDLE && slots[whiteHandle].tex != 0) {
            glEnable(GLES1_TEXTURE_2D);
            glBindTexture(GLES1_TEXTURE_2D, slots[whiteHandle].tex);
        } else {
            glDisable(GLES1_TEXTURE_2D);
        }

        int filter = (sampler == MARNI_SAMPLER_LINEAR) ? GLES1_LINEAR : GLES1_NEAREST;
        glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_MAG_FILTER, filter);

        switch (blend) {
        case MARNI_BLEND_DISABLE:
            glDisable(GLES1_BLEND);
            break;
        case MARNI_BLEND_ADD:
            glEnable(GLES1_BLEND);
            glBlendFunc(GLES1_ONE, GLES1_ONE);
            break;
        case MARNI_BLEND_ADD_ALPHA:
            glEnable(GLES1_BLEND);
            glBlendFunc(GLES1_SRC_ALPHA, GLES1_ONE);
            break;
        case MARNI_BLEND_ALPHA:
        default:
            glEnable(GLES1_BLEND);
            glBlendFunc(GLES1_SRC_ALPHA, GLES1_ONE_MINUS_SRC_ALPHA);
            break;
        }
    }

    void SubmitTriangles(const Gles1Vertex* v, int vertCount, int posComponents = 3) {
        glEnableClientState(GLES1_VERTEX_ARRAY);
        glEnableClientState(GLES1_TEXTURE_COORD_ARRAY);
        glEnableClientState(GLES1_COLOR_ARRAY);

        glVertexPointer(posComponents, GLES1_FLOAT, sizeof(Gles1Vertex), &v[0].x);
        glTexCoordPointer(2, GLES1_FLOAT, sizeof(Gles1Vertex), &v[0].u);
        glColorPointer(4, GLES1_FLOAT, sizeof(Gles1Vertex), &v[0].r);

        glDrawArrays(GLES1_TRIANGLES, 0, vertCount);

        glDisableClientState(GLES1_COLOR_ARRAY);
        glDisableClientState(GLES1_TEXTURE_COORD_ARRAY);
        glDisableClientState(GLES1_VERTEX_ARRAY);
    }

    void SetOrthoProjection() {
        glMatrixMode(GLES1_PROJECTION);
        glLoadIdentity();
        glOrthof(0.0f, (float)viewW, (float)viewH, 0.0f, 1.0f, -1.0f);
        glMatrixMode(GLES1_MODELVIEW);
        glLoadIdentity();
    }

    void SetIdentityProjection() {
        glMatrixMode(GLES1_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GLES1_MODELVIEW);
        glLoadIdentity();
    }

    void ReleaseSlot(MarniHandle h) {
        if (h == MARNI_NULL_HANDLE || h >= MARNI_MAX_TEXTURES) return;
        if (slots[h].tex != 0) {
            glDeleteTextures(1, &slots[h].tex);
        }
        slots[h].tex = 0;
        slots[h].w = slots[h].h = 0;
        slots[h].serial++;
    }
};

// ---------------------------------------------------------------------------
// Singleton accessors
// ---------------------------------------------------------------------------
MarniDX* Marni_DX() { return s_instance; }

MarniDX* MarniDX_Create()
{
    if (!s_instance) {
        s_instance = new MarniDX();
    }
    return s_instance;
}

void MarniDX_DestroyGlobal()
{
    if (s_instance) {
        s_instance->Destroy();
        delete s_instance;
        s_instance = NULL;
    }
}

MarniDX::MarniDX() : m_pImpl(new Impl()) {}

MarniDX::~MarniDX()
{
    Destroy();
    delete m_pImpl;
    m_pImpl = NULL;
}

BOOL MarniDX::Create(HWND hWnd, int width, int height, BOOL fullScreen,
                     int* outWidth, int* outHeight)
{
    (void)hWnd;
    (void)fullScreen;
    if (!m_pImpl) return FALSE;

    m_pImpl->width  = (width  >= 320) ? width  : 640;
    m_pImpl->height = (height >= 240) ? height : 480;
    m_pImpl->UpdateView();

    if (g_Zeebo.pEGL && g_Zeebo.pEGL->pvt) {
        const IEGLVtbl* egl = g_Zeebo.pEGL->pvt;
        m_pImpl->display = egl->eglGetDisplay(ZEEBO_EGL_DEFAULT_DISPLAY);
        egl->eglInitialize(m_pImpl->display, NULL, NULL);

        const AEEEGLint attribs[] = {
            ZEEBO_EGL_RED_SIZE,   5,
            ZEEBO_EGL_GREEN_SIZE, 6,
            ZEEBO_EGL_BLUE_SIZE,  5,
            ZEEBO_EGL_DEPTH_SIZE, 16,
            ZEEBO_EGL_NONE
        };
        AEEEGLConfig cfg = NULL;
        AEEEGLint numCfg = 0;
        egl->eglChooseConfig(m_pImpl->display, attribs, &cfg, 1, &numCfg);
        m_pImpl->surface = egl->eglCreateWindowSurface(m_pImpl->display, cfg, NULL, NULL);
        m_pImpl->context = egl->eglCreateContext(m_pImpl->display, cfg, ZEEBO_EGL_NO_CONTEXT, NULL);
        egl->eglMakeCurrent(m_pImpl->display, m_pImpl->surface, m_pImpl->surface, m_pImpl->context);
    }

    // Create 1x1 white fallback texture
    const uint32_t whitePixel = 0xFFFFFFFFu;
    m_pImpl->ready = true;
    m_pImpl->whiteHandle = CreateTexture(1, 1, 32, &whitePixel, NULL, NULL);

    if (outWidth)  *outWidth  = m_pImpl->width;
    if (outHeight) *outHeight = m_pImpl->height;
    return TRUE;
}

void MarniDX::Destroy()
{
    if (!m_pImpl || !m_pImpl->ready) return;

    for (MarniHandle h = 1; h < MARNI_MAX_TEXTURES; ++h) {
        m_pImpl->ReleaseSlot(h);
    }
    m_pImpl->whiteHandle = MARNI_NULL_HANDLE;
    m_pImpl->fontHandle  = MARNI_NULL_HANDLE;

    if (g_Zeebo.pEGL && g_Zeebo.pEGL->pvt && m_pImpl->display) {
        const IEGLVtbl* egl = g_Zeebo.pEGL->pvt;
        egl->eglMakeCurrent(m_pImpl->display, ZEEBO_EGL_NO_SURFACE, ZEEBO_EGL_NO_SURFACE, ZEEBO_EGL_NO_CONTEXT);
        if (m_pImpl->context) egl->eglDestroyContext(m_pImpl->display, m_pImpl->context);
        if (m_pImpl->surface) egl->eglDestroySurface(m_pImpl->display, m_pImpl->surface);
        egl->eglTerminate(m_pImpl->display);
    }
    m_pImpl->ready = false;
}

BOOL MarniDX::IsReady() const { return (m_pImpl && m_pImpl->ready) ? TRUE : FALSE; }

void MarniDX::GetBackBufferSize(DWORD* outWidth, DWORD* outHeight) const
{
    if (!m_pImpl || !m_pImpl->ready) {
        if (outWidth)  *outWidth  = 0;
        if (outHeight) *outHeight = 0;
        return;
    }
    if (outWidth)  *outWidth  = (DWORD)m_pImpl->viewW;
    if (outHeight) *outHeight = (DWORD)m_pImpl->viewH;
}

void MarniDX::SetAspectLock(BOOL lock)
{
    if (!m_pImpl) return;
    m_pImpl->aspectLock = (lock != FALSE);
    m_pImpl->UpdateView();
}

int MarniDX::ChangeDisplayMode(DWORD newWidth, DWORD newHeight, BOOL fullScreen)
{
    (void)fullScreen;
    if (!m_pImpl) return 0;
    m_pImpl->width  = (int)newWidth;
    m_pImpl->height = (int)newHeight;
    m_pImpl->UpdateView();
    return 1;
}

int MarniDX::HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hwnd; (void)msg; (void)wParam; (void)lParam;
    return 1;
}

unsigned int MarniDX::QueryVideoMemory(BOOL softwareRenderer) const
{
    (void)softwareRenderer;
    return 64u * 1024u * 1024u;
}

void MarniDX::EnumerateDisplayModes(DisplayModeInfo* outModes, int maxModes, int* outCount)
{
    if (outModes && maxModes > 0) {
        outModes[0].dwWidth  = 640;
        outModes[0].dwHeight = 480;
        outModes[0].dwBPP    = 16;
        if (outCount) *outCount = 1;
    } else if (outCount) {
        *outCount = 0;
    }
}

void MarniDX::EnumerateAdapters(D3DRendererInfo* outRenderers, int max, int* outCount)
{
    (void)outRenderers; (void)max;
    if (outCount) *outCount = 0;
}

void MarniDX::Clear(float r, float g, float b, float a)
{
    if (!m_pImpl || !m_pImpl->ready) return;

    glViewport(m_pImpl->viewX, m_pImpl->viewY, m_pImpl->viewW, m_pImpl->viewH);
    glMatrixMode(GLES1_PROJECTION);
    glLoadIdentity();
    // zNear=1, zFar=-1 maps input z in [0,1] positively so smaller z is closer (GL_LEQUAL).
    glOrthof(0.0f, (float)m_pImpl->viewW, (float)m_pImpl->viewH, 0.0f, 1.0f, -1.0f);
    glMatrixMode(GLES1_MODELVIEW);
    glLoadIdentity();

    glDisable(GLES1_CULL_FACE);
    glDisable(GLES1_DEPTH_TEST);
    glDepthMask(GLES1_TRUE);
    glClearColor(r, g, b, a);
    glClearDepthf(1.0f);
    glClear(GLES1_COLOR_BUFFER_BIT | GLES1_DEPTH_BUFFER_BIT);
}

void MarniDX::Present()
{
    if (!m_pImpl || !m_pImpl->ready) return;
    static int s_frameLog = 0;
    if (s_frameLog < 5) {
        ++s_frameLog;
        Zeebo_Log("[FRAME] Present #%d view=%dx%d\n", s_frameLog, m_pImpl->viewW, m_pImpl->viewH);
    }
    if (g_Zeebo.pEGL && g_Zeebo.pEGL->pvt && m_pImpl->display && m_pImpl->surface) {
        g_Zeebo.pEGL->pvt->eglSwapBuffers(m_pImpl->display, m_pImpl->surface);
    }
}

MarniHandle MarniDX::CreateTexture(int width, int height, int bpp, const void* pixelData,
                                   int* outWidth, int* outHeight)
{
    if (!m_pImpl || !m_pImpl->ready || width <= 0 || height <= 0) {
        return MARNI_NULL_HANDLE;
    }

    MarniHandle slot = MARNI_NULL_HANDLE;
    for (MarniHandle h = 1; h < MARNI_MAX_TEXTURES; ++h) {
        if (m_pImpl->slots[h].tex == 0) {
            slot = h;
            break;
        }
    }
    if (slot == MARNI_NULL_HANDLE) return MARNI_NULL_HANDLE;

    uint32_t glTex = 0;
    glGenTextures(1, &glTex);
    static int s_texLogCount = 0;
    if (s_texLogCount < 80) {
        ++s_texLogCount;
        Zeebo_Log("[TEX] CreateTexture %dx%d bpp=%d -> slot=%u glTex=%u\n",
                  width, height, bpp, (unsigned)slot, (unsigned)glTex);
    }
    if (glTex == 0) return MARNI_NULL_HANDLE;

    glBindTexture(GLES1_TEXTURE_2D, glTex);
    glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_WRAP_S, GLES1_REPEAT);
    glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_WRAP_T, GLES1_REPEAT);
    glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_MIN_FILTER, GLES1_NEAREST);
    glTexParameteri(GLES1_TEXTURE_2D, GLES1_TEXTURE_MAG_FILTER, GLES1_NEAREST);

    uint8_t* rgba = ConvertToRGBA8888(width, height, bpp, pixelData);
    glTexImage2D(GLES1_TEXTURE_2D, 0, GLES1_RGBA, width, height, 0,
                 GLES1_RGBA, GLES1_UNSIGNED_BYTE, rgba);
    free(rgba);

    m_pImpl->slots[slot].tex = glTex;
    m_pImpl->slots[slot].w   = width;
    m_pImpl->slots[slot].h   = height;

    if (outWidth)  *outWidth  = width;
    if (outHeight) *outHeight = height;
    return slot;
}

MarniHandle MarniDX::CreateTextureFromBits(CMarniBits* bits)
{
    if (!bits || !bits->m_pPixelData) return MARNI_NULL_HANDLE;
    return CreateTexture((int)bits->m_width, (int)bits->m_height, (int)bits->m_bitDepth, bits->m_pPixelData, NULL, NULL);
}

BOOL MarniDX::UpdateTexturePixels(MarniHandle tex, const void* pixelData,
                                  int width, int height, int bpp)
{
    if (!m_pImpl || !m_pImpl->ready || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) {
        return FALSE;
    }
    if (m_pImpl->slots[tex].tex == 0) return FALSE;

    uint8_t* rgba = ConvertToRGBA8888(width, height, bpp, pixelData);
    if (!rgba) return FALSE;

    glBindTexture(GLES1_TEXTURE_2D, m_pImpl->slots[tex].tex);
    glTexSubImage2D(GLES1_TEXTURE_2D, 0, 0, 0, width, height,
                    GLES1_RGBA, GLES1_UNSIGNED_BYTE, rgba);
    free(rgba);
    return TRUE;
}

BOOL MarniDX::ReplaceTexture(MarniHandle tex, int width, int height, const void* rgba32)
{
    if (!m_pImpl || !m_pImpl->ready || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) {
        return FALSE;
    }
    if (m_pImpl->slots[tex].tex == 0 || !rgba32) return FALSE;

    glBindTexture(GLES1_TEXTURE_2D, m_pImpl->slots[tex].tex);
    glTexImage2D(GLES1_TEXTURE_2D, 0, GLES1_RGBA, width, height, 0,
                 GLES1_RGBA, GLES1_UNSIGNED_BYTE, rgba32);
    m_pImpl->slots[tex].w = width;
    m_pImpl->slots[tex].h = height;
    return TRUE;
}

unsigned int MarniDX::TextureSerial(MarniHandle tex) const
{
    if (!m_pImpl || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) return 0;
    return m_pImpl->slots[tex].serial;
}

void MarniDX::GetTextureSize(MarniHandle tex, int* outWidth, int* outHeight) const
{
    if (!m_pImpl || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) return;
    if (outWidth)  *outWidth  = m_pImpl->slots[tex].w;
    if (outHeight) *outHeight = m_pImpl->slots[tex].h;
}

void MarniDX::DestroyTexture(MarniHandle tex)
{
    if (m_pImpl) m_pImpl->ReleaseSlot(tex);
}

MarniHandle MarniDX::WhiteTexture() const
{
    return m_pImpl ? m_pImpl->whiteHandle : MARNI_NULL_HANDLE;
}

void MarniDX::DrawSprite(float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         DWORD color, MarniHandle tex,
                         MarniSampler sampler, MarniBlend blend)
{
    if (!m_pImpl || !m_pImpl->ready) return;

    float r, g, b, a;
    UnpackColor(color, &r, &g, &b, &a);

    Gles1Vertex q[6] = {
        { x,     y,     0.0f, 1.0f, u0, v0, r, g, b, a },
        { x + w, y,     0.0f, 1.0f, u1, v0, r, g, b, a },
        { x,     y + h, 0.0f, 1.0f, u0, v1, r, g, b, a },
        { x + w, y,     0.0f, 1.0f, u1, v0, r, g, b, a },
        { x + w, y + h, 0.0f, 1.0f, u1, v1, r, g, b, a },
        { x,     y + h, 0.0f, 1.0f, u0, v1, r, g, b, a },
    };

    glDisable(GLES1_DEPTH_TEST);
    m_pImpl->ApplyBlendAndSampler(tex, sampler, blend);
    m_pImpl->SubmitTriangles(q, 6);
}

void MarniDX::DrawRect(int x, int y, int w, int h, DWORD color)
{
    DrawSprite((float)x, (float)y, (float)w, (float)h,
               0.0f, 0.0f, 1.0f, 1.0f,
               color, MARNI_NULL_HANDLE, MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
}

void MarniDX::DrawLine(float x0, float y0, float x1, float y1,
                       float thickness, DWORD color)
{
    if (!m_pImpl || !m_pImpl->ready) return;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * (thickness * 0.5f);
    float ny =  dx / len * (thickness * 0.5f);
    float r, g, b, a;
    UnpackColor(color, &r, &g, &b, &a);

    Gles1Vertex q[6] = {
        { x0 - nx, y0 - ny, 0.0f, 1.0f, 0.0f, 0.0f, r, g, b, a },
        { x1 - nx, y1 - ny, 0.0f, 1.0f, 1.0f, 0.0f, r, g, b, a },
        { x0 + nx, y0 + ny, 0.0f, 1.0f, 0.0f, 1.0f, r, g, b, a },
        { x1 - nx, y1 - ny, 0.0f, 1.0f, 1.0f, 0.0f, r, g, b, a },
        { x1 + nx, y1 + ny, 0.0f, 1.0f, 1.0f, 1.0f, r, g, b, a },
        { x0 + nx, y0 + ny, 0.0f, 1.0f, 0.0f, 1.0f, r, g, b, a },
    };

    glDisable(GLES1_DEPTH_TEST);
    m_pImpl->ApplyBlendAndSampler(MARNI_NULL_HANDLE, MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
    m_pImpl->SubmitTriangles(q, 6);
}

void MarniDX::DrawTriangles(const float* verts, int triCount, MarniHandle tex,
                            MarniSampler sampler, MarniBlend blend)
{
    if (!m_pImpl || !m_pImpl->ready || !verts || triCount <= 0) return;
    if (triCount > 1024) triCount = 1024;

    int count = triCount * 3;
    for (int i = 0; i < count; ++i) {
        const float* v = verts + i * 8;
        s_scratchVerts[i] = { v[0], v[1], 0.0f, 1.0f, v[2], v[3], v[4], v[5], v[6], v[7] };
    }

    glDisable(GLES1_DEPTH_TEST);
    m_pImpl->ApplyBlendAndSampler(tex, sampler, blend);
    m_pImpl->SubmitTriangles(s_scratchVerts, count, 3);
}

void MarniDX::DrawTrianglesPersp(const float* verts, int triCount, MarniHandle tex,
                                 MarniSampler sampler, MarniBlend blend, bool depthTest)
{
    if (!m_pImpl || !m_pImpl->ready || !verts || triCount <= 0) return;
    if (triCount > 1024) triCount = 1024;

    int count = triCount * 3;
    float invW = 2.0f / (float)m_pImpl->viewW;
    float invH = 2.0f / (float)m_pImpl->viewH;
    for (int i = 0; i < count; ++i) {
        const float* v = verts + i * 10;
        float w = (v[3] > 0.001f) ? v[3] : 1.0f;
        float ndcX = v[0] * invW - 1.0f;
        float ndcY = 1.0f - v[1] * invH;
        float ndcZ = v[2] * 2.0f - 1.0f;
        s_scratchVerts[i] = { ndcX * w, ndcY * w, ndcZ * w, w, v[4], v[5], v[6], v[7], v[8], v[9] };
    }

    if (depthTest) {
        glEnable(GLES1_DEPTH_TEST);
        glDepthFunc(GLES1_LEQUAL);
        glDepthMask(GLES1_FALSE);
    } else {
        glDisable(GLES1_DEPTH_TEST);
    }

    m_pImpl->SetIdentityProjection();
    m_pImpl->ApplyBlendAndSampler(tex, sampler, blend);
    m_pImpl->SubmitTriangles(s_scratchVerts, count, 4);
    m_pImpl->SetOrthoProjection();
    glDisable(GLES1_DEPTH_TEST);
    glDepthMask(GLES1_TRUE);
}

void MarniDX::DrawTriangles3D(const float* verts, int triCount, MarniHandle tex,
                              MarniSampler sampler, MarniBlend blend, bool depthWrite)
{
    if (!m_pImpl || !m_pImpl->ready || !verts || triCount <= 0) return;
    static int s_3dLog = 0;
    if (s_3dLog < 5) {
        ++s_3dLog;
        Zeebo_Log("[3D] DrawTriangles3D tris=%d tex=%u v0=(%.1f,%.1f,%.3f,w=%.1f) uv=(%.3f,%.3f) col=(%.2f,%.2f,%.2f,%.2f)\n",
                  triCount, (unsigned)tex, verts[0], verts[1], verts[2], verts[3],
                  verts[4], verts[5], verts[6], verts[7], verts[8], verts[9]);
    }
    if (triCount > 1024) triCount = 1024;

    int count = triCount * 3;
    float invW = 2.0f / (float)m_pImpl->viewW;
    float invH = 2.0f / (float)m_pImpl->viewH;
    for (int i = 0; i < count; ++i) {
        const float* v = verts + i * 10;
        float w = (v[3] > 0.001f) ? v[3] : 1.0f;
        float ndcX = v[0] * invW - 1.0f;
        float ndcY = 1.0f - v[1] * invH;
        float ndcZ = v[2] * 2.0f - 1.0f;
        s_scratchVerts[i] = { ndcX * w, ndcY * w, ndcZ * w, w, v[4], v[5], v[6], v[7], v[8], v[9] };
    }

    glEnable(GLES1_DEPTH_TEST);
    glDepthFunc(GLES1_LEQUAL);
    glDepthMask(depthWrite ? GLES1_TRUE : GLES1_FALSE);

    m_pImpl->SetIdentityProjection();
    m_pImpl->ApplyBlendAndSampler(tex, sampler, blend);
    m_pImpl->SubmitTriangles(s_scratchVerts, count, 4);
    m_pImpl->SetOrthoProjection();

    glDisable(GLES1_DEPTH_TEST);
    glDepthMask(GLES1_TRUE);
}

BOOL MarniDX::CaptureBackbufferToRGBA(void** outPixels, DWORD* outWidth, DWORD* outHeight)
{
    if (!m_pImpl || !m_pImpl->ready || !outPixels) return FALSE;
    int w = m_pImpl->viewW;
    int h = m_pImpl->viewH;
    void* buf = operator new((size_t)w * (size_t)h * 4);
    glReadPixels(m_pImpl->viewX, m_pImpl->viewY, w, h, GLES1_RGBA, GLES1_UNSIGNED_BYTE, buf);
    *outPixels = buf;
    if (outWidth)  *outWidth  = (DWORD)w;
    if (outHeight) *outHeight = (DWORD)h;
    return TRUE;
}

BOOL MarniDX::ReadTextureRGBA(MarniHandle tex, void** outPixels, int* outWidth, int* outHeight)
{
    (void)tex; (void)outPixels; (void)outWidth; (void)outHeight;
    return FALSE;
}

void MarniDX::SetFontTexture(MarniHandle tex, int width, int height)
{
    if (!m_pImpl) return;
    m_pImpl->fontHandle = tex;
    m_pImpl->fontW = width;
    m_pImpl->fontH = height;
}

MarniHandle MarniDX::FontTexture() const { return m_pImpl ? m_pImpl->fontHandle : MARNI_NULL_HANDLE; }
int MarniDX::FontTextureWidth() const  { return m_pImpl ? m_pImpl->fontW : 0; }
int MarniDX::FontTextureHeight() const { return m_pImpl ? m_pImpl->fontH : 0; }
