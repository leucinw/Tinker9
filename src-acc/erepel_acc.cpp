#include "add.h"
#include "erepel.h"
#include "glob.nblist.h"
#include "image.h"
#include "md.h"
#include "seq_bsplgen.h"
#include "seq_damp_hippo.h"
#include "seq_pair_repel.h"
#include "seq_switch.h"
#include "switch.h"
#include "tool/gpu_card.h"


namespace tinker {
#define DEVICE_PTRS                                                            \
   x, y, z, derx, dery, derz, rpole, sizpr, dmppr, elepr, nrep, er, vir_er,    \
      trqx, trqy, trqz
template <class Ver>
void erepel_acc1()
{
   constexpr bool do_e = Ver::e;
   constexpr bool do_a = Ver::a;
   constexpr bool do_g = Ver::g;
   constexpr bool do_v = Ver::v;

   real cut = switch_cut(switch_repuls);
   real off = switch_off(switch_repuls);

   const real off2 = off * off;
   const int maxnlst = mlist_unit->maxnlst;
   const auto* mlst = mlist_unit.deviceptr();

   size_t bufsize = buffer_size();

   PairRepelGrad pgrad;


   MAYBE_UNUSED int GRID_DIM = get_grid_size(BLOCK_DIM);
   #pragma acc parallel async num_gangs(GRID_DIM) vector_length(BLOCK_DIM)\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(DEVICE_PTRS,mlst)
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
      real sizi = sizpr[i];
      real dmpi = dmppr[i];
      real vali = elepr[i];

      MAYBE_UNUSED real gxi = 0, gyi = 0, gzi = 0;
      MAYBE_UNUSED real txi = 0, tyi = 0, tzi = 0;

      int nmlsti = mlst->nlst[i];
      int base = i * maxnlst;
      #pragma acc loop vector independent private(pgrad)\
                  reduction(+:gxi,gyi,gzi,txi,tyi,tzi)
      for (int kk = 0; kk < nmlsti; ++kk) {
         int offset = (kk + i * n) & (bufsize - 1);
         int k = mlst->lst[base + kk];
         real xr = x[k] - xi;
         real yr = y[k] - yi;
         real zr = z[k] - zi;
         real sizk = sizpr[k];
         real dmpk = dmppr[k];
         real valk = elepr[k];

         real r2 = image2(xr, yr, zr);
         if (r2 <= off2) {
            real e;
            zero(pgrad);
            pair_repel<do_g>( //
               r2, 1, cut, off, xr, yr, zr, sizi, dmpi, vali, ci, dix, diy, diz,
               qixx, qixy, qixz, qiyy, qiyz, qizz, sizk, dmpk, valk,
               rpole[k][mpl_pme_0], rpole[k][mpl_pme_x], rpole[k][mpl_pme_y],
               rpole[k][mpl_pme_z], rpole[k][mpl_pme_xx], rpole[k][mpl_pme_xy],
               rpole[k][mpl_pme_xz], rpole[k][mpl_pme_yy], rpole[k][mpl_pme_yz],
               rpole[k][mpl_pme_zz], e, pgrad);
            if CONSTEXPR (do_a)
               if (e != 0)
                  atomic_add(1, nrep, offset);
            if CONSTEXPR (do_e)
               atomic_add(e, er, offset);
            if CONSTEXPR (do_g) {
               gxi += pgrad.frcx;
               gyi += pgrad.frcy;
               gzi += pgrad.frcz;
               atomic_add(-pgrad.frcx, derx, k);
               atomic_add(-pgrad.frcy, dery, k);
               atomic_add(-pgrad.frcz, derz, k);

               txi += pgrad.ttqi[0];
               tyi += pgrad.ttqi[1];
               tzi += pgrad.ttqi[2];
               atomic_add(pgrad.ttqk[0], trqx, k);
               atomic_add(pgrad.ttqk[1], trqy, k);
               atomic_add(pgrad.ttqk[2], trqz, k);

               // virial

               if CONSTEXPR (do_v) {
                  real vxx = -xr * pgrad.frcx;
                  real vxy = -0.5f * (yr * pgrad.frcx + xr * pgrad.frcy);
                  real vxz = -0.5f * (zr * pgrad.frcx + xr * pgrad.frcz);
                  real vyy = -yr * pgrad.frcy;
                  real vyz = -0.5f * (zr * pgrad.frcy + yr * pgrad.frcz);
                  real vzz = -zr * pgrad.frcz;

                  atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_er, offset);
               } // end if (do_v)
            }    // end if (do_g)
         }       // end if (r2 <= off2)
      }          // end for (int kk)
      if CONSTEXPR (do_g) {
         atomic_add(gxi, derx, i);
         atomic_add(gyi, dery, i);
         atomic_add(gzi, derz, i);
         atomic_add(txi, trqx, i);
         atomic_add(tyi, trqy, i);
         atomic_add(tzi, trqz, i);
      }
   } // end for (int i)


   #pragma acc parallel async\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(DEVICE_PTRS,repexclude,repexclude_scale)
   #pragma acc loop independent private(pgrad)
   for (int ii = 0; ii < nrepexclude; ++ii) {
      int offset = ii & (bufsize - 1);

      int i = repexclude[ii][0];
      int k = repexclude[ii][1];
      real rscale = repexclude_scale[ii] - 1;

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
      real sizi = sizpr[i];
      real dmpi = dmppr[i];
      real vali = elepr[i];
      real xr = x[k] - xi;
      real yr = y[k] - yi;
      real zr = z[k] - zi;
      real sizk = sizpr[k];
      real dmpk = dmppr[k];
      real valk = elepr[k];

      zero(pgrad);
      real r2 = image2(xr, yr, zr);
      if (r2 <= off2 and rscale != 0) {
         real e;
         pair_repel<do_g>( //
            r2, rscale, cut, off, xr, yr, zr, sizi, dmpi, vali, ci, dix, diy,
            diz, qixx, qixy, qixz, qiyy, qiyz, qizz, sizk, dmpk, valk,
            rpole[k][mpl_pme_0], rpole[k][mpl_pme_x], rpole[k][mpl_pme_y],
            rpole[k][mpl_pme_z], rpole[k][mpl_pme_xx], rpole[k][mpl_pme_xy],
            rpole[k][mpl_pme_xz], rpole[k][mpl_pme_yy], rpole[k][mpl_pme_yz],
            rpole[k][mpl_pme_zz], e, pgrad);
         if CONSTEXPR (do_a)
            if (rscale == -1 and e != 0)
               atomic_add(-1, nrep, offset);
         if CONSTEXPR (do_e)
            atomic_add(e, er, offset);
         if CONSTEXPR (do_g) {
            atomic_add(pgrad.frcx, derx, i);
            atomic_add(pgrad.frcy, dery, i);
            atomic_add(pgrad.frcz, derz, i);
            atomic_add(-pgrad.frcx, derx, k);
            atomic_add(-pgrad.frcy, dery, k);
            atomic_add(-pgrad.frcz, derz, k);

            atomic_add(pgrad.ttqi[0], trqx, i);
            atomic_add(pgrad.ttqi[1], trqy, i);
            atomic_add(pgrad.ttqi[2], trqz, i);
            atomic_add(pgrad.ttqk[0], trqx, k);
            atomic_add(pgrad.ttqk[1], trqy, k);
            atomic_add(pgrad.ttqk[2], trqz, k);

            // virial

            if CONSTEXPR (do_v) {
               real vxx = -xr * pgrad.frcx;
               real vxy = -0.5f * (yr * pgrad.frcx + xr * pgrad.frcy);
               real vxz = -0.5f * (zr * pgrad.frcx + xr * pgrad.frcz);
               real vyy = -yr * pgrad.frcy;
               real vyz = -0.5f * (zr * pgrad.frcy + yr * pgrad.frcz);
               real vzz = -zr * pgrad.frcz;

               atomic_add(vxx, vxy, vxz, vyy, vyz, vzz, vir_er, offset);
            } // end if (do_v)
         }    // end if (do_g)
      }       // end if (r2 <= off2)
   }          // end for (int i)
}

void erepel_acc(int vers)
{
   if (vers == calc::v0)
      erepel_acc1<calc::V0>();
   else if (vers == calc::v1)
      erepel_acc1<calc::V1>();
   else if (vers == calc::v3)
      erepel_acc1<calc::V3>();
   else if (vers == calc::v4)
      erepel_acc1<calc::V4>();
   else if (vers == calc::v5)
      erepel_acc1<calc::V5>();
   else if (vers == calc::v6)
      erepel_acc1<calc::V6>();
}
}
