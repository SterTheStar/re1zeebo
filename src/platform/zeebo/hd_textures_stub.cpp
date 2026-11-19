// hd_textures_stub.cpp - Lightweight no-op HdTextures implementation for Zeebo.
//
// Avoids pulling std::thread / std::mutex into the single-threaded BREW target
// while keeping every call site in src/game/ and src/marni/ linking cleanly.
#include "../../system/HdTextures.h"

void HdTextures_Configure(bool replace, bool dump) { (void)replace; (void)dump; }
bool HdTextures_ReplaceEnabled(void) { return false; }
bool HdTextures_Active(void) { return false; }
void HdTextures_HashName(int width, int height, const void* rgba, char* out) {
    (void)width; (void)height; (void)rgba;
    if (out) out[0] = '\0';
}
bool HdTextures_Find(int width, int height, const void* rgba, HdPixels* out, int* outWidth, int* outHeight) {
    (void)width; (void)height; (void)rgba; (void)out; (void)outWidth; (void)outHeight;
    return false;
}
void HdTextures_Prefetch(int width, int height, const void* rgba) { (void)width; (void)height; (void)rgba; }
void HdTextures_PrefetchCancel(void) {}
void HdTextures_SetOwnerKey(const void* owner, const void* tag, const char* key) { (void)owner; (void)tag; (void)key; }
void HdTextures_SetOwnerKeyPerPalette(const void* owner, const void* tag, const char* key) { (void)owner; (void)tag; (void)key; }
bool HdTextures_OwnerPerPalette(const void* owner) { (void)owner; return false; }
const char* HdTextures_OwnerKey(const void* owner, const void** tag) { (void)owner; if (tag) *tag = NULL; return NULL; }
void HdTextures_SetBaseFallback(const char* key, bool allowed) { (void)key; (void)allowed; }
bool HdTextures_BaseFallbackKnown(const char* key) { (void)key; return false; }
bool HdTextures_NamedDumpWanted(const char* key) { (void)key; return false; }
void HdTextures_DumpNamed(const char* key, int width, int height, const void* rgba) { (void)key; (void)width; (void)height; (void)rgba; }
bool HdTextures_FindNamed(const char* key, HdPixels* out, int* outWidth, int* outHeight) {
    (void)key; (void)out; (void)outWidth; (void)outHeight; return false;
}
bool HdTextures_PrefetchNamed(const char* key) { (void)key; return false; }
bool HdTextures_NamedFile(const char* key, const char* ext, char* out, size_t outSize) {
    (void)key; (void)ext; if (out && outSize) out[0] = '\0'; return false;
}
void HdTextures_BeginSource(const char* key, int width, int height) { (void)key; (void)width; (void)height; }
void HdTextures_EndSource(void) {}
const char* HdTextures_SourceFor(int width, int height) { (void)width; (void)height; return NULL; }
void HdTextures_BeginRegionSource(const char* key, int x, int y, int w, int h, int pageW, int pageH) {
    (void)key; (void)x; (void)y; (void)w; (void)h; (void)pageW; (void)pageH;
}
bool HdTextures_SourceRegion(int* x, int* y, int* w, int* h, int* pageW, int* pageH) {
    (void)x; (void)y; (void)w; (void)h; (void)pageW; (void)pageH; return false;
}
void HdTextures_BeginPageSource(const char* key, int width, int height, const void* reference) {
    (void)key; (void)width; (void)height; (void)reference;
}
const void* HdTextures_SourceReference(void) { return NULL; }
void HdTextures_BeginPicture(int width, int height, const void* rgba, int hdWidth, int hdHeight, const void* hdRgba) {
    (void)width; (void)height; (void)rgba; (void)hdWidth; (void)hdHeight; (void)hdRgba;
}
void HdTextures_EndPicture(void) {}
bool HdTextures_PictureFor(int width, int height, const void* rgba, const void** hdRgba, int* hdWidth, int* hdHeight) {
    (void)width; (void)height; (void)rgba; (void)hdRgba; (void)hdWidth; (void)hdHeight; return false;
}
const char* HdTextures_KeyForPath(const char* path, char* out, size_t outSize) {
    (void)path; if (out && outSize) out[0] = '\0'; return out ? out : "";
}
void HdTextures_BeginDefer(void) {}
void HdTextures_EndDefer(void) {}
bool HdTextures_Deferring(void) { return false; }
int HdTextures_TryNamed(const char* key, HdPixels* out, int* outWidth, int* outHeight, std::string* token) {
    (void)key; (void)out; (void)outWidth; (void)outHeight; (void)token; return 0;
}
int HdTextures_TryHash(int width, int height, const void* rgba, HdPixels* out, int* outWidth, int* outHeight, std::string* token) {
    (void)width; (void)height; (void)rgba; (void)out; (void)outWidth; (void)outHeight; (void)token; return 0;
}
int HdTextures_Poll(const std::string& token, HdPixels* out, int* outWidth, int* outHeight) {
    (void)token; (void)out; (void)outWidth; (void)outHeight; return 0;
}
void HdTextures_NoteLoad(const void* buffer, size_t size, const char* assetPath) { (void)buffer; (void)size; (void)assetPath; }
const char* HdTextures_KeyForBuffer(const void* ptr, char* out, size_t outSize) {
    (void)ptr; if (out && outSize) out[0] = '\0'; return out ? out : "";
}
HdBufferScope::HdBufferScope(const void* ptr) { (void)ptr; }
