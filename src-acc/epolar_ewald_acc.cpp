#include "add.h"
#include "epolar.h"
#include "glob.nblist.h"
#include "image.h"
#include "md.h"
#include "pme.h"
#include "seq_pair_polar.h"
#include "seq_switch.h"
#include "switch.h"
#include "tool/gpu_card.h"

namespace tinker {
#define POLAR_DPTRS                                                            \
   x, y, z, depx, depy, depz, rpole, thole, pdamp, uind, uinp, nep, ep,        \
      vir_ep, ufld, dufld
template <class Ver>
void epolar_ewald_real_acc1(const real (*uind)[3], const real (*uinp)[3])
{
   constexpr bool do_e = Ver::e;
   constexpr bool do_a = Ver::a;
   constexpr bool do_g = Ver::g;
   constexpr bool do_v = Ver::v;

   if CONSTEXPR (do_g)
      darray::zero(g::q0, n, ufld, dufld);

   const real off = switch_off(switch_ewald);
   const real off2 = off * off;
   const int maxnlst = mlist_unit->maxnlst;
   const auto* mlst = mlist_unit.deviceptr();

   auto bufsize = buffer_size();
   PairPolarGrad pgrad;

   const real f = 0.5f * electric / dielec;

   const PMEUnit pu = ppme_unit;
   const real aewald = pu->aewald;

   MAYBE_UNUSED int GRID_DIM = get_grid_size(BLOCK_DIM);
   #pragma acc parallel async num_gangs(GRID_DIM) vector_length(BLOCK_DIM)\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(POLAR_DPTRS,mlst)
   #pragma acc loop gang independent
   for (int i = 0; i < n; ++i) {
      real xi = x[i];
      real yi = y[i];
      real zi = z[i];
      real ci = rpole[i][mpl_pme_0];
      real dix = rpole[i][mpl_pme_x];
      real diy = rpole[i][mpl_pme_y];
      real diz = rpole[i][mpl_pme_z];
      real qixx = rpole[i][mpl_pme_xx];
      real qixy = rpole[i][mpl_pme_xy];
      real qixz = rpole[i][mpl_pme_xz];
      real qiyy = rpole[i][mpl_pme_yy];
      real qiyz = rpole[i][mpl_pme_yz];
      real qizz = rpole[i][mpl_pme_zz];
      real uix = uind[i][0];
      real uiy = uind[i][1];
      real uiz = uind[i][2];
      real pdi = pdamp[i];
      real pti = thole[i];
      real uixp = 0, uiyp = 0, uizp = 0;
      if CONSTEXPR (do_g) {
         uixp = uinp[i][0];
         uiyp = uinp[i][1];
         uizp = uinp[i][2];
      }
      real gxi = 0, gyi = 0, gzi = 0;
      real txi = 0, tyi = 0, tzi = 0;
      real du0 = 0, du1 = 0, du2 = 0, du3 = 0, du4 = 0, du5 = 0;

      int nmlsti = mlst->nlst[i];
      int base = i * maxnlst;
      #pragma acc loop vector independent private(pgrad)\
                  reduction(+:gxi,gyi,gzi,txi,tyi,tzi,du0,du1,du2,du3,du4,du5)
      for (int kk = 0; kk < nmlsti; ++kk) {
         int offset = kk & (bufsize - 1);
         int k = mlst->lst[base + kk];
         real xr = x[k] - xi;
         real yr = y[k] - yi;
         real zr = z[k] - zi;

         real r2 = image2(xr, yr, zr);
         if (r2 <= off2) {
            real ck = rpole[k][mpl_pme_0];
            real dkx = rpole[k][mpl_pme_x];
            real dky = rpole[k][mpl_pme_y];
            real dkz = rpole[k][mpl_pme_z];
            real qkxx = rpole[k][mpl_pme_xx];
            real qkxy = rpole[k][mpl_pme_xy];
            real qkxz = rpole[k][mpl_pme_xz];
            real qkyy = rpole[k][mpl_pme_yy];
            real qkyz = rpole[k][mpl_pme_yz];
            real qkzz = rpole[k][mpl_pme_zz];
            real ukx = uind[k][0];
            real uky = uind[k][1];
            real ukz = uind[k][2];
            real ukxp = 0, ukyp = 0, ukzp = 0;
            if CONSTEXPR (do_g) {
               ukxp = uinp[k][0];
               ukyp = uinp[k][1];
               ukzp = uinp[k][2];
            }

            MAYBE_UNUSED real e;
            pair_polar<do_e, do_g, EWALD>( //
               r2, xr, yr, zr, 1, 1, 1,    //
               ci, dix, diy, diz, qixx, qixy, qixz, qiyy, qiyz, qizz, uix, uiy,
               uiz, uixp, uiyp, uizp, pdi, pti, //
               ck, dkx, dky, dkz, qkxx, qkxy, qkxz, qkyy, qkyz, qkzz, ukx, uky,
               ukz, ukxp, ukyp, ukzp, pdamp[k], thole[k], //
               f, aewald, e, pgrad);

            if CONSTEXPR (do_a)
               atomic_add(1, nep, offset);
            if CONSTEXPR (do_e)
               atomic_add(e, ep, offset);

            if CONSTEXPR (do_g) {
               gxi += pgrad.frcx;
               gyi += pgrad.frcy;
               gzi += pgrad.frcz;
               atomic_add(-pgrad.frcx, depx, k);
               atomic_add(-pgrad.frcy, depy, k);
               atomic_add(-pgrad.frcz, depz, k);

               txi += pgrad.ufldi[0];
               tyi += pgrad.ufldi[1];
               tzi += pgrad.ufldi[2];
               atomic_add(pgrad.ufldk[0], &ufld[k][0]);
               atomic_add(pgrad.ufldk[1], &ufld[k][1]);
               atomic_add(pgrad.ufldk[2], &ufld[k][2]);

               du0 += pgrad.dufldi[0];
               du1 += pgrad.dufldi[1];
               du2 += pgrad.dufldi[2];
               du3 += pgrad.dufldi[3];
               du4 += pgrad.dufldi[4];
               du5 += pgrad.dufldi[5];
               atomic_add(pgrad.dufldk[0], &dufld[k][0]);
               atomic_add(pgrad.dufldk[1], &dufld[k][1]);
               atomic_add(pgrad.dufldk[2], &dufld[k][2]);
               atomic_add(pgrad.dufldk[3], &dufld[k][3]);
               atomic_add(pgrad.dufldk[4], &dufld[k][4]);
               atomic_add(pgrad.dufldk[5], &dufld[k][5]);

               if CONSTEXPR (do_v) {
                  real vxx = -xr * pgrad.frcx;
                  real vxy = -0.5f * (yr * pgrad.frcx + xr * pgrad.frcy);
                  real vxz = -0.5f * (zr * pgrad.frcx + xr * pgrad.frcz);
                  real vyy = -yr * pgrad.frcy;
                  real vyz = -0.5f * (zr * pgrad.frcy + yr * pgrad.frcz);
                  real vzz = -zr * pgrad.frcz;

                  atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_ep, offset);
               }
            }
         }
         // end if use_thole

      } // end for (int kk)

      if CONSTEXPR (do_g) {
         atomic_add(gxi, depx, i);
         atomic_add(gyi, depy, i);
         atomic_add(gzi, depz, i);
         atomic_add(txi, &ufld[i][0]);
         atomic_add(tyi, &ufld[i][1]);
         atomic_add(tzi, &ufld[i][2]);
         atomic_add(du0, &dufld[i][0]);
         atomic_add(du1, &dufld[i][1]);
         atomic_add(du2, &dufld[i][2]);
         atomic_add(du3, &dufld[i][3]);
         atomic_add(du4, &dufld[i][4]);
         atomic_add(du5, &dufld[i][5]);
      }
   } // end for (int i)

