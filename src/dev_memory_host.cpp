#include "macro.h"

#ifdef TINKER_HOST
#  include "macro.h"
#  include <cstdlib>
#  include <cstring>

TINKER_NAMESPACE_BEGIN
void device_memory_copyin_bytes(void* dst, const void* src, size_t nbytes) {
  std::memcpy(dst, src, nbytes);
}

void device_memory_copyout_bytes(void* dst, const void* src, size_t nbytes) {
  std::memcpy(dst, src, nbytes);
}

void device_memory_copy_bytes(void* dst, const void* src, size_t nbytes) {
  std::memcpy(dst, src, nbytes);
}

void device_memory_zero_bytes(void* dst, size_t nbytes) {
  std::memset(dst, 0, nbytes);
}

void device_memory_deallocate_bytes(void* ptr) { std::free(ptr); }

void device_memory_allocate_bytes(void** pptr, size_t nbytes) {
  *pptr = std::malloc(nbytes);
}
TINKER_NAMESPACE_END
#endif