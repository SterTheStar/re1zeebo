# Resident Evil 1 — Zeebo (AGENTS.md)

## 1. Identidade e escopo

Este repositório é o **port nativo de Resident Evil 1 (PC, 1997) para Zeebo** —
console da Tectoy baseado em **Qualcomm BREW 4.0.2**, CPU **ARM1136J-S (ARMv6K,
MSM7201A)** e GPU **Adreno 130** com **OpenGL ES 1.0/1.1**.

- Port não oficial, gratuito e de código aberto, por **Esther**
  ([pandoragg.eu.cc](https://pandoragg.eu.cc)). Sem vínculo com a Capcom.
- **Nenhum arquivo original do jogo é incluído**. O runtime usa os dados da
  versão PC (GOG/retail) copiados para a pasta `USA/`.
- Base de código: a decompilação Win32 do RE1 PC. O código do jogo
  (`src/game/`, `src/marni/`, `src/system/`) é **compartilhado** com os builds
  Windows e Linux; o que muda é a camada de plataforma e o renderer.
- O guia detalhado da decompilação original (Ghidra, tabelas, ferramentas de
  engenharia reversa, memória, DC/Saturn/JPN) foi preservado em
  **`AGENTS_DECOMP.md`** na raiz. Consulte-o sempre que for portar comportamento
  de outra versão ou usar as ferramentas de RE. Este arquivo é a autoridade
  para **tudo que é específico do Zeebo**.
- `README.md` é o guia do usuário final (instalação, controles, assets).
- **Os builds Linux e Windows PC foram removidos** (junto com `Game.vcxproj`,
  `build.bat`, `src/platform/{linux,win32}/` e o backend GL desktop). Só existem
  `build/zeebo` (cross ARM) e `build/zeebo_sim` (DLL do BREW Simulator). As
  ferramentas de RE, os docs e o `tools/asset_migrator/` foram preservados.

### Arquitetura em uma linha

```
código do jogo (idêntico ao PC/Linux)
    + src/platform/zeebo/*        -> BREW: IApplet, IFileMgr, IHID, IMedia, newlib
    + src/marni/MarniDX_GLES1.cpp -> GLES1 fixo via IGL (no lugar de D3D11/GL3.3)
    + src/game/TaskScheduler_arm.S -> troca de contexto A32 (no lugar do asm x86)
    = re1zeebo.mod  (módulo BREW relocável, carregado no console ou no zeebx-emu)
```

## 2. Mapa do repositório

| Caminho | Papel |
|---|---|
| `CMakeLists.txt` | Dispatch Zeebo-only: `ZEEBO=ON` é o padrão; só inclui `CMakeLists.zeebo.txt` |
| `CMakeLists.zeebo.txt` | **Source of truth do Zeebo**: listas de fontes, defines, targets, linker, pós-build |
| `cmake/zeebo-arm.cmake` | Toolchain cross ARM (`arm-none-eabi-gcc/g++`, `Generic`, `arm1136j-s`, soft-float) |
| `src/platform/zeebo/` | Toda a camada BREW (20 arquivos): ver §7–§12 |
| `src/marni/MarniDX_GLES1.cpp` | Backend GLES1 da interface `MarniDX` |
| `src/marni/MarniGLES1Funcs.h` | Constantes GLES1/EGL + vtable `IGL` (80 slots) |
| `src/game/TaskScheduler_arm.S` | `TaskSwitch_ArmSwap` (stmia/ldmia r4–r11, sp, lr) |
| `src/game/TaskScheduler.cpp` | `#elif defined(__ZEEBO__)` (linhas ~189-238) usa o contexto ARM |
| `src/platform/zeebo/hd_textures_stub.cpp` | Substitui `src/system/HdTextures.cpp`: tudo no-op |
| `src/platform/zeebo/re1_lz4.{h,cpp}` | Decoder R1LZ (LZ4 block + ADPCM + CRC32) dos assets compactados |
| `src/platform/zeebo/re1_archive.{h,cpp}` | Arquivador R1PA: serve os assets de um único `RE1DATA.PAK` |
| `src/platform/zeebo/pcm_source.{h,cpp}` | `ISource` de PCM cru: ADPCM decodificado sob demanda (~8 KB por voz) |
| `tools/zeebo/re1pack.py` | Empacotador offline: gera os arquivos R1LZ a partir de uma `USA/` original |
| `tools/zeebo/adpcm_fast.{c,dll}` | Encoder IMA ADPCM em C, chamado pelo `re1pack.py` via `ctypes` |
| `tools/zeebo/make_pak.py` | Junta a árvore R1LZ em `RE1DATA.PAK` com índice ordenado |
| `dist/zeebo_sd/` | **Pacote oficial**: 3 arquivos — `mif/990001.mif`, `mod/990001/990001.mod`, `mod/990001/RE1DATA.PAK` (352,22 MB, 2962 entradas; áudio em streaming) |
| `tools/zeebo/elf2mod.x` | Script do linker da Qualcomm para módulos BREW (entry em `0x0`, relocações) |
| `tools/zeebo/zeebo_mod.ld` | Script de fallback: `.mod` flat binário (com `.bss_pad`) |
| `tools/zeebo/package_mod.py` | Gera `.mif` e monta o layout SD/flat com os assets `USA/` |
| `build/zeebo/` | Build de hardware/emulador (Release, Ninja, `--emit-relocs`) |
| `build/zeebo_sim/` | Build da DLL x86 para o BREW Simulator (Visual Studio) |
| `dist/zeebo_sd/` | Pacote oficial pronto para o SD: 3 arquivos, inclui `RE1DATA.PAK` |
| `AGENTS_DECOMP.md` | Guia completo da decompilação PC (Ghidra, RE, ferramentas) |
| `docs/` | Documentação do projeto base (Linux, memória, Marni, DC, Saturn etc.) |

## 3. Como compilar

### 3.1 Requisitos

- **Arm GNU Toolchain** (`arm-none-eabi-gcc` / `arm-none-eabi-g++`) — a máquina
  de desenvolvimento usa o xpack `arm-none-eabi-gcc 15.2.1-1.1`.
- **BREW Toolset 7.10 Rev 12.0.1549664**: `elf2mod.exe` (necessário para gerar
  o `.mod` relocável). Sem ele, o CMake cai no fallback `objcopy -O binary`.
- **CMake >= 3.16**, **Ninja**, **Python 3** (para empacotar).
- Para o modo simulador: MSVC (Visual Studio 2022) e o BREW 4.0.2 Simulator.

### 3.2 Hardware / zeebx-emu (target principal)

```powershell
cmake -S . -B build/zeebo -G Ninja -DZEEBO=ON `
  -DCMAKE_TOOLCHAIN_FILE=cmake/zeebo-arm.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/zeebo -j 8
```

Saída: `build/zeebo/re1zeebo.elf` (~2,7 MB) e
**`build/zeebo/re1zeebo.mod` (~11,5 MB)**, gerado por `elf2mod` como POST_BUILD.

### 3.3 BREW Simulator (DLL x86)

```powershell
cmake -S . -B build/zeebo_sim -DZEEBO=ON -DZEEBO_SIM=ON
cmake --build build/zeebo_sim
```

Nesse modo: sem `TaskScheduler_arm.S` e sem `syscalls.cpp`; sai uma DLL com
`AEEMod_Load` exportado, `AEE_SIMULATOR=1`, `_CRT_SECURE_NO_WARNINGS`,
`WIN32_LEAN_AND_MEAN`, `NOMINMAX`, `/W0 /EHsc`. O CRT do Windows roda os
construtores globais automaticamente.

### 3.4 Flags e defines que **não** devem mudar sem motivo

| Item | Valor | Por quê |
|---|---|---|
| `-mcpu=arm1136j-s -marm -mfloat-abi=soft -mno-unaligned-access` | toolchain | **Nunca usar VFP/VFPv2**: o MSM7201A real não tem VFP confiável. É soft-float puro, não `softfp`. O badge do README foi corrigido |
| `-O2 -w -fsigned-char -fno-strict-aliasing -fno-strict-overflow` | compile | preserva as suposições da decomp (casts de buffers em structs, math com wrap-around) |
| `-fno-exceptions -fno-rtti -fno-threadsafe-statics` | compile | BREW é single-threaded; evita runtime extra |
| `-mword-relocations` | compile | exigido pelo formato de relocação do `.mod` |
| `-include algorithm` (CXX) | compile | workaround de include em headers antigos |
| `__ZEEBO__=1` | define | seleciona o código ARM em `TaskScheduler.cpp` e o painel de diagnóstico |
| `RE1_NO_HD_TEXTURES=1` | define | HD textures desligadas; use sempre o stub |
| `WholeProgramOptimization` | **desligado** | `/GL` + `/LTCG` quebra o escalonador por assembly (vale para o build Windows, mantido aqui como regra do projeto) |

> **Regra absoluta:** não existe VFP no alvo. Use somente `-mfloat-abi=soft`
> (soft-float puro, nem `softfp` nem VFPv2). O commit `8076cbf` removeu o VFP
> justamente porque o hardware real não o suporta de forma confiável. Se algum
> arquivo, badge ou comentário voltar a citar VFPv2, corrijá-o.

### 3.5 Link

Com `elf2mod` encontrado:

```
-T tools/zeebo/elf2mod.x -nostartfiles -Wl,--entry=AEEMod_Load
-Wl,--emit-relocs -Wl,--gc-sections
elf2mod -output re1zeebo.mod re1zeebo.elf
```

O `elf2mod.x` coloca `.text.entry` (`AEEMod_Load`) em `0x0`, define os segmentos
`ER_RO` (`PT_LOAD FLAGS(5)`) e `ER_RW`, e descarta `.ARM.exidx`/`.ARM.extab`.

Fallback (`zeebo_mod.ld`): load address `0x00010000`, `.text` → `.rodata` →
`.init_array` → `.data` → `.bss` → `.bss_pad` (materializa zeros no binário
porque o `.bss` estático excede o `MODULE_BSS_SLACK = 1 MiB` do zeebx-emu),
`objcopy -O binary` para `.mod`.

## 4. Empacotamento e releases

```powershell
# 1. gera o PAK com os 2935 arquivos R1LZ + 27 entradas .AUDIO (ver §18)
python tools/zeebo/make_pak.py "<r1lz>\USA" "<r1lz>\RE1DATA.PAK" --verify

# 2. monta o pacote oficial (3 arquivos, sem USA solta)
python tools/zeebo/package_mod.py --mod build/zeebo/re1zeebo.mod `
  --pak "<r1lz>\RE1DATA.PAK" --out-dir dist\zeebo_sd --layout sd
```

- `--layout sd` (padrão): `mif/<app-id>.mif` + `mod/<app-id>/<app-id>.mod` +
  `mod/<app-id>/RE1DATA.PAK` → **console real**. Sem `--pak`, cai para a copia
  da pasta `USA/` solta (modo legado).
- `--layout flat`: tudo em uma pasta → **zeebx-emu**.
- Padrões: `--app-id 990001`, `--clsid 0x01099001`, `--app-name "Resident Evil"`.
- O `.mif` é gerado byte a byte no layout BREW 4.0.2/Zeebo: magic `0x0011`,
  4 entradas de índice (nome do módulo `0x0014`, nome do applet `0x0028`, flags
  `0x05000`, registro do applet `0x25000`), 4 seções e trailer de 20 bytes.

## 5. Entry point e ciclo de vida BREW

Tudo em `src/platform/zeebo/zeebo_brew.cpp` e `main.cpp`.

### 5.1 `AEEMod_Load` (ponto de entrada do módulo)

1. `ResolveHelpers(ph)` — usa `ph` se fornecido; senão procura o marcador
   `"BREW"` (`0x57455242`) nos 256 words anteriores ao entry (`GET_HELPER =
   mod_base - 4`). `AEEHelperFuncs` é uma vtable de funções do kernel BREW:
   `pfnMalloc` (slot 26), `pfnFree` (27), `pfnRealloc` (29), `pfnDbgPrintf` (39),
   `pfnGetUpTimeMS` (44), `pfnGetFSFree` (77), `pfnGetRAMFree` (78).
2. `RunCppGlobalConstructors()` percorre `__init_array_start..end` (o ELF não
   tem CRT; no simulador o CRT da DLL já faz isso).
3. Aloca `Re1Module` com `pfnMalloc`, seta `s_ModuleVtbl` e devolve `IModule**`.

### 5.2 `IModule::CreateInstance`

Guarda `IShell*` em `g_Zeebo` (`ZeeboRuntime`, `zeebo_brew.h:528`) e adquire,
por `ISHELL_CreateInstance`:

| Classe | CLSID | Uso |
|---|---|---|
| FILEMGR | `0x01001003` | arquivos/saves/assets |
| DISPLAY | `0x01001001` | native window EGL (`GetDeviceBitmap` → `IDIB`) |
| EGL | `0x01014BC4` | `eglGetDisplay/ChooseConfig/CreateWindowSurface/...` |
| GL | `0x01014BC3` | vtable `IGL` (GLES1); fallback `GLES11` `0x0103D8EB` |
| HID | `0x0106C411` | Z-Pad (IHID/IHIDDevice) |
| SOUND | `0x01001056` | reservado (áudio usa MEDIAUTIL) |
| MEDIAUTIL | `0x0100550D` | `CreateMedia` para WAV/FMV |

CLSID do applet: `AEECLSID_RE1_ZEEBO = 0x01099001`.

### 5.3 `IApplet::HandleEvent` e o frame pump

| Evento | Ação |
|---|---|
| `EVT_APP_START` | `Zeebo_OnAppStart()`; `bRunning=TRUE`; `ISHELL_SetTimer(1ms, FrameTimerCallback)` |
| `EVT_APP_STOP` | cancela timer, `bRunning=FALSE`, `Zeebo_OnAppStop()` |
| `EVT_APP_SUSPEND` | chama `g_Zeebo.pfnAudioSuspend` (para todos os bancos) e cancela timer |
| `EVT_APP_RESUME` | rearma timer em 33 ms se ainda `bRunning` |
| `EVT_APP_NO_SLEEP` | retorna TRUE (mantém a tela ligada) |

`FrameTimerCallback` mede `plat_time_ms()` antes/depois de `Zeebo_OnFrameTick()`
e rearma com `delay = (elapsed >= 25) ? 8 : (33 - elapsed)` ms — alvo de ~30 fps
com pelo menos 8 ms de yield para o event loop do BREW/emulador.

### 5.4 Start / tick / stop do jogo (`main.cpp`)

- `Zeebo_OnAppStart`: `crashlog_install`/`crashlog_mark` → `ConfigFile_Load()` →
  `StartSoundSystemAsync(NULL)` → `TaskScheduler_Init()` →
  `InitializeMarniSystem()` (falha = `plat_fatal`) → flags de tela
  (640×480, fullscreen, `g_bWindowFocused/Active`) → `InitStartupSplash()`.
- Splash: logo **Pandora** (2800 ms) seguido do **aviso** (4200 ms), com fade-in
  de 400 ms e fade-out de 500 ms, desenhados como texturas RLE
  (`pandora_logo.h`, `splash_image.h`) via `MarniDX::DrawSprite`.
- `Zeebo_OnFrameTick`: se `g_bQuitFlag` → `plat_request_quit()`; aplica
  `ChangeDisplayMode` pendente; atualiza `g_dwGameTimer1`; roda o splash; se
  `g_bMCINotifyEnabled/g_bMCINotifyFlag` → `UpdateVideoPlayback()`; senão
  `main_loop()`.
- `Zeebo_OnAppStop`: libera splash, `CleanupVideoConfigAndSaveAllSettings()`,
  `MarniDX_DestroyGlobal()`.

## 6. Runtime C e syscalls (`syscalls.cpp`)

Não há newlib completa nem sistema operacional: as primitivas são implementadas
sobre BREW.

- `malloc/free/realloc/calloc` → `pfnMalloc/pfnFree/pfnRealloc` (heap BREW,
  `HEAP_BASE 0x10000000`).
- `_sbrk` → heap estático de **64 KB** (`s_heap`), alinhado a 8; mantido
  pequeno de propósito para caber no `.bss`.
- Tabela de **32 FDs** (`s_fdTable`,fd >= 3) ligados a `IFile*`;
  `_open/_read/_write/_lseek/_close/_fstat` traduzem `O_CREAT/O_TRUNC` para
  `_OFM_*` e `SEEK_*` para `FileSeekType`.
- fd 1/2 → `pfnDbgPrintf`; `_isatty` TRUE para 0..2; `_getpid` = 1;
  `_gettimeofday` via `pfnGetUpTimeMS`; `_exit` fecha o applet e entra em loop.
- `__dso_handle = 0`, `_fini`, `__gnu_Unwind_Find_exidx` e `__libc_fini`
  (este último em `stubs.cpp`) satisfazem referências fracas de libgcc/newlib
  para o `elf2mod` não ver relocações `SHN_UNDEF` fora de `PT_LOAD`.

## 7. Arquivos, assets e saves (`platform.cpp`, `config.cpp`)

- Todo caminho é relativo à raiz VFS do módulo (`/mod/<app>/`).
  `plat_normalize_path` remove `./` ou `.\` e converte `\` em `/`, colapsando
  barras repetidas. `plat_exe_dir` devolve `"."`.
- **Nenhuma escrita sai da pasta do app**: `plat_write_path_safe` (em
  `platform.cpp`, declarado em `platform.h`) é chamado por `plat_file_write`,
  `plat_mkdir` e pelo `_open` dos `fopen(...,"w"/"wb")` dos saves. Recusa
  absolutos (`/`, `\`), prefixos de drive/esquema (`C:`, `fs:`, `mmc4:`) e
  segmentos `..`. Motivo: IFileMgr resolveria isso no filesystem do dispositivo
  e poderia chegar à NAND de 128 MB do sistema — ver §20.
- Saves: `SAVE/savedat1.dat` … `savedat16.dat` (2690 B cada, `SAVE_FILE_SIZE
  = 0xA86` em `SaveLoadScreen.cpp`), `SAVE/bonus.dat` (32 B) e
  `SAVE/batrank.dat`. `GetSaveRoot()` resolve para `SAVE/` e a pasta é criada
  com `plat_mkdir`. Total ~45 KB, abaixo do limite de 64 KB do guia Zeebo.
- `plat_file_read_all` usa `IFile::GetInfo` + `Read` em loop, com padding de 16
  bytes; `plat_file_write` usa `_OFM_READWRITE|_OFM_CREATE` + `Truncate(0)`;
  `plat_dir_list` faz duas passagens (dirs, depois arquivos) com
  `EnumInit/EnumNext`; `plat_file_mtime` usa `AEEFileInfo.dwCreationDate`.
- `plat_readable_bytes` devolve `0x04000000` para ponteiros >= `0x00010000`
  (não existe `VirtualQuery`). Comentário histórico: devolver 4096 clampava
  TIMs 256×256 em 256×16.
- `plat_debug_output` → `pfnDbgPrintf`; `plat_fatal` escreve log, marca
  crashlog, seta `g_bQuitFlag` e fecha o applet.
- `plat_display_desktop_size` fixa 640×480 (composite NTSC/PAL-M);
  `plat_single_instance_check` = TRUE (BREW roda um applet foreground).
- `ConfigFile_Load/Save` em `src/system/ConfigFile.cpp`; o save da config é
  forçado no shutdown (`CleanupVideoConfigAndSaveAllSettings`).

## 8. Renderer GLES1 (`MarniDX_GLES1.cpp`, `MarniGLES1Funcs.h`)

Implementa **toda a interface `MarniDX`** (a mesma de `MarniDX.cpp`/D3D11 e
`MarniDX_GL.cpp`/GL 3.3), sem shaders, sobre a vtable `IGL` do BREW.

- **Ponte**: `ZeeboGL()` devolve `((IGL*)g_Zeebo.pGL)->pvt`. A vtable tem 80
  slots e, conforme `AEEGL.h`/zeebx-core, os métodos a partir do slot 3
  **não recebem `this`**. Funções float (`glClearColor`, `glOrthof`,
  `glClearDepthf`) são convertidas para `GLfixed` (16.16) via `ToFixed16`.
- **EGL**: `eglGetDisplay` (no hardware, passa `pDisplay` como display nativo
  quando o vendor é Qualcomm/Adreno; no simulador usa
  `ZEEBO_EGL_DEFAULT_DISPLAY`), `eglInitialize`, `eglChooseConfig` com
  RGB565 + DEPTH16, `eglCreateWindowSurface` (native window via
  `Zeebo_GetNativeWindow()`: `IDisplay::GetDeviceBitmap` →
  `QueryInterface(AEEIID_IDIB)`), `eglCreateContext`, `eglMakeCurrent`;
  `eglSwapBuffers` no `Present()`.
- **Texturas**: `MARNI_MAX_TEXTURES = 2048` slots; `CreateTexture` converte
  32/24/16 bpp para RGBA8888 (em 16 bpp, `pixel == 0` vira alpha 0), faz
  `glTexImage2D` e guarda slot/w/h/filter/serial. `ReplaceTexture` usa
  `glTexSubImage2D` quando o tamanho não muda (caminho usado pelo FMV).
- **Cache de estado**: `boundGlTex`, `tex2dEnabled`, `lastBlend`,
  `depthTestEnabled/depthWriteEnabled`, `orthoActive`, `clientArraysEnabled`;
  `ApplyBlendAndSampler` evita `glBindTexture`/`glTexParameteri`/`glBlendFunc`
  redundantes. `ResetStateCache` após cada `Clear`.
- **Desenho**: `DrawSprite`/`DrawRect`/`DrawLine` em projeção ortográfica
  4:3 com letterbox (`aspectLock`); `DrawTriangles` também ortográfico;
  `DrawTrianglesPersp`/`DrawTriangles3D` convertem vértices do jogo
  (`x,y,z,w,u,v,r,g,b,a` com `w`) para NDC multiplicado por `w`, matriz
  identidade e `GL_LEQUAL`. Limite de 1024 triângulos por chamada e buffer
  scratch de `1024*3` vértices.
- **Diagnóstico**: `Present()` chama
  `ZeeboMemPanel::Draw(this, drawCalls, tris, activeTex, MAX)` antes do swap.
- **Não implementado no Zeebo**: `ReadTextureRGBA` retorna FALSE;
  `QueryVideoMemory` devolve 64 MB fixos; `EnumerateDisplayModes` só 640×480.

## 9. Escalonador de tarefas ARM (`TaskScheduler_arm.S`)

Substitui o naked-assembly do MSVC x86 e o `ucontext` do Linux.

```
struct ArmTaskContext {   // 10 uint32_t = 40 bytes
    DWORD r4, r5, r6, r7, r8, r9, r10, r11;
    DWORD sp;
    DWORD lr;   // PC de retorno
};
```

`TaskSwitch_ArmSwap(from, to)`: `stmia r0, {r4-r11, sp, lr}` seguido de
`ldmia r1, {r4-r11, sp, lr}` e `bx lr`. Em `TaskScheduler.cpp:189-238`:

- `TaskSwitch_Start` monta `sp` alinhado a 8 bytes no topo da stack da task e
  `lr = &TaskEntry`, depois troca do contexto do scheduler para o da task.
- `TaskSwitch_Resume` volta do scheduler para a task corrente;
  `TaskYield` volta da task para o scheduler.
- A stack é alocada por `plat_alloc_guarded_stacks` (guard pages simuladas com
  `malloc` + `memset` zero).

**Nunca** habilitar LTCG/`/GL` em targets que compilam esse assembly.

## 10. Áudio (`audio.cpp`)

- `DirectSound` é uma fachada: até `MAX_BANKS = 81` bancos `ZeeboSoundBank`
  (path, volume em millibels, pan DirectSound, estado `BrewPlayState`, `IMedia*`).
- `CreateSound(wavName)` normaliza o path (remove `assets/` se o arquivo não
  existir lá), lê o WAV com `plat_file_read_all` (que já desfaz o R1LZ/ADPCM)
  e cria o `IMedia` via `IMediaUtil::CreateMedia(MMD_BUFFER, wavBytes)` — o
  buffer fica em `b.wavBuf` até `DestroySound`. Registra `OnBankMediaNotify`
  e consulta `GetTotalTime`.
- `PlaySound(bank, slot)` usa `MM_PARM_PLAY_REPEAT` (slot 0 = loop infinito),
  `MM_PARM_VOLUME` (millibels → 0..100 via `pow(10, mB/2000)`) e
  `MM_PARM_PAN` (−10000..+10000 → 0..128), com `StopMediaSafe` antes de
  tocar (verifica `GetState( &changing )`).
- `OnRe1AudioSuspend` (registrado em `g_Zeebo.pfnAudioSuspend`) para todos os
  bancos em `EVT_APP_SUSPEND`; `DirectSound::Release` desregistra e libera.
- **Simulador**: `CreateMedia` não é chamado (`#ifndef AEE_SIMULATOR`);
  `GetStatus` usa fallback de duração por tempo. Feito para evitar stalls do
  IMedia.dll do simulador (commits `01d884c`, `ad7a41a`).

## 11. Controle (`input.cpp`)

- Acesso direto a `IHID`/`IHIDDevice`: `GetConnectedDevices(JOYSTICK)`,
  `CreateDevice`, `GetNextButtonEvent` (drena eventos para `s_hidButtonState`),
  `GetPositionState` (25 eixos), `GetMin/MaxPositionInfo`, `GetAxesInfo`.
- UIDs Tectoy (`0x0106c40a..0x0106c40d` = B1..B4, `0x0106c406..409` =
  ZL/L2/ZR/R2, `0x0106c402` = START, `0x0106c403` = BACK,
  `0x0106c3fe..401` = D-Pad) são mapeados para os índices 0..15 do Z-Pad e
  daí para `MARNI_XI_BTN_A/B/X/Y/RB/LTRIGGER/LB/RTRIGGER/...` e bits de
  D-Pad (`0x1/0x2/0x4/0x8`).
- Analógico: centro = `(min+max)/2`, half = `(max-min)/2`, normalizado para
  ±32767 com deadzone configurável (`MARNI_XI_DEFAULT_DEADZONE`). Eixos com
  `min == max` (não mapeados no simulador) retornam 0 — isso corrigia drift
  constante. No simulador o Y é invertido (`INVERT:Y` no `hid_devices.cfg`).
- `MarniXInput::{Init,Poll,IsConnected,SetDeadzone}` e
  `CMarniDirectInput::UpdateAllInputStates` populam `joysticks[0]` com
  `prevPress/currPress/newPress`.
- `Zeebo_OnKeyEvent` hoje retorna FALSE: **não há fallback `EVT_KEY`** (foi
  removido para não conflitar com o IHID). `plat_key_*` existe, mas o pad é
  a única fonte real de input.
- **Combo secreta**: START + D-Pad DOWN alterna o painel de memória
  (`ZeeboMemPanel::Toggle`) — lembre disso ao depurar ou ao testar com usuário.

## 12. FMV (`avi_demux.*`, `cinepak.*`, `video.cpp`)

- Demuxer RIFF/AVI próprio (`AviDemux_Open/NextVideoFrame/...`), sem
  libavformat. Lê `00dc`/`00db`, `01wb`, `strh` (fps, tamanho, áudio).
  Cada chunk pode vir embrulhado em R1LZ: os frames são LZ4 e o áudio é
  ADPCM (`R1OriginalSize` devolve o tamanho original e `ReadChunkDecoded`
  infla um chunk por vez — ver §18). **`AviDemux_ExtractWav` foi removido**:
  montar o WAV PCM inteiro (~20 MB) não cabe nos 32 MB.
- Decoder **Cinepak software** (`cinepak.cpp`) gera RGBA8888 em memória;
  `video.cpp` faz `CreateTexture`/`ReplaceTexture` + `DrawSprite`, respeitando
  o callback de overlay do jogo (`plat_video_set_overlay_callback`) e o
  `playToMs` convertido em frame.
- Áudio: **somente** streaming via `<FILME>.AUDIO` + `ISource`/`AEECLSID_MEDIAPCM`
  (ver §18). Se falhar, o filme roda sem áudio e grava
  `[VID] ERRO: sem audio stream`. Não existe mais fallback de WAV em RAM.
- `plat_video_tick` acompanha `plat_time_ms()` e avança frames; no EOF ou ao
  atingir `playToFrame`, seta `s_endEvent` (`plat_video_take_end_event`).
- O `Zeebo_OnFrameTick` chama `UpdateVideoPlayback()` quando
  `g_bMCINotifyEnabled` — é o caminho que mantém FMV e jogo sincronizados.

## 13. HD textures e fontes: desligadas de propósito

- `hd_textures_stub.cpp` implementa todas as funções de `HdTextures.h` como
  no-op/false; `RE1_NO_HD_TEXTURES=1` reforça o define.
- Motivo: evitar `std::thread`, `std::mutex` e o custo de memória dos HD packs
  num alvo single-threaded com `.bss` limitado.
- `src/game/HdText.cpp` ainda compila, mas sem textura HD o texto usa o font
  bitmap original. **Não** reative `src/system/HdTextures.cpp` no CMake Zeebo
  sem redesenhar o runtime (threading, heap, limites de `.bss`).

## 14. Debug e diagnóstico

- **Zeebx** (emulador): carregar a pasta flat ou o layout SD. Logs de sessões
  antigas ficam em `build/zeebx_*.txt`, `zeebx_out*.txt`, `zeebx_info.ini`.
- **BREW Simulator**: DLL de `build/zeebo_sim`; `SimTrace` e `Zeebo_Log`
  escrevem em `C:\Users\Esther\AppData\Local\Temp\re1_sim.log` (caminho fixo —
  atualize se trocar de máquina).
- **No console**: `Zeebo_Log` usa **apenas** `pfnDbgPrintf` — o arquivo
  `re1_zeebo.log` via `IFileMgr` está **desligado no hardware** (`#ifdef
  AEE_SIMULATOR` em `crash.cpp`) para não deixar vestígio crescente na flash.
  `crashlog_mark("etapa")` deixa breadcrumbs no startup.
- **Painel na tela**: `zeebo_mempanel.cpp` mostra draw calls, triângulos e
  texturas ativas no canto superior esquerdo; toggle com START + ↓.
- **Erros de inicialização**: `plat_fatal` fecha o applet — no hardware, leia
  o `pfnDbgPrintf`; no simulador, `re1_sim.log`.
- Caminhos fixos conhecidos (não portáteis): `ZEEBO_ELF2MOD` no
  `CMakeLists.zeebo.txt`, toolchain e logs do simulador. Ajuste ao migrar.

## 15. Regras de código (herdadas da decomp, obrigatórias)

1. **Fronteira de plataforma**: `src/game/` nunca inclui `<windows.h>` nem
   chama API do SO; tudo passa por `src/platform/platform.h` e pela pasta da
   plataforma. `tests/check_platform_boundary.py` fiscaliza isso. No Zeebo,
   **nenhum tipo BREW/AEE pode vazar** para `src/game/` ou headers públicos de
   `src/marni/` — é por isso que `zeebo_brew.h` vive em `src/platform/zeebo/`.
2. **Lista de fontes**: `CMakeLists.zeebo.txt` é a única source of truth.
   Ao adicionar um `.cpp` do jogo, adicione ali. Não há mais builds Linux/PC
   para sincronizar — não os ressuscite sem uma decisão explícita.
3. **32 bits / ARM**: sem assumptions de 64-bit; `DWORD`/`int` são 32 bits;
   alinhamento de stack 8 bytes na entry de função ARM EABI; sem unaligned
   access ( `-mno-unaligned-access` ).
4. **Engenharia reversa**: todo function/global reescrito deve comentar o
   endereço original (`// 0x004xxxxx`) e, se vier de `Biohazard.exe`, nomear o
   programa no comentário (`// 0x004912C0 (Biohazard.exe)`). Use o MCP do
   Ghidra (programas `ResidentEvil.exe`, `Biohazard.exe`, `SLUS_001.70`,
   `SLUS_005.51`); **se o servidor estiver indisponível, pare e peça ajuda —
   nunca chute código da memória**. Detalhes em `AGENTS_DECOMP.md`.
5. **Globais**: antes de criar uma global, leia `docs/MEMORY_LAYOUT.md` e
   verifique o endereço original. Globais em `0x00be41e0..0x00be9620` precisam
   ser limpos por `ResetGameStateBlock()` (`src/game/GameStart.cpp`); nunca
   dependa de adjacência de linker para `memclr`/`memcpy` ou ponteiros
   past-the-end — modele a faixa explicitamente.
6. **VTables**: as convenções mudam por interface. `CMarniDirect3D` e
   `CMarniBits` usam wrappers `__cdecl` (`self` como primeiro arg);
   `CMarniViewport2` usa `__stdcall` (`RET N`). No Zeebo, as vtables BREW
   `IEGL`/`IGL` **omitem `this`** a partir do slot 3. Um mismatch causa
   Run-Time Check Failure #0 ou crash silencioso em ARM.
7. **Não stubar dependências**: implemente o que for portado; nunca deixe
   função vazia "pra depois" no código do jogo. Exceção documentada e
   intencional: `hd_textures_stub.cpp` (feature desligada no alvo).
8. **Teste**: se você não pode rodar no emulador/console, **não assuma** que a
   mudança funcionou — peça ao usuário para testar e relate o resultado.

## 16. Estado atual e limitações conhecidas

- Build de hardware compila e gera `.mod`; há releases v1.0–v1.2 e dezenas de
  logs do Zeebx — o port já foi iterado no emulador. O **pacote oficial** é
  `dist/zeebo_sd/` (326,40 MB em vez dos 598,45 MB originais) — ver §18.
  Os assets crus do PC **não** são mais mantidos no repositório: para regerar,
  copie `USA/` da instalação GOG/retail para uma pasta temporária.
- O código do jogo é o mesmo do PC: 1800/1801 funções implementadas na base
  (a única pendente é `ent_setanim_walkto_helper`, `0x0040c3d0`), com recursos
  DC/Saturn/OPTIONS já portados no shared source.
- **Não incluídos no Zeebo**: HD textures e HD fonts (por design), backend
  OpenGL 3.3/SDL2 do Linux, áudio IMedia no simulador, VFP/VFPv2 (proibido).
- O comando de uso no topo de `CMakeLists.zeebo.txt` (`-C cmake/zeebo-arm.cmake`
  sem `-DZEEBO=ON`) está desatualizado — use o comando da §3.2 / do README.
- `Zeebo_OnKeyEvent` é no-op: teclado/EVT_KEY não funciona no hardware.
- `plat_display_request_apply` só seta flag; a mudança real acontece no
  próximo `Zeebo_OnFrameTick`.

## 17. Checklists de tarefas comuns

**Adicionar um `.cpp` do jogo**
1. Colocar em `src/game/` (ou subpasta).
2. Adicionar em `CMakeLists.zeebo.txt` (única lista de fontes viva).
3. Se definir global, checar `docs/MEMORY_LAYOUT.md` e `ResetGameStateBlock`.
4. Rodar `python tests/check_platform_boundary.py`.
5. Recompilar `build/zeebo` e, se aplicável, `build/zeebo_sim`.

**Portar feature de `Biohazard.exe` / DC / Saturn**
1. Ler `AGENTS_DECOMP.md` (Ghidra, tabelas, verifiers).
2. Manter comportamento USA como default; comentar endereço + programa.
3. Implementar dependências; rodar o verifier correspondente em `tools/`.
4. Confirmar que não usa `std::thread`/API de desktop no caminho Zeebo.

**Gerar um release**
1. Build Release em `build/zeebo` (confira `elf2mod` no cache do CMake).
2. `python tools/zeebo/package_mod.py --mod ... --assets <USA compactada> --out-dir dist/zeebo_sd`
3. Conferir `mif/<id>.mif`, `mod/<id>/<id>.mod`, `mod/<id>/USA/{data,enemy,movie,sound,stage1..5}`.
4. Testar no Zeebx antes de publicar; só então zipar em `build/`.

## 18. Compressão de assets: R1LZ (LZ4 + IMA ADPCM)

Os assets **não** são mais copiados crus para o pacote. O oficial é
`dist/zeebo_sd/`, gerado pelo `tools/zeebo/re1pack.py`. A árvore original do PC
foi removida do repositório a pedido; para regerar, restaure `USA/` de uma
instalação GOG/retail para uma pasta temporária **fora** de `dist/`.

### Formato R1LZ

Container little-endian com magic `'R1LZ'`, versão 1 e um dos dois modos:

| flags | Conteúdo | Uso |
|---|---|---|
| `1` | bloco **LZ4 cru** + CRC32 | `.rdt`, `.pak`, `.tim`, `.emd`, `.IVM`, frames Cinepak (`00dc`/`00db`) |
| `2` | **IMA ADPCM 4-bit** + CRC32 | todo `.wav` (inclui chunks `smpl` de loop) e chunks de áudio `01wb` dos AVI |

Header fixo de 20 bytes; o modo 2 usa 36 bytes e acrescenta
`sampleRate`, `channels`, `bits`, `pcmDataBytes` e `dataOffset`. Quando
`dataOffset=0` o payload é PCM cru (chunks AVI); quando é 44, o payload começa
com o cabeçalho WAV original — é assim que os loop points sobrevivem.
O CRC é sempre calculado sobre o **reconstruído** (ADPCM é lossy).

### Onde a descompactação acontece (transparente para `src/game/`)

- `platform.cpp::plat_file_read_all` — desembrulha arquivos inteiros;
- `syscalls.cpp::_open/_read/_lseek/_close/_fstat` — FDs `fopen()` servidos de
  RAM (limite `RE1_MEMFILE_MAX = 4 MB`); FMVs grandes continuam em streaming;
- `audio.cpp::CreateSound` — os bancos WAV são lidos via `plat_file_read_all`
  e tocados por `IMedia`; hoje o caminho principal é o `ISource` de PCM cru
  (§18), restando `MMD_BUFFER` apenas como fallback dos bancos;
- `avi_demux.cpp` — cada frame/chunk é inflado na leitura (`R1OriginalSize` e
  `ReadChunkDecoded`), inclusive ADPCM de áudio; a RAM nunca vê o AVI inteiro.

### Renderer / Cinepak

Os frames de vídeo **continuam Cinepak** e são descompactados com LZ4 bloco.
Cinepak já é altamente comprimido (LZ4 só render ~4% nele); reencodar com o
encoder Cinepak do ffmpeg é possível, mas o teste com `pu.avi` levou **882 s
(15 min) para 18% de redução** (52,89 → 43,32 MB com áudio PCM ainda) — 27
filmes levariam horas para um ganho pequeno. Por isso **não** está no pipeline.
O áudio dos filmes é o que renderia muito e agora vai em ADPCM.

### Como regenerar

```powershell
# 1. encoder rápido (necessário para 181 MB de WAV; o fallback Python é lento)
$vc="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "call `"$vc`" >nul && cl /nologo /LD /O2 tools\zeebo\adpcm_fast.c"

# 2. restaure a USA original da GOG para uma pasta temporaria, fora de dist/
#    (os originais nao estao mais no repositorio)
python tools/zeebo/re1pack.py selftest
python tools/zeebo/re1pack.py pack "<temporario>\USA" "<temporario>\USA_r1lz" --verify

# 3. junta tudo em um unico RE1DATA.PAK
python tools/zeebo/make_pak.py "<temporario>\USA_r1lz" "<temporario>\RE1DATA.PAK" --verify

# 4. monta o pacote OFICIAL (3 arquivos, substitui dist/zeebo_sd)
python tools/zeebo/package_mod.py --mod build/zeebo/re1zeebo.mod `
  --pak "<temporario>\RE1DATA.PAK" --out-dir dist\zeebo_sd --layout sd
```

`--verify` checa tamanho, estrutura dos AVI, igualdade dos frames de vídeo e
CRC do PCM decodificado. O decoder C foi validado com 29 vetores reais
(WAV, RDT, TIM e chunks de FMV) via harness MSVC.

### Resultado medido (antes da remoção dos originais)

| Categoria | Original | Compactado | Ganho |
|---|---:|---:|---:|
| Movie (27 AVI) | 241,10 MB | 156,59 MB | 35,1% |
| Voice (563 WAV) | 102,98 MB | 25,79 MB | 75,0% |
| Sound (547 WAV) | 78,01 MB | 28,63 MB | 63,3% |
| Stage1–7 | 136,79 MB | 91,22 MB | 33,3% |
| Demais | 28,63 MB | 13,23 MB | 53,8% |
| **Total da árvore** | **587,51 MB** | **315,46 MB** | **46,3%** |
| **Pacote SD final** | 598,45 MB (2937 arquivos) | **352,22 MB (3 arquivos)** | **41,2%** |

Perda aceita: áudio 4-bit IMA ADPCM (audível e inteligível) e nenhuma perda
em vídeo/imagens/RDT — esses continuam byte-idênticos após a descompactação.

### `RE1DATA.PAK` — arquivo unico (R1PA)

O pacote oficial nao tem mais a pasta `USA/` com 2935 arquivos. Todos eles
estao concatenados em `mod/990001/RE1DATA.PAK` (341,27 MB) com um indice
ordenado de 88 bytes/entrada (~0,26 MB) carregado na RAM no boot. As 27
entradas extras `<FILME>.AUDIO` (o mesmo audio dos AVI em ADPCM puro) servem
para o streaming descrito abaixo.

```
header 32B : 'R1PA' | u16 ver=1 | u16 flags | u32 count | u32 indexOff |
             u32 indexSize | u32 dataOff | u32 r0 | u32 r1
entrada 88B: nome[72] (MAIUSCULAS, '/') | u32 offset | u32 size |
             u32 flags | u32 reserved
```

`src/platform/zeebo/re1_archive.{h,cpp}` abre o PAK em `Zeebo_OnAppStart` e
intercepta tres pontos, todos transparentes para `src/game/`:

| Ponto | O que faz |
|---|---|
| `syscalls.cpp::_open` | `fopen()` somente-leitura passa a vir da RAM (indice + `RE1LZ_Unpack`); escrita continua no filesystem |
| `platform.cpp::plat_file_read_all` | texturas/RDT/modelos vem da entrada do PAK |
| `avi_demux.cpp::ReadAt` | `RE1Arc_ReadAt` faz Seek+Read no mesmo `IFile` aberto, um chunk por vez |

Nomes consultados como `.\usa\MOVIE\PU.avi`, `assets/...`, `USA/...` e
maiúsculas/minúsculas misturadas sao normalizados antes da busca binaria no
indice. `config.ini`, saves, `bonus.dat` e `batrank.dat` **ficam soltos** (fora
do PAK) porque precisam de escrita; `re1_zeebo.log` só existe no simulador.

Plano B se o emulador/hardware nao abrir um arquivo de 341 MB: dividir em
2–3 PAKs por categoria com o mesmo formato — o indice e por arquivo, entao a
mudanca e so no `make_pak.py` e no `RE1Arc_Open`.

### Audio em streaming — somente o necessario na RAM

Os dois picos de memoria antigos (WAV de FMV com ~20 MB de PCM e bancos com
ate 2,78 MB cada) foram eliminados com a API oficial do BREW 4.0.2 confirmada
nos headers do toolset (`AEEISource.h`, `AEEIMedia.h`, `AEEMediaFormats.h`):

- `src/platform/zeebo/pcm_source.{h,cpp}` implementa um **`ISource`** de PCM
  cru (5 slots: `AddRef/Release/QueryInterface/Read/Readable`). O `Read()`
  decodifica IMA ADPCM **sob demanda** direto da entrada do PAK, com duas
  janelas de nibbles de 4 KB por canal. `Readable()` usa
  `IShell::Resume(pcb)` — o callback e' agendado pelo shell.
- `AEECLSID_MEDIAPCM` (`0x01005511`) e' criado com `ISHELL_CreateInstance`;
  a fonte e' passada por `SetMediaParm(m, MM_PARM_MEDIA_DATA, &mdEx, 1)` com
  `mdEx.clsData = MMD_ISOURCE (0x01001012)`, `bRaw = TRUE` e
  `AEEMediaWaveSpec` (taxa/canais/16-bit). Isso e' o
  `IMedia_SetMediaDataEx()` dos headers, sem funcao nova na vtable.
- **Bancos** (`audio.cpp::CreateSound`): tentam streaming primeiro; se
  `CreateMedia`/`SetMediaParm` falhar, caem para o `MMD_BUFFER` antigo.
  `PlaySound` chama `RE1PcmSource_Reset()` antes de tocar de novo, e
  `GetStatus` controla duracao/fim e loop por tempo (PCM cru nao tem
  `MM_STATUS_DONE`).
- **FMVs** (`video.cpp::StartVideoAudio`): usam obrigatoriamente a entrada
  `<filme>.AUDIO` e o mesmo `ISource`; `fromMs` e' aplicado descartando PCM do
  inicio. **Nao ha fallback de WAV em RAM** — `AviDemux_ExtractWav` foi
  removido; se o stream falhar, o filme continua sem audio.

RAM por voz ativa caiu de ate 2,78 MB para **~8 KB**; o pico do `pu.avi` caiu
de **~19,9 MB de PCM para ~8 KB**, mantendo os 341 MB do PAK no SD.
O decoder incremental foi validado com harness MSVC: PCM gerado identico ao
`RE1LZ_Unpack` completo, tanto para um WAV de efeito quanto para o audio de
`PU.AVI` (19.915.936 bytes comparados byte a byte).

## 19. Referências

- `README.md` — instalação, controles Z-Pad, comandos rápidos.
- `AGENTS_DECOMP.md` — guia completo da decompilação PC/ Ghidra / ferramentas / DC / Saturn / JPN / Linux.
- `docs/` — `MEMORY_LAYOUT.md`, `MARNI_SYSTEM.md`,
  `CLASSES_AND_VTABLES.md`, `TASK_SCHEDULER.md`, `HD_TEXTURES.md`,
  `SATURN_VERSION_ANALYSIS.md`, etc.
- `tools/zeebo/` — `elf2mod.x`, `zeebo_mod.ld`, `re1pack.py`, `make_pak.py`,
  `adpcm_fast.c`, `package_mod.py`.
- `src/platform/zeebo/zeebo_brew.h` — todas as estruturas/CLSIDs/constantes BREW usadas pelo port.

## 20. Armazenamento e regra da NAND (LEIA ANTES DE INSTALAR)

O Zeebo tem **duas** memórias flash internas — confundi-las é o erro clássico
que "ferra com tudo":

| Memória | Tamanho | Conteúdo |
|---|---|---|
| NAND (MCP com a SDRAM) | 128 MB | bootloaders, BREW/Rex/L4, sistema, logs do sistema, **Z-Wheel** e assets. Livre: 30–60 MB. |
| eNAND (eMMC, montada em `/mmc4/`) | 1 GB | jogos/apps (`mif/`, `mod/<id>/`) e **saves** (`mod/<id>/udata/`). |
| SD | ≤32 GB | slot nunca usado oficialmente; só EMAPPLET/Media Player. |

**Regra de ouro:** este pacote tem 352 MB e **não cabe na NAND de 128 MB**.
Instale com `mif/`+`mod/` no SD via **EMAPPLET → Field Test → Memory Copy**
(destino: eNAND) e abra pelo **Appmgr**. **Nunca coloque na Z-Wheel**: o
**ZeeboMCP** copia o jogo da eNAND para a NAND antes de rodar e, se não
conseguir mover a cópia de volta ao sair, deixa `zmcp.dat` + parte do jogo na
NAND, esgotando o espaço e causando instabilidade. Nosso desligamento é limpo
(`ISHELL_CloseApplet` → `EVT_APP_STOP` → `Zeebo_OnAppStop`), o que ajuda o
ZeeboMCP a limpar; um crash ainda pode deixar vestígio — isso é do sistema.

**Escritas do port** (todas relativas, resolvidas em `/mod/990001/` na eNAND):

| Arquivo | Tamanho | Caminho no código |
|---|---|---|
| `SAVE/savedat1..16.dat` | 2690 B × 16 | `SaveLoadScreen.cpp:435` (`fopen "wb"`) → `_open` → `IFile::Write` |
| `SAVE/bonus.dat` | 32 B | `Bonus.cpp:89` (`plat_file_write`) |
| `SAVE/batrank.dat` | ~1 KB | `SaturnBattle.cpp:1132` |
| `config.ini` | ~alguns KB | `ConfigFile.cpp:670` (`fopen "w"`), no shutdown |
| `re1_zeebo.log` | **nenhum no hardware** | `crash.cpp` — `#ifdef AEE_SIMULATOR` |

`RE1DATA.PAK` é aberto com `_OFM_READ` (`re1_archive.cpp:105`) e **jamais**
escrito. `plat_write_path_safe` barra qualquer escrita que tente sair da pasta
do app (absoluto, `fs:`, drive, `..`). O guia oficial limita saves a 64 KB;
nosso pior caso normal é ~45 KB.

Fontes: `https://www.tripleoxygen.net/wiki/console/zeebo/nand`,
`.../sistema_de_arquivos`, `.../hardware/enand` e o `ZeeboDeveloperGuide0.97.pdf`
(§5.1 — filesystem de 1 GB, máximo 64 KB de save).
