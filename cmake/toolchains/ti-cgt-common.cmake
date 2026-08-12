# Shared TI codegen-tools (CGT) discovery for the toolchain files beside this one.
#
# No install path is baked in: where the compiler lives is a property of the
# machine, not of this repository. ti_cgt_resolve() tries, in order:
#
#   1. -DTI_CGT_ROOT=<path>      explicit, and what a cmake-gui entry writes
#   2. TI_CGT_ROOT in the env    a machine default
#   3. the compiler on PATH      how a CI image usually has it
#
# The environment is the one layer shared across build directories, so it can
# stand for one target at a time. Configuring the other one with it set fails
# loudly — "no bin/cl2000 under <the msp430 path>" — and -DTI_CGT_ROOT on that
# build directory is the fix, once: the cache keeps it.

# ti_cgt_resolve(<compiler exe>)
#
# Leaves TI_CGT_ROOT holding a directory whose bin/<exe> exists, or stops with a
# message naming every way to point at one. A macro, not a function: it writes
# TI_CGT_ROOT in the caller's scope.
macro(ti_cgt_resolve _exe)
    if(NOT TI_CGT_ROOT AND DEFINED ENV{TI_CGT_ROOT})
        set(TI_CGT_ROOT "$ENV{TI_CGT_ROOT}")
    endif()

    if(NOT TI_CGT_ROOT)
        find_program(_ti_cgt_exe_path "${_exe}")
        if(_ti_cgt_exe_path)
            get_filename_component(_ti_cgt_bin "${_ti_cgt_exe_path}" DIRECTORY)
            get_filename_component(TI_CGT_ROOT "${_ti_cgt_bin}" DIRECTORY)
        endif()
        # find_program caches; drop it so a changed PATH is seen next configure.
        unset(_ti_cgt_exe_path CACHE)
    endif()

    set(TI_CGT_ROOT "${TI_CGT_ROOT}" CACHE STRING
        "TI codegen tools root — the directory holding bin/${_exe}")

    if(NOT EXISTS "${TI_CGT_ROOT}/bin/${_exe}")
        if(TI_CGT_ROOT)
            set(_ti_cgt_why "no bin/${_exe} under '${TI_CGT_ROOT}'")
        else()
            set(_ti_cgt_why "${_exe} was not found")
        endif()
        message(FATAL_ERROR
            "${_ti_cgt_why}.\n"
            "Point at the codegen tools in any one of these ways:\n"
            "  -DTI_CGT_ROOT=<path>     (or fill it in from cmake-gui/ccmake)\n"
            "  export TI_CGT_ROOT=<path>\n"
            "  put ${_exe} on PATH")
    endif()

    # A try_compile re-reads this toolchain file with a cache of its own, where
    # none of the above is visible — hand the resolved root through, or the
    # compiler check fails on its own.
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TI_CGT_ROOT)

    unset(_ti_cgt_bin)
    unset(_ti_cgt_why)
endmacro()
