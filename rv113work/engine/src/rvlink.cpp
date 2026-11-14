#include "rv/rvlink.hpp"
#include <iostream>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
namespace rv{int runBridgeServer(unsigned short port){
#ifdef _WIN32
 WSADATA wd{};if(WSAStartup(MAKEWORD(2,2),&wd))return 2;SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s==INVALID_SOCKET)return 3;
 sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
 if(bind(s,(sockaddr*)&a,sizeof(a))==SOCKET_ERROR||listen(s,8)==SOCKET_ERROR)return 4;
 std::cout<<"REALVISION bridge listening 127.0.0.1:"<<port<<"\n";
 for(;;){SOCKET c=accept(s,nullptr,nullptr);if(c==INVALID_SOCKET)continue;char buf[1024]{};int n=recv(c,buf,sizeof(buf)-1,0);std::string req(buf,n>0?n:0),resp;
 if(req.rfind("PING",0)==0)resp="PONG REALVISION_RENDER 0.2\n";
 else resp="ERR unsupported bridge command; use PING\n";
 send(c,resp.c_str(),(int)resp.size(),0);closesocket(c);}
#else
 (void)port;std::cerr<<"Bridge service currently Windows-only\n";return 1;
#endif
}}
