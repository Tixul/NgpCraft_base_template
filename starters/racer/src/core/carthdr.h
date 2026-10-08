
#ifndef CARTHDR_H
#define CARTHDR_H
#include "ngpc_types.h"
extern void main(void);
const char Licensed[28] = " LICENSED BY SNK CORPORATION";
const FuncPtr EntryPoint = main;
const short CartID = 0x0000;
const short CartSystem = 0x1000;
const char CartTitle[12] = "RACE ENGINE ";
const long CartReserved[4] = { 0, 0, 0, 0 };
#endif 

