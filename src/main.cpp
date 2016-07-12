#include "Log.h"
#include "Global.h"
#include "Path.h"
#include "Thread.h"
#include "TcpIpClient.h"
#include "SslClient.h"

Log *__LOG;

#ifdef WIN32
std::string __CertPath = Path::GetPathToExecutableFolder() + "../../../dep/certs/";
#elif __APPLE__
std::string __CertPath = "/Volumes/WORK/projects/Demos/Openssl/dep/certs/";
#else
std::string __CertPath = "/home/robo/workspace/Openssl/dep/certs/";
#endif

int main(int argc, const char * argv[])
{
    //Create log class !importand to do it before anything else
    __LOG = new Log();
    __LOG->Create("stderr");
    
    
    std::vector<unsigned char> Data;
	Data.push_back('T');
	Data.push_back('E');
	Data.push_back('S');
	Data.push_back('T');
    
    SslClient SslClient;
    SslClient.Initialize();
    
    Thread::Create();
    SLEEP(2);
    
    TcpIpClient NosslClient;
    NosslClient.SetHost("localhost");
    NosslClient.SetThePort("9999");
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
    SslClient.SetThePort("9999");
    SslClient.SetReadTimeout(20);
    SslClient.SetWriteTimeout(20);
	SslClient.SetCaCert(__CertPath + "serverCA.pem");
    SslClient.SetClientCert(__CertPath + "client.pem");
    SslClient.SetClientPrivateKey(__CertPath + "client.pem");
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
