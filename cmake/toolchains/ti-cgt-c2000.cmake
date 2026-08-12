# TI C2000 (cl2000) toolchain file.
#
#   cmake --preset c2000
#   cmake -B build/c2000 --toolchain cmake/toolchains/ti-cgt-c2000.cmake
#
# CHAR_BIT is 16 here, so there is no uint8_t: the library uses atc_menu_u8
# (unsigned char) throughout, and a port must mask every outgoing byte with
# & 0xFF — only the low half of a char belongs on the wire.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR C28x)

include("${CMAKE_CURRENT_LIST_DIR}/ti-cgt-common.cmake")

ti_cgt_resolve(cl2000)

set(CMAKE_C_COMPILER "${TI_CGT_ROOT}/bin/cl2000")
set(CMAKE_AR         "${TI_CGT_ROOT}/bin/ar2000" CACHE FILEPATH "TI C2000 archiver")

# Library-only toolchain: linking needs an application-provided linker command file.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The one setting here with a consequence beyond this build: a static library
# carries its codegen choices as build attributes, and the linker rejects an
# archive whose attributes disagree with the application's. The default mirrors
# the CCS example projects — override the whole string if yours differ. The menu
# uses no floating point, but --float_support still has to match: it is part of
# the attribute set the linker compares.
set(TI_CGT_ABI_FLAGS
    "-v28 -ml -mt --abi=eabi --float_support=fpu32 --cla_support=cla1 --vcu_support=vcu2 --tmu_support=tmu0"
    CACHE STRING "cl2000 target/ABI flags; must match the application's")

set(CMAKE_C_FLAGS_INIT
    "${TI_CGT_ABI_FLAGS} --display_error_number --diag_warning=225 --diag_wrap=off \
--advice:performance=none -I\"${TI_CGT_ROOT}/include\"")

# Empty on purpose: optimization is a per-build choice, and CMAKE_BUILD_TYPE's
# slots would otherwise append a second --opt_level behind whatever was passed.
set(CMAKE_C_FLAGS_DEBUG_INIT   "")
set(CMAKE_C_FLAGS_RELEASE_INIT "")
