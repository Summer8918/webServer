#include "server_base.hpp"
#include <fstream>

using namespace std;
using namespace WebServer;

template<typename SERVER_TYPE>
void start_server(SERVER_TYPE &server){
    //向服务器增加请求资源的处理方法

    //处理访问/string 的POST请求，返回POST的字符串
    server.resource["^/string/?$"]["POST"]=[](std::shared_ptr<std::ostream> response,std::shared_ptr<Request> request){
        cout<<"handle POST request 1 "<<endl;
       
        //从istream中获取字符串(*request.content)
        //std::stringstream ss;
        
    //    cout<<"handle POST request 2"<<endl;
        //*(request->content) >> ss.rdbuf();   //将请求内容读取到stringstream
        
        //cout<<"handle POST request 3 "<<endl;
        //string content=ss.str();
        
        std::string content(std::istreambuf_iterator<char>(*(request->content)),{});
        cout<<"handle POST request 2 "<<endl;
        //直接返回请求结果
        *response << "HTTP/1.1 200 OK\r\nContent-Length: "<<content.length() << "\r\n\r\n"<<content; 
        cout<<"handle POST request 3"<<endl;
    };

    //处理访问info的GET请求，返回请求的信息
    server.resource["^/info/?$"]["GET"]=[](std::shared_ptr<std::ostream> response,std::shared_ptr<Request> request){
        stringstream content_stream;
        content_stream<<"<h1>Request:</h1>";
        content_stream<<request->method<<" "<<request->path<<" HTTP/"<<request->http_version<<"<br>";
        for(auto& header:request->header){
            content_stream<<header.first<<": "<<header.second<<"<br>";
        }
        //获得content_stream的长度（使用content.tellp()获得）
        content_stream.seekp(0,ios::end);
        *response<<"HTTP/1.1 200 OK\r\nContent-Length: "<<content_stream.tellp()<<"\r\n\r\n"<<content_stream.rdbuf();
    };

    //处理访问/match/[字母+数字组成的字符串]的GET请求，例如执行请求GET /match/abc123,回abc123
    server.resource["^/match/([0-9a-zA-Z]+)/?$"]["GET"]=[](std::shared_ptr<std::ostream> response,std::shared_ptr<Request> request){
      string number=request->path_match[1];
      *response<<"HTTP/1.1 200 OK \r\nContent-Length: "<<number.length()<<"\r\n\r\n"<<number;
    };

   //处理默认GET请求，如果没有其他匹配成功，则这个匿名函数会被调用
   //将应答web/目录及其子目录中的文件
   //默认文件:index.html
   server.default_resource["^/?(.*)$"]["GET"]=[](std::shared_ptr<std::ostream> response,std::shared_ptr<Request> request){
       string filename = "web/"; 
       string path = request->path_match[1];
        //防止使用'..'来访问web目录外的内容
        size_t last_pos = path.rfind(".");
        size_t current_pos=0;
        size_t pos;
        while((pos=path.find('.',current_pos))!=string::npos && pos !=last_pos){
            current_pos=pos;
            path.erase(pos,1);
            last_pos--;
        }
        filename+=path;
        ifstream ifs;
        //简单的平台无关的文件或目录检查
        if(filename.find('.')==string::npos){
            if(filename[filename.length()-1]!='/')
                filename+='/';
            filename += "index.html";
        }
        ifs.open(filename,ifstream::in);
        if(ifs){
            ifs.seekg(0,ios::end);
            size_t length=ifs.tellg();

            ifs.seekg(0,ios::beg);

            //文件内容拷贝到response-stream中，不应该用于大型文件
            *response<<"HTTP/1.1 200 OK\r\nContent-Length: "<<length<<"\r\n\r\n"<<ifs.rdbuf();
            ifs.close();
        }
        else{
            //文件不存在时，返回无法打开文件
            string content="Counld not open file "+filename;
            *response<<"HTTP/1.1 400 Bad Request\r\nContent-Length: "<<content.length()<<"\r\n\r\n"<<content;
        }
   };

   //运行HTTP服务器
   server.start();
}
    