   #pragma acc parallel async\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(POLAR_DPTRS,dpuexclude,dpuexclude_scale)
   #pragma acc loop independent private(pgrad)
   for (int ii = 0; ii < ndpuexclude; ++ii) {
      int offset = ii & (bufsize - 1);

      int i = dpuexclude[ii][0];
      int k = dpuexclude[ii][1];
      real dscale = dpuexclude_scale[ii][0] - 1;
      real pscale = dpuexclude_scale[ii][1] - 1;
      real uscale = dpuexclude_scale[ii][2] - 1;

      real xi = x[i];
      real yi = y[i];
      real zi = z[i];
      real ci = rpole[i][mpl_pme_0];
      real dix = rpole[i][mpl_pme_x];
      real diy = rpole[i][mpl_pme_y];
      real diz = rpole[i][mpl_pme_z];
      real qixx = rpole[i][mpl_pme_xx];
      real qixy = rpole[i][mpl_pme_xy];
      real qixz = rpole[i][mpl_pme_xz];
      real qiyy = rpole[i][mpl_pme_yy];
      real qiyz = rpole[i][mpl_pme_yz];
      real qizz = rpole[i][mpl_pme_zz];
      real uix = uind[i][0];
      real uiy = uind[i][1];
      real uiz = uind[i][2];
      real pdi = pdamp[i];
      real pti = thole[i];
      real uixp = 0, uiyp = 0, uizp = 0;
      if CONSTEXPR (do_g) {
         uixp = uinp[i][0];
         uiyp = uinp[i][1];
         uizp = uinp[i][2];
      }

      real xr = x[k] - xi;
      real yr = y[k] - yi;
      real zr = z[k] - zi;

      real r2 = image2(xr, yr, zr);
      if (r2 <= off2) {
         real ck = rpole[k][mpl_pme_0];
         real dkx = rpole[k][mpl_pme_x];
         real dky = rpole[k][mpl_pme_y];
         real dkz = rpole[k][mpl_pme_z];
         real qkxx = rpole[k][mpl_pme_xx];
         real qkxy = rpole[k][mpl_pme_xy];
         real qkxz = rpole[k][mpl_pme_xz];
         real qkyy = rpole[k][mpl_pme_yy];
         real qkyz = rpole[k][mpl_pme_yz];
         real qkzz = rpole[k][mpl_pme_zz];
         real ukx = uind[k][0];
         real uky = uind[k][1];
         real ukz = uind[k][2];
         real ukxp = 0, ukyp = 0, ukzp = 0;
         if CONSTEXPR (do_g) {
            ukxp = uinp[k][0];
            ukyp = uinp[k][1];
            ukzp = uinp[k][2];
         }

         MAYBE_UNUSED real e;
         pair_polar<do_e, do_g, NON_EWALD>(         //
            r2, xr, yr, zr, dscale, pscale, uscale, //
            ci, dix, diy, diz, qixx, qixy, qixz, qiyy, qiyz, qizz, uix, uiy,
            uiz, uixp, uiyp, uizp, pdi, pti, //
            ck, dkx, dky, dkz, qkxx, qkxy, qkxz, qkyy, qkyz, qkzz, ukx, uky,
            ukz, ukxp, ukyp, ukzp, pdamp[k], thole[k], //
            f, 0, e, pgrad);

         if CONSTEXPR (do_a)
            if (pscale == -1)
               atomic_add(-1, nep, offset);
         if CONSTEXPR (do_e)
            atomic_add(e, ep, offset);

         if CONSTEXPR (do_g) {
            atomic_add(pgrad.frcx, depx, i);
            atomic_add(pgrad.frcy, depy, i);
            atomic_add(pgrad.frcz, depz, i);
            atomic_add(-pgrad.frcx, depx, k);
            atomic_add(-pgrad.frcy, depy, k);
            atomic_add(-pgrad.frcz, depz, k);

            atomic_add(pgrad.ufldi[0], &ufld[i][0]);
            atomic_add(pgrad.ufldi[1], &ufld[i][1]);
            atomic_add(pgrad.ufldi[2], &ufld[i][2]);
            atomic_add(pgrad.ufldk[0], &ufld[k][0]);
            atomic_add(pgrad.ufldk[1], &ufld[k][1]);
            atomic_add(pgrad.ufldk[2], &ufld[k][2]);

            atomic_add(pgrad.dufldi[0], &dufld[i][0]);
            atomic_add(pgrad.dufldi[1], &dufld[i][1]);
            atomic_add(pgrad.dufldi[2], &dufld[i][2]);
            atomic_add(pgrad.dufldi[3], &dufld[i][3]);
            atomic_add(pgrad.dufldi[4], &dufld[i][4]);
            atomic_add(pgrad.dufldi[5], &dufld[i][5]);
            atomic_add(pgrad.dufldk[0], &dufld[k][0]);
            atomic_add(pgrad.dufldk[1], &dufld[k][1]);
            atomic_add(pgrad.dufldk[2], &dufld[k][2]);
            atomic_add(pgrad.dufldk[3], &dufld[k][3]);
            atomic_add(pgrad.dufldk[4], &dufld[k][4]);
            atomic_add(pgrad.dufldk[5], &dufld[k][5]);

            if CONSTEXPR (do_v) {
               real vxx = -xr * pgrad.frcx;
               real vxy = -0.5f * (yr * pgrad.frcx + xr * pgrad.frcy);
               real vxz = -0.5f * (zr * pgrad.frcx + xr * pgrad.frcz);
               real vyy = -yr * pgrad.frcy;
               real vyz = -0.5f * (zr * pgrad.frcy + yr * pgrad.frcz);
               real vzz = -zr * pgrad.frcz;

               atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_ep, offset);
            }
         }
      }
   }

   // torque

   if CONSTEXPR (do_g) {
      #pragma acc parallel loop independent async\
                  deviceptr(rpole,trqx,trqy,trqz,ufld,dufld)
      for (int i = 0; i < n; ++i) {
         real dix = rpole[i][mpl_pme_x];
         real diy = rpole[i][mpl_pme_y];
         real diz = rpole[i][mpl_pme_z];
         real qixx = rpole[i][mpl_pme_xx];
         real qixy = rpole[i][mpl_pme_xy];
         real qixz = rpole[i][mpl_pme_xz];
         real qiyy = rpole[i][mpl_pme_yy];
         real qiyz = rpole[i][mpl_pme_yz];
         real qizz = rpole[i][mpl_pme_zz];

         real tep1 = diz * ufld[i][1] - diy * ufld[i][2] + qixz * dufld[i][1] -
            qixy * dufld[i][3] + 2 * qiyz * (dufld[i][2] - dufld[i][5]) +
            (qizz - qiyy) * dufld[i][4];
         real tep2 = dix * ufld[i][2] - diz * ufld[i][0] - qiyz * dufld[i][1] +
            qixy * dufld[i][4] + 2 * qixz * (dufld[i][5] - dufld[i][0]) +
            (qixx - qizz) * dufld[i][3];
         real tep3 = diy * ufld[i][0] - dix * ufld[i][1] + qiyz * dufld[i][3] -
            qixz * dufld[i][4] + 2 * qixy * (dufld[i][0] - dufld[i][2]) +
            (qiyy - qixx) * dufld[i][1];

         trqx[i] += tep1;
         trqy[i] += tep2;
         trqz[i] += tep3;
      }
   }
}

