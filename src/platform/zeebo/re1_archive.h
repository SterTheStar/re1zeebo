// re1_archive.h - Arquivo unico RE1DATA.PAK sobre IFileMgr (formato R1PA).
//
// Substitui as 2935 entradas soltas da pasta USA/ por um unico arquivo.
// Os bytes de cada entrada JA estao no formato R1LZ (LZ4/ADPCM); este modulo
// apenas localiza a entrada e serve leituras com Seek/Read no IFile aberto.
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE1PA_NAME_MAX 72u

typedef struct RE1ArcEntry {
    char     name[RE1PA_NAME_MAX];  /* nome normalizado, ASCII maiusculo, '/' */
    uint32_t offset;                /* offset absoluto no .PAK */
    uint32_t size;                  /* tamanho dos bytes R1LZ gravados */
    uint32_t flags;                 /* reservado para o futuro */
    uint32_t reserved;
} RE1ArcEntry;                       /* 88 bytes */

typedef struct RE1ArcFile {
    const RE1ArcEntry* entry;
    uint32_t           pos;
} RE1ArcFile;

/* Abre o PAK e carrega o indice na RAM. Deve ser chamado no boot do applet. */
int  RE1Arc_Open(const char* pakPath);
void RE1Arc_Close(void);
int  RE1Arc_IsOpen(void);
uint32_t RE1Arc_EntryCount(void);

/* Busca case-insensitive; aceita prefixos './', 'assets/' e 'usa/'. */
const RE1ArcEntry* RE1Arc_Find(const char* path);
int  RE1Arc_OpenFile(const char* path, RE1ArcFile* out);
int  RE1Arc_Read(RE1ArcFile* f, void* dst, uint32_t len);
int  RE1Arc_Seek(RE1ArcFile* f, int32_t pos, int whence);
uint32_t RE1Arc_Size(const RE1ArcFile* f);

/* Leitura aleatoria (usada pelo demuxer de AVI). */
int  RE1Arc_ReadAt(const RE1ArcEntry* e, uint32_t off, void* dst, uint32_t len);

#ifdef __cplusplus
}
#endif
