#include "TcpIpClient.h"
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "Log.h"
#include "Conv.h"
#include "Global.h"

TcpIpClient::TcpIpClient()
{
    _ReadTimeout = 10;
    _WriteTimeout = 10;
    _Conn = NULL;
    _Host = "";
    _Port = "";
}

TcpIpClient::~TcpIpClient()
{
    if(_Conn)
        Close();
}

void TcpIpClient::SetThePort(std::string Port)
{
    _Port = Port;
}

void TcpIpClient::SetHost(std::string Host)
{
    _Host = Host;
}

void TcpIpClient::SetReadTimeout(int Timeout)
{
    _ReadTimeout = Timeout;
}

void TcpIpClient::SetWriteTimeout(int Timeout)
{
    _WriteTimeout = Timeout;
}

int TcpIpClient::Open()
{
    int Socket = -1;
    struct timeval TimeVal;
    fd_set FdSet;
    
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
        Socket = (int)BIO_get_fd(_Conn, NULL);
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
    
    LOG("Sucesfully connected to the remote machine.", OPENSSL_LOG_BIT);
    return OK;
}

int TcpIpClient::Write(std::vector<unsigned char> Data)
{
    int Ret = 0;
    
    if(_Conn == NULL)
    {
        LOG("Error - connection not opened.", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    Ret = BIO_write(_Conn, &Data[0], (int)Data.size());
    if(Ret >= 0)
        return OK;
    else
        LOG("Error - write failed.", OPENSSL_LOG_BIT);
    
    return FAIL;
}

int TcpIpClient::Read(unsigned char *Buffer, int Length)
{
    int Socket = -1;
    int Ret = 0;
    struct timeval TimeVal;
    fd_set FdSet;
    
    if(_Conn == NULL)
    {
        LOG("Error - connection not opened.", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    Socket = (int)BIO_get_fd(_Conn, NULL);
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
    
    Ret = BIO_read(_Conn, Buffer, Length);
    if(Ret <= 0)
    {
        LOG("Error on reading from socket", OPENSSL_LOG_BIT);
        return FAIL;
    }
    
    return Ret;
}

void TcpIpClient::Close()
{
    if(_Conn)
    {
        BIO_free(_Conn);
        _Conn = NULL;
    }
}