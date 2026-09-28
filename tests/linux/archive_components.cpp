#include <cstdio>
#include <cstdlib>
extern "C" {
#include "7zip/C/7z.h"
#include "7zip/C/7zCrc.h"
}
#include "unrar/raros.hpp"
#include "unrar/dll.hpp"
int main() {
 ISzAlloc alloc = {[](void*, size_t n)->void* {return malloc(n);}, [](void*, void* p) {free(p);}};
 CSzArEx db; SzArEx_Init(&db); SzArEx_Free(&db, &alloc);
 CrcGenerateTable();
 printf("7-Zip linked; RAR DLL API version=%d\n", RARGetDllVersion());
 return RARGetDllVersion() < 6;
}
