cmake_minimum_required(VERSION 3.16)

# Pull in SDK (must be before project)
include(cmake/pico_sdk_import.cmake)

project(applevga)
set(CMAKE_C_STANDARD 11)

set(COMPUTER_MODEL "IIPLUS" CACHE STRING "The model of Computer to support")
message("Target Computer model is ${COMPUTER_MODEL}")
add_definitions(-DCOMPUTER_MODEL_${COMPUTER_MODEL}=1)

pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DCONFIG_SYSCLOCK=252 -DPICO_FLASH_SPI_CLKDIV=8")

add_compile_options(-Wall -Wextra -Wnull-dereference -Wno-unused-parameter -Wno-unused-function)


add_executable(applevga)
pico_set_binary_type(applevga copy_to_ram)
pico_set_linker_script(applevga ${CMAKE_CURRENT_SOURCE_DIR}/memmap_copy_to_ram_custom.ld)

pico_generate_pio_header(applevga
    ${CMAKE_CURRENT_SOURCE_DIR}/vga.pio)
pico_generate_pio_header(applevga
    ${CMAKE_CURRENT_SOURCE_DIR}/abus.pio)

file(GLOB TEXTFONT_SRC_FILES textfont/*.c)
target_sources(applevga PUBLIC
    main.c
    abus.c
    buffers.c
    colors.c
    config.c
    render.c
    render_testpat.c
    ${TEXTFONT_SRC_FILES}
    vga.c
    v9938.c
)
target_include_directories(applevga PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(applevga PUBLIC
    pico_multicore
    pico_stdlib
    hardware_dma
    hardware_flash
    hardware_pio
)

pico_add_extra_outputs(applevga)
