#pragma once
#include "elec.h"
#include "md.h"
#include "seq_damp_hippo.h"
#include "seq_switch.h"
#include "switch.h"


namespace tinker {
struct PairRepelGrad
{
   real frcx, frcy, frcz;
   real ttqi[3];
   real ttqk[3];
};


SEQ_ROUTINE
inline void zero(PairRepelGrad& pgrad)
{
   pgrad.frcx = 0;
   pgrad.frcy = 0;
   pgrad.frcz = 0;
   pgrad.ttqi[0] = 0;
   pgrad.ttqi[1] = 0;
   pgrad.ttqi[2] = 0;
   pgrad.ttqk[0] = 0;
   pgrad.ttqk[1] = 0;
   pgrad.ttqk[2] = 0;
}


#pragma acc routine seq
template <bool do_g>
SEQ_CUDA
void pair_repel(real r2, real rscale, real cut, real off, real xr, real yr,
                real zr, real sizi, real dmpi, real vali, real ci, real dix,
                real diy, real diz, real qixx, real qixy, real qixz, real qiyy,
                real qiyz, real qizz, real sizk, real dmpk, real valk, real ck,
                real dkx, real dky, real dkz, real qkxx, real qkxy, real qkxz,
                real qkyy, real qkyz, real qkzz, real& restrict e,
                PairRepelGrad& restrict pgrad)
{
   real cut2 = cut * cut;
   real r = REAL_SQRT(r2);
   real rInv = REAL_RECIP(r);
   real rr2 = rInv * rInv;
   real rr1 = rInv;
   real rr3 = rr1 * rr2;
   real rr5 = 3 * rr3 * rr2;
   real rr7 = 5 * rr5 * rr2;
   real rr9 = 7 * rr7 * rr2;
   real rr11 = 0;
   if CONSTEXPR (do_g) {
      rr11 = 9 * rr9 * rr2;
   }

   real3 dr = make_real3(xr, yr, zr);
   real3 di = make_real3(dix, diy, diz);
   real3 dk = make_real3(dkx, dky, dkz);

   // get damping coefficients for the Pauli repulsion energy
   real dmpik[6];

   if CONSTEXPR (do_g) {
      damp_rep<11>(dmpik, r, rr1, r2, rr3, rr5, rr7, rr9, rr11, dmpi, dmpk);
   } else {
      damp_rep<9>(dmpik, r, rr1, r2, rr3, rr5, rr7, rr9, rr11, dmpi, dmpk);
   }

   // Fortran tinker indexing
   // dmpik(1) == dmpik[0]
   // dmpik(3) == dmpik[1]
   // dmpik(5) == dmpik[2]
   // dmpik(7) == dmpik[3]
   // dmpik(9) == dmpik[4]
   // dmpik(11) == dmpik[5]

   // same as in emplar_cu
   real dir = dot3(di, dr);
   real3 qi_dr = matvec(qixx, qixy, qixz, qiyy, qiyz, qizz, dr);
   real qir = dot3(dr, qi_dr);
   real dkr = dot3(dk, dr);
   real3 qk_dr = matvec(qkxx, qkxy, qkxz, qkyy, qkyz, qkzz, dr);
   real qkr = dot3(dr, qk_dr);

   real dik = dot3(di, dk);
   real qik = dot3(qi_dr, qk_dr);
   real diqk = dot3(di, qk_dr);
   real dkqi = dot3(dk, qi_dr);
   real qiqk = dot3(qixx, qiyy, qizz, qkxx, qkyy, qkzz) +
      2 * dot3(qixy, qixz, qiyz, qkxy, qkxz, qkyz);

   real term1 = vali * valk;
   real term2 = valk * dir - vali * dkr + dik;
   real term3 = vali * qkr + valk * qir - dir * dkr + 2 * (dkqi - diqk + qiqk);
   real term4 = dir * qkr - dkr * qir - 4 * qik;
   real term5 = qir * qkr;

   real sizik = sizi * sizk * rscale;
   real eterm = term1 * dmpik[0] + term2 * dmpik[1] + term3 * dmpik[2] +
      term4 * dmpik[3] + term5 * dmpik[4];

   e = sizik * eterm * rInv;
   // gradient
   if CONSTEXPR (do_g) {
      real de = term1 * dmpik[1] + term2 * dmpik[2] + term3 * dmpik[3] +
         term4 * dmpik[4] + term5 * dmpik[5];
      term1 = -valk * dmpik[1] + dkr * dmpik[2] - qkr * dmpik[3];
      term2 = vali * dmpik[1] + dir * dmpik[2] + qir * dmpik[3];
      term3 = 2 * dmpik[2];
      term4 = 2 * (-valk * dmpik[2] + dkr * dmpik[3] - qkr * dmpik[4]);
      term5 = 2 * (-vali * dmpik[2] - dir * dmpik[3] - qir * dmpik[4]);
      real term6 = 4 * dmpik[3];

      // few intermediates, same as in emplar
      real qix = qi_dr.x;
      real qiy = qi_dr.y;
      real qiz = qi_dr.z;
      real qkx = qk_dr.x;
      real qky = qk_dr.y;
      real qkz = qk_dr.z;

      real qixk = qixx * qkx + qixy * qky + qixz * qkz; // |Qi Qk r>
      real qiyk = qixy * qkx + qiyy * qky + qiyz * qkz;
      real qizk = qixz * qkx + qiyz * qky + qizz * qkz;
      real qkxi = qkxx * qix + qkxy * qiy + qkxz * qiz; // |Qk Qi r>
      real qkyi = qkxy * qix + qkyy * qiy + qkyz * qiz;
      real qkzi = qkxz * qix + qkyz * qiy + qkzz * qiz;

      real diqkx = di.x * qkxx + di.y * qkxy + di.z * qkxz; // |Qk Di>
      real diqky = di.x * qkxy + di.y * qkyy + di.z * qkyz;
      real diqkz = di.x * qkxz + di.y * qkyz + di.z * qkzz;
      real dkqix = dk.x * qixx + dk.y * qixy + dk.z * qixz; // |Qi Dk>
      real dkqiy = dk.x * qixy + dk.y * qiyy + dk.z * qiyz;
      real dkqiz = dk.x * qixz + dk.y * qiyz + dk.z * qizz;

      real3 frc0;
      frc0.x = de * dr.x + term1 * di.x + term2 * dk.x +
         term3 * (diqkx - dkqix) + term4 * qix + term5 * qkx +
         term6 * (qixk + qkxi);
      frc0.y = de * dr.y + term1 * di.y + term2 * dk.y +
         term3 * (diqky - dkqiy) + term4 * qiy + term5 * qky +
         term6 * (qiyk + qkyi);
      frc0.z = de * dr.z + term1 * di.z + term2 * dk.z +
         term3 * (diqkz - dkqiz) + term4 * qiz + term5 * qkz +
         term6 * (qizk + qkzi);

      pgrad.frcx = frc0.x * rr1 + eterm * rr3 * dr.x;
      pgrad.frcy = frc0.y * rr1 + eterm * rr3 * dr.y;
      pgrad.frcz = frc0.z * rr1 + eterm * rr3 * dr.z;

      pgrad.frcx *= sizik;
      pgrad.frcy *= sizik;
      pgrad.frcz *= sizik;

      // torque
      real dirx = di.y * dr.z - di.z * dr.y; // Di x r
      real diry = di.z * dr.x - di.x * dr.z;
      real dirz = di.x * dr.y - di.y * dr.x;
      real dkrx = dk.y * dr.z - dk.z * dr.y; // Dk x r
      real dkry = dk.z * dr.x - dk.x * dr.z;
      real dkrz = dk.x * dr.y - dk.y * dr.x;
      real dikx = di.y * dk.z - di.z * dk.y; // Di x Dk
      real diky = di.z * dk.x - di.x * dk.z;
      real dikz = di.x * dk.y - di.y * dk.x;

      real qirx = qiz * dr.y - qiy * dr.z; // r x (Qi r)
      real qiry = qix * dr.z - qiz * dr.x;
      real qirz = qiy * dr.x - qix * dr.y;
      real qkrx = qkz * dr.y - qky * dr.z; // r x (Qk r)
      real qkry = qkx * dr.z - qkz * dr.x;
      real qkrz = qky * dr.x - qkx * dr.y;
      real qikx = qky * qiz - qkz * qiy; // (Qk r) x (Qi r)
      real qiky = qkz * qix - qkx * qiz;
      real qikz = qkx * qiy - qky * qix;

      real qikrx = qizk * dr.y - qiyk * dr.z;
      real qikry = qixk * dr.z - qizk * dr.x;
      real qikrz = qiyk * dr.x - qixk * dr.y;
      real qkirx = qkzi * dr.y - qkyi * dr.z;
      real qkiry = qkxi * dr.z - qkzi * dr.x;
      real qkirz = qkyi * dr.x - qkxi * dr.y;

      real diqkrx = diqkz * dr.y - diqky * dr.z;
      real diqkry = diqkx * dr.z - diqkz * dr.x;
      real diqkrz = diqky * dr.x - diqkx * dr.y;
      real dkqirx = dkqiz * dr.y - dkqiy * dr.z;
      real dkqiry = dkqix * dr.z - dkqiz * dr.x;
      real dkqirz = dkqiy * dr.x - dkqix * dr.y;

      real dqikx = di.y * qkz - di.z * qky + dk.y * qiz - dk.z * qiy -
         2 *
            (qixy * qkxz + qiyy * qkyz + qiyz * qkzz - qixz * qkxy -
             qiyz * qkyy - qizz * qkyz);
      real dqiky = di.z * qkx - di.x * qkz + dk.z * qix - dk.x * qiz -
         2 *
            (qixz * qkxx + qiyz * qkxy + qizz * qkxz - qixx * qkxz -
             qixy * qkyz - qixz * qkzz);
      real dqikz = di.x * qky - di.y * qkx + dk.x * qiy - dk.y * qix -
         2 *
            (qixx * qkxy + qixy * qkyy + qixz * qkyz - qixy * qkxx -
             qiyy * qkxy - qiyz * qkxz);

      real3 trq0;
      trq0.x = -dmpik[1] * dikx + term1 * dirx + term3 * (dqikx + dkqirx) -
         term4 * qirx - term6 * (qikrx + qikx);
      trq0.y = -dmpik[1] * diky + term1 * diry + term3 * (dqiky + dkqiry) -
         term4 * qiry - term6 * (qikry + qiky);
      trq0.z = -dmpik[1] * dikz + term1 * dirz + term3 * (dqikz + dkqirz) -
         term4 * qirz - term6 * (qikrz + qikz);

      pgrad.ttqi[0] += sizik * rr1 * trq0.x;
      pgrad.ttqi[1] += sizik * rr1 * trq0.y;
      pgrad.ttqi[2] += sizik * rr1 * trq0.z;

      trq0.x = dmpik[1] * dikx + term2 * dkrx - term3 * (dqikx + diqkrx) -
         term5 * qkrx - term6 * (qkirx - qikx);
      trq0.y = dmpik[1] * diky + term2 * dkry - term3 * (dqiky + diqkry) -
         term5 * qkry - term6 * (qkiry - qiky);
      trq0.z = dmpik[1] * dikz + term2 * dkrz - term3 * (dqikz + diqkrz) -
         term5 * qkrz - term6 * (qkirz - qikz);

      pgrad.ttqk[0] += sizik * rr1 * trq0.x;
      pgrad.ttqk[1] += sizik * rr1 * trq0.y;
      pgrad.ttqk[2] += sizik * rr1 * trq0.z;
   }

   if (r2 > cut2) {
      real taper, dtaper;
      switch_taper5<do_g>(r, cut, off, taper, dtaper);
      if CONSTEXPR (do_g) {
         dtaper *= e * rr1;
         pgrad.frcx = pgrad.frcx * taper - dtaper * xr;
         pgrad.frcy = pgrad.frcy * taper - dtaper * yr;
         pgrad.frcz = pgrad.frcz * taper - dtaper * zr;

         pgrad.ttqi[0] *= taper;
         pgrad.ttqi[1] *= taper;
         pgrad.ttqi[2] *= taper;
         pgrad.ttqk[0] *= taper;
         pgrad.ttqk[1] *= taper;
         pgrad.ttqk[2] *= taper;
      }
      e *= taper;
   }
}
}
