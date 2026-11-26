// SPDX-License-Identifier: MIT
#pragma once
namespace nlauncher {
#ifdef NLAUNCHER_DIAGNOSTIC
bool diagnostic_start();
void diagnostic_stop();
void diagnostic_phase(bool worker,const char* phase);
void diagnostic_memory();
#else
inline bool diagnostic_start(){return true;}
inline void diagnostic_stop(){}
inline void diagnostic_phase(bool,const char*){}
inline void diagnostic_memory(){}
#endif
}
