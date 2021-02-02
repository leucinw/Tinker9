#include "add.h"
#include "epolar.h"
#include "glob.nblist.h"
#include "image.h"
#include "md.h"
#include "seq_pair_field.h"
#include "switch.h"
#include "tool/gpu_card.h"

namespace tinker {
// see also subroutine dfield0b in induce.f
#define DFIELD_DPTRS x, y, z, thole, pdamp, field, fieldp, rpole
void dfield_nonewald_acc(real (*field)[3], real (*fieldp)[3])
{
   darray::zero(g::q0, n, field, fieldp);

   const real off = switch_off(switch_mpole);
   const real off2 = off * off;
   const int maxnlst = mlist_unit->maxnlst;
   const auto* mlst = mlist_unit.deviceptr();

   MAYBE_UNUSED int GRID_DIM = get_grid_size(BLOCK_DIM);
   #pragma acc parallel async num_gangs(GRID_DIM) vector_length(BLOCK_DIM)\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(DFIELD_DPTRS,mlst)
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
      real pdi = pdamp[i];
      real pti = thole[i];
      real gxi = 0, gyi = 0, gzi = 0;
      real txi = 0, tyi = 0, tzi = 0;

      int nmlsti = mlst->nlst[i];
      int base = i * maxnlst;
      #pragma acc loop vector independent reduction(+:gxi,gyi,gzi,txi,tyi,tzi)
      for (int kk = 0; kk < nmlsti; ++kk) {
         int k = mlst->lst[base + kk];
         real xr = x[k] - xi;
         real yr = y[k] - yi;
         real zr = z[k] - zi;

         real r2 = image2(xr, yr, zr);
         if (r2 <= off2) {
            real3 fid = make_real3(0, 0, 0);
            real3 fip = make_real3(0, 0, 0);
            real3 fkd = make_real3(0, 0, 0);
            real3 fkp = make_real3(0, 0, 0);
            pair_dfield<NON_EWALD>(  //
               r2, xr, yr, zr, 1, 1, //
               ci, dix, diy, diz, qixx, qixy, qixz, qiyy, qiyz, qizz, pdi,
               pti, //
               rpole[k][mpl_pme_0], rpole[k][mpl_pme_x], rpole[k][mpl_pme_y],
               rpole[k][mpl_pme_z], rpole[k][mpl_pme_xx], rpole[k][mpl_pme_xy],
               rpole[k][mpl_pme_xz], rpole[k][mpl_pme_yy], rpole[k][mpl_pme_yz],
               rpole[k][mpl_pme_zz], pdamp[k], thole[k], //
               0, fid, fip, fkd, fkp);

            gxi += fid.x;
            gyi += fid.y;
            gzi += fid.z;
            txi += fip.x;
            tyi += fip.y;
            tzi += fip.z;

            atomic_add(fkd.x, &field[k][0]);
            atomic_add(fkd.y, &field[k][1]);
            atomic_add(fkd.z, &field[k][2]);
            atomic_add(fkp.x, &fieldp[k][0]);
            atomic_add(fkp.y, &fieldp[k][1]);
            atomic_add(fkp.z, &fieldp[k][2]);
         }
      } // end for (int kk)

      atomic_add(gxi, &field[i][0]);
      atomic_add(gyi, &field[i][1]);
      atomic_add(gzi, &field[i][2]);
      atomic_add(txi, &fieldp[i][0]);
      atomic_add(tyi, &fieldp[i][1]);
      atomic_add(tzi, &fieldp[i][2]);
   } // end for (int i)

