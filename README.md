### Networking demo using Openssl library.



Programming language: C++

OS Usage: MacOS 64bit, Windows 32bit, Linux 64bit

Projects: Xcode, Visual Studio 2010, Eclipse

**Notes**

Before run, set correct paths to certificate files. Look to main.cpp file to variable CertPath.  

**What is this**

This simple demo demonstrate how to use simple TCP/IP un-secure server - client communication and TCP/IP secure server - client communication. In both cases this application use Openssl library.
Because this is just demo application, it can serve only one client at a time in both cases.

**In nutshell this application doing this**

After successful build and run, this application initialise Openssl library, create thread with server wait few seconds, then create client (in main thread) connect to server send 4bytes of data and close connection. This happened 2 times, first time using un-secure connection and second time using secure connection.
