#include "add.h"
#include "box.h"
#include "echglj.h"
#include "glob.spatial.h"
#include "image.h"
#include "launch.h"
#include "mathfunc.h"
#include "md.h"
#include "osrw.h"
#include "seq_pair_charge.h"
#include "seq_pair_lj.h"
#include "seq_triangle.h"
#include "spatial2.h"
#include "switch.h"
#include "tool/gpu_card.h"


extern "C"
{
   using real = tinker::real;


   struct RAD_GEOM
   {
      __device__
      static real avg(real ra, real rb)
      {
         return 2 * REAL_SQRT(ra * rb);
      }
   };


   struct RAD_ARITH
   {
      __device__
      static real avg(real ra, real rb)
      {
         return (ra + rb);
      }
   };


   struct EPS_GEOM
   {
      __device__
      static real avg(real a, real b)
      {
         return REAL_SQRT(a * b);
      }


      __device__
      static real savg(real a, real b)
      {
         return a * b;
      }
   };


   struct EPS_ARITH
   {
      __device__
      static real avg(real a, real b)
      {
         return 0.5f * (a + b);
      }
   };
}


namespace tinker {
void echglj_data_cu(rc_op op)
{
   if (op & rc_dealloc) {
      use_pme_stream = false;
   }


   if (op & rc_alloc) {
      use_pme_stream = true;
   }
}


void pme_stream_start_record_cu(bool use_pmestream)
{
   if (use_pmestream) {
      check_rt(cudaEventRecord(pme_event_start, g::s0));
   }
}
void pme_stream_start_wait_cu(bool use_pmestream)
{
   if (use_pmestream) {
      check_rt(cudaStreamWaitEvent(g::spme, pme_event_start, 0));
   }
}
void pme_stream_finish_record_cu(bool use_pmestream)
{
   if (use_pmestream) {
      check_rt(cudaEventRecord(pme_event_finish, g::spme));
   }
}
void pme_stream_finish_wait_cu(bool use_pmestream)
{
   if (use_pmestream) {
      check_rt(cudaStreamWaitEvent(g::s0, pme_event_finish, 0));
   }
}


template <class Ver, class IMG, class ETYP, class RADRULE, class EPSRULE,
          bool VOUT>
__global__
void echglj_cu5(energy_buffer restrict ebuf, virial_buffer restrict vbuf,
                grad_prec* restrict gx, grad_prec* restrict gy,
                grad_prec* restrict gz, TINKER_IMAGE_PARAMS, //
                int n, const Spatial::SortedAtom* restrict sorted, int nakpl,
                const int* restrict iakpl, int niak, const int* restrict iak,
                const int* restrict lst, //
                const int* restrict bnum,
                const Spatial2::Center* restrict akc, //
                int nexclude, const int (*restrict exclude)[2],
                const real (*restrict exclude_scale)[2], //
                real eccut, real ecoff, real f, real aewald,
                const real* restrict chg,
                const unsigned int* restrict cvinfo,                  //
                real evcut, real evoff, const real2* restrict radeps, //
                energy_buffer restrict ev, virial_buffer restrict vev,
                grad_prec* restrict devx, grad_prec* restrict devy,
                grad_prec* restrict devz)
{
   constexpr bool do_e = Ver::e;
   constexpr bool do_g = Ver::g;
   constexpr bool do_v = Ver::v;
   const int ithread = threadIdx.x + blockIdx.x * blockDim.x;
   const int iwarp = ithread / WARP_SIZE;
   const int nwarp = blockDim.x * gridDim.x / WARP_SIZE;
   const int ilane = threadIdx.x & (WARP_SIZE - 1);


   using ebuf_prec = energy_buffer_traits::type;
   ebuf_prec ectl;
   if CONSTEXPR (do_e) {
      ectl = 0;
   }
   using vbuf_prec = virial_buffer_traits::type;
   vbuf_prec vctlxx, vctlyx, vctlzx, vctlyy, vctlzy, vctlzz;
   if CONSTEXPR (do_v) {
      vctlxx = 0;
      vctlyx = 0;
      vctlzx = 0;
      vctlyy = 0;
      vctlzy = 0;
      vctlzz = 0;
   }
   ebuf_prec evtl;
   if CONSTEXPR (do_e and VOUT) {
      evtl = 0;
   }
   vbuf_prec vvtlxx, vvtlyx, vvtlzx, vvtlyy, vvtlzy, vvtlzz;
   if CONSTEXPR (do_v and VOUT) {
      vvtlxx = 0;
      vvtlyx = 0;
      vvtlzx = 0;
      vvtlyy = 0;
      vvtlzy = 0;
      vvtlzz = 0;
   }


   real xi, yi, zi, chgi, radi, epsi;
   real xk, yk, zk, chgk, radk, epsk;
   real fix, fiy, fiz, fkx, fky, fkz;
   real ivfx, ivfy, ivfz, kvfx, kvfy, kvfz;


   //* /
   for (int ii = ithread; ii < nexclude; ii += blockDim.x * gridDim.x) {
      int i = exclude[ii][0];
      int k = exclude[ii][1];
      real cscale = exclude_scale[ii][0];
      real vscale = exclude_scale[ii][1];
      int atomi = bnum[i];
      int atomk = bnum[k];


      xi = sorted[atomi].x;
      yi = sorted[atomi].y;
      zi = sorted[atomi].z;
      xk = sorted[atomk].x;
      yk = sorted[atomk].y;
      zk = sorted[atomk].z;
      chgi = chg[atomi];
      radi = radeps[atomi].x;
      epsi = radeps[atomi].y;
      chgk = chg[atomk];
      radk = radeps[atomk].x;
      epsk = radeps[atomk].y;


      real xr = xi - xk;
      real yr = yi - yk;
      real zr = zi - zk;
      real r2 =
         IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS, TINKER_IMAGE_RECIP_ARGS);
      real r = REAL_SQRT(r2);
      real invr = REAL_RECIP(r);
      real ecik, decik;
      real evik, devik;
      pair_chg_v2<do_g, ETYP, 0>( //
         r, invr, cscale, chgi, chgk, f, aewald, eccut, ecoff, ecik, decik);
      pair_lj_v2<do_g, RADRULE, EPSRULE, 0>( //
         r, invr, vscale, radi, epsi, radk, epsk, evcut, evoff, evik, devik);
      if CONSTEXPR (do_e and not VOUT) {
         ectl += cvt_to<ebuf_prec>(ecik + evik);
      } else if CONSTEXPR (do_e and VOUT) {
         ectl += cvt_to<ebuf_prec>(ecik);
         evtl += cvt_to<ebuf_prec>(evik);
      }
      if CONSTEXPR (do_g and not VOUT) {
         real dedx, dedy, dedz;
         decik += devik;
         decik *= invr;
         dedx = decik * xr;
         dedy = decik * yr;
         dedz = decik * zr;
         atomic_add(dedx, gx, i);
         atomic_add(dedy, gy, i);
         atomic_add(dedz, gz, i);
         atomic_add(-dedx, gx, k);
         atomic_add(-dedy, gy, k);
         atomic_add(-dedz, gz, k);
         if CONSTEXPR (do_v) {
            vctlxx += cvt_to<vbuf_prec>(xr * dedx);
            vctlyx += cvt_to<vbuf_prec>(yr * dedx);
            vctlzx += cvt_to<vbuf_prec>(zr * dedx);
            vctlyy += cvt_to<vbuf_prec>(yr * dedy);
            vctlzy += cvt_to<vbuf_prec>(zr * dedy);
            vctlzz += cvt_to<vbuf_prec>(zr * dedz);
         }
      } else if CONSTEXPR (do_g and VOUT) {
         real dedx, dedy, dedz;
         decik *= invr;
         dedx = decik * xr;
         dedy = decik * yr;
         dedz = decik * zr;
         atomic_add(dedx, gx, i);
         atomic_add(dedy, gy, i);
         atomic_add(dedz, gz, i);
         atomic_add(-dedx, gx, k);
         atomic_add(-dedy, gy, k);
         atomic_add(-dedz, gz, k);
         if CONSTEXPR (do_v) {
            vctlxx += cvt_to<vbuf_prec>(xr * dedx);
            vctlyx += cvt_to<vbuf_prec>(yr * dedx);
            vctlzx += cvt_to<vbuf_prec>(zr * dedx);
            vctlyy += cvt_to<vbuf_prec>(yr * dedy);
            vctlzy += cvt_to<vbuf_prec>(zr * dedy);
            vctlzz += cvt_to<vbuf_prec>(zr * dedz);
         }
         devik *= invr;
         dedx = devik * xr;
         dedy = devik * yr;
         dedz = devik * zr;
         atomic_add(dedx, devx, i);
         atomic_add(dedy, devy, i);
         atomic_add(dedz, devz, i);
         atomic_add(-dedx, devx, k);
         atomic_add(-dedy, devy, k);
         atomic_add(-dedz, devz, k);
         if CONSTEXPR (do_v) {
            vvtlxx += cvt_to<vbuf_prec>(xr * dedx);
            vvtlyx += cvt_to<vbuf_prec>(yr * dedx);
            vvtlzx += cvt_to<vbuf_prec>(zr * dedx);
            vvtlyy += cvt_to<vbuf_prec>(yr * dedy);
            vvtlzy += cvt_to<vbuf_prec>(zr * dedy);
            vvtlzz += cvt_to<vbuf_prec>(zr * dedz);
         }
      }
   }
   // */


