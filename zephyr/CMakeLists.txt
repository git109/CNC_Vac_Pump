# CNC Vacuum Pump Controller — Zephyr application
# Build against your existing workspace at /Users/robert/Documents/dev-env/zephyresp32s3
#
#   cd /Users/robert/Documents/dev-env/zephyresp32s3
#   source .venv/bin/activate
#   west build -b esp32s3_devkitc/esp32s3/procpu /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/zephyr
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cnc_vac_pump)

target_sources(app PRIVATE
  src/main.c
  src/vac_filter.c
  src/encoder.c
)

# --- SquareLine UI (LVGL 9 re-export) -------------------------------------
# After you re-export source/SquareLine_VacDisp for LVGL 9 into src/ui/,
# uncomment the two lines below and replace ui_placeholder() in main.c with
# ui_init().
#
# file(GLOB_RECURSE UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.c)
# target_sources(app PRIVATE ${UI_SOURCES})
