#pragma once
#include "tool/rc_man.h"


namespace tinker {
const int couple_maxn12 = 8;
extern int (*couple_i12)[couple_maxn12];
extern int* couple_n12;


void couple_data(rc_op);
}
