#include "add.h"
#include "epolar.h"
#include "glob.nblist.h"
#include "image.h"
#include "md.h"
#include "seq_pair_polar.h"
#include "seq_switch.h"
#include "switch.h"
#include "tool/gpu_card.h"

namespace tinker {
#define POLAR_DPTRS                                                            \
   x, y, z, depx, depy, depz, rpole, thole, pdamp, uind, uinp, nep, ep,        \
      vir_ep, ufld, dufld
template <class Ver>
void epolar_nonewald_acc1(const real (*uind)[3], const real (*uinp)[3])
{
   constexpr bool do_e = Ver::e;
   constexpr bool do_a = Ver::a;
   constexpr bool do_g = Ver::g;
   constexpr bool do_v = Ver::v;

   if CONSTEXPR (do_g)
      darray::zero(g::q0, n, ufld, dufld);

   const real off = switch_off(switch_mpole);
   const real off2 = off * off;
   const int maxnlst = mlist_unit->maxnlst;
   const auto* mlst = mlist_unit.deviceptr();

   auto bufsize = buffer_size();
   PairPolarGrad pgrad;

   const real f = 0.5 * electric / dielec;

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
            pair_polar<do_e, do_g, NON_EWALD>( //
               r2, xr, yr, zr, 1, 1, 1,        //
               ci, dix, diy, diz, qixx, qixy, qixz, qiyy, qiyz, qizz, uix, uiy,
               uiz, uixp, uiyp, uizp, pdi, pti, //
               ck, dkx, dky, dkz, qkxx, qkxy, qkxz, qkyy, qkyz, qkzz, ukx, uky,
               ukz, ukxp, ukyp, ukzp, pdamp[k], thole[k], //
               f, 0, e, pgrad);

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

void epolar_nonewald_acc(int vers, const real (*uind)[3], const real (*uinp)[3])
{
   if (vers == calc::v0) {
      epolar_nonewald_acc1<calc::V0>(uind, uinp);
   } else if (vers == calc::v1) {
      epolar_nonewald_acc1<calc::V1>(uind, uinp);
   } else if (vers == calc::v3) {
      epolar_nonewald_acc1<calc::V3>(uind, uinp);
   } else if (vers == calc::v4) {
      epolar_nonewald_acc1<calc::V4>(uind, uinp);
   } else if (vers == calc::v5) {
      epolar_nonewald_acc1<calc::V5>(uind, uinp);
   } else if (vers == calc::v6) {
      epolar_nonewald_acc1<calc::V6>(uind, uinp);
   }
}
}
