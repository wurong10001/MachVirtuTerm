# VirtuTerm - HP700 (Mach kernel) VM for Android
# https://d.android.com/studio/projects/add-native-code.html
# https://github.com/android/ndk-samples

cmake_minimum_required(VERSION 3.22.1)
project("VirtuTerm")

# ---------------------------------------------------------------------------
# Mach kernel sources are ANSI C (C89).  MACH_KERNEL (which makes the
# device-layer headers pick the kernel-side definitions) is applied per
# library via target_compile_definitions, NOT globally, so the C++ bridge
# stays on plain Bionic headers.
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Layout under app/src/main/cpp:
#   src/jni/            JNI bridge (C++), kept free of Mach headers
#   src/kernel/         Mach kernel source tree (copied verbatim from its repo)
#   src/compat/         android_compat adaptation headers
set(KERNEL_ROOT   ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel/src)
set(MACH_KERNEL   ${KERNEL_ROOT}/mach_kernel)
set(HP700_DIR     ${MACH_KERNEL}/hp_pa/HP700)
set(COMPAT_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/android_compat)

# ---------------------------------------------------------------------------
# HP700 Mach kernel device layer.
# The original kernel sources are compiled unmodified; all environment
# differences (types.h, machine/*.h, config-generated headers, ...) are
# absorbed by the android_compat/ adaptation layer.
#
# This is a STATIC library (not INTERFACE) on purpose: the 1990s sources
# need flags like -fcommon that must be applied directly to their own C
# compilation.  COMPILE_LANGUAGE generator expressions do NOT evaluate
# correctly when propagated through INTERFACE properties, so gating them
# there silently dropped -fcommon in some build paths (Android Studio).
# ---------------------------------------------------------------------------
add_library(hp700_sources STATIC
        ${HP700_DIR}/autoconf.c
        ${HP700_DIR}/busconf.c
        ${HP700_DIR}/cons_conf.c
        ${HP700_DIR}/glue.c
        ${HP700_DIR}/grf.c
        ${HP700_DIR}/grf_conf.c
        ${HP700_DIR}/grf_machdep.c
        ${HP700_DIR}/grf_sti.c
        ${HP700_DIR}/ite.c
        ${HP700_DIR}/ite_sti.c
        ${HP700_DIR}/dca.c
        ${HP700_DIR}/gkd.c
        ${HP700_DIR}/hil.c
        ${HP700_DIR}/eisa_common.c
        ${HP700_DIR}/if_i596.c
        ${HP700_DIR}/if_i596kgdb.c
        ${HP700_DIR}/hpux_label.c
        ${HP700_DIR}/kgdb_support.c
        ${HP700_DIR}/gkd_keymaps.c
        ${HP700_DIR}/hil_keymaps.c
)
# Mach include tree + android_compat are needed ONLY by the HP700 C sources.
# PRIVATE keeps them away from the C++ bridge entirely (no generator
# expression needed, no risk of hijacking Bionic headers in C++).
target_include_directories(hp700_sources PRIVATE
        ${COMPAT_DIR}   # android_compat (types.h, machine/, config shims)
        ${MACH_KERNEL}  # hp_pa/, device/, kern/, vm/, mach/, ...
        ${KERNEL_ROOT}  # xkern/ etc.
)
target_compile_definitions(hp700_sources PRIVATE MACH_KERNEL)

# The 1990s kernel code trips many modern Clang diagnostics.  Keep the
# original sources byte-for-byte and tolerate their idioms for now.
target_compile_options(hp700_sources PRIVATE
        # The 1990s sources hand-declare libc functions (e.g. grf_sti.c:
        # "unsigned long strlen(...)").  NDK enables -D_FORTIFY_SOURCE=2,
        # which turns strlen into an always_inline *definition* -> hard
        # "conflicting types" error on 32-bit (size_t != unsigned long).
        # Disabling fortify restores a plain declaration, where the
        # mismatch becomes a warning suppressed by
        # -Wno-incompatible-library-redeclaration below.
        -U_FORTIFY_SOURCE
        # Mach 3.0 defines globals directly in headers (master_cpu,
        # vm_page_queue_lock, kbd_keymap, ...) and relies on the classic C
        # common-symbol merging; clang >= 19 defaults to -fno-common.
        -fcommon
        -Wno-switch
        -Wno-parentheses
        -Wno-unused-comparison
        -Wno-int-to-pointer-cast
        -Wno-implicit-function-declaration
        -Wno-implicit-int
        -Wno-int-conversion
        -Wno-pointer-to-int-cast
        -Wno-macro-redefined
        -Wno-incompatible-library-redeclaration
        -Wno-typedef-redefinition
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-parameter
        -Wno-dangling-else
        -Wno-bool-conversion
        -Wno-comment
        -Wno-asm-operand-widths
        -Wno-unused-but-set-variable
        -Wno-unknown-pragmas
        -Wno-deprecated-declarations
)

# ---------------------------------------------------------------------------
# Main shared library: JNI bridge + HP700 kernel sources.
# The Java layer loads this library as "VirtuTerm".
# ---------------------------------------------------------------------------
add_library(${CMAKE_PROJECT_NAME} SHARED
        src/jni/native-lib.cpp
        src/jni/jni_bridge.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}
        PRIVATE
        hp700_sources
        android
        log)

message(STATUS "VirtuTerm: HP700 kernel sources under ${HP700_DIR}")
