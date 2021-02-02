#include "mathfunc.h"
#include "test.h"
using namespace tinker;


TEST_CASE("MathFuncPow2", "[util][math]")
{
   SECTION("IsPow2")
   {
      auto f = is_pow2;

      REQUIRE(f(0) == false);
      REQUIRE(f(1) == true);
      REQUIRE(f(2) == true);
      REQUIRE(f(3) == false);
      REQUIRE(f(4) == true);
      REQUIRE(f(5) == false);
      REQUIRE(f(6) == false);
      REQUIRE(f(7) == false);
      REQUIRE(f(8) == true);
   }


   SECTION("Pow2")
   {
      auto f = pow2;
      auto g = pow2ll;

      REQUIRE(f(0) == 0x00000001);
      REQUIRE(f(1) == 0x00000002);
      REQUIRE(f(2) == 0x00000004);
      REQUIRE(f(30) == 0x40000000);
      REQUIRE((unsigned int)f(31) == 0x80000000);

      REQUIRE(g(0) == 0x00000001);
      REQUIRE(g(1) == 0x00000002);
      REQUIRE(g(2) == 0x00000004);
      REQUIRE(g(30) == 0x40000000);
      REQUIRE(g(31) == 0x80000000);

      REQUIRE(g(32) == 0x100000000ll);
      REQUIRE((unsigned long long)g(63) == 0x8000000000000000ull);
   }


   SECTION("FloorLog2")
   {
      auto f = floor_log2_constexpr;
      int (*g)(int) = floor_log2;
      int (*k)(long long) = floor_log2;

      // corner case
      REQUIRE(f(0) == 0);
      REQUIRE(g(0) == 0);
      REQUIRE(k(0) == 0);

      REQUIRE(f(1) == 0);
      REQUIRE(f(2) == 1);
      REQUIRE(f(3) == 1);
      REQUIRE(f(4) == 2);
      REQUIRE(f(5) == 2);
      REQUIRE(f(6) == 2);
      REQUIRE(f(7) == 2);
      REQUIRE(f(8) == 3);

      REQUIRE(g(1) == f(1));
      REQUIRE(g(2) == f(2));
      REQUIRE(g(3) == f(3));
      REQUIRE(g(4) == f(4));
      REQUIRE(g(5) == f(5));
      REQUIRE(g(6) == f(6));
      REQUIRE(g(7) == f(7));
      REQUIRE(g(8) == f(8));

      REQUIRE(k(1) == f(1));
      REQUIRE(k(2) == f(2));
      REQUIRE(k(3) == f(3));
      REQUIRE(k(4) == f(4));
      REQUIRE(k(5) == f(5));
      REQUIRE(k(6) == f(6));
      REQUIRE(k(7) == f(7));
      REQUIRE(k(8) == f(8));
   }


   SECTION("CeilLog2")
   {
      int (*f)(int) = ceil_log2;
      int (*g)(long long) = ceil_log2;

      REQUIRE(f(1) == 0);
      REQUIRE(f(2) == 1);
      REQUIRE(f(3) == 2);
      REQUIRE(f(4) == 2);
      REQUIRE(f(5) == 3);
      REQUIRE(f(6) == 3);
      REQUIRE(f(7) == 3);
      REQUIRE(f(8) == 3);
      REQUIRE(f(9) == 4);

      REQUIRE(g(1) == 0);
      REQUIRE(g(2) == 1);
      REQUIRE(g(3) == 2);
      REQUIRE(g(4) == 2);
      REQUIRE(g(5) == 3);
      REQUIRE(g(6) == 3);
      REQUIRE(g(7) == 3);
      REQUIRE(g(8) == 3);
      REQUIRE(g(9) == 4);
   }


   SECTION("Pow2LessOrEqual")
   {
      auto f = pow2_le;

      REQUIRE(f(1) == 1);
      REQUIRE(f(2) == 2);
      REQUIRE(f(3) == 2);
      REQUIRE(f(4) == 4);
      REQUIRE(f(5) == 4);
      REQUIRE(f(6) == 4);
      REQUIRE(f(7) == 4);
      REQUIRE(f(8) == 8);
      REQUIRE(f(9) == 8);
      REQUIRE(f(1023) == 512);
      REQUIRE(f(1024) == 1024);
      REQUIRE(f(1025) == 1024);
   }


   SECTION("Pow2GreaterOrEqual")
   {
      auto f = pow2_ge;

      REQUIRE(f(0) == 1);
      REQUIRE(f(1) == 1);
      REQUIRE(f(2) == 2);
      REQUIRE(f(3) == 4);
      REQUIRE(f(4) == 4);
      REQUIRE(f(5) == 8);
      REQUIRE(f(6) == 8);
      REQUIRE(f(7) == 8);
      REQUIRE(f(8) == 8);
      REQUIRE(f(9) == 16);
      REQUIRE(f(1023) == 1024);
      REQUIRE(f(1024) == 1024);
   }
}


TEST_CASE("LUSolve", "[util][math]")
{
#if TINKER_HOST
   //  1  2  3                      1.5      8
   //  *  5  5  matrix-vector-dot  -0.5  =  13
   //  *  * 12                      2.5     32
   float aUpRow[] = {1, 2, 3, 5, 5, 12};
   float b[] = {8, 13, 32};
   float x[] = {1.5, -0.5, 2.5};


   symlusolve<3, float>(aUpRow, b);
   for (int i = 0; i < 3; ++i)
      REQUIRE(x[i] == b[i]);
#endif
}
