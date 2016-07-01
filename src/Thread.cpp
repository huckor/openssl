#include "Thread.h"
#include "Log.h"
#include "Global.h"
#include "Conv.h"
#include <string>
#include <stdlib.h>
#include "TcpIpServer.h"
#include "SslServer.h"


//This function will be executed in thread
static void THREAD_CC ThreadProcess(void *Arg)
{
    std::vector<unsigned char> Buffer(1);
    
    LOG("Thread started", THREAD_LOG_BIT);
    
#ifndef _WIN32
    pthread_detach(pthread_self());
#endif
    
    TcpIpServer NosslServer;
    NosslServer.SetPort("9999");
    NosslServer.SetReadTimeout(10);
    if(NosslServer.Bind() == OK)
    {
        while(true)
        {
            if(NosslServer.Accept() == OK)
            {
                LOG("Server start reading", THREAD_LOG_BIT);
                while(NosslServer.Read(&Buffer[0], 1) != FAIL)
                {
                    LOG("Server read " + Conv::BinToAscii(Buffer, false), THREAD_LOG_BIT);
                }
                
                break;
            }
        }
    }
    NosslServer.Close();
    SLEEP(5);
    
    
    SslServer MySslServer;
    MySslServer.SetPort("9999");
    MySslServer.SetReadTimeout(20);
    MySslServer.SetWriteTimeout(20);
    MySslServer.SetServerCert("/Volumes/WORK/projects/Demos/Openssl/dep/certs/server.pem");
    MySslServer.SetServerPrivateKey("/Volumes/WORK/projects/Demos/Openssl/dep/certs/server.pem");
    MySslServer.SetPrivateKeyPwd("password");
    if(MySslServer.Bind() == OK)
    {
        while(true)
        {
            if(MySslServer.Accept() == OK)
            {
                LOG("Server start reading", THREAD_LOG_BIT);
                while(MySslServer.Read(&Buffer[0], 1) != FAIL)
                {
                    LOG("Server read " + Conv::BinToAscii(Buffer, false), THREAD_LOG_BIT);
                }
            
                break;
            }
        }
    }
    MySslServer.Close();
    
    LOG("Thread ends", THREAD_LOG_BIT);
    
    return;
}

//This function will create new thread and pass him thread structure as argument
int Thread::Create()
{
    void *Params;
    THREAD_TYPE Tid;
    
    LOG("Creating new thread", THREAD_LOG_BIT);
    
#ifndef _WIN32
    pthread_create(&Tid, NULL, (void* (*)(void*))&ThreadProcess, Params);
#else
    HANDLE Thread = CreateThread(NULL, 0, (unsigned long (__stdcall *)(void *))ThreadProcess, (void *)(Params), 0, &Tid);
    CloseHandle(Thread); 
#endif
    
    return OK;
}