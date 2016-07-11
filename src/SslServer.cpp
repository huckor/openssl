#include "SslServer.h"
#include "Log.h"
#include "Global.h"
#include "Conv.h"

SslServer::SslServer()
{
    _Conn = NULL;
    _Ssl = NULL;
    _Context = NULL;
    _SslSession = NULL;
    _ServerCertFile = "";
    _ServerPrivateKeyFile = "";
    _Port = "";
    _WriteTimeout = 20;
    _ReadTimeout = 20;
    _CacheSslSession = false;
}

SslServer::~SslServer()
{
    Close();
}

//Initialize openssl library !!! CALL ONLY ONE TIME PER APPLICATION LIFE !!!
bool SslServer::Initialize()
{
    if (!SSL_library_init())
    {
        LOG("OpenSSL initialization failed.", OPENSSL_LOG_BIT);
        return false;
    }
    
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    OpenSSL_add_all_digests();
    
    std::string Version = SSLeay_version(SSLEAY_VERSION);
    LOG("Openssl library version is " + Version, OPENSSL_LOG_BIT);
    
    return true;
}

int SslServer::Bind()
{
    if(_Port.length() <= 0)
    {
        LOG("Port is not set", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    if(_Conn != NULL)
        Close();
    
    _Conn = BIO_new_accept((char *)_Port.c_str());
    if(!_Conn)
    {
        LOG("Unable to create accept BIO on port " + _Port, OPENSSL_LOG_BIT);
        return FAIL;
    }
    else
        LOG("Accept BIO created on port " + _Port, OPENSSL_LOG_BIT);
    
    if(!BIO_set_nbio(_Conn, 1))
    {
        LOG("Unable to configure BIO to NBIO", OPENSSL_LOG_BIT);
        return FAIL;
    }
    else
        LOG("BIO is configured to NBIO", OPENSSL_LOG_BIT);
    
    BIO_set_bind_mode(_Conn, BIO_BIND_REUSEADDR);
    
    if(BIO_do_accept(_Conn) <= 0)
    {
        LOG("BIO do accept failed", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    LOG("BIO is configured for accept operations", OPENSSL_LOG_BIT);
    
    return OK;
}

int SslServer::Accept()
{
    int Socket = -1;
    int Ret = -1;
    int Status = -1;
    struct timeval TimeVal;
    fd_set FdSet;
    
    if(_Conn == NULL)
    {
        LOG("Accept BIO is not initialized. First call Bind()", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    Socket = BIO_get_fd(_Conn, NULL);
    TimeVal.tv_sec = _ReadTimeout;
    TimeVal.tv_usec = 0;
    FD_ZERO(&FdSet);
    FD_SET(Socket, &FdSet);
    
    LOG("Waiting for new connecion.", OPENSSL_LOG_BIT);
    Ret = select(Socket + 1, &FdSet, 0, 0, &TimeVal);
    if(Ret < 0)
    {
        LOG("Error on socket", OPENSSL_LOG_BIT);
        return FAIL;
    }
    else if(Ret == 0)
    {
        LOG("Timeout on socket during accept", OPENSSL_LOG_BIT);
        return TIMEOUT;
    }
    
    if(BIO_do_accept(_Conn) <= 0)
    {
        LOG("BIO do accept failed", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    _Client = BIO_pop(_Conn);
    
    if(CreateSslStructure() != OK)
        return FAIL;
    
    SSL_set_bio(_Ssl, _Client, _Client);
    
    //SSL handshake
    while(true)
    {
        Ret = SSL_accept(_Ssl);
        Status = CheckStatus(Ret);
        
        //Fatal error during handshake
        if(Status == FAIL)
        {
            LOG("Handshake failed", OPENSSL_LOG_BIT);
            return FAIL;
        }
        //Timeout on socket
        else if(Status == TIMEOUT)
        {
            LOG("Handshake timeouted", OPENSSL_LOG_BIT);
            return TIMEOUT;
        }
        //Handshake complete
        else if(Status == OK)
        {
            break;
        }
        //Repeat handshake
    }

    LOG("New ssl connection accepted.", OPENSSL_LOG_BIT);
    
    return OK;
}

int SslServer::CreateSslStructure()
{
    //Create new ssl context
    _Context = SSL_CTX_new(TLSv1_method());
    if(!_Context)
    {
        LOG("Ssl context creation failed", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    //Load server certificate or server chain file
    //If the client requests server authentication, you have to authenticate yourself to the peer (since
    //this is the server side, in this case peer is the client), so you need load your own certificate and
    //private key, which are in the keystore. Or can be in separated files, see below "load client private key"
    if(_ServerCertFile.length() > 0)
    {
        if(SSL_CTX_use_certificate_chain_file(_Context, _ServerCertFile.c_str()) != 1)
        {
            LOG("Error loading ClientCert file " + _ServerCertFile, OPENSSL_LOG_BIT);
            return false;
        }
        else
            LOG("ClientCert loaded", OPENSSL_LOG_BIT);
    }
    
    if(_ServerPrivateKeyPwd.length() > 0)
        SSL_CTX_set_default_passwd_cb(_Context, PwdCallback);
    
    //Load server private key
    //If the server certificate and server private key are in separate files, you need to load private key separately
    if(_ServerPrivateKeyFile.length() > 0)
    {
        if(SSL_CTX_use_PrivateKey_file(_Context, _ServerPrivateKeyFile.c_str(), SSL_FILETYPE_PEM) != 1)
        {
            LOG("Error loading PrivateKeyFile file " + _ServerPrivateKeyFile, OPENSSL_LOG_BIT);
            return false;
        }
        else
            LOG("PrivateKeyFile loaded", OPENSSL_LOG_BIT);
    }
    
    //Set the verification mode
    SSL_CTX_set_verify(_Context, SSL_VERIFY_PEER, VerifyCallback);
    
    //Sets the depth for verification
    SSL_CTX_set_verify_depth(_Context, 4);
    
    //Create a new SSL structure
    if (!(_Ssl = SSL_new(_Context)))
    {
        LOG("Error - SSL structure not created", OPENSSL_LOG_BIT);
        return FAIL;
    }
    else
        LOG("Context and SSL structure created", OPENSSL_LOG_BIT);
    
    return OK;
}


int SslServer::Write(std::vector<unsigned char> Data)
{
    int Ret = -1;
    int Status = -1;
    
    while(true)
    {
        Ret = SSL_write(_Ssl, &Data[0], (int)Data.size());
        Status = CheckStatus(Ret);
        
        //Boken or closed connection
        if(Status == FAIL)
        {
            LOG("Write failed", OPENSSL_LOG_BIT);
            return FAIL;
        }
        //Timeout on socket
        else if(Status == TIMEOUT)
        {
            LOG("Write timeouted", OPENSSL_LOG_BIT);
            return TIMEOUT;
        }
        //We wrote data
        else if(Status == OK)
        {
            return OK;
        }
        //Repeat write operation because new handshake
    }
    
    return true;
}

int SslServer::Read(unsigned char *Data, int Length)
{
    int Ret = -1;
    int Status = -1;
    
    while(true)
    {
        Ret = SSL_read(_Ssl, Data, Length);
        Status = CheckStatus(Ret);
        
        //Boken or closed connection
        if(Status == FAIL)
        {
            LOG("Read failed", OPENSSL_LOG_BIT);
            return FAIL;
        }
        //Timeout on socket
        else if(Status == TIMEOUT)
        {
            LOG("Read timeouted", OPENSSL_LOG_BIT);
            return TIMEOUT;
        }
        //We have data
        else if(Status == OK)
        {
            return Ret;
        }
        //Repeat read operation because new handshake
    }
}

int SslServer::CheckStatus(int StatusCode)
{
    int Ret = 0;
    struct timeval TimeVal;
    fd_set FdSet;
    char ErrorString[200];
    int Socket = -1;
    
    Ret = SSL_get_error(_Ssl, StatusCode);
    Socket = SSL_get_fd(_Ssl);
    
    switch(Ret)
    {
            //No error occurred with the I/O call.
        case SSL_ERROR_NONE:
            return OK;
            
            //The operation couldn't be completed. The underlying medium
            //could not fulfill the read requested. This error code means the
            //call should be retried.
        case SSL_ERROR_WANT_READ:
            TimeVal.tv_sec = _ReadTimeout;
            TimeVal.tv_usec = 0;
            FD_ZERO(&FdSet);
            FD_SET(Socket, &FdSet);
            //Wait until operation be done or timeout elapsed
            if(select(Socket + 1, &FdSet, 0, 0, &TimeVal) <= 0)
            {
                return TIMEOUT;
            }
            return REPEAT;
            
            //The operation couldn't be completed. The underlying medium
            //could not fulfill the write requested. This error code means the
            //call should be retried.
        case SSL_ERROR_WANT_WRITE:
            TimeVal.tv_sec = _WriteTimeout;
            TimeVal.tv_usec = 0;
            FD_ZERO(&FdSet);
            FD_SET(Socket, &FdSet);
            //Wait until operation be done or timeout elapsed
            if (select(Socket + 1, 0, &FdSet, 0, &TimeVal) <= 0)
            {
                return TIMEOUT;
            }
            return REPEAT;
            
            //An error occurred.
        default:
            ERR_error_string_n(Ret, ErrorString, sizeof(ErrorString));
            LOG("Error: " + Conv::CharArrayToString(ErrorString), OPENSSL_LOG_BIT);
            return FAIL;
    }
}

void SslServer::Close()
{
    if(_CacheSslSession && _Ssl != NULL)
    {
        _SslSession = SSL_get1_session(_Ssl);
        
        if(_SslSession != NULL)
            LOG("Ssl session saved", OPENSSL_LOG_BIT);
        else
            LOG("Error - Ssl session is not saved", OPENSSL_LOG_BIT);
    }
    
    if(_Ssl != NULL)
    {
        SSL_free(_Ssl);
        _Ssl = NULL;
        _Conn = NULL;
    }
    
    if(_Context != NULL)
    {
        SSL_CTX_free(_Context);
        _Context = NULL;
    }
}

void SslServer::SetThePort(std::string Port)
{
    _Port = Port;
}

//Set server certificate or server chain file
//If the client requests server authentication, you have to authenticate yourself to the peer (since
//this is the server side, in this case peer is the client), so you need load your own certificate and
//private key, which are in the keystore. Or can be in separated files, see below function SetServerPrivateKey
void SslServer::SetServerCert(std::string ServerCertFile)
{
    _ServerCertFile = ServerCertFile;
}

//Set server private key
//If the server certificate and server private key are in separate files, you need to load private key separately
void SslServer::SetServerPrivateKey(std::string ServerPrivateKeyFile)
{
    _ServerPrivateKeyFile = ServerPrivateKeyFile;
}

void SslServer::SetPrivateKeyPwd(std::string ServerPrivateKeyPwd)
{
    _ServerPrivateKeyPwd = ServerPrivateKeyPwd;
}

void SslServer::SetReadTimeout(int TimeoutSec)
{
    _ReadTimeout = TimeoutSec;
}

void SslServer::SetWriteTimeout(int TimeoutSec)
{
    _WriteTimeout = TimeoutSec;
}

void SslServer::SetSessionCaching(bool CacheSslSession)
{
    _CacheSslSession = CacheSslSession;
}





std::string SslServer::_ServerPrivateKeyPwd = "";

int SslServer::PwdCallback(char *Buf, int Size, int Flag, void *UserData)
{
    strcpy(Buf, _ServerPrivateKeyPwd.c_str());
    return (int)_ServerPrivateKeyPwd.length();
}

int SslServer::VerifyCallback(int Ok, X509_STORE_CTX *Store)
{
    if (!Ok)
    {
        X509 *Cert = X509_STORE_CTX_get_current_cert(Store);
        int Depth = X509_STORE_CTX_get_error_depth(Store);
        int Err = X509_STORE_CTX_get_error(Store);
        char Data[256];
        ASN1_TIME *Tm;
        ASN1_INTEGER *Sn;
        BIGNUM *BnSn;
        
        LOG("Error with certificate at depth: " + Conv::IntToStr(Depth), OPENSSL_LOG_BIT);
        
        X509_NAME_oneline(X509_get_issuer_name(Cert), Data, 256);
        LOG("Certificate issuer: " + Conv::CharArrayToString(Data), OPENSSL_LOG_BIT);
        
        X509_NAME_oneline(X509_get_subject_name(Cert), Data, 256);
        LOG("Certificate subject: " + Conv::CharArrayToString(Data), OPENSSL_LOG_BIT);
        
        Sn = X509_get_serialNumber(Cert);
        BnSn = ASN1_INTEGER_to_BN(Sn, NULL);
        LOG("Certificate serial number: " + Conv::CharArrayToString(BN_bn2hex(BnSn)), OPENSSL_LOG_BIT);
        
        Tm = X509_get_notBefore(Cert);
        LOG("Certificate not before date: " + Asn1TimeToString(Tm), OPENSSL_LOG_BIT);
        
        Tm = X509_get_notAfter(Cert);
        LOG("Certificate not after date: " + Asn1TimeToString(Tm), OPENSSL_LOG_BIT);
        
        LOG("Error string: " + Conv::IntToStr(Err) + ":" + Conv::CharArrayToString((char *)X509_verify_cert_error_string(Err)), OPENSSL_LOG_BIT);
    }
    
    return Ok;
}

std::string SslServer::Asn1TimeToString(ASN1_TIME* Time)
{
    std::string Return = "";
    std::string Data = Conv::BinToText(Time->data, Time->length);
    
    //YYmmddHHMMSS
    if(Time->type == V_ASN1_UTCTIME && Data.length() >= 10)
    {
        Return += "Year: ";
        Return += Data[0];
        Return += Data[1];
        Return += ", Month: ";
        Return += Data[2];
        Return += Data[3];
        Return += ", Day: ";
        Return += Data[4];
        Return += Data[5];
        Return += ", Hour: ";
        Return += Data[6];
        Return += Data[7];
        Return += ", Minute: ";
        Return += Data[8];
        Return += Data[9];
    }
    //YYYYmmddHHMMSS
    else if (Time->type == V_ASN1_GENERALIZEDTIME && Data.length() >= 12)
    {
        Return += "Year: ";
        Return += Data[0];
        Return += Data[1];
        Return += Data[2];
        Return += Data[3];
        Return += ", Month: ";
        Return += Data[4];
        Return += Data[5];
        Return += ", Day: ";
        Return += Data[6];
        Return += Data[7];
        Return += ", Hour: ";
        Return += Data[8];
        Return += Data[9];
        Return += ", Minute: ";
        Return += Data[10];
        Return += Data[11];
    }
    
    return Return;
}