   for (int iw = iwarp; iw < nakpl; iw += nwarp) {
      if CONSTEXPR (do_g) {
         fix = 0;
         fiy = 0;
         fiz = 0;
         fkx = 0;
         fky = 0;
         fkz = 0;
      }
      if CONSTEXPR (do_g and VOUT) {
         ivfx = 0;
         ivfy = 0;
         ivfz = 0;
         kvfx = 0;
         kvfy = 0;
         kvfz = 0;
      }


      int tri, tx, ty;
      tri = iakpl[iw];
      tri_to_xy(tri, tx, ty);


      int iid = ty * WARP_SIZE + ilane;
      int atomi = min(iid, n - 1);
      int i = sorted[atomi].unsorted;
      int kid = tx * WARP_SIZE + ilane;
      int atomk = min(kid, n - 1);
      int k = sorted[atomk].unsorted;
      xi = sorted[atomi].x;
      yi = sorted[atomi].y;
      zi = sorted[atomi].z;
      xk = sorted[atomk].x;
      yk = sorted[atomk].y;
      zk = sorted[atomk].z;


      chgi = chg[atomi];
      radi = radeps[atomi].x;
      epsi = radeps[atomi].y;
      chgk = chg[atomk];
      radk = radeps[atomk].x;
      epsk = radeps[atomk].y;


      real xc = akc[ty].x;
      real yc = akc[ty].y;
      real zc = akc[ty].z;
      const bool ilocal = akc[ty].w != 0;
      if (ilocal) {
         real xr, yr, zr;
         xr = xi - xc;
         yr = yi - yc;
         zr = zi - zc;
         IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS, TINKER_IMAGE_RECIP_ARGS);
         xi = xr + xc;
         yi = yr + yc;
         zi = zr + zc;
         xr = xk - xc;
         yr = yk - yc;
         zr = zk - zc;
         IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS, TINKER_IMAGE_RECIP_ARGS);
         xk = xr + xc;
         yk = yr + yc;
         zk = zr + zc;
      }


      int cvinfo0 = cvinfo[iw * WARP_SIZE + ilane];
      if (ilocal) {
         for (int j = 0; j < WARP_SIZE; ++j) {
            int srclane = (ilane + j) & (WARP_SIZE - 1);
            bool incl = iid < kid and kid < n;
            int srcmask = 1 << srclane;
            incl = incl and (cvinfo0 & srcmask) == 0;
            real xr = xi - xk;
            real yr = yi - yk;
            real zr = zi - zk;
            real r2 = xr * xr + yr * yr + zr * zr;
            real r = REAL_SQRT(r2);
            real invr = REAL_RECIP(r);
            real ecik, decik;
            real evik, devik;
            pair_chg_v2<do_g, ETYP, 1>( //
               r, invr, 1, chgi, chgk, f, aewald, eccut, ecoff, ecik, decik);
            pair_lj_v2<do_g, RADRULE, EPSRULE, 1>( //
               r, invr, 1, radi, epsi, radk, epsk, evcut, evoff, evik, devik);
            if CONSTEXPR (do_e and not VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik + evik) : 0;
            } else if CONSTEXPR (do_e and VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik) : 0;
               evtl += incl ? cvt_to<ebuf_prec>(evik) : 0;
            }
            if CONSTEXPR (do_g and not VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? (decik + devik) * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vctlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vctlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vctlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vctlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vctlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
            } else if CONSTEXPR (do_g and VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? decik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vctlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vctlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vctlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vctlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vctlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
               devik = incl ? devik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vvtlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vvtlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vvtlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vvtlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vvtlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vvtlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               ivfx += dedx;
               ivfy += dedy;
               ivfz += dedz;
               kvfx -= dedx;
               kvfy -= dedy;
               kvfz -= dedz;
            }


            iid = __shfl_sync(ALL_LANES, iid, ilane + 1);
            chgi = __shfl_sync(ALL_LANES, chgi, ilane + 1);
            radi = __shfl_sync(ALL_LANES, radi, ilane + 1);
            epsi = __shfl_sync(ALL_LANES, epsi, ilane + 1);
            xi = __shfl_sync(ALL_LANES, xi, ilane + 1);
            yi = __shfl_sync(ALL_LANES, yi, ilane + 1);
            zi = __shfl_sync(ALL_LANES, zi, ilane + 1);
            fix = __shfl_sync(ALL_LANES, fix, ilane + 1);
            fiy = __shfl_sync(ALL_LANES, fiy, ilane + 1);
            fiz = __shfl_sync(ALL_LANES, fiz, ilane + 1);
            if CONSTEXPR (VOUT) {
               ivfx = __shfl_sync(ALL_LANES, ivfx, ilane + 1);
               ivfy = __shfl_sync(ALL_LANES, ivfy, ilane + 1);
               ivfz = __shfl_sync(ALL_LANES, ivfz, ilane + 1);
            }
         }


         if CONSTEXPR (do_v) {
            vctlxx += cvt_to<vbuf_prec>(xi * fix + xk * fkx);
            vctlyx += cvt_to<vbuf_prec>(yi * fix + yk * fkx);
            vctlzx += cvt_to<vbuf_prec>(zi * fix + zk * fkx);
            vctlyy += cvt_to<vbuf_prec>(yi * fiy + yk * fky);
            vctlzy += cvt_to<vbuf_prec>(zi * fiy + zk * fky);
            vctlzz += cvt_to<vbuf_prec>(zi * fiz + zk * fkz);
            if CONSTEXPR (VOUT) {
               vvtlyx += cvt_to<vbuf_prec>(yi * ivfx + yk * kvfy);
               vvtlxx += cvt_to<vbuf_prec>(xi * ivfx + xk * kvfx);
               vvtlzx += cvt_to<vbuf_prec>(zi * ivfx + zk * kvfz);
               vvtlyy += cvt_to<vbuf_prec>(yi * ivfy + yk * kvfy);
               vvtlzy += cvt_to<vbuf_prec>(zi * ivfy + zk * kvfz);
               vvtlzz += cvt_to<vbuf_prec>(zi * ivfz + zk * kvfz);
            }
         }
      } else {
         for (int j = 0; j < WARP_SIZE; ++j) {
            int srclane = (ilane + j) & (WARP_SIZE - 1);
            bool incl = iid < kid and kid < n;
            int srcmask = 1 << srclane;
            incl = incl and (cvinfo0 & srcmask) == 0;
            real xr = xi - xk;
            real yr = yi - yk;
            real zr = zi - zk;
            real r2 = IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS,
                                TINKER_IMAGE_RECIP_ARGS);
            real r = REAL_SQRT(r2);
            real invr = REAL_RECIP(r);
            real ecik, decik;
            real evik, devik;
            pair_chg_v2<do_g, ETYP, 1>( //
               r, invr, 1, chgi, chgk, f, aewald, eccut, ecoff, ecik, decik);
            pair_lj_v2<do_g, RADRULE, EPSRULE, 1>( //
               r, invr, 1, radi, epsi, radk, epsk, evcut, evoff, evik, devik);
            if CONSTEXPR (do_e and not VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik + evik) : 0;
            } else if CONSTEXPR (do_e and VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik) : 0;
               evtl += incl ? cvt_to<ebuf_prec>(evik) : 0;
            }
            if CONSTEXPR (do_g and not VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? (decik + devik) * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vctlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vctlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vctlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vctlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vctlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
            } else if CONSTEXPR (do_g and VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? decik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vctlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vctlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vctlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vctlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vctlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
               devik = incl ? devik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vvtlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vvtlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vvtlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vvtlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vvtlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vvtlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               ivfx += dedx;
               ivfy += dedy;
               ivfz += dedz;
               kvfx -= dedx;
               kvfy -= dedy;
               kvfz -= dedz;
            }


            iid = __shfl_sync(ALL_LANES, iid, ilane + 1);
            chgi = __shfl_sync(ALL_LANES, chgi, ilane + 1);
            radi = __shfl_sync(ALL_LANES, radi, ilane + 1);
            epsi = __shfl_sync(ALL_LANES, epsi, ilane + 1);
            xi = __shfl_sync(ALL_LANES, xi, ilane + 1);
            yi = __shfl_sync(ALL_LANES, yi, ilane + 1);
            zi = __shfl_sync(ALL_LANES, zi, ilane + 1);
            fix = __shfl_sync(ALL_LANES, fix, ilane + 1);
            fiy = __shfl_sync(ALL_LANES, fiy, ilane + 1);
            fiz = __shfl_sync(ALL_LANES, fiz, ilane + 1);
            if CONSTEXPR (VOUT) {
               ivfx = __shfl_sync(ALL_LANES, ivfx, ilane + 1);
               ivfy = __shfl_sync(ALL_LANES, ivfy, ilane + 1);
               ivfz = __shfl_sync(ALL_LANES, ivfz, ilane + 1);
            }
         }
      } // end if ilocal


      if CONSTEXPR (do_g) {
         atomic_add(fix, gx, i);
         atomic_add(fiy, gy, i);
         atomic_add(fiz, gz, i);
         atomic_add(fkx, gx, k);
         atomic_add(fky, gy, k);
         atomic_add(fkz, gz, k);
      }
      if CONSTEXPR (do_g and VOUT) {
         atomic_add(ivfx, devx, i);
         atomic_add(ivfy, devy, i);
         atomic_add(ivfz, devz, i);
         atomic_add(kvfx, devx, k);
         atomic_add(kvfy, devy, k);
         atomic_add(kvfz, devz, k);
      }
   }


   for (int iw = iwarp; iw < niak; iw += nwarp) {
      if CONSTEXPR (do_g) {
         fix = 0;
         fiy = 0;
         fiz = 0;
         fkx = 0;
         fky = 0;
         fkz = 0;
      }
      if CONSTEXPR (do_g and VOUT) {
         ivfx = 0;
         ivfy = 0;
         ivfz = 0;
         kvfx = 0;
         kvfy = 0;
         kvfz = 0;
      }


      int ty = iak[iw];
      int atomi = ty * WARP_SIZE + ilane;
      int i = sorted[atomi].unsorted;
      int atomk = lst[iw * WARP_SIZE + ilane];
      int k = sorted[atomk].unsorted;
      xi = sorted[atomi].x;
      yi = sorted[atomi].y;
      zi = sorted[atomi].z;
      xk = sorted[atomk].x;
      yk = sorted[atomk].y;
      zk = sorted[atomk].z;
      chgi = chg[atomi];
      radi = radeps[atomi].x;
      epsi = radeps[atomi].y;
      chgk = chg[atomk];
      radk = radeps[atomk].x;
      epsk = radeps[atomk].y;


      real xc = akc[ty].x;
      real yc = akc[ty].y;
      real zc = akc[ty].z;
      const bool ilocal = akc[ty].w != 0;
      if (ilocal) {
         real xr, yr, zr;
         xr = xi - xc;
         yr = yi - yc;
         zr = zi - zc;
         IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS, TINKER_IMAGE_RECIP_ARGS);
         xi = xr + xc;
         yi = yr + yc;
         zi = zr + zc;
         xr = xk - xc;
         yr = yk - yc;
         zr = zk - zc;
         IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS, TINKER_IMAGE_RECIP_ARGS);
         xk = xr + xc;
         yk = yr + yc;
         zk = zr + zc;
      }


      if (ilocal) {
         for (int j = 0; j < WARP_SIZE; ++j) {
            bool incl = atomk > 0;
            real xr = xi - xk;
            real yr = yi - yk;
            real zr = zi - zk;
            real r2 = xr * xr + yr * yr + zr * zr;
            real r = REAL_SQRT(r2);
            real invr = REAL_RECIP(r);
            real ecik, decik;
            real evik, devik;
            pair_chg_v2<do_g, ETYP, 1>( //
               r, invr, 1, chgi, chgk, f, aewald, eccut, ecoff, ecik, decik);
            pair_lj_v2<do_g, RADRULE, EPSRULE, 1>( //
               r, invr, 1, radi, epsi, radk, epsk, evcut, evoff, evik, devik);
            if CONSTEXPR (do_e and not VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik + evik) : 0;
            } else if CONSTEXPR (do_e and VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik) : 0;
               evtl += incl ? cvt_to<ebuf_prec>(evik) : 0;
            }
            if CONSTEXPR (do_g and not VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? (decik + devik) * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vctlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vctlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vctlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vctlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vctlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
            } else if CONSTEXPR (do_g and VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? decik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vctlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vctlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vctlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vctlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vctlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
               devik = incl ? devik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               // if CONSTEXPR (do_v) {
               //    vvtlyx += cvt_to<vbuf_prec>(yr * dedx);
               //    vvtlxx += cvt_to<vbuf_prec>(xr * dedx);
               //    vvtlzx += cvt_to<vbuf_prec>(zr * dedx);
               //    vvtlyy += cvt_to<vbuf_prec>(yr * dedy);
               //    vvtlzy += cvt_to<vbuf_prec>(zr * dedy);
               //    vvtlzz += cvt_to<vbuf_prec>(zr * dedz);
               // }
               ivfx += dedx;
               ivfy += dedy;
               ivfz += dedz;
               kvfx -= dedx;
               kvfy -= dedy;
               kvfz -= dedz;
            }


            chgi = __shfl_sync(ALL_LANES, chgi, ilane + 1);
            radi = __shfl_sync(ALL_LANES, radi, ilane + 1);
            epsi = __shfl_sync(ALL_LANES, epsi, ilane + 1);
            xi = __shfl_sync(ALL_LANES, xi, ilane + 1);
            yi = __shfl_sync(ALL_LANES, yi, ilane + 1);
            zi = __shfl_sync(ALL_LANES, zi, ilane + 1);
            fix = __shfl_sync(ALL_LANES, fix, ilane + 1);
            fiy = __shfl_sync(ALL_LANES, fiy, ilane + 1);
            fiz = __shfl_sync(ALL_LANES, fiz, ilane + 1);
            if CONSTEXPR (VOUT) {
               ivfx = __shfl_sync(ALL_LANES, ivfx, ilane + 1);
               ivfy = __shfl_sync(ALL_LANES, ivfy, ilane + 1);
               ivfz = __shfl_sync(ALL_LANES, ivfz, ilane + 1);
            }
         }


         if CONSTEXPR (do_v) {
            vctlxx += cvt_to<vbuf_prec>(xi * fix + xk * fkx);
            vctlyx += cvt_to<vbuf_prec>(yi * fix + yk * fkx);
            vctlzx += cvt_to<vbuf_prec>(zi * fix + zk * fkx);
            vctlyy += cvt_to<vbuf_prec>(yi * fiy + yk * fky);
            vctlzy += cvt_to<vbuf_prec>(zi * fiy + zk * fky);
            vctlzz += cvt_to<vbuf_prec>(zi * fiz + zk * fkz);
            if CONSTEXPR (VOUT) {
               vvtlyx += cvt_to<vbuf_prec>(yi * ivfx + yk * kvfy);
               vvtlxx += cvt_to<vbuf_prec>(xi * ivfx + xk * kvfx);
               vvtlzx += cvt_to<vbuf_prec>(zi * ivfx + zk * kvfz);
               vvtlyy += cvt_to<vbuf_prec>(yi * ivfy + yk * kvfy);
               vvtlzy += cvt_to<vbuf_prec>(zi * ivfy + zk * kvfz);
               vvtlzz += cvt_to<vbuf_prec>(zi * ivfz + zk * kvfz);
            }
         }
      } else {
         for (int j = 0; j < WARP_SIZE; ++j) {
            bool incl = atomk > 0;
            real xr = xi - xk;
            real yr = yi - yk;
            real zr = zi - zk;
            real r2 = IMG::img2(xr, yr, zr, TINKER_IMAGE_LVEC_ARGS,
                                TINKER_IMAGE_RECIP_ARGS);
            real r = REAL_SQRT(r2);
            real invr = REAL_RECIP(r);
            real ecik, decik;
            real evik, devik;
            pair_chg_v2<do_g, ETYP, 1>( //
               r, invr, 1, chgi, chgk, f, aewald, eccut, ecoff, ecik, decik);
            pair_lj_v2<do_g, RADRULE, EPSRULE, 1>( //
               r, invr, 1, radi, epsi, radk, epsk, evcut, evoff, evik, devik);
            if CONSTEXPR (do_e and not VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik + evik) : 0;
            } else if CONSTEXPR (do_e and VOUT) {
               ectl += incl ? cvt_to<ebuf_prec>(ecik) : 0;
               evtl += incl ? cvt_to<ebuf_prec>(evik) : 0;
            }
            if CONSTEXPR (do_g and not VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? (decik + devik) * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vctlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vctlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vctlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vctlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vctlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
            } else if CONSTEXPR (do_g and VOUT) {
               real dedx, dedy, dedz;
               decik = incl ? decik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vctlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vctlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vctlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vctlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vctlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vctlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               fix += dedx;
               fiy += dedy;
               fiz += dedz;
               fkx -= dedx;
               fky -= dedy;
               fkz -= dedz;
               devik = incl ? devik * invr : 0;
               dedx = decik * xr;
               dedy = decik * yr;
               dedz = decik * zr;
               if CONSTEXPR (do_v) {
                  vvtlyx += cvt_to<vbuf_prec>(yr * dedx);
                  vvtlxx += cvt_to<vbuf_prec>(xr * dedx);
                  vvtlzx += cvt_to<vbuf_prec>(zr * dedx);
                  vvtlyy += cvt_to<vbuf_prec>(yr * dedy);
                  vvtlzy += cvt_to<vbuf_prec>(zr * dedy);
                  vvtlzz += cvt_to<vbuf_prec>(zr * dedz);
               }
               ivfx += dedx;
               ivfy += dedy;
               ivfz += dedz;
               kvfx -= dedx;
               kvfy -= dedy;
               kvfz -= dedz;
            }


            chgi = __shfl_sync(ALL_LANES, chgi, ilane + 1);
            radi = __shfl_sync(ALL_LANES, radi, ilane + 1);
            epsi = __shfl_sync(ALL_LANES, epsi, ilane + 1);
            xi = __shfl_sync(ALL_LANES, xi, ilane + 1);
            yi = __shfl_sync(ALL_LANES, yi, ilane + 1);
            zi = __shfl_sync(ALL_LANES, zi, ilane + 1);
            fix = __shfl_sync(ALL_LANES, fix, ilane + 1);
            fiy = __shfl_sync(ALL_LANES, fiy, ilane + 1);
            fiz = __shfl_sync(ALL_LANES, fiz, ilane + 1);
            if CONSTEXPR (VOUT) {
               ivfx = __shfl_sync(ALL_LANES, ivfx, ilane + 1);
               ivfy = __shfl_sync(ALL_LANES, ivfy, ilane + 1);
               ivfz = __shfl_sync(ALL_LANES, ivfz, ilane + 1);
            }
         }
      } // end if ilocal


      if CONSTEXPR (do_g) {
         atomic_add(fix, gx, i);
         atomic_add(fiy, gy, i);
         atomic_add(fiz, gz, i);
         atomic_add(fkx, gx, k);
         atomic_add(fky, gy, k);
         atomic_add(fkz, gz, k);
      }
      if CONSTEXPR (do_g and VOUT) {
         atomic_add(ivfx, devx, i);
         atomic_add(ivfy, devy, i);
         atomic_add(ivfz, devz, i);
         atomic_add(kvfx, devx, k);
         atomic_add(kvfy, devy, k);
         atomic_add(kvfz, devz, k);
      }
   }


   if CONSTEXPR (do_e) {
      atomic_add(ectl, ebuf, ithread);
   }
   if CONSTEXPR (do_v) {
      atomic_add(vctlxx, vctlyx, vctlzx, vctlyy, vctlzy, vctlzz, vbuf, ithread);
   }
   if CONSTEXPR (do_e and VOUT) {
      atomic_add(evtl, ev, ithread);
   }
   if CONSTEXPR (do_v and VOUT) {
      atomic_add(vvtlxx, vvtlyx, vvtlzx, vvtlyy, vvtlzy, vvtlzz, vev, ithread);
   }
}


