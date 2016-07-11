#include "TcpIpServer.h"
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "Log.h"
#include "Conv.h"
#include "Global.h"

TcpIpServer::TcpIpServer()
{
    _ReadTimeout = 10;
    _Conn = NULL;
    _Client = NULL;
    _Port = "";
}

TcpIpServer::~TcpIpServer()
{
    if(_Conn)
        Close();
}

void TcpIpServer::SetThePort(std::string Port)
{
    _Port = Port;
}

void TcpIpServer::SetReadTimeout(int Timeout)
{
    _ReadTimeout = Timeout;
}

int TcpIpServer::Bind()
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

int TcpIpServer::Accept()
{
    int Socket = -1;
    int Ret = -1;
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

    LOG("New connection accepted.", OPENSSL_LOG_BIT);
    
    return OK;
}

int TcpIpServer::Write(std::vector<unsigned char> Data)
{
    int Ret = 0;
    
    if(_Client == NULL)
    {
        LOG("Error - connection not registered.", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    Ret = BIO_write(_Client, &Data[0], (int)Data.size());
    if(Ret >= 0)
        return OK;
    else
        LOG("Error - write failed.", OPENSSL_LOG_BIT);
    
    return FAIL;
}

int TcpIpServer::Read(unsigned char *Buffer, int Length)
{
    int Socket = -1;
    int Ret = 0;
    struct timeval TimeVal;
    fd_set FdSet;
    
    if(_Client == NULL)
    {
        LOG("Error - connection not registered.", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    Socket = BIO_get_fd(_Client, NULL);
    TimeVal.tv_sec = _ReadTimeout;
    TimeVal.tv_usec = 0;
    FD_ZERO(&FdSet);
    FD_SET(Socket, &FdSet);
    Ret = select(Socket + 1, &FdSet, 0, 0, &TimeVal);
    if(Ret < 0)
    {
        LOG("Error on socket", OPENSSL_LOG_BIT);
        return FAIL;
    }
    else if(Ret == 0)
    {
        LOG("Timeout on socket", OPENSSL_LOG_BIT);
        return TIMEOUT;
    }
    
    Ret = BIO_read(_Client, Buffer, Length);
    if(Ret <= 0)
    {
        LOG("Error on reading from socket", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    return Ret;
}

void TcpIpServer::Close()
{
    if(_Conn)
    {
        BIO_free(_Conn);
        _Conn = NULL;
    }
    
    if(_Client)
    {
        BIO_free(_Client);
        _Client = NULL;
    }
}