   #pragma acc parallel async\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(DFIELD_DPTRS,dpexclude,dpexclude_scale)
   #pragma acc loop independent
   for (int ii = 0; ii < ndpexclude; ++ii) {
      int i = dpexclude[ii][0];
      int k = dpexclude[ii][1];
      real dscale = dpexclude_scale[ii][0] - 1;
      real pscale = dpexclude_scale[ii][1] - 1;

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
      real pdi = pdamp[i];
      real pti = thole[i];

      real xr = x[k] - xi;
      real yr = y[k] - yi;
      real zr = z[k] - zi;

      real r2 = image2(xr, yr, zr);
      if (r2 <= off2) {
         real3 fid = make_real3(0, 0, 0);
         real3 fip = make_real3(0, 0, 0);
         real3 fkd = make_real3(0, 0, 0);
         real3 fkp = make_real3(0, 0, 0);
         pair_dfield<NON_EWALD>(                                             //
            r2, xr, yr, zr, dscale, pscale,                                  //
            ci, dix, diy, diz, qixx, qixy, qixz, qiyy, qiyz, qizz, pdi, pti, //
            rpole[k][mpl_pme_0], rpole[k][mpl_pme_x], rpole[k][mpl_pme_y],
            rpole[k][mpl_pme_z], rpole[k][mpl_pme_xx], rpole[k][mpl_pme_xy],
            rpole[k][mpl_pme_xz], rpole[k][mpl_pme_yy], rpole[k][mpl_pme_yz],
            rpole[k][mpl_pme_zz], pdamp[k], thole[k], //
            0, fid, fip, fkd, fkp);

         atomic_add(fid.x, &field[i][0]);
         atomic_add(fid.y, &field[i][1]);
         atomic_add(fid.z, &field[i][2]);
         atomic_add(fip.x, &fieldp[i][0]);
         atomic_add(fip.y, &fieldp[i][1]);
         atomic_add(fip.z, &fieldp[i][2]);

         atomic_add(fkd.x, &field[k][0]);
         atomic_add(fkd.y, &field[k][1]);
         atomic_add(fkd.z, &field[k][2]);
         atomic_add(fkp.x, &fieldp[k][0]);
         atomic_add(fkp.y, &fieldp[k][1]);
         atomic_add(fkp.z, &fieldp[k][2]);
      }
   }
}

