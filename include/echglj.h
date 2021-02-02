#pragma once
#include "echarge.h"
#include "evdw.h"
#include "glob.accasync.h"
#include "glob.chglj.h"
#include "tool/rc_man.h"


namespace tinker {
void echglj_data(rc_op op);
void echglj_data_cu(rc_op);
void echglj(int vers);


void echglj_rad_arith_eps_geom_nonewald_cu(int);
void echglj_rad_arith_eps_geom_ewald_real_cu(int);


void pme_stream_start_record_cu(bool use_pmestream);
void pme_stream_start_wait_cu(bool use_pmestream);
void pme_stream_finish_record_cu(bool use_pmestream);
void pme_stream_finish_wait_cu(bool use_pmestream);
#if TINKER_CUDART
inline void pme_stream_start_record(bool use_pmestream)
{
   pme_stream_start_record_cu(use_pmestream);
}
inline void pme_stream_start_wait(bool use_pmestream)
{
   pme_stream_start_wait_cu(use_pmestream);
}
inline void pme_stream_finish_record(bool use_pmestream)
{
   pme_stream_finish_record_cu(use_pmestream);
}
inline void pme_stream_finish_wait(bool use_pmestream)
{
   pme_stream_finish_wait_cu(use_pmestream);
}
#else
inline void pme_stream_start_record(bool use_pmestream) {}
inline void pme_stream_start_wait(bool use_pmestream) {}
inline void pme_stream_finish_record(bool use_pmestream) {}
inline void pme_stream_finish_wait(bool use_pmestream) {}
#endif


extern real* chg_coalesced;    // n
extern real* radeps_coalesced; // 2*n
}
