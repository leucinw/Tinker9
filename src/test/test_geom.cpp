#include "test.h"
#include "test_rt.h"


using namespace tinker;


TEST_CASE("Geom-Group-Local-Frame2", "[ff][egeom][local-frame2]")
{
   const char* k = "test_local_frame2.key";
   const char* k0 = R"**(
a-axis   30.0

group 1     5
group 2   7,8
group 3 -9,14

# force dist1 (dist2)
restrain-groups 1 2  2.1     1.3
restrain-groups 2 3  5.3     2.9 3.7
restrainterm        only
)**";
   const char* x = "test_local_frame2.xyz";
   const char* argv[] = {"dummy", x};
   int argc = 2;
   int usage = calc::xyz | calc::mass | calc::vmask;


   TestFile fpr(TINKER9_DIRSTR "/src/test/file/commit_6fe8e913/amoeba09.prm");
   TestFile fx1(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame2.xyz",
                x);
   TestFile fke(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame.key", k,
                k0);


   TestReference r(TINKER9_DIRSTR "/src/test/ref/geom.1.txt");
   auto ref_e = r.get_energy();
   auto ref_v = r.get_virial();
   auto ref_count = r.get_count();
   auto ref_g = r.get_gradient();


   const double eps_e = 0.0001;
   const double eps_g = 0.0001;
   const double eps_v = 0.001;


   test_begin_with_args(argc, argv);
   rc_flag = usage;
   initialize();


   energy(calc::v3);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_INTS(ngfix, ref_count);


   energy(calc::v1);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   energy(calc::v4);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v5);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v6);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   finish();
   test_end();
}


TEST_CASE("Geom-Distance-Local-Frame2", "[ff][egeom][local-frame2]")
{
   const char* k = "test_local_frame2.key";
   const char* k0 = R"**(
a-axis   30.0

restrain-distance   3  4  10.0  0.1  0.5
restrainterm        only
)**";
   const char* x = "test_local_frame2.xyz";
   const char* argv[] = {"dummy", x};
   int argc = 2;
   int usage = calc::xyz | calc::mass | calc::vmask;


   TestFile fpr(TINKER9_DIRSTR "/src/test/file/commit_6fe8e913/amoeba09.prm");
   TestFile fx1(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame2.xyz",
                x);
   TestFile fke(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame.key", k,
                k0);


   TestReference r(TINKER9_DIRSTR "/src/test/ref/geom.2.txt");
   auto ref_e = r.get_energy();
   auto ref_v = r.get_virial();
   auto ref_count = r.get_count();
   auto ref_g = r.get_gradient();


   const double eps_e = 0.0001;
   const double eps_g = 0.0001;
   const double eps_v = 0.001;


   test_begin_with_args(argc, argv);
   rc_flag = usage;
   initialize();


   energy(calc::v3);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_INTS(ndfix, ref_count);


   energy(calc::v1);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   energy(calc::v4);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v5);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v6);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   finish();
   test_end();
}


TEST_CASE("Geom-Angle-Local-Frame2", "[ff][egeom][local-frame2]")
{
   const char* k = "test_local_frame2.key";
   const char* k0 = R"**(
a-axis   30.0

restrain-angle      4  3  5  5.0  115.0
restrainterm        only
)**";
   const char* x = "test_local_frame2.xyz";
   const char* argv[] = {"dummy", x};
   int argc = 2;
   int usage = calc::xyz | calc::mass | calc::vmask;


   TestFile fpr(TINKER9_DIRSTR "/src/test/file/commit_6fe8e913/amoeba09.prm");
   TestFile fx1(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame2.xyz",
                x);
   TestFile fke(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame.key", k,
                k0);


   TestReference r(TINKER9_DIRSTR "/src/test/ref/geom.3.txt");
   auto ref_e = r.get_energy();
   auto ref_v = r.get_virial();
   auto ref_count = r.get_count();
   auto ref_g = r.get_gradient();


   const double eps_e = 0.0001;
   const double eps_g = 0.0001;
   const double eps_v = 0.001;


   test_begin_with_args(argc, argv);
   rc_flag = usage;
   initialize();


   energy(calc::v3);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_INTS(nafix, ref_count);


   energy(calc::v1);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   energy(calc::v4);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v5);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v6);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   finish();
   test_end();
}


TEST_CASE("Geom-Torsion-Local-Frame2", "[ff][egeom][local-frame2]")
{
   const char* k = "test_local_frame2.key";
   const char* k0 = R"**(
a-axis   30.0

restrain-torsion    9  10  11  12  1.0  -160.0
restrainterm        only
)**";
   const char* x = "test_local_frame2.xyz";
   const char* argv[] = {"dummy", x};
   int argc = 2;
   int usage = calc::xyz | calc::mass | calc::vmask;


   TestFile fpr(TINKER9_DIRSTR "/src/test/file/commit_6fe8e913/amoeba09.prm");
   TestFile fx1(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame2.xyz",
                x);
   TestFile fke(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame.key", k,
                k0);


   TestReference r(TINKER9_DIRSTR "/src/test/ref/geom.4.txt");
   auto ref_e = r.get_energy();
   auto ref_v = r.get_virial();
   auto ref_count = r.get_count();
   auto ref_g = r.get_gradient();


   const double eps_e = 0.0005;
   const double eps_g = 0.1000;
   const double eps_v = 0.0015;


   test_begin_with_args(argc, argv);
   rc_flag = usage;
   initialize();


   energy(calc::v3);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_INTS(ntfix, ref_count);


   energy(calc::v1);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   energy(calc::v4);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v5);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v6);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   finish();
   test_end();
}


TEST_CASE("Geom-Position-Local-Frame2", "[ff][egeom][local-frame2]")
{
   const char* k = "test_local_frame2.key";
   const char* k0 = R"**(
a-axis   30.0

restrain-position   3  0.0  0.0  0.0  5.0  0.0
restrainterm        only
)**";
   const char* x = "test_local_frame2.xyz";
   const char* argv[] = {"dummy", x};
   int argc = 2;
   int usage = calc::xyz | calc::mass | calc::vmask;


   TestFile fpr(TINKER9_DIRSTR "/src/test/file/commit_6fe8e913/amoeba09.prm");
   TestFile fx1(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame2.xyz",
                x);
   TestFile fke(TINKER9_DIRSTR "/src/test/file/local_frame/local_frame.key", k,
                k0);


   TestReference r(TINKER9_DIRSTR "/src/test/ref/geom.5.txt");
   auto ref_e = r.get_energy();
   auto ref_v = r.get_virial();
   auto ref_count = r.get_count();
   auto ref_g = r.get_gradient();


   const double eps_e = 0.0005;
   const double eps_g = 0.1000;
   const double eps_v = 0.0015;


   test_begin_with_args(argc, argv);
   rc_flag = usage;
   initialize();


   energy(calc::v3);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_INTS(npfix, ref_count);


   energy(calc::v1);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   energy(calc::v4);
   COMPARE_REALS(esum, ref_e, eps_e);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v5);
   COMPARE_GRADIENT(ref_g, eps_g);


   energy(calc::v6);
   COMPARE_GRADIENT(ref_g, eps_g);
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
         COMPARE_REALS(vir[i * 3 + j], ref_v[i][j], eps_v);


   finish();
   test_end();
}
