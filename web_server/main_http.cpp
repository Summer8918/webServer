#include <iostream>
#include "server_http.hpp"
#include "handler.hpp"

using namespace WebServer;
using namespace std;
int main(int argc,char *argv[]){
    //HTTP服务运行在12345端口，并启用4个线程
    Server<HTTP> server(12345,1);
    std::cout<<"Server starting at port:12345"<<std::endl;
    start_server<Server<HTTP>>(server);
    return 0;
}
