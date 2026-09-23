cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME gogufw)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

find_package(Git QUIET)
set(BUILD_COMMIT "source-zip")
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT BUILD_COMMIT)
        set(BUILD_COMMIT "source-zip")
    endif()
endif()

if(ENABLE_FEAT_F4HWN)
    if(NOT AUTHOR_STRING_1)
        set(AUTHOR_STRING_1 "EGZUMER")
    endif()
    if(NOT AUTHOR_STRING_2)
        set(AUTHOR_STRING_2 "GOGUFW")
    endif()
    if(NOT VERSION_STRING_1)
        set(VERSION_STRING_1 "v0.22")
    endif()
    if(NOT VERSION_STRING_2)
        set(VERSION_STRING_2 "v2.0.6")
    endif()
    if(NOT EDITION_STRING)
        set(EDITION_STRING "GGFW")
    endif()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "${AUTHOR_STRING_1}+${AUTHOR_STRING_2}")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING ${VERSION_STRING_2})
    endif()
else()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "EGZUMER")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING "NOGIT")
    endif()
endif()

if(TARGET)
    set(EXE_NAME ${TARGET})
else()
    set(EXE_NAME "firmware")
endif()

message("EXE_NAME = " ${EXE_NAME})

add_executable(${EXE_NAME})

add_subdirectory(Drivers)
add_subdirectory(Middlewares)
add_subdirectory(Core)
add_subdirectory(App)

# mb_flash.c contains only the RAM copier; mb_flash_logic.c remains LTO-enabled.
# The internal application Flash is unavailable while the multiboot restore
# stub runs. Keep it as a concrete non-LTO object so its RAM-only
# branches and references can be audited after linking.
if(ENABLE_FEAT_F4HWN_MULTIBOOT_OVERLAY)
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/App/driver/mb_flash.c"
        PROPERTIES COMPILE_OPTIONS "-fno-lto"
    )
endif()

target_link_libraries(${EXE_NAME} Core App)

target_compile_definitions(${EXE_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_link_libraries(${EXE_NAME} ${TOOLCHAIN_LINK_LIBRARIES})

if(ENABLE_LTO)
    target_compile_options(${EXE_NAME} PRIVATE -flto=auto)
endif()

# Generate map file
target_link_options(${EXE_NAME} PRIVATE "-Wl,-Map=${EXE_NAME}.map")

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${EXE_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${EXE_NAME}.map)

# Force linker to ignore RWX segment warning even if cache exists
target_link_options(${EXE_NAME} PRIVATE -Wl,--no-warn-rwx-segment)

# -----------------------------------
#  Post build processing
#

# Reject a build if the restore stub reaches outside its RAM section. Such a
# reference would become unreadable as soon as internal Flash erase begins.
if(ENABLE_FEAT_F4HWN_MULTIBOOT_OVERLAY)
    add_custom_command(
        TARGET ${EXE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                "-DELF=$<TARGET_FILE:${EXE_NAME}>"
                "-DMAP=${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME}.map"
                "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/check_mb_ramfunc.cmake"
        COMMENT "Checking multiboot RAM stub isolation"
        VERBATIM
    )
endif()

# Generate .bin file
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXE_NAME}.elf ${EXE_NAME}.bin
    COMMENT "Generating .bin file"
)

# Keep the normal target-named binary for direct flashing and also emit the
# canonical F4HWN multiboot/release filename from the exact same image.
if(ENABLE_FEAT_F4HWN AND ENABLE_FEAT_F4HWN_MULTIBOOT)
    set(MULTIBOOT_BIN "f4hwn.gogufw.${VERSION_STRING_2}.bin")
    add_custom_command(
        TARGET ${EXE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME}.bin"
                "${CMAKE_CURRENT_BINARY_DIR}/${MULTIBOOT_BIN}"
        COMMENT "Generating multiboot-compatible ${MULTIBOOT_BIN}"
        VERBATIM
    )
endif()

# Generate .hex file
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXE_NAME}.elf ${EXE_NAME}.hex
    COMMENT "Generating .hex file"
)

# Pack
# TODO: Incorparate venv?
# if(ENABLE_FEAT_F4HWN)
#     add_custom_command(
#         TARGET ${EXE_NAME}
#         POST_BUILD
#         COMMAND python ${CMAKE_SOURCE_DIR}/fw-pack.py ${TARGET_ELF} ${AUTHOR_STRING_2} ${VERSION_STRING_2} ${EXE_NAME}.packed.bin
#         COMMENT "Generating packed binary"
#     )
# else()
#     add_custom_command(
#         TARGET ${EXE_NAME}
#         POST_BUILD
#         COMMAND python ${CMAKE_SOURCE_DIR}/fw-pack.py ${TARGET_ELF} ${AUTHOR_STRING} ${VERSION_STRING} ${EXE_NAME}.packed.bin
#         COMMENT "Generating packed binary"
#     )
# endif()
