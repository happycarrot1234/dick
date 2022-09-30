#include "includes.h"
#include "xorstr.h"

int __stdcall DllMain(HMODULE self, ulong_t reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) {

        AllocConsole();
        freopen_s((FILE**)stdin, XOR_NOT("CONIN$"), XOR_NOT("r"), stdin);
        freopen_s((FILE**)stdout, XOR_NOT("CONOUT$"), XOR_NOT("w"), stdout);
        SetConsoleTitleA("why am i crashing101 | debugger");

        Client::init(nullptr);
        return 1;
    }

    return 0;
}