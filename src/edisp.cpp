#include "edisp.h"
#include "box.h"
#include "md.h"
#include "nblist.h"
#include "pmestuf.h"
#include "potent.h"
#include "tool/fc.h"
#include "tool/host_zero.h"
#include <tinker/detail/couple.hh>
#include <tinker/detail/disp.hh>
#include <tinker/detail/dsppot.hh>
#include <tinker/detail/limits.hh>
#include <tinker/detail/sizes.hh>


namespace tinker {
bool use_dewald()
{
   return use_potent(disp_term) and limits::use_dewald;
}


void edisp_data(rc_op op)
{
   if (!use_potent(disp_term))
      return;


   bool rc_a = rc_flag & calc::analyz;


   if (op & rc_dealloc) {
      darray::deallocate(csix, adisp);
      darray::deallocate(dspexclude, dspexclude_scale);


      if (rc_a) {
         buffer_deallocate(rc_flag, ndisp);
         buffer_deallocate(rc_flag, edsp, vir_edsp, dedspx, dedspy, dedspz);
      }
      ndisp = nullptr;
      edsp = nullptr;
      vir_edsp = nullptr;
      dedspx = nullptr;
      dedspy = nullptr;
      dedspz = nullptr;


      elrc_vol_dsp = 0;
      vlrc_vol_dsp = 0;
   }


   if (op & rc_alloc) {
      darray::allocate(n, &csix, &adisp);


      ndisp = nullptr;
      edsp = eng_buf_vdw;
      vir_edsp = vir_buf_vdw;
      dedspx = gx_vdw;
      dedspy = gy_vdw;
      dedspz = gz_vdw;
      if (rc_a) {
         buffer_allocate(rc_flag, &ndisp);
         buffer_allocate(rc_flag, &edsp, &vir_edsp, &dedspx, &dedspy, &dedspz);
      }


      if (dsppot::use_dcorr && !use_dewald()) {
         double elrc = 0, vlrc = 0;
         t_evcorr1("DISP", &elrc, &vlrc);
         elrc_vol_dsp = elrc * volbox();
         vlrc_vol_dsp = vlrc * volbox();
      } else {
         elrc_vol_dsp = 0;
         vlrc_vol_dsp = 0;
      }
      dsp2scale = dsppot::dsp2scale;
      dsp3scale = dsppot::dsp3scale;
      dsp4scale = dsppot::dsp4scale;
      dsp5scale = dsppot::dsp5scale;
      std::vector<int> exclik;
      std::vector<real> excls;
      // see also attach.f
      const int maxn13 = 3 * sizes::maxval;
      const int maxn14 = 9 * sizes::maxval;
      const int maxn15 = 27 * sizes::maxval;
      for (int i = 0; i < n; ++i) {
         int nn;
         int bask;

         if (dsp2scale != 1) {
            nn = couple::n12[i];
            for (int j = 0; j < nn; ++j) {
               int k = couple::i12[i][j];
               k -= 1;
               if (k > i) {
                  exclik.push_back(i);
                  exclik.push_back(k);
                  excls.push_back(dsp2scale);
               }
            }
         }


         if (dsp3scale != 1) {
            nn = couple::n13[i];
            bask = i * maxn13;
            for (int j = 0; j < nn; ++j) {
               int k = couple::i13[bask + j];
               k -= 1;
               if (k > i) {
                  exclik.push_back(i);
                  exclik.push_back(k);
                  excls.push_back(dsp3scale);
               }
            }
         }


         if (dsp4scale != 1) {
            nn = couple::n14[i];
            bask = i * maxn14;
            for (int j = 0; j < nn; ++j) {
               int k = couple::i14[bask + j];
               k -= 1;
               if (k > i) {
                  exclik.push_back(i);
                  exclik.push_back(k);
                  excls.push_back(dsp4scale);
               }
            }
         }


         if (dsp5scale != 1) {
            nn = couple::n15[i];
            bask = i * maxn15;
            for (int j = 0; j < nn; ++j) {
               int k = couple::i15[bask + j];
               k -= 1;
               if (k > i) {
                  exclik.push_back(i);
                  exclik.push_back(k);
                  excls.push_back(dsp5scale);
               }
            }
         }
      }
      ndspexclude = excls.size();
      darray::allocate(ndspexclude, &dspexclude, &dspexclude_scale);
      darray::copyin(g::q0, ndspexclude, dspexclude, exclik.data());
      darray::copyin(g::q0, ndspexclude, dspexclude_scale, excls.data());
      wait_for(g::q0);
   }


   if (op & rc_init) {
      csixpr = disp::csixpr;
      darray::copyin(g::q0, n, csix, disp::csix);
      darray::copyin(g::q0, n, adisp, disp::adisp);
      wait_for(g::q0);
   }
}


void edisp(int vers)
{
   bool rc_a = rc_flag & calc::analyz;
   bool do_a = vers & calc::analyz;
   bool do_e = vers & calc::energy;
   bool do_v = vers & calc::virial;
   bool do_g = vers & calc::grad;


   host_zero(energy_edsp, virial_edsp);
   size_t bsize = buffer_size();
   if (rc_a) {
      if (do_a)
         darray::zero(g::q0, bsize, ndisp);
      if (do_e)
         darray::zero(g::q0, bsize, edsp);
      if (do_v)
         darray::zero(g::q0, bsize, vir_edsp);
      if (do_g)
         darray::zero(g::q0, n, dedspx, dedspy, dedspz);
   }


   if (use_dewald())
      edisp_ewald(vers);
   else
      edisp_nonewald(vers);


   if (do_e) {
      if (elrc_vol_dsp != 0) {
         energy_prec corr = elrc_vol_dsp / volbox();
         energy_edsp += corr;
         energy_vdw += corr;
      }
   }
   if (do_v) {
      if (vlrc_vol_dsp != 0) {
         virial_prec term = vlrc_vol_dsp / volbox();
         virial_edsp[0] += term; // xx
         virial_edsp[4] += term; // yy
         virial_edsp[8] += term; // zz
         virial_vdw[0] += term;
         virial_vdw[4] += term;
         virial_vdw[8] += term;
      }
   }
   if (rc_a) {
      if (do_e) {
         energy_buffer u = edsp;
         energy_prec e = energy_reduce(u);
         energy_edsp += e;
         energy_vdw += e;
      }
      if (do_v) {
         virial_buffer u = vir_edsp;
         virial_prec v[9];
         virial_reduce(v, u);
         for (int iv = 0; iv < 9; ++iv) {
            virial_edsp[iv] += v[iv];
            virial_vdw[iv] += v[iv];
         }
      }
      if (do_g)
         sum_gradient(gx_vdw, gy_vdw, gz_vdw, dedspx, dedspy, dedspz);
   }
}


void edisp_ewald(int vers)
{
#if TINKER_CUDART
   if (dsplist_version() & NBL_SPATIAL)
      edisp_ewald_real_cu(vers);
   else
#endif
      edisp_ewald_real_acc(vers);


   // recip and self
   bool do_e = vers & calc::energy;
   bool do_v = vers & calc::virial;
   bool do_g = vers & calc::grad;
   PMEUnit u = dpme_unit;


   grid_disp(u, csix);
   fftfront(u);
   disp_pme_conv_acc(vers);
   if (do_g) {
      fftback(u);
   }
#if TINKER_CUDART
   if (pltfm_config & CU_PLTFM)
      edisp_ewald_recip_self_cu(vers);
   else
#endif
      edisp_ewald_recip_self_acc(vers);


   // account for the total energy and virial correction term
   if CONSTEXPR (do_e || do_v) {
      const real aewald = u->aewald;
      const real denom0 = 6 * volbox() / std::pow(M_PI, 1.5);
      energy_prec term = csixpr * aewald * aewald * aewald / denom0;
      if CONSTEXPR (do_e) {
         energy_edsp -= term;
         energy_vdw -= term;
      }
      if CONSTEXPR (do_v) {
         virial_edsp[0] += term; // xx
         virial_edsp[4] += term; // yy
         virial_edsp[8] += term; // zz
         virial_vdw[0] += term;  // xx
         virial_vdw[4] += term;  // yy
         virial_vdw[8] += term;  // zz
      }
   }
}


void edisp_nonewald(int vers)
{
#if TINKER_CUDART
   if (dsplist_version() & NBL_SPATIAL)
      edisp_nonewald_cu(vers);
   else
#endif
      edisp_nonewald_acc(vers);
}
}
