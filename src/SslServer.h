#ifndef ssl_h
#define ssl_h

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <string>
#include <vector>

class SslServer
{
public:
    SslServer();
    ~SslServer();
    bool Initialize();
    void Close();
    int Write(std::vector<unsigned char> Data);
    int Read(unsigned char *Data, int Length);
    int Bind();
    int Accept();
    void SetPort(std::string Port);
    void SetServerCert(std::string ServerCertFile);
    void SetServerPrivateKey(std::string ServerPrivateKeyFile);
    void SetPrivateKeyPwd(std::string PrivateKeyPwd);
    void SetReadTimeout(int TimeoutSec);
    void SetWriteTimeout(int TimeoutSec);
    void SetSessionCaching(bool CacheSslSession);
    
private:
    BIO *_Conn;
    BIO *_Client;
    SSL *_Ssl;
    SSL_CTX *_Context;
    SSL_SESSION *_SslSession;
    std::string _ServerCertFile;
    std::string _ServerPrivateKeyFile;
    static std::string _ServerPrivateKeyPwd;
    std::string _Port;
    int _WriteTimeout;
    int _ReadTimeout;
    bool _CacheSslSession;
    
    int CreateSslStructure();
    int CheckStatus(int StatusCode);
    static std::string Asn1TimeToString(ASN1_TIME* Time);
    static int VerifyCallback(int Ok, X509_STORE_CTX *Store);
    static int PwdCallback(char *Buf, int Size, int Flag, void *UserData);

};


#endif /* ssl_h */
