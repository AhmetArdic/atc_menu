# TI MSP430 (cl430) toolchain file.
#
#   cmake --preset msp430
#   cmake -B build/msp430 --toolchain cmake/toolchains/ti-cgt-msp430.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR MSP430)

include("${CMAKE_CURRENT_LIST_DIR}/ti-cgt-common.cmake")

ti_cgt_resolve(cl430)

set(CMAKE_C_COMPILER "${TI_CGT_ROOT}/bin/cl430")
set(CMAKE_AR         "${TI_CGT_ROOT}/bin/ar430" CACHE FILEPATH "TI MSP430 archiver")

# Library-only toolchain: linking needs an application-provided linker command file.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The one setting here with a consequence beyond this build: a static library
# carries its codegen choices as build attributes, and the linker rejects an
# archive whose attributes disagree with the application's. The default mirrors
# the CCS example projects — override the whole string if yours differ.
set(TI_CGT_ABI_FLAGS
    "-vmspx --abi=eabi --data_model=restricted --use_hw_mpy=F5"
    CACHE STRING "cl430 target/ABI flags; must match the application's")

set(CMAKE_C_FLAGS_INIT
    "${TI_CGT_ABI_FLAGS} --display_error_number --diag_warning=225 --diag_wrap=off \
-I\"${TI_CGT_ROOT}/include\"")

# Empty on purpose: optimization is a per-build choice, and CMAKE_BUILD_TYPE's
# slots would otherwise append a second --opt_level behind whatever was passed.
set(CMAKE_C_FLAGS_DEBUG_INIT   "")
set(CMAKE_C_FLAGS_RELEASE_INIT "")
