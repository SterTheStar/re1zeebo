<div align="center">

# Resident Evil 1 Zeebo

Port nativo de **Resident Evil 1 (PC)** para **Zeebo (Qualcomm BREW 4.0.2)**

![Platform](https://img.shields.io/badge/Plataforma-Zeebo%20(BREW%204.0.2)-0066cc?style=flat-square)
![Arch](https://img.shields.io/badge/CPU-ARMv6K%20%2B%20VFPv2-1f6feb?style=flat-square)
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
| **Desempenho** | Build `-O2` com hardware VFPv2 (`softfp`), cache de estado GLES1 e buffers reutilizáveis para modelos 3D |
| **Cutscenes (FMV)** | Demuxer AVI em streaming, decodificador Cinepak e faixa de áudio PCM reproduzida via `IMedia` |
| **Áudio e Controles** | Bancos WAV via `IMediaUtil` e suporte nativo ao Z-Pad via `IHID` (com fallback de teclado via `EVT_KEY`) |

---

## Como preparar os assets

Você precisa dos arquivos da versão de PC (por exemplo, a edição da **GOG**). Localize a pasta **`USA/`** da instalação original e coloque-a dentro da pasta do módulo ao lado de `re1zeebo.mod` e `re1zeebo.mif`.

Estrutura final da pasta descompactada (ex.: `re1zeebo/`): `re1zeebo.mod`, `re1zeebo.mif` e a pasta `USA/` (com `data/`, `enemy/`, `movie/`, `sound/`, `stage1/`..`stage5/`, etc.).

Para montar a pasta automaticamente após compilar:

```powershell
python tools/zeebo/package_mod.py --mod build/zeebo/re1zeebo.mod --assets "C:\GOG Games\Resident Evil\USA" --out-dir dist/zeebo/re1zeebo
```

---

## Como compilar e rodar

Requisitos: **Arm GNU Toolchain** (`arm-none-eabi-gcc`), **CMake** (>= 3.16), **Ninja** e **Python 3**.

```powershell
cmake -S . -B build/zeebo -G Ninja -DZEEBO=ON -DCMAKE_TOOLCHAIN_FILE=cmake/zeebo-arm.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/zeebo -j 8
```

O binário final será gerado em `build/zeebo/re1zeebo.mod`. Depois de copiar a pasta `USA/` para `dist/zeebo/re1zeebo/`, basta carregar a pasta no **Zeebx** ou no console.

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