template <class Ver>
void epolar_ewald_recip_self_acc1(const real (*gpu_uind)[3],
                                  const real (*gpu_uinp)[3])
{
   constexpr bool do_e = Ver::e;
   constexpr bool do_a = Ver::a;
   constexpr bool do_g = Ver::g;
   constexpr bool do_v = Ver::v;

   const PMEUnit pu = ppme_unit;
   const auto& st = *pu;
   const int nfft1 = st.nfft1;
   const int nfft2 = st.nfft2;
   const int nfft3 = st.nfft3;
   const real aewald = st.aewald;

   const real f = electric / dielec;

   auto bufsize = buffer_size();

   auto* fphid = fdip_phi1;
   auto* fphip = fdip_phi2;

   cuind_to_fuind(pu, gpu_uind, gpu_uinp, fuind, fuinp);
   if CONSTEXPR (do_e) {
      #pragma acc parallel loop independent async deviceptr(fuind,fphi,ep)
      for (int i = 0; i < n; ++i) {
         int offset = i & (bufsize - 1);
         real e = 0.5f * f *
            (fuind[i][0] * fphi[i][1] + fuind[i][1] * fphi[i][2] +
             fuind[i][2] * fphi[i][3]);
         atomic_add(e, ep, offset);
      }
   }
   grid_uind(pu, fuind, fuinp);
   fftfront(pu);
   // TODO: store vs. recompute qfac
   pme_conv(pu);
   fftback(pu);
   fphi_uind(pu, fphid, fphip, fphidp);

   // increment the dipole polarization gradient contributions

   #pragma acc parallel loop independent async deviceptr(depx,depy,depz,\
               fmp,fphi,fuind,fuinp,fphid,fphip,fphidp)\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)
   for (int i = 0; i < n; ++i) {
      // data deriv1  / 2, 5,  8,  9, 11, 16, 18, 14, 15, 20 /
      // data deriv2  / 3, 8,  6, 10, 14, 12, 19, 16, 20, 17 /
      // data deriv3  / 4, 9, 10,  7, 15, 17, 13, 20, 18, 19 /
      constexpr int deriv1[10] = {1, 4, 7, 8, 10, 15, 17, 13, 14, 19};
      constexpr int deriv2[10] = {2, 7, 5, 9, 13, 11, 18, 15, 19, 16};
      constexpr int deriv3[10] = {3, 8, 9, 6, 14, 16, 12, 19, 17, 18};

      real f1 = 0;
      real f2 = 0;
      real f3 = 0;
      #pragma acc loop seq
      for (int k = 0; k < 3; ++k) {
         int j1 = deriv1[k + 1];
         int j2 = deriv2[k + 1];
         int j3 = deriv3[k + 1];
         f1 += (fuind[i][k] + fuinp[i][k]) * fphi[i][j1];
         f2 += (fuind[i][k] + fuinp[i][k]) * fphi[i][j2];
         f3 += (fuind[i][k] + fuinp[i][k]) * fphi[i][j3];
         // if poltyp .eq. 'MUTUAL'
         f1 += fuind[i][k] * fphip[i][j1] + fuinp[i][k] * fphid[i][j1];
         f2 += fuind[i][k] * fphip[i][j2] + fuinp[i][k] * fphid[i][j2];
         f3 += fuind[i][k] * fphip[i][j3] + fuinp[i][k] * fphid[i][j3];
         // end if
      }
      #pragma acc loop seq
      for (int k = 0; k < 10; ++k) {
         f1 += fmp[i][k] * fphidp[i][deriv1[k]];
         f2 += fmp[i][k] * fphidp[i][deriv2[k]];
         f3 += fmp[i][k] * fphidp[i][deriv3[k]];
      }
      if CONSTEXPR (do_g) {
         f1 *= 0.5f * nfft1;
         f2 *= 0.5f * nfft2;
         f3 *= 0.5f * nfft3;
         real h1 = recipa.x * f1 + recipb.x * f2 + recipc.x * f3;
         real h2 = recipa.y * f1 + recipb.y * f2 + recipc.y * f3;
         real h3 = recipa.z * f1 + recipb.z * f2 + recipc.z * f3;
         atomic_add(h1 * f, depx, i);
         atomic_add(h2 * f, depy, i);
         atomic_add(h3 * f, depz, i);
      }
   } // end for (int i)

   // set the potential to be the induced dipole average

   // see also subroutine eprecip1 in epolar1.f
   // do i = 1, npole
   //    do j = 1, 10
   //       fphidp(j,i) = 0.5d0 * fphidp(j,i)
   //    end do
   // end do
   // Notice that only 10 * n elements were scaled in the original code.
   darray::scale(g::q0, n, 0.5f * f, fphidp);
   fphi_to_cphi(pu, fphidp, cphidp);

   // recip and self torques

   real term = f * aewald * aewald * aewald * 4 / 3 / sqrtpi;
   real fterm_term = -2 * f * aewald * aewald * aewald / 3 / sqrtpi;
   #pragma acc parallel loop independent async\
               deviceptr(ep,nep,trqx,trqy,trqz,\
               rpole,cmp,gpu_uind,gpu_uinp,cphidp)
   for (int i = 0; i < n; ++i) {
      int offset = i & (bufsize - 1);
      real dix = rpole[i][mpl_pme_x];
      real diy = rpole[i][mpl_pme_y];
      real diz = rpole[i][mpl_pme_z];
      real uix = 0.5f * (gpu_uind[i][0] + gpu_uinp[i][0]);
      real uiy = 0.5f * (gpu_uind[i][1] + gpu_uinp[i][1]);
      real uiz = 0.5f * (gpu_uind[i][2] + gpu_uinp[i][2]);

      if CONSTEXPR (do_g) {
         real tep1 = cmp[i][3] * cphidp[i][2] - cmp[i][2] * cphidp[i][3] +
            2 * (cmp[i][6] - cmp[i][5]) * cphidp[i][9] +
            cmp[i][8] * cphidp[i][7] + cmp[i][9] * cphidp[i][5] -
            cmp[i][7] * cphidp[i][8] - cmp[i][9] * cphidp[i][6];
         real tep2 = cmp[i][1] * cphidp[i][3] - cmp[i][3] * cphidp[i][1] +
            2 * (cmp[i][4] - cmp[i][6]) * cphidp[i][8] +
            cmp[i][7] * cphidp[i][9] + cmp[i][8] * cphidp[i][6] -
            cmp[i][8] * cphidp[i][4] - cmp[i][9] * cphidp[i][7];
         real tep3 = cmp[i][2] * cphidp[i][1] - cmp[i][1] * cphidp[i][2] +
            2 * (cmp[i][5] - cmp[i][4]) * cphidp[i][7] +
            cmp[i][7] * cphidp[i][4] + cmp[i][9] * cphidp[i][8] -
            cmp[i][7] * cphidp[i][5] - cmp[i][8] * cphidp[i][9];

         // self term

         tep1 += term * (diy * uiz - diz * uiy);
         tep2 += term * (diz * uix - dix * uiz);
         tep3 += term * (dix * uiy - diy * uix);

         trqx[i] += tep1;
         trqy[i] += tep2;
         trqz[i] += tep3;
      }

      if CONSTEXPR (do_e) {
         uix = gpu_uind[i][0];
         uiy = gpu_uind[i][1];
         uiz = gpu_uind[i][2];
         real uii = dix * uix + diy * uiy + diz * uiz;
         atomic_add(fterm_term * uii, ep, offset);
      }
      if CONSTEXPR (do_a)
         atomic_add(1, nep, offset);
   }

   // recip virial

   if CONSTEXPR (do_v) {
      auto size = buffer_size() * virial_buffer_traits::value;
      #pragma acc parallel loop independent async deviceptr(vir_ep,vir_m)
      for (int i = 0; i < (int)size; ++i) {
         vir_ep[0][i] -= vir_m[0][i];
      }

      darray::scale(g::q0, n, f, cphi, fphid, fphip);

      #pragma acc parallel loop independent async\
                  present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
                  deviceptr(vir_ep,cmp,\
                  gpu_uind,gpu_uinp,fphid,fphip,cphi,cphidp)
      for (int i = 0; i < n; ++i) {
         real cphid[4], cphip[4];
         real ftc[3][3];

         // frac_to_cart

         ftc[0][0] = nfft1 * recipa.x;
         ftc[1][0] = nfft2 * recipb.x;
         ftc[2][0] = nfft3 * recipc.x;
         ftc[0][1] = nfft1 * recipa.y;
         ftc[1][1] = nfft2 * recipb.y;
         ftc[2][1] = nfft3 * recipc.y;
         ftc[0][2] = nfft1 * recipa.z;
         ftc[1][2] = nfft2 * recipb.z;
         ftc[2][2] = nfft3 * recipc.z;

         #pragma acc loop independent
         for (int j = 0; j < 3; ++j) {
            cphid[j + 1] = 0;
            cphip[j + 1] = 0;
            #pragma acc loop seq
            for (int k = 0; k < 3; ++k) {
               cphid[j + 1] += ftc[k][j] * fphid[i][k + 1];
               cphip[j + 1] += ftc[k][j] * fphip[i][k + 1];
            }
         }

         real vxx = 0;
         real vyy = 0;
         real vzz = 0;
         real vxy = 0;
         real vxz = 0;
         real vyz = 0;

         vxx = vxx - cmp[i][1] * cphidp[i][1] -
            0.5f * ((gpu_uind[i][0] + gpu_uinp[i][0]) * cphi[i][1]);
         vxy = vxy -
            0.5f * (cphidp[i][1] * cmp[i][2] + cphidp[i][2] * cmp[i][1]) -
            0.25f *
               ((gpu_uind[i][1] + gpu_uinp[i][1]) * cphi[i][1] +
                (gpu_uind[i][0] + gpu_uinp[i][0]) * cphi[i][2]);
         vxz = vxz -
            0.5f * (cphidp[i][1] * cmp[i][3] + cphidp[i][3] * cmp[i][1]) -
            0.25f *
               ((gpu_uind[i][2] + gpu_uinp[i][2]) * cphi[i][1] +
                (gpu_uind[i][0] + gpu_uinp[i][0]) * cphi[i][3]);
         vyy = vyy - cmp[i][2] * cphidp[i][2] -
            0.5f * ((gpu_uind[i][1] + gpu_uinp[i][1]) * cphi[i][2]);
         vyz = vyz -
            0.5f * (cphidp[i][2] * cmp[i][3] + cphidp[i][3] * cmp[i][2]) -
            0.25f *
               ((gpu_uind[i][2] + gpu_uinp[i][2]) * cphi[i][2] +
                (gpu_uind[i][1] + gpu_uinp[i][1]) * cphi[i][3]);
         vzz = vzz - cmp[i][3] * cphidp[i][3] -
            0.5f * ((gpu_uind[i][2] + gpu_uinp[i][2]) * cphi[i][3]);
         vxx = vxx - 2 * cmp[i][4] * cphidp[i][4] - cmp[i][7] * cphidp[i][7] -
            cmp[i][8] * cphidp[i][8];
         vxy = vxy - (cmp[i][4] + cmp[i][5]) * cphidp[i][7] -
            0.5f *
               (cmp[i][7] * (cphidp[i][5] + cphidp[i][4]) +
                cmp[i][8] * cphidp[i][9] + cmp[i][9] * cphidp[i][8]);
         vxz = vxz - (cmp[i][4] + cmp[i][6]) * cphidp[i][8] -
            0.5f *
               (cmp[i][8] * (cphidp[i][4] + cphidp[i][6]) +
                cmp[i][7] * cphidp[i][9] + cmp[i][9] * cphidp[i][7]);
         vyy = vyy - 2 * cmp[i][5] * cphidp[i][5] - cmp[i][7] * cphidp[i][7] -
            cmp[i][9] * cphidp[i][9];
         vyz = vyz - (cmp[i][5] + cmp[i][6]) * cphidp[i][9] -
            0.5f *
               (cmp[i][9] * (cphidp[i][5] + cphidp[i][6]) +
                cmp[i][7] * cphidp[i][8] + cmp[i][8] * cphidp[i][7]);
         vzz = vzz - 2 * cmp[i][6] * cphidp[i][6] - cmp[i][8] * cphidp[i][8] -
            cmp[i][9] * cphidp[i][9];

         // if (poltyp .eq. 'MUTUAL')
         vxx = vxx -
            0.5f * (cphid[1] * gpu_uinp[i][0] + cphip[1] * gpu_uind[i][0]);
         vxy = vxy -
            0.25f *
               (cphid[1] * gpu_uinp[i][1] + cphip[1] * gpu_uind[i][1] +
                cphid[2] * gpu_uinp[i][0] + cphip[2] * gpu_uind[i][0]);
         vxz = vxz -
            0.25f *
               (cphid[1] * gpu_uinp[i][2] + cphip[1] * gpu_uind[i][2] +
                cphid[3] * gpu_uinp[i][0] + cphip[3] * gpu_uind[i][0]);
         vyy = vyy -
            0.5f * (cphid[2] * gpu_uinp[i][1] + cphip[2] * gpu_uind[i][1]);
         vyz = vyz -
            0.25f *
               (cphid[2] * gpu_uinp[i][2] + cphip[2] * gpu_uind[i][2] +
                cphid[3] * gpu_uinp[i][1] + cphip[3] * gpu_uind[i][1]);
         vzz = vzz -
            0.5f * (cphid[3] * gpu_uinp[i][2] + cphip[3] * gpu_uind[i][2]);
         // end if

         atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_ep, i & (bufsize - 1));
      }

      // qgrip: pvu_qgrid
      const PMEUnit pvu = pvpme_unit;
      #pragma acc parallel loop independent async deviceptr(cmp,gpu_uinp)
      for (int i = 0; i < n; ++i) {
         cmp[i][1] += gpu_uinp[i][0];
         cmp[i][2] += gpu_uinp[i][1];
         cmp[i][3] += gpu_uinp[i][2];
      }
      cmp_to_fmp(pvu, cmp, fmp);
      grid_mpole(pvu, fmp);
      fftfront(pvu);

      // qgrid: pu_qgrid
      #pragma acc parallel loop independent async\
                  deviceptr(cmp,gpu_uind,gpu_uinp)
      for (int i = 0; i < n; ++i) {
         cmp[i][1] += (gpu_uind[i][0] - gpu_uinp[i][0]);
         cmp[i][2] += (gpu_uind[i][1] - gpu_uinp[i][1]);
         cmp[i][3] += (gpu_uind[i][2] - gpu_uinp[i][2]);
      }
      cmp_to_fmp(pu, cmp, fmp);
      grid_mpole(pu, fmp);
      fftfront(pu);

      const auto* d = pu.deviceptr();
      const auto* p = pvu.deviceptr();
      const int nff = nfft1 * nfft2;
      const int ntot = nfft1 * nfft2 * nfft3;
      real pterm = (pi / aewald) * (pi / aewald);
      real box_volume = volbox();

      #pragma acc parallel loop independent async deviceptr(d,p,vir_ep)\
                  present(lvec1,lvec2,lvec3,recipa,recipb,recipc)
      for (int i = 1; i < ntot; ++i) {
         const real volterm = pi * box_volume;

         int k3 = i / nff;
         int j = i - k3 * nff;
         int k2 = j / nfft1;
         int k1 = j - k2 * nfft1;

         int r1 = (k1 < (nfft1 + 1) / 2) ? k1 : (k1 - nfft1);
         int r2 = (k2 < (nfft2 + 1) / 2) ? k2 : (k2 - nfft2);
         int r3 = (k3 < (nfft3 + 1) / 2) ? k3 : (k3 - nfft3);

         real h1 = recipa.x * r1 + recipb.x * r2 + recipc.x * r3;
         real h2 = recipa.y * r1 + recipb.y * r2 + recipc.y * r3;
         real h3 = recipa.z * r1 + recipb.z * r2 + recipc.z * r3;
         real hsq = h1 * h1 + h2 * h2 + h3 * h3;
         real term = -pterm * hsq;
         real expterm = 0;
         if (term > -50) {
            real denom =
               volterm * hsq * d->bsmod1[k1] * d->bsmod2[k2] * d->bsmod3[k3];
            expterm = REAL_EXP(term) / denom;
            if (box_shape == UNBOUND_BOX)
               expterm *= (1 - REAL_COS(pi * lvec1.x * REAL_SQRT(hsq)));
            else if (box_shape == OCT_BOX)
               if ((k1 + k2 + k3) & 1)
                  expterm = 0; // end if ((k1 + k2 + k3) % 2 != 0)

            real struc2 = d->qgrid[2 * i] * p->qgrid[2 * i] +
               d->qgrid[2 * i + 1] * p->qgrid[2 * i + 1];
            real eterm = 0.5f * f * expterm * struc2;
            real vterm = (2 / hsq) * (1 - term) * eterm;

            real vxx = (h1 * h1 * vterm - eterm);
            real vxy = h1 * h2 * vterm;
            real vxz = h1 * h3 * vterm;
            real vyy = (h2 * h2 * vterm - eterm);
            real vyz = h2 * h3 * vterm;
            real vzz = (h3 * h3 * vterm - eterm);

            atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_ep, i & (bufsize - 1));
         }
      }
   }
}


