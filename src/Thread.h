#ifndef Thread_h
#define Thread_h

#ifndef _WIN32  // Linux, MacOS
#include <pthread.h>
#include <unistd.h>

#define SLEEP(Seconds) usleep(Seconds * 1000000)

#define THREAD_CC
#define THREAD_TYPE pthread_t
#else //Windows
#include <windows.h>

#define SLEEP(Seconds) Sleep(Seconds * 1000)

#define THREAD_CC   __cdecl
#define THREAD_TYPE DWORD
#endif

class Thread
{
public:
    static int Create();
};

#endif /* Thread_h */
