# Resident Evil 1 — Zeebo (BREW 4.0.2)

Port nativo de **Resident Evil (1996 / PC)** para **Zeebo** (**Qualcomm BREW 4.0.2**, ARM11 / ARMv6K com VFPv2 e OpenGL ES 1.0/1.1), compilado como módulo `.mod` e testado no emulador **Zeebx (`zeebx-emu`)**.

---

## O que está implementado

- **Plataforma BREW 4.0.2 (`src/platform/zeebo/`)**:
  - Ciclo de vida `AEEMod_Load` / `IModule` / `IApplet` e loop de quadros em ~30 fps (`zeebo_brew.cpp`, `main.cpp`).
  - Sistema de arquivos via `IFileMgr` / `IFile` com normalização de caminhos e leitura direta da pasta `USA/` (`platform.cpp`).
  - Troca cooperativa de contexto para as tasks do jogo em Assembly ARMv6 (`src/game/TaskScheduler_arm.S`).
  - Áudio PCM via `IMediaPCM` / `ISound` (`audio.cpp`).
  - Vídeo FMV com demuxer AVI e decodificador Cinepak (`avi_demux.cpp`, `cinepak.cpp`, `video.cpp`).
  - Entrada completa para **Z-Pad / Boomerang** via `IHID` / `IHIDDevice` e fallback de teclado via `EVT_KEY` (`input.cpp`).
- **Renderização OpenGL ES 1.0/1.1 (`src/marni/MarniDX_GLES1.cpp`)**:
  - Inicialização EGL 16-bit (`RGB565` + `DEPTH16`), saída `640x480` (4:3).
  - Texturas PS1 TIM (4bpp/8bpp/16bpp/24bpp/32bpp) convertidas para `RGBA8888`, `GL_REPEAT` para UVs contínuas e cache de estado GLES1.
  - Renderização 3D em perspectiva (`w` homogêneo + depth buffer `GL_LEQUAL`) e sprites/fundos 2D.

---

## Como preparar os assets (obrigatório)

O repositório **não inclui** os arquivos originais do jogo. Você precisa dos assets da versão de PC (por exemplo, a edição da **GOG** de *Resident Evil*).

### 1. Localize a pasta `USA`
Na instalação original do jogo no PC (ex.: `C:\GOG Games\Resident Evil\USA`), existe a pasta **`USA/`** contendo subpastas como:

```text
USA/
├── data/
├── effspr/
├── enemy/
├── movie/
├── objspr/
├── players/
├── sound/
├── stage1/
├── stage2/
├── stage3/
├── stage4/
├── stage5/
└── voice/
```

> **Importante:** mantenha a estrutura de subpastas exatamente como está dentro de `USA/`. O código normaliza os caminhos e lê tudo a partir de `USA/...` no diretório do módulo.

### 2. Estrutura final da pasta do módulo no Zeebo / Zeebx

O módulo deve ser distribuído como uma **pasta descompactada** (não `.zip`) contendo o binário `.mod`, o manifesto `.mif` e a pasta `USA/`:

```text
re1zeebo/
├── re1zeebo.mod
├── re1zeebo.mif
└── USA/
    ├── data/
    ├── enemy/
    ├── movie/
    ├── sound/
    ├── stage1/
    └── ...
```

### 3. Copiando automaticamente com `package_mod.py`

Depois de compilar o `re1zeebo.mod`, você pode montar a pasta `dist/zeebo/re1zeebo/` automaticamente com:

```powershell
python tools/zeebo/package_mod.py `
  --mod build/zeebo/re1zeebo.mod `
  --assets "C:\GOG Games\Resident Evil\USA" `
  --out-dir dist/zeebo/re1zeebo
```

Se você já copiou a pasta `USA/` antes e só recompilou o código, basta atualizar o `re1zeebo.mod` dentro de `dist/zeebo/re1zeebo/`.

---

## Como compilar

### Pré-requisitos
- **Arm GNU Toolchain** (`arm-none-eabi-gcc` / `arm-none-eabi-g++` no `PATH`)
- **CMake** (>= 3.16)
- **Ninja** (ou outro gerador compatível)
- **Python 3** (para `tools/zeebo/package_mod.py`)

### Build Release (ARMv6K + VFPv2 + `-O2`)

```powershell
cmake -S . -B build/zeebo -G Ninja `
  -DZEEBO=ON `
  -DCMAKE_TOOLCHAIN_FILE=cmake/zeebo-arm.cmake `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/zeebo -j 8
```

A saída será gerada em:
- `build/zeebo/re1zeebo.elf`
- `build/zeebo/re1zeebo.mod` (binário flat BREW pronto para o Zeebo / Zeebx)

---

## Como rodar no Zeebx

1. Garanta que `dist/zeebo/re1zeebo/` tenha `re1zeebo.mod`, `re1zeebo.mif` e `USA/`.
2. No **Zeebx Qt**, aponte/carregue a pasta ou o arquivo `dist/zeebo/re1zeebo/re1zeebo.mod`.
3. Pelo terminal (usando `zeebx-headless.exe` em `tools/zeebx-emu/`):

```powershell
.\tools\zeebo\run_zeebx.ps1
```

---

## Controles (Z-Pad)

- **D-Pad / Analógico**: movimentação e navegação nos menus
- **B1 (`A`)**: confirmar / atirar / interagir
- **B2 (`B`)**: cancelar / correr
- **B3 (`X`) / B4 (`Y`)**: ações secundárias / inventário
- **ZL / ZR / L2 / R2**: mirar / mapa / atalhos conforme mapeamento do jogo
- **Start / Home**: pausa / menu

---

## Estrutura dos arquivos do port Zeebo

```text
CMakeLists.zeebo.txt           # Configuração de build específica para Zeebo
cmake/zeebo-arm.cmake          # Toolchain ARMv6K + VFPv2 (softfp)
src/platform/zeebo/            # Camada BREW 4.0.2 (vídeo, áudio, input, FS, crashlog)
src/marni/MarniDX_GLES1.cpp    # Backend gráfico OpenGL ES 1.0/1.1 + EGL
src/marni/MarniGLES1Funcs.h    # Tabela de funções GLES1 via AEEGLES
src/game/TaskScheduler_arm.S   # Context switch cooperativo ARMv6
tools/zeebo/package_mod.py     # Gera .mif e copia .mod + pasta USA/
tools/zeebo/zeebo_mod.ld       # Linker script do módulo flat BREW
tools/zeebo/run_zeebx.ps1      # Atalho para rodar no zeebx-headless
```
