#ifndef TcpIpClient_h
#define TcpIpClient_h

#include <vector>
#include <string>
#include <openssl/bio.h>

class TcpIpClient
{
public:
    TcpIpClient();
    ~TcpIpClient();
    int Open();
    void Close();
    int Write(std::vector<unsigned char> Data);
    int Read(unsigned char *Buffer, int Length);
    void SetPort(std::string Port);
    void SetHost(std::string Host);
    void SetReadTimeout(int Timeout);
    void SetWriteTimeout(int Timeout);
    
private:
    std::string _Host;
    std::string _Port;
    int _ReadTimeout;
    int _WriteTimeout;
    BIO *_Conn;
};

#endif /* TcpIpClient_h */