void epolar_ewald_real_acc(int vers, const real (*uind)[3],
                           const real (*uinp)[3])
{
   if (vers == calc::v0) {
      epolar_ewald_real_acc1<calc::V0>(uind, uinp);
   } else if (vers == calc::v1) {
      epolar_ewald_real_acc1<calc::V1>(uind, uinp);
   } else if (vers == calc::v3) {
      epolar_ewald_real_acc1<calc::V3>(uind, uinp);
   } else if (vers == calc::v4) {
      epolar_ewald_real_acc1<calc::V4>(uind, uinp);
   } else if (vers == calc::v5) {
      epolar_ewald_real_acc1<calc::V5>(uind, uinp);
   } else if (vers == calc::v6) {
      epolar_ewald_real_acc1<calc::V6>(uind, uinp);
   }
}

void epolar_ewald_recip_self_acc(int vers, const real (*uind)[3],
                                 const real (*uinp)[3])
{
   if (vers == calc::v0) {
      epolar_ewald_recip_self_acc1<calc::V0>(uind, uinp);
   } else if (vers == calc::v1) {
      epolar_ewald_recip_self_acc1<calc::V1>(uind, uinp);
   } else if (vers == calc::v3) {
      epolar_ewald_recip_self_acc1<calc::V3>(uind, uinp);
   } else if (vers == calc::v4) {
      epolar_ewald_recip_self_acc1<calc::V4>(uind, uinp);
   } else if (vers == calc::v5) {
      epolar_ewald_recip_self_acc1<calc::V5>(uind, uinp);
   } else if (vers == calc::v6) {
      epolar_ewald_recip_self_acc1<calc::V6>(uind, uinp);
   }
}
}
