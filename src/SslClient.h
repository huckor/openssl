#ifndef SslClient_h
#define SslClient_h

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <string>
#include <vector>

class SslClient
{
public:
    SslClient();
    ~SslClient();
    bool Initialize();
    int Open();
    void Close();
    int Write(std::vector<unsigned char> Data);
    int Read(unsigned char *Data, int Length);
    void SetHost(std::string Host);
    void SetPort(std::string Port);
    void SetCaCert(std::string CaCertFile);
    void SetClientCert(std::string ClientCertFile);
    void SetClientPrivateKey(std::string PrivateKeyFile);
    void SetClientCertPwd(std::string ClientCertPwd);
    void SetWriteTimeout(int TimeoutSec);
    void SetReadTimeout(int TimeoutSec);
    void SetSessionCaching(bool CacheSslSession);
    
private:
    BIO *_Conn;
    SSL *_Ssl;
    SSL_CTX *_Context;
    SSL_SESSION *_SslSession;
    std::string _ClientCertFile;
    std::string _PrivateKeyFile;
    std::string _CaCertFile;
    static std::string _ClientCertPwd;
    std::string _Host;
    std::string _Port;
    int _WriteTimeout;
    int _ReadTimeout;
    bool _CacheSslSession;
    
    int CreateSslStructure();
    int CheckStatus(int StatusCode);
    static std::string Asn1TimeToString(ASN1_TIME* Time);
    static int PwdCallback(char *Buf, int Size, int Flag, void *UserData);
    static int VerifyCallback(int Ok, X509_STORE_CTX *Store);
};

#endif /* SslClient_h */
