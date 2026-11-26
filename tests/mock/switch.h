// SPDX-License-Identifier: MIT
// Test-only native FS facade. Production builds include the real libounce switch.h.
#pragma once
#include <cstdint>
#include <cstdio>
using u8=uint8_t;using u32=uint32_t;using u64=uint64_t;using s64=int64_t;
struct FsFile {FILE* file;};struct FsFileSystem {};
constexpr int FS_MAX_PATH=0x301,FsOpenMode_Read=1,FsReadOption_None=0;
#define R_FAILED(x) ((x)!=0)
inline void svcSleepThread(s64){}
FsFileSystem* fsdevGetDeviceFileSystem(const char*);
u32 fsFsOpenFile(FsFileSystem*,const char*,int,FsFile*);
u32 fsFileRead(FsFile*,s64,void*,u64,int,u64*);
u32 fsFileGetSize(FsFile*,s64*);
void fsFileClose(FsFile*);
