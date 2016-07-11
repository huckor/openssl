#include "SslClient.h"
#include "Log.h"
#include "Global.h"
#include "Conv.h"

SslClient::SslClient()
{
    _Conn = NULL;
    _Ssl = NULL;
    _Context = NULL;
    _SslSession = NULL;
    _ClientCertFile = "";
    _PrivateKeyFile = "";
    _CaCertFile = "";
    _ClientCertPwd = "";
    _Host = "";
    _Port = "";
    _WriteTimeout = 10;
    _ReadTimeout = 10;
    _CacheSslSession = false;
}

SslClient::~SslClient()
{
    Close();
}

//Initialize openssl library !!! CALL ONLY ONE TIME PER APPLICATION LIFE !!!
bool SslClient::Initialize()
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

int SslClient::Open()
{
    int Ret = -1;
    int Status = -1;
    int Socket = -1;
    struct timeval TimeVal;
    fd_set FdSet;
    
    //Create ssl structure and context
    if(CreateSslStructure() != OK)
        return FAIL;
        
    //Create TCP/IP connection
    _Conn = BIO_new_connect((char *)std::string(_Host + ":" + _Port).c_str());
    if(!_Conn)
    {
        LOG("Unable to create new BIO connection to host " + _Host + " port " + _Port, OPENSSL_LOG_BIT);
        return FAIL;
    }
    else
        LOG("New BIO created to host " + _Host + " port " + _Port, OPENSSL_LOG_BIT);
        
    //Set BIO to non blocking BIO
    if(!BIO_set_nbio(_Conn, 1))
    {
        LOG("Unable to configure BIO to NBIO", OPENSSL_LOG_BIT);
        return FAIL;
    }
    else
        LOG("BIO is configured to NBIO", OPENSSL_LOG_BIT);
    
    //Less or equal 0 means we need to wait on socket till openssl finish write operation
    if(BIO_do_connect(_Conn) <= 0)
    {
        Socket = BIO_get_fd(_Conn, NULL);
        if(Socket == -1)
        {
            LOG("Unable to connect to the remote machine.", OPENSSL_LOG_BIT);
            return FAIL;
        }
        
        TimeVal.tv_sec = _WriteTimeout;
        TimeVal.tv_usec = 0;
        FD_ZERO(&FdSet);
        FD_SET(Socket, &FdSet);
        if(select(Socket + 1, 0, &FdSet, 0, &TimeVal) <= 0)
        {
            LOG("Unable to connect to the remote machine.", OPENSSL_LOG_BIT);
            return FAIL;
        }
    }
    
    //Retrieve socket from BIO
    if(Socket == -1)
        Socket = BIO_get_fd(_Conn, NULL);
    
    //Pin connection to SSL
    SSL_set_bio(_Ssl, _Conn, _Conn);
    
    //Session caching ?
    if(_CacheSslSession && _SslSession != NULL)
    {
        SSL_set_session(_Ssl, _SslSession);
        _SslSession = NULL;
    }
    
    //SSL handshake
    while(true)
    {
        Ret = SSL_connect(_Ssl);
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
    
    //Check if ssl re-used saved ssl session
    Ret = SSL_session_reused(_Ssl);
    if(Ret == 0)
        LOG("SSL Connection created and opened, new session was negotiated.", OPENSSL_LOG_BIT);
    else
        LOG("SSL Connection created and opened, cached session was reused.", OPENSSL_LOG_BIT);
    
    return OK;
}

int SslClient::CreateSslStructure()
{
    //Create new ssl context
    _Context = SSL_CTX_new(TLSv1_method());
    if(!_Context)
    {
        LOG("Ssl context creation failed", OPENSSL_LOG_BIT);
        return FAIL;
    }

    //Load truststore used to authenticate peers (since this is the client side, in this case peer is the server)
    if(_CaCertFile.length() > 0)
    {
        if(SSL_CTX_load_verify_locations(_Context, _CaCertFile.c_str(), NULL) != 1)
        {
            LOG("Error loading CaCert file " + _CaCertFile, OPENSSL_LOG_BIT);
            return FAIL;
        }
        else
            LOG("CaCert loaded", OPENSSL_LOG_BIT);
    }
    
    if(_ClientCertPwd.length() > 0)
        SSL_CTX_set_default_passwd_cb(_Context, PwdCallback);
    
    //Load client certificate or client chain file
    //If the server requests client authentication, you have to authenticate yourself to the peer (since
    //this is the client side, in this case peer is the server), so you need load your own certificate and
    //private key, which are in the keystore. Or can be in separated files, see below "load client private key"
    if(_ClientCertFile.length() > 0)
    {
        if(SSL_CTX_use_certificate_chain_file(_Context, _ClientCertFile.c_str()) != 1)
        {
            LOG("Error loading ClientCert file " + _ClientCertFile, OPENSSL_LOG_BIT);
            return false;
        }
        else
            LOG("ClientCert loaded", OPENSSL_LOG_BIT);
    }
    
    //Load client private key
    //If the client certificate and client private key are in separate files, you need to load private key separately
    if(_PrivateKeyFile.length() > 0)
    {
        if(SSL_CTX_use_PrivateKey_file(_Context, _PrivateKeyFile.c_str(), SSL_FILETYPE_PEM) != 1)
        {
            LOG("Error loading PrivateKeyFile file " + _PrivateKeyFile, OPENSSL_LOG_BIT);
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

int SslClient::Write(std::vector<unsigned char> Data)
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

int SslClient::Read(unsigned char *Data, int Length)
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

int SslClient::CheckStatus(int StatusCode)
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

void SslClient::Close()
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

void SslClient::SetHost(std::string Host)
{
    _Host = Host;
}

void SslClient::SetThePort(std::string Port)
{
    _Port = Port;
}

//Truststore file name including path used to authenticate peers.
//(since this is the client side, in this case peer is the server)
void SslClient::SetCaCert(std::string CaCertFile)
{
    _CaCertFile = CaCertFile;
}

//Client certificate or client chain file used to authenticate client. File name must including path.
//If the server requests client authentication, you have to authenticate yourself to the peer (since
//this is the client side, in this case peer is the server), so you need load your own certificate and
//private key, which are in the keystore. Or can be in separated files, see below function SetClientPrivateKey
void SslClient::SetClientCert(std::string ClientCertFile)
{
    _ClientCertFile = ClientCertFile;
}

//Client private key file load. File name must including path.
//If the client certificate and client private key are in separate files, you need to load private key separately using this function.
void SslClient::SetClientPrivateKey(std::string PrivateKeyFile)
{
    _PrivateKeyFile = PrivateKeyFile;
}

void SslClient::SetClientCertPwd(std::string ClientCertPwd)
{
    _ClientCertPwd = ClientCertPwd;
}

void SslClient::SetWriteTimeout(int TimeoutSec)
{
    _WriteTimeout = TimeoutSec;
}

void SslClient::SetReadTimeout(int TimeoutSec)
{
    _ReadTimeout = TimeoutSec;
}

void SslClient::SetSessionCaching(bool CacheSslSession)
{
    _CacheSslSession = CacheSslSession;
}




std::string SslClient::_ClientCertPwd = "";

int SslClient::PwdCallback(char *Buf, int Size, int Flag, void *UserData)
{
    strcpy(Buf, _ClientCertPwd.c_str());
    return (int)_ClientCertPwd.length();
}

int SslClient::VerifyCallback(int Ok, X509_STORE_CTX *Store)
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

std::string SslClient::Asn1TimeToString(ASN1_TIME* Time)
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