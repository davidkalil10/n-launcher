#!/usr/bin/env python3
"""Generate a portable frontend adapter from the pinned nx2-flow MIT decoder."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
def prepare():
    name='loader/runtime/zbic.c'
    pin=json.loads((ROOT/'config/flow.json').read_text(encoding='utf-8'))
    source=ROOT/'third_party/nx2-flow'/name
    if hashlib.sha256(source.read_bytes()).hexdigest()!=pin['files'][name]:raise RuntimeError('Pinned ZBIC source integrity mismatch')
    text=source.read_text(encoding='utf-8');header='#include "nx2.h"'
    if text.count(header)!=1:raise RuntimeError('ZBIC adapter no longer applies')
    text=text.replace(header,'''#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;''')
    target=ROOT/'build/generated/zbic.c';target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Olmectron; generated adapter, original decoder unchanged.\n'+text,encoding='utf-8',newline='\n')
    return target
if __name__=='__main__':print(prepare())
