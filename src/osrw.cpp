#include "osrw.h"
#include "box.h"
#include "energy.h"
#include "potent.h"
#include "tinker_rt.h"
#include "tool/fc.h"
#include "tool/host_zero.h"


namespace tinker {
bool use_osrw;
double osrw_lambda;
int osrw_vdw;
int osrw_ele;
int osrw_tor;


energy_prec osrw_du1;
virial_prec osrw_dv1[9];
grad_prec *osrw_dgx, *osrw_dgy, *osrw_dgz;


real* osrw_pchg;
real (*osrw_pole)[mpl_total];
real* osrw_polarity;
int osrw_ntbnd;
int (*osrw_itbnd)[2];
real (*osrw_tors1)[4];
real (*osrw_tors2)[4];
real (*osrw_tors3)[4];
real (*osrw_tors4)[4];
real (*osrw_tors5)[4];
real (*osrw_tors6)[4];


namespace {
grad_prec *osrw_gx, *osrw_gy, *osrw_gz;
}


void osrw_mech()
{
   use_osrw = false;
   osrw_lambda = 1.0;
   osrw_vdw = OSRW_LAM_LINEAR;
   osrw_ele = OSRW_LAM_LINEAR;
   osrw_tor = OSRW_LAM_LINEAR;


   osrw_du1 = 0;
   for (int iv = 0; iv < 9; ++iv)
      osrw_dv1[iv] = 0;
   osrw_dgx = nullptr;
   osrw_dgy = nullptr;
   osrw_dgz = nullptr;


   osrw_pchg = nullptr;
   osrw_pole = nullptr;
   osrw_polarity = nullptr;
   osrw_ntbnd = 0;
   osrw_itbnd = nullptr;
   osrw_tors1 = nullptr;
   osrw_tors2 = nullptr;
   osrw_tors3 = nullptr;
   osrw_tors4 = nullptr;
   osrw_tors5 = nullptr;
   osrw_tors6 = nullptr;


   osrw_gx = nullptr;
   osrw_gy = nullptr;
   osrw_gz = nullptr;


   // OSRW keywords.


   get_kbool("OSRW-LAMBDA", use_osrw, false);
   get_kv("OSRW-LAMBDA", osrw_lambda, 1.0);


   auto assign = [](int& func_int, const std::string& name) {
      if (name == "LINEAR")
         func_int = OSRW_LAM_LINEAR;
      else if (name == "QUADRATIC")
         func_int = OSRW_LAM_QUADRATIC;
   };
   std::string record;
   get_kv("OSRW-VDW", record, "LINEAR");
   assign(osrw_vdw, record);
   get_kv("OSRW-ELE", record, "LINEAR");
   assign(osrw_ele, record);
   get_kv("OSRW-TORS", record, "LINEAR");
   assign(osrw_tor, record);
}


void osrw_data(rc_op op)
{
   if (!use_osrw)
      return;


   if (op & rc_dealloc) {
      if (rc_flag & calc::grad) {
         darray::deallocate(osrw_dgx, osrw_dgy, osrw_dgz);
         darray::deallocate(osrw_gx, osrw_gy, osrw_gz);
      }


      if (use_potent(torsion_term)) {
         if (osrw_ntbnd > 0) {
            darray::deallocate(osrw_itbnd, osrw_tors1, osrw_tors2, osrw_tors3,
                               osrw_tors4, osrw_tors5, osrw_tors6);
            osrw_ntbnd = 0;
         }
      }


      if (use_potent(charge_term)) {
         darray::deallocate(osrw_pchg);
      }


      if (use_potent(mpole_term) || use_potent(polar_term)) {
         darray::deallocate(osrw_pole);
         if (use_potent(polar_term)) {
            darray::deallocate(osrw_polarity);
         }
      }
   }


   if (op & rc_alloc) {
      if (rc_flag & calc::grad) {
         darray::allocate(n, &osrw_dgx, &osrw_dgy, &osrw_dgz);
         darray::allocate(n, &osrw_gx, &osrw_gy, &osrw_gz);
      }


      if (use_potent(torsion_term)) {
         std::vector<int> buf;
         std::vector<std::string> vs;
         get_kv("ROTATABLE-BOND", vs, "");
         for (size_t i = 0; i < vs.size(); i += 2) {
            std::string s1 = vs.at(i);
            std::string s2 = vs.at(i + 1);
            buf.push_back(std::stoi(s1) - 1);
            buf.push_back(std::stoi(s2) - 1);
            osrw_ntbnd += 1;
         }
         if (osrw_ntbnd > 0) {
            darray::allocate(osrw_ntbnd, &osrw_itbnd);
            darray::allocate(ntors, &osrw_tors1, &osrw_tors2, &osrw_tors3,
                             &osrw_tors4, &osrw_tors5, &osrw_tors6);
            darray::copy(g::q0, ntors, osrw_tors1, tors1);
            darray::copy(g::q0, ntors, osrw_tors2, tors2);
            darray::copy(g::q0, ntors, osrw_tors3, tors3);
            darray::copy(g::q0, ntors, osrw_tors4, tors4);
            darray::copy(g::q0, ntors, osrw_tors5, tors5);
            darray::copy(g::q0, ntors, osrw_tors6, tors6);
            darray::copyin(g::q0, osrw_ntbnd, osrw_itbnd, buf.data());
            wait_for(g::q0);
         }
      }


      if (use_potent(charge_term)) {
         darray::allocate(n, &osrw_pchg);
      }


      if (use_potent(mpole_term) || use_potent(polar_term)) {
         darray::allocate(n, &osrw_pole);
         if (use_potent(polar_term)) {
            darray::allocate(n, &osrw_polarity);
         }
      }
   }


   if (op & rc_init) {
      if (use_potent(charge_term)) {
         darray::copy(g::q0, n, osrw_pchg, pchg);
      }


      if (use_potent(mpole_term) || use_potent(polar_term)) {
         darray::copy(g::q0, n, osrw_pole, pole);
         if (use_potent(polar_term)) {
            darray::copy(g::q0, n, osrw_polarity, polarity);
         }
      }
   }
}


double osrw_lam_expr0(int form, double lam)
{
   if (lam <= 0)
      return 0;
   else if (lam >= 1)
      return 1;


   double ans;
   switch (form) {
   case OSRW_LAM_QUADRATIC:
      ans = lam * lam;
      break;
   default: // LINEAR
      ans = lam;
      break;
   }
   return ans;
}


double osrw_lam_expr1(int form, double lam)
{
   double ans;
   switch (form) {
   case OSRW_LAM_QUADRATIC:
      ans = 2 * lam;
      break;
   default: // LINEAR
      ans = 1;
      break;
   }
   return ans;
}


double osrw_lam_expr2(int form, double lam)
{
   double ans;
   switch (form) {
   case OSRW_LAM_QUADRATIC:
      ans = 2;
      break;
   default: // LINEAR
      ans = 0;
      break;
   }
   return ans;
}


void osrw_altele(double el)
{
   osrw_altele_acc(el);
}


void osrw_alttor(double tl)
{
   osrw_alttor_acc(tl);
}


void osrw_altvdw(double vl)
{
   vlam = vl;
}


void osrw_energy(int vers, unsigned tsflag, const TimeScaleConfig& tsconfig)
{
   bool do_e = vers & calc::energy;
   bool do_v = vers & calc::virial;
   bool do_g = vers & calc::grad;
   bool do_a = vers & calc::analyz;
   bool rc_a = rc_flag & calc::analyz;


   double sele1 = osrw_lam_expr1(osrw_ele, osrw_lambda);
   double svdw1 = osrw_lam_expr1(osrw_vdw, osrw_lambda);
   double stor1 = osrw_lam_expr1(osrw_tor, osrw_lambda);


   energy_prec osrw_du0 = 0;
   osrw_du1 = 0;
   virial_prec osrw_dv0[9] = {0};
   host_zero(osrw_dv1);
   if (vers & calc::grad)
      darray::zero(g::q0, n, osrw_dgx, osrw_dgy, osrw_dgz);


   energy_prec aec = 0, aem = 0, aep = 0;
   energy_prec aect = 0;
   energy_prec aevdw = 0;
   energy_prec aetor = 0;


   double sele0;
   double svdw0;
   double stor0;


   sele0 = 1 - osrw_lam_expr0(osrw_ele, osrw_lambda);
   svdw0 = 1 - osrw_lam_expr0(osrw_vdw, osrw_lambda);
   stor0 = 1 - osrw_lam_expr0(osrw_tor, osrw_lambda);
   osrw_alttor(0);
   osrw_altvdw(0);
   osrw_altele(0);


   zero_egv(vers);
   energy_core(vers, tsflag, tsconfig);
   if (do_e) {
      if (!rc_a) {
         energy_prec e;
         e = energy_reduce(eng_buf);
         energy_valence += e;
         if (eng_buf_vdw) {
            e = energy_reduce(eng_buf_vdw);
            energy_vdw += e;
         }
         if (eng_buf_elec) {
            e = energy_reduce(eng_buf_elec);
            energy_elec += e;
         }
      }
      osrw_du0 +=
         (stor0 * energy_valence + svdw0 * energy_vdw + sele0 * energy_elec);
      osrw_du1 -=
         (stor1 * energy_valence + svdw1 * energy_vdw + sele1 * energy_elec);
      if (do_a) {
         aetor += stor0 * energy_et;
         aevdw += svdw0 * energy_ev;
         aec += sele0 * energy_ec;
         aem += sele0 * energy_em;
         aep += sele0 * energy_ep;
         aect += sele0 * energy_ect;
      }
   }
   if (do_v) {
      if (!rc_a) {
         virial_prec v[9];
         virial_reduce(v, vir_buf);
         for (int iv = 0; iv < 9; ++iv)
            virial_valence[iv] += v[iv];
         if (vir_buf_vdw) {
            virial_reduce(v, vir_buf_vdw);
            for (int iv = 0; iv < 9; ++iv)
               virial_vdw[iv] += v[iv];
         }
         if (vir_buf_elec) {
            virial_reduce(v, vir_buf_elec);
            for (int iv = 0; iv < 9; ++iv)
               virial_elec[iv] += v[iv];
         }
      }
      for (int iv = 0; iv < 9; ++iv) {
         osrw_dv0[iv] += (stor0 * virial_valence[iv] + svdw0 * virial_vdw[iv] +
                          sele0 * virial_elec[iv]);
         osrw_dv1[iv] -= (stor1 * virial_valence[iv] + svdw1 * virial_vdw[iv] +
                          sele1 * virial_elec[iv]);
      }
   }
   if (do_g) {
      sum_gradient(-stor1, osrw_dgx, osrw_dgy, osrw_dgz, gx, gy, gz);
      scale_gradient(stor0, gx, gy, gz);
      if (gx_vdw) {
         sum_gradient(-svdw1, osrw_dgx, osrw_dgy, osrw_dgz, gx_vdw, gy_vdw,
                      gz_vdw);
         sum_gradient(svdw0, gx, gy, gz, gx_vdw, gy_vdw, gz_vdw);
      }
      if (gx_elec) {
         sum_gradient(-sele1, osrw_dgx, osrw_dgy, osrw_dgz, gx_elec, gy_elec,
                      gz_elec);
         sum_gradient(sele0, gx, gy, gz, gx_elec, gy_elec, gz_elec);
      }
      darray::copy(g::q0, n, osrw_gx, gx);
      darray::copy(g::q0, n, osrw_gy, gy);
      darray::copy(g::q0, n, osrw_gz, gz);
   }


   sele0 = 1.0 - sele0;
   svdw0 = 1.0 - svdw0;
   stor0 = 1.0 - stor0;
   osrw_alttor(1);
   osrw_altvdw(1);
   osrw_altele(1);


   zero_egv(vers);
   energy_core(vers, tsflag, tsconfig);
   if (do_e) {
      if (!rc_a) {
         energy_prec e;
         e = energy_reduce(eng_buf);
         energy_valence += e;
         if (eng_buf_vdw) {
            e = energy_reduce(eng_buf_vdw);
            energy_vdw += e;
         }
         if (eng_buf_elec) {
            e = energy_reduce(eng_buf_elec);
            energy_elec += e;
         }
      }
      osrw_du0 +=
         (stor0 * energy_valence + svdw0 * energy_vdw + sele0 * energy_elec);
      osrw_du1 +=
         (stor1 * energy_valence + svdw1 * energy_vdw + sele1 * energy_elec);
      esum = osrw_du0;
      if (do_a) {
         aetor += stor0 * energy_et;
         aevdw += svdw0 * energy_ev;
         aec += sele0 * energy_ec;
         aem += sele0 * energy_em;
         aep += sele0 * energy_ep;
         aect += sele0 * energy_ect;
         energy_et = aetor;
         energy_ev = aevdw;
         energy_ec = aec;
         energy_em = aem;
         energy_ep = aep;
         energy_ect = aect;
      }
   }
   if (do_v) {
      if (!rc_a) {
         virial_prec v[9];
         virial_reduce(v, vir_buf);
         for (int iv = 0; iv < 9; ++iv)
            virial_valence[iv] += v[iv];
         if (vir_buf_vdw) {
            virial_reduce(v, vir_buf_vdw);
            for (int iv = 0; iv < 9; ++iv)
               virial_vdw[iv] += v[iv];
         }
         if (vir_buf_elec) {
            virial_reduce(v, vir_buf_elec);
            for (int iv = 0; iv < 9; ++iv)
               virial_elec[iv] += v[iv];
         }
      }
      for (int iv = 0; iv < 9; ++iv) {
         osrw_dv0[iv] += (stor0 * virial_valence[iv] + svdw0 * virial_vdw[iv] +
                          sele0 * virial_elec[iv]);
         osrw_dv1[iv] += (stor1 * virial_valence[iv] + svdw1 * virial_vdw[iv] +
                          sele1 * virial_elec[iv]);
      }
      for (int iv = 0; iv < 9; ++iv) {
         vir[iv] = osrw_dv0[iv];
      }
   }
   if (do_g) {
      sum_gradient(stor1, osrw_dgx, osrw_dgy, osrw_dgz, gx, gy, gz);
      scale_gradient(stor0, gx, gy, gz);
      if (gx_vdw) {
         sum_gradient(svdw1, osrw_dgx, osrw_dgy, osrw_dgz, gx_vdw, gy_vdw,
                      gz_vdw);
         sum_gradient(svdw0, gx, gy, gz, gx_vdw, gy_vdw, gz_vdw);
      }
      if (gx_elec) {
         sum_gradient(sele1, osrw_dgx, osrw_dgy, osrw_dgz, gx_elec, gy_elec,
                      gz_elec);
         sum_gradient(sele0, gx, gy, gz, gx_elec, gy_elec, gz_elec);
      }
      sum_gradient(gx, gy, gz, osrw_gx, osrw_gy, osrw_gz);
   }
}


void osrw_energy(int vers)
{
   osrw_energy(vers, 1, default_tsconfig());
}
}
