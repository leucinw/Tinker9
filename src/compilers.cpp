#include "tool/compilers.h"
#include "tool/io_print.h"


namespace tinker {
std::string cxx_compiler_name()
{
   std::string n = "unknown";
#if defined(TINKER_ICPC)
   n = format("icpc %d.%d", __INTEL_COMPILER, __INTEL_COMPILER_BUILD_DATE);

#elif defined(TINKER_PGI)
   if (__PGIC__ <= 19)
      n = format("pgc++ %d.%d.%d", __PGIC__, __PGIC_MINOR__,
                 __PGIC_PATCHLEVEL__);
   else
      n = format("nvc++ %d.%d.%d", __PGIC__, __PGIC_MINOR__,
                 __PGIC_PATCHLEVEL__);

#elif defined(TINKER_APPLE_CLANG)
   n = format("clang++ %d.%d.%d (xcode)", __clang_major__, __clang_minor__,
              __clang_patchlevel__);

#elif defined(TINKER_LLVM_CLANG)
   n = format("clang++ %d.%d.%d (llvm)", __clang_major__, __clang_minor__,
              __clang_patchlevel__);

#elif defined(TINKER_GCC)
   n = format("g++ %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);

#endif
   return n;
}
}
