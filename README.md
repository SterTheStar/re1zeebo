<div align="center">

# Resident Evil 1 Zeebo

Port nativo de **Resident Evil 1 (PC)** para **Zeebo (Qualcomm BREW 4.0.2)**

![Platform](https://img.shields.io/badge/Plataforma-Zeebo%20(BREW%204.0.2)-0066cc?style=flat-square)
![Arch](https://img.shields.io/badge/CPU-ARMv6K%20soft--float-1f6feb?style=flat-square)
![Graphics](https://img.shields.io/badge/GPU-OpenGL%20ES%201.1-2ea44f?style=flat-square)
![Release](https://img.shields.io/badge/Release-v1.0-d73a49?style=flat-square)

</div>

Port não oficial, gratuito e de código aberto desenvolvido por **Esther** ([pandoragg.eu.cc](https://pandoragg.eu.cc)). Sem vínculo oficial com a Capcom. **Nenhum arquivo original do jogo é incluído neste repositório.**

---

## Recursos implementados

| Área | Detalhes |
| :--- | :--- |
| **Sistema (BREW 4.0.2)** | Ciclo `IApplet`, timer em 30 fps, heap/I/O via `pfnMalloc` e `IFileMgr`, troca de contexto ARMv6 (`TaskScheduler_arm.S`) |
| **Gráficos (GLES 1.1)** | Contexto EGL `RGB565` + `DEPTH16` em `640x480`, texturas TIM em `RGBA8888`, perspectiva 3D com `w` e depth test |
| **Desempenho** | Build `-O2` com soft-float puro (`-mfloat-abi=soft`, **sem VFP/VFPv2**); o `.mod` final é limpo por `patch_vfp.py` (zero instruções VFP/WMMX), cache de estado GLES1 e buffers reutilizáveis para modelos 3D |
| **Cutscenes (FMV)** | Demuxer AVI em streaming, decodificador Cinepak e faixa de áudio PCM reproduzida via `IMedia` |
| **Áudio e Controles** | Bancos WAV via `IMediaUtil` e suporte nativo ao Z-Pad via `IHID` (com fallback de teclado via `EVT_KEY`) |

---

## Como preparar os assets

O pacote **oficial** já está pronto em **`dist/zeebo_sd/`** e tem apenas
**3 arquivos**:

```
mif/990001.mif
mod/990001/990001.mod
mod/990001/RE1DATA.PAK      <- 2935 arquivos do jogo em arquivo unico
```

São **352 MB** em vez de 598 MB. Os dados vêm em LZ4 e o áudio em ADPCM 4-bit;
o módulo descompacta tudo transparentemente na leitura. O áudio é tocado em
**streaming** direto do PAK (`ISource` + `AEECLSID_MEDIAPCM`), então nem o PCM
dos filmes nem os bancos de som ficam inteiros na RAM — só ~8 KB por voz.
Basta copiar a pasta para o SD (ou carregar no Zeebx). `config.ini` e saves
continuam soltos, sempre dentro de `udata/` (exigência do Zeebo Developer Guide
§11.3); o log em arquivo é desligado no hardware (só o simulador escreve
`udata/re1_zeebo.log`).

Os arquivos crus do PC não são mais incluídos no repositório. Se precisar
regerar, copie `USA/` de uma instalação GOG/retail para uma pasta temporária
e siga `AGENTS.md` §18 (que inclui a geração do `RE1DATA.PAK`).

---

## Instalação no console — cuidado com a NAND

O Zeebo tem **duas** memórias flash internas:

| Memória | Tamanho | O que guarda |
| :--- | :--- | :--- |
| **NAND** | 128 MB | bootloaders, BREW, sistema, logs do sistema, **Z-Wheel** |
| **eNAND** | 1 GB | jogos/apps (`/mmc4/mod/<id>/`) e **saves** (`udata/`) |

Este pacote tem **352 MB** — **não cabe na NAND**. Por isso:

1. Copie `mif/` e `mod/` para um cartão SD e rode **EMAPPLET → Field Test →
   Memory Copy** para instalar na **eNAND**.
2. Abra o jogo pelo **Appmgr**.
3. **Nunca adicione este jogo à Z-Wheel.** O ZeeboMCP copia o jogo da eNAND
   para a NAND antes de executá-lo; se não conseguir devolver a cópia ao sair,
   sobra um `zmcp.dat` e parte do jogo na NAND, esgotando os 30–60 MB livres e
   desestabilizando o console.

Os saves (`udata/SAVE/savedat1..16.dat`, `bonus.dat`, `batrank.dat`) e o
`config.ini` são criados em **`/mod/990001/udata/`** na **eNAND**, conforme o
**Zeebo Developer Guide §11.3**: todo dado gerado pelo jogo deve ficar em
`udata/`, nunca na raiz do módulo. O `RE1DATA.PAK` é **somente leitura**.
Nenhuma escrita pode sair da pasta do app: caminhos absolutos, prefixos
`fs:`/`mmc4:`/drive e segmentos `..` são recusados pelo runtime.

---

## Como compilar e rodar

Requisitos: **Arm GNU Toolchain** (`arm-none-eabi-gcc`), **CMake** (>= 3.16), **Ninja** e **Python 3**.

```powershell
cmake -S . -B build/zeebo -G Ninja -DZEEBO=ON -DCMAKE_TOOLCHAIN_FILE=cmake/zeebo-arm.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/zeebo -j 8
```

O binário final será gerado em `build/zeebo/re1zeebo.mod`. Para rodar, use o
pacote oficial **`dist/zeebo_sd/`** (que já tem o `.mod` e o `RE1DATA.PAK`) no
**Zeebx** ou no console. O `.mod` sozinho, sem o PAK, não é suficiente.

---

## Controles (Z-Pad)

| Botão | Ação |
| :--- | :--- |
| **D-Pad / Analógico** | Movimentação e navegação nos menus |
| **B1 (`A`)** | Confirmar, atirar, interagir |
| **B2 (`B`)** | Cancelar, correr |
| **B3 (`X`) / B4 (`Y`)** | Inventário / ações secundárias |
| **ZL / ZR / L2 / R2** | Mirar / mapa / atalhos |
| **Start / Home** | Pausa / menu |
