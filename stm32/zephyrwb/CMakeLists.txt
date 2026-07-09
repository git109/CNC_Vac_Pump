# CNC Vacuum Pump Controller — Zephyr application (STM32WB55 / P-NUCLEO-WB55)
# Build against the existing workspace at /Users/robert/Documents/dev-env/zephyresp32s3
#
#   cd /Users/robert/Documents/dev-env/zephyresp32s3
#   source .venv/bin/activate
#   west build -b nucleo_wb55rg /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/stm32/zephyrwb
#   west flash        # single ST-LINK USB cable does flash + console
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cnc_vac_pump_wb)

target_sources(app PRIVATE
  src/main.c
  src/vac_filter.c
  src/encoder.c
)

# --- SquareLine UI (LVGL 9 re-export) -------------------------------------
# Exported UI in src/ui/. The SquareLine-generated src/ui/CMakeLists.txt (an
# ESP-IDF component file) is ignored; we just compile the generated .c files.
file(GLOB_RECURSE UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.c)
target_sources(app PRIVATE ${UI_SOURCES})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