template <class RADRULE, class EPSRULE>
__global__
void echglj_coalesce(int n, real* restrict schg, real2* restrict svdw, //
                     const Spatial::SortedAtom* restrict sorted,
                     const real* restrict chg, const real* restrict rad,
                     const real* restrict eps)
{
   for (int i = threadIdx.x + blockIdx.x * blockDim.x; i < n;
        i += blockDim.x * gridDim.x) {
      int iold = sorted[i].unsorted;
      schg[i] = chg[iold];
      real radold = rad[iold];
      real epsold = eps[iold];
      if CONSTEXPR (eq<RADRULE, RAD_ARITH>()) {
         svdw[i].x = radold;
      }
      if CONSTEXPR (eq<EPSRULE, EPS_GEOM>()) {
         svdw[i].y = REAL_SQRT(epsold);
      }
   }
}


template <class Ver, class ETYP, class RADRULE, class EPSRULE, bool VOUT>
void echglj_cu3()
{
   auto& st = *cspatial_v2_unit;


   if (st.fresh & cspatial_fresh_mask_echglj) {
      auto ker = echglj_coalesce<RADRULE, EPSRULE>;
      launch_k1s(g::s0, st.n, ker,                              //
                 st.n, chg_coalesced, (real2*)radeps_coalesced, //
                 st.sorted, pchg, atom_rad, atom_eps);
      st.fresh &= ~cspatial_fresh_mask_echglj;
   }


   real eccut, ecoff;
   real aewald = 0;
   if CONSTEXPR (eq<ETYP, EWALD>()) {
      ecoff = switch_off(switch_ewald);
      eccut = ecoff; // not used
      PMEUnit pu = epme_unit;
      aewald = pu->aewald;
   } else {
      ecoff = switch_off(switch_charge);
      eccut = switch_cut(switch_charge);
   }
   real f = electric / dielec;
   assert(ebuffer == 0);


   real evoff, evcut;
   evoff = switch_off(switch_vdw);
   evcut = switch_cut(switch_vdw);


#define ECHGLJ_CU3_V2_ARGS                                                     \
   ec, vir_ec, decx, decy, decz, TINKER_IMAGE_ARGS, st.n, st.sorted, st.nakpl, \
      st.iakpl, st.niak, st.iak, st.lst, st.bnum, st.akc, ncvexclude,          \
      cvexclude, cvexclude_scale, eccut, ecoff, f, aewald, chg_coalesced,      \
      st.si3.bit0, evcut, evoff, (const real2*)radeps_coalesced, ev, vir_ev,   \
      devx, devy, devz


   int ngrid = get_grid_size(BLOCK_DIM);
   if (box_shape == ORTHO_BOX) {
      auto ker1 = echglj_cu5<Ver, PBC_ORTHO, ETYP, RADRULE, EPSRULE, VOUT>;
      ker1<<<ngrid, BLOCK_DIM, 0, g::s0>>>(ECHGLJ_CU3_V2_ARGS);
   } else if (box_shape == MONO_BOX) {
      auto ker1 = echglj_cu5<Ver, PBC_MONO, ETYP, RADRULE, EPSRULE, VOUT>;
      ker1<<<ngrid, BLOCK_DIM, 0, g::s0>>>(ECHGLJ_CU3_V2_ARGS);
   } else if (box_shape == TRI_BOX) {
      auto ker1 = echglj_cu5<Ver, PBC_TRI, ETYP, RADRULE, EPSRULE, VOUT>;
      ker1<<<ngrid, BLOCK_DIM, 0, g::s0>>>(ECHGLJ_CU3_V2_ARGS);
   } else if (box_shape == OCT_BOX) {
      auto ker1 = echglj_cu5<Ver, PBC_OCT, ETYP, RADRULE, EPSRULE, VOUT>;
      ker1<<<ngrid, BLOCK_DIM, 0, g::s0>>>(ECHGLJ_CU3_V2_ARGS);
   } else {
      assert(false);
   }
#undef ECHGLJ_CU3_V2_ARGS
}


