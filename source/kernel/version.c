#include "version.h"

/* Kernel name */
const char * __kernel_name = "butterfly";
const char * __kernel_version = "1.9.1";

/*
 * The release codename.
 */
const char * __kernel_version_codename = "bluewings";

/* Build architecture (should probably not be
 * here as a string, but rather some sort of
 * preprocessor macro, or pulled from a script) */
const char * __kernel_arch = "i686";

/* Rebuild from clean to reset these. */
const char * __kernel_build_date = __DATE__;
const char * __kernel_build_time = __TIME__;
