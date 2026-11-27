// SPDX-License-Identifier: MIT
// Synthetic frame: a mixed-radix LL probability table with counts [16,16],
// RLE offset/match-length symbols and one sequence expanding to 100 ASCII As.
#include <array>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "zstd.h"
extern "C" int64_t nx2_zbic_to_zstd(const uint8_t*,uint64_t,uint8_t*,uint64_t);
int main(){
 const uint8_t packed[]={0x5a,0x42,0x49,0x43,0x20,0x64,0x65,0,0,0x08,0x41,0x01,0x94,0x03,0x60,0x72,0x03,0x00,0x2a,0x60,0x04};
 const uint8_t expected[]={0x28,0xb5,0x2f,0xfd,0x20,0x64,0x55,0,0,0x08,0x41,0x01,0x94,0x10,0x3f,0x00,0x2a,0x60,0x04};
 std::array<uint8_t,8194> frame{};frame.front()=0xAB;frame.back()=0xCD;
 auto n=nx2_zbic_to_zstd(packed,sizeof packed,frame.data()+1,8192);assert(n==sizeof expected&&!std::memcmp(frame.data()+1,expected,sizeof expected));
 std::array<uint8_t,100> plain{};assert(ZSTD_decompress(plain.data(),plain.size(),frame.data()+1,n)==100);for(auto b:plain)assert(b=='A');
 assert(nx2_zbic_to_zstd(packed,sizeof packed,frame.data()+1,4)<0);assert(frame.front()==0xAB&&frame.back()==0xCD);
 assert(nx2_zbic_to_zstd(packed,0,frame.data()+1,8192)==0);
 for(size_t size=1;size<sizeof packed;++size)assert(nx2_zbic_to_zstd(packed,size,frame.data()+1,8192)<0);
 std::vector<uint8_t> joined(std::begin(packed),std::end(packed));joined.insert(joined.end(),std::begin(packed),std::end(packed));
 n=nx2_zbic_to_zstd(joined.data(),joined.size(),frame.data()+1,8192);assert(n==2*sizeof expected);std::array<uint8_t,200> two{};assert(ZSTD_decompress(two.data(),two.size(),frame.data()+1,n)==two.size());for(auto b:two)assert(b=='A');
 joined.push_back(0);assert(nx2_zbic_to_zstd(joined.data(),joined.size(),frame.data()+1,8192)<0);
 uint32_t seed=0x12345678;
 for(unsigned test=0;test<1500;++test){std::array<uint8_t,256> input{};for(auto& b:input){seed=seed*1664525+1013904223;b=uint8_t(seed>>24);}std::memcpy(input.data(),"ZBIC",4);nx2_zbic_to_zstd(input.data(),5+test%251,frame.data()+1,8192);assert(frame.front()==0xAB&&frame.back()==0xCD);}
 std::cout<<"PASS: genuine ZBIC table, golden Zstd frame, concatenation, truncation and malformed-input bounds\n";
}
