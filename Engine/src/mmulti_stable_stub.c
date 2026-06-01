/*
 * Stub implementations of the "stable" netcode variant.
 *
 * Chocolate's original "stable" multiplayer was a separate C++ implementation
 * (mmulti_stable.cpp) that this fork does not build. Only the "unstable" UDP
 * netcode (mmulti.c, the default — network.c's nNetMode == 0) is used.
 * network.c still references the stable_* entry points in its nNetMode == 1
 * switch branches, so these stubs exist purely to satisfy the linker; they are
 * never called unless the game is started with -netmode_stable.
 */

#include "mmulti_stable.h"

void  stable_callcommit(void) {}
void  stable_initcrc(void) {}
long  stable_getcrc(char *buffer, short bufleng) { (void)buffer; (void)bufleng; return 0; }
void  stable_initmultiplayers(char a, char b, char c) { (void)a; (void)b; (void)c; }
void  stable_sendpacket(long other, char *bufptr, long messleng) { (void)other; (void)bufptr; (void)messleng; }
void  stable_setpackettimeout(long t, long r) { (void)t; (void)r; }
void  stable_uninitmultiplayers(void) {}
void  stable_sendlogon(void) {}
void  stable_sendlogoff(void) {}
int   stable_getoutputcirclesize(void) { return 0; }
void  stable_setsocket(short s) { (void)s; }
short stable_getpacket(short *other, char *bufptr) { (void)other; (void)bufptr; return 0; }
void  stable_flushpackets(void) {}
void  stable_genericmultifunction(long other, char *bufptr, long messleng, long command)
{ (void)other; (void)bufptr; (void)messleng; (void)command; }