//====================================================================//


void echglj_rad_arith_eps_geom_nonewald_cu(int vers)
{
   if (use_osrw) {
      constexpr bool VOUT = true;
      if (vers == calc::v0)
         echglj_cu3<calc::V0, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v1)
         echglj_cu3<calc::V1, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v3)
         echglj_cu3<calc::V3, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v4)
         echglj_cu3<calc::V4, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v5)
         echglj_cu3<calc::V5, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v6)
         echglj_cu3<calc::V6, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
   } else {
      constexpr bool VOUT = false;
      if (vers == calc::v0)
         echglj_cu3<calc::V0, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v1)
         echglj_cu3<calc::V1, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v3)
         echglj_cu3<calc::V3, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v4)
         echglj_cu3<calc::V4, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v5)
         echglj_cu3<calc::V5, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v6)
         echglj_cu3<calc::V6, NON_EWALD_TAPER, RAD_ARITH, EPS_GEOM, VOUT>();
   }


   elj14_cu(vers);
}


void echglj_rad_arith_eps_geom_ewald_real_cu(int vers)
{
   if (use_osrw) {
      constexpr bool VOUT = true;
      if (vers == calc::v0)
         echglj_cu3<calc::V0, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v1)
         echglj_cu3<calc::V1, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v3)
         echglj_cu3<calc::V3, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v4)
         echglj_cu3<calc::V4, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v5)
         echglj_cu3<calc::V5, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v6)
         echglj_cu3<calc::V6, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
   } else {
      constexpr bool VOUT = false;
      if (vers == calc::v0)
         echglj_cu3<calc::V0, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v1)
         echglj_cu3<calc::V1, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v3)
         echglj_cu3<calc::V3, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v4)
         echglj_cu3<calc::V4, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v5)
         echglj_cu3<calc::V5, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
      else if (vers == calc::v6)
         echglj_cu3<calc::V6, EWALD, RAD_ARITH, EPS_GEOM, VOUT>();
   }


   elj14_cu(vers);
}
}
