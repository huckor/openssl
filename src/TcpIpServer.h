#ifndef TcpIpServer_h
#define TcpIpServer_h

#include <vector>
#include <string>
#include <openssl/bio.h>

class TcpIpServer
{
public:
    TcpIpServer();
    ~TcpIpServer();
    void Close();
    int Write(std::vector<unsigned char> Data);
    int Read(unsigned char *Buffer, int Length);
    void SetPort(std::string Port);
    void SetReadTimeout(int Timeout);
    int Bind();
    int Accept();

private:
    std::string _Port;
    int _ReadTimeout;
    BIO *_Conn;
    BIO *_Client;
};

#endif /* TcpIpServer_h */
