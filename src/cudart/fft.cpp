#include "fft.h"
#include "edisp.h"
#include "elec.h"
#include "glob.accasync.h"
#include "pme.h"
#include "tool/cudalib.h"
#include "tool/error.h"
#include <cufft.h>


namespace tinker {
struct FFTPlanCUFFT : public FFTPlan
{
   cufftHandle h;
};


void fft_data(rc_op op)
{
   if (!use_ewald() && !use_dewald())
      return;

   if (op & rc_dealloc) {
      int idx = 0;
      while (idx < FFTPlanUnit::size()) {
         FFTPlanUnit u = idx;
         cufftDestroy(u->self<FFTPlanCUFFT>().h);
         ++idx;
      }
      FFTPlanUnit::clear();
   }

   if (op & rc_alloc) {
      assert(FFTPlanUnit::size() == 0);

      const size_t size = PMEUnit::size();
      FFTPlanUnit::resize<FFTPlanCUFFT>(size);
   }

   if (op & rc_init) {
#if TINKER_REAL_SIZE == 4
      const cufftType typ = CUFFT_C2C;
#elif TINKER_REAL_SIZE == 8
      const cufftType typ = CUFFT_Z2Z;
#else
      static_assert(false, "");
#endif

      int idx = 0;
      while (idx < FFTPlanUnit::size()) {
         FFTPlanUnit plan_u = idx;
         PMEUnit pme_u = idx;
         auto& iplan = plan_u->self<FFTPlanCUFFT>().h;
         auto& st = *pme_u;

         int nfast = st.nfft1;
         int nslow = st.nfft3;
         // different from FFTW Fortran API
         check_rt(cufftPlan3d(&iplan, nslow, st.nfft2, nfast, typ));
         auto stream = g::s0;
         if (use_pme_stream)
            stream = g::spme;
         check_rt(cufftSetStream(iplan, stream));
         ++idx;
      }
   }
}


void fftfront(PMEUnit pme_u)
{
   FFTPlanUnit iplan_u = static_cast<int>(pme_u);
   auto& iplan = iplan_u->self<FFTPlanCUFFT>().h;
   auto& st = *pme_u;

#if TINKER_REAL_SIZE == 4
   cufftExecC2C(iplan, reinterpret_cast<cufftComplex*>(st.qgrid),
                reinterpret_cast<cufftComplex*>(st.qgrid), CUFFT_FORWARD);
#elif TINKER_REAL_SIZE == 8
   cufftExecZ2Z(iplan, reinterpret_cast<cufftDoubleComplex*>(st.qgrid),
                reinterpret_cast<cufftDoubleComplex*>(st.qgrid), CUFFT_FORWARD);
#else
   static_assert(false, "");
#endif
}


void fftback(PMEUnit pme_u)
{
   FFTPlanUnit iplan_u = static_cast<int>(pme_u);
   auto& iplan = iplan_u->self<FFTPlanCUFFT>().h;
   auto& st = *pme_u;

#if TINKER_REAL_SIZE == 4
   cufftExecC2C(iplan, reinterpret_cast<cufftComplex*>(st.qgrid),
                reinterpret_cast<cufftComplex*>(st.qgrid), CUFFT_INVERSE);
#elif TINKER_REAL_SIZE == 8
   cufftExecZ2Z(iplan, reinterpret_cast<cufftDoubleComplex*>(st.qgrid),
                reinterpret_cast<cufftDoubleComplex*>(st.qgrid), CUFFT_INVERSE);
#else
   static_assert(false, "");
#endif
}
}
