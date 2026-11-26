#include "nx2link/fixed_report.hpp"
#include <cassert>
#include <string>
#include <iostream>
int main(){
 nx2link::FixedReport<128> report;report.text("pc=");report.hex(0x1234);report.text(" count=");report.number(UINT64_MAX);
 assert(std::string(report.bytes,report.size)=="pc=0x0000000000001234 count=18446744073709551615");
 struct Guard{nx2link::FixedReport<4> report;unsigned guard=0xC0FFEE;};Guard g;g.report.text("abcdef");g.report.hex(UINT64_MAX);g.report.number(UINT64_MAX);assert(g.report.size==4&&g.guard==0xC0FFEE);
 nx2link::FixedReport<8> limited;limited.text("abcdef",3);limited.text(nullptr);limited.number(0);assert(std::string(limited.bytes,limited.size)=="abc0");
 std::cout<<"PASS: allocation-free fatal report formatting and capacity bounds\n";
}