// see also subroutine ufield0b in induce.f
#define UFIELD_DPTRS x, y, z, thole, pdamp, field, fieldp, uind, uinp
void ufield_nonewald_acc(const real (*uind)[3], const real (*uinp)[3],
                         real (*field)[3], real (*fieldp)[3])
{
   darray::zero(g::q0, n, field, fieldp);

   const real off = switch_off(switch_mpole);
   const real off2 = off * off;
   const int maxnlst = mlist_unit->maxnlst;
   const auto* mlst = mlist_unit.deviceptr();

   MAYBE_UNUSED int GRID_DIM = get_grid_size(BLOCK_DIM);
   #pragma acc parallel async num_gangs(GRID_DIM) vector_length(BLOCK_DIM)\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(UFIELD_DPTRS,mlst)
   #pragma acc loop gang independent
   for (int i = 0; i < n; ++i) {
      real xi = x[i];
      real yi = y[i];
      real zi = z[i];
      real uindi0 = uind[i][0];
      real uindi1 = uind[i][1];
      real uindi2 = uind[i][2];
      real uinpi0 = uinp[i][0];
      real uinpi1 = uinp[i][1];
      real uinpi2 = uinp[i][2];
      real pdi = pdamp[i];
      real pti = thole[i];
      real gxi = 0, gyi = 0, gzi = 0;
      real txi = 0, tyi = 0, tzi = 0;

      int nmlsti = mlst->nlst[i];
      int base = i * maxnlst;
      #pragma acc loop vector independent reduction(+:gxi,gyi,gzi,txi,tyi,tzi)
      for (int kk = 0; kk < nmlsti; ++kk) {
         int k = mlst->lst[base + kk];
         real xr = x[k] - xi;
         real yr = y[k] - yi;
         real zr = z[k] - zi;

         real r2 = image2(xr, yr, zr);
         if (r2 <= off2) {
            real3 fid = make_real3(0, 0, 0);
            real3 fip = make_real3(0, 0, 0);
            real3 fkd = make_real3(0, 0, 0);
            real3 fkp = make_real3(0, 0, 0);
            pair_ufield<NON_EWALD>(                                      //
               r2, xr, yr, zr, 1,                                        //
               uindi0, uindi1, uindi2, uinpi0, uinpi1, uinpi2, pdi, pti, //
               uind[k][0], uind[k][1], uind[k][2], uinp[k][0], uinp[k][1],
               uinp[k][2], pdamp[k], thole[k], //
               0, fid, fip, fkd, fkp);

            gxi += fid.x;
            gyi += fid.y;
            gzi += fid.z;
            txi += fip.x;
            tyi += fip.y;
            tzi += fip.z;

            atomic_add(fkd.x, &field[k][0]);
            atomic_add(fkd.y, &field[k][1]);
            atomic_add(fkd.z, &field[k][2]);
            atomic_add(fkp.x, &fieldp[k][0]);
            atomic_add(fkp.y, &fieldp[k][1]);
            atomic_add(fkp.z, &fieldp[k][2]);
         }
      } // end for (int kk)

      atomic_add(gxi, &field[i][0]);
      atomic_add(gyi, &field[i][1]);
      atomic_add(gzi, &field[i][2]);
      atomic_add(txi, &fieldp[i][0]);
      atomic_add(tyi, &fieldp[i][1]);
      atomic_add(tzi, &fieldp[i][2]);
   } // end for (int i)

   #pragma acc parallel async\
               present(lvec1,lvec2,lvec3,recipa,recipb,recipc)\
               deviceptr(UFIELD_DPTRS,uexclude,uexclude_scale)
   #pragma acc loop independent
   for (int ii = 0; ii < nuexclude; ++ii) {
      int i = uexclude[ii][0];
      int k = uexclude[ii][1];
      real uscale = uexclude_scale[ii] - 1;

      real xi = x[i];
      real yi = y[i];
      real zi = z[i];
      real uindi0 = uind[i][0];
      real uindi1 = uind[i][1];
      real uindi2 = uind[i][2];
      real uinpi0 = uinp[i][0];
      real uinpi1 = uinp[i][1];
      real uinpi2 = uinp[i][2];
      real pdi = pdamp[i];
      real pti = thole[i];

      real xr = x[k] - xi;
      real yr = y[k] - yi;
      real zr = z[k] - zi;

      real r2 = image2(xr, yr, zr);
      if (r2 <= off2) {
         real3 fid = make_real3(0, 0, 0);
         real3 fip = make_real3(0, 0, 0);
         real3 fkd = make_real3(0, 0, 0);
         real3 fkp = make_real3(0, 0, 0);
         pair_ufield<NON_EWALD>(                                      //
            r2, xr, yr, zr, uscale,                                   //
            uindi0, uindi1, uindi2, uinpi0, uinpi1, uinpi2, pdi, pti, //
            uind[k][0], uind[k][1], uind[k][2], uinp[k][0], uinp[k][1],
            uinp[k][2], pdamp[k], thole[k], //
            0, fid, fip, fkd, fkp);

         atomic_add(fid.x, &field[i][0]);
         atomic_add(fid.y, &field[i][1]);
         atomic_add(fid.z, &field[i][2]);
         atomic_add(fip.x, &fieldp[i][0]);
         atomic_add(fip.y, &fieldp[i][1]);
         atomic_add(fip.z, &fieldp[i][2]);

         atomic_add(fkd.x, &field[k][0]);
         atomic_add(fkd.y, &field[k][1]);
         atomic_add(fkd.z, &field[k][2]);
         atomic_add(fkp.x, &fieldp[k][0]);
         atomic_add(fkp.y, &fieldp[k][1]);
         atomic_add(fkp.z, &fieldp[k][2]);
      }
   }
}
}
