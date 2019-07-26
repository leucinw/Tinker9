#ifndef TINKER_TEST_RT_H_
#define TINKER_TEST_RT_H_

#include "util_rc_man.h"
#include <string>

TINKER_NAMESPACE_BEGIN
namespace test {
/**
 * @brief
 * Write a file in the current working directory in its constructor and remove
 * this file in its destructor.
 */
class file {
private:
  bool good_;
  std::string name_;

public:
  file(const std::string& name, const std::string& content);
  ~file();
};

class file_expected {
private:
  std::string name_;

public:
  file_expected(const std::string& name);
  ~file_expected();
};

double test_get_eps2(double eps_single, double eps_double);

void test_begin_1_xyz(int argc, const char** argv);
void test_end();
void test_mdinit(double t = 0, double atm = 0);
}
TINKER_NAMESPACE_END

#endif
