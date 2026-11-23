# CMakeLists.txt - Ponto de entrada do build Zeebo / BREW 4.0.2.
#
# Este repositório só tem dois alvos:
#   * hardware / zeebx-emu (cross ARM, re1zeebo.mod)  -> padrão
#   * BREW Simulator (DLL x86, -DZEEBO_SIM=ON)
#
# Todo o trabalho está em CMakeLists.zeebo.txt. A opção ZEEBO existe para
# compatibilidade com os comandos já documentados (-DZEEBO=ON) e com scripts
# antigos; hoje o único valor aceito é ON.
#
# Build (Windows, com a toolchain ARM no PATH):
#   cmake -S . -B build/zeebo -G Ninja -DZEEBO=ON \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/zeebo-arm.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/zeebo -j 8
#
# Simulador BREW (MSVC):
#   cmake -S . -B build/zeebo_sim -DZEEBO=ON -DZEEBO_SIM=ON
#   cmake --build build/zeebo_sim
cmake_minimum_required(VERSION 3.16)
project(residentevil_zeebo C CXX ASM)

option(ZEEBO "Build for Zeebo / Qualcomm BREW 4.0.2" ON)
if(NOT ZEEBO)
    message(FATAL_ERROR
        "Este repositório só suporta o build Zeebo (-DZEEBO=ON).\n"
        "Os builds Linux e Windows PC foram removidos; use CMakeLists.zeebo.txt.")
endif()

include(CMakeLists.zeebo.txt)
