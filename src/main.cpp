#include "Log.h"
#include "Global.h"
#include "Path.h"
#include "Thread.h"
#include "TcpIpClient.h"
#include "SslClient.h"

Log *__LOG;

int main(int argc, const char * argv[])
{
    //Create log class !importand to do it before anything else
    __LOG = new Log();
    __LOG->Create("stderr");
    
    
    std::vector<unsigned char> Data = { 'T', 'E', 'S', 'T'};
    SslClient SslClient;
    SslClient.Initialize();
    
    Thread::Create();
    SLEEP(2);
    
    TcpIpClient NosslClient;
    NosslClient.SetHost("localhost");
    NosslClient.SetPort("9999");
    NosslClient.SetReadTimeout(20);
    NosslClient.SetWriteTimeout(20);
    if(NosslClient.Open() == OK)
    {
        if(NosslClient.Write(Data) == OK)
            LOG("Client write success", MAIN_LOG_BIT);
    }
    NosslClient.Close();
    SLEEP(10);
    
    SslClient.SetHost("localhost");
    SslClient.SetPort("9999");
    SslClient.SetReadTimeout(20);
    SslClient.SetWriteTimeout(20);
    SslClient.SetCaCert("/Volumes/WORK/projects/Demos/Openssl/dep/certs/serverCA.pem");
    SslClient.SetClientCert("/Volumes/WORK/projects/Demos/Openssl/dep/certs/client.pem");
    SslClient.SetClientPrivateKey("/Volumes/WORK/projects/Demos/Openssl/dep/certs/client.pem");
    SslClient.SetClientCertPwd("password");
    if(SslClient.Open() == OK)
    {
        if(SslClient.Write(Data) == OK)
            LOG("Client write success", MAIN_LOG_BIT);
    }
    
    SslClient.Close();
    
    SLEEP(10);
    
    //Delete log class !important to do it as last thing
    delete __LOG;
    
    return 0;
}
