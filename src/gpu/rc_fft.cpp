#include "mod_pme.h"
#include "util_rt.h"

TINKER_NAMESPACE_BEGIN
std::vector<fft_plan_t>& fft_plans() {
  static std::vector<fft_plan_t> objs;
  return objs;
}
TINKER_NAMESPACE_END
