# Vendored quickjs-ng v0.14.0 (MIT) - library target only (no CLI, no libc modules).
# Upstream exposes lib target `qjs`; trimmed to the core interpreter (dtoa,
# libregexp, libunicode, quickjs) which is all far2l needs to drive mermaid.
cmake_minimum_required(VERSION 3.10)

add_library(qjs STATIC
    dtoa.c
    libregexp.c
    libunicode.c
    quickjs.c
)

target_compile_definitions(qjs PRIVATE
    _GNU_SOURCE
    QUICKJS_NG_BUILD
)

target_include_directories(qjs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(qjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME qjs
    VERSION 1.0.0
    SOVERSION 1
    MACOSX_RPATH TRUE
    INSTALL_RPATH "@loader_path"
)

# Flags quickjs relies on: unsigned char is assumed by the interpreter.
include(CheckCCompilerFlag)
check_c_compiler_flag(-funsigned-char QJS_SUPPORTS_UNSIGNED_CHAR)
if(QJS_SUPPORTS_UNSIGNED_CHAR)
    target_compile_options(qjs PRIVATE -funsigned-char)
endif()
foreach(_qjs_flag -Wno-implicit-fallthrough -Wno-sign-compare
        -Wno-missing-field-initializers -Wno-unused-parameter
        -Wno-unused-but-set-variable -Wno-unused-result -Wno-stringop-truncation)
    string(REPLACE "-" "" _qjs_flagname ${_qjs_flag})
    check_c_compiler_flag(${_qjs_flag} QJS_SUPPORTS_${_qjs_flagname})
    if(QJS_SUPPORTS_${_qjs_flagname})
        target_compile_options(qjs PRIVATE ${_qjs_flag})
    endif()
endforeach()

find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(qjs PUBLIC Threads::Threads)
endif()
if(NOT APPLE)
    target_link_libraries(qjs PUBLIC ${CMAKE_DL_LIBS})
endif()
find_library(QJS_M_LIBRARIES m)
if(QJS_M_LIBRARIES)
    target_link_libraries(qjs PUBLIC m)
endif()
