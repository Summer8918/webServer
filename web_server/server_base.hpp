#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include<regex>
#include<unordered_map>
#include<thread>
#include<string>
using namespace std;

namespace WebServer{
    //用于解析请求，如请求方法、路径和HTTP版本等信息
    //定义一个std::istream指针来保存请求体中包含的内容
    struct Request{
        //请求方法:POST,GET
        std::string method;
        //请求路径
        std::string path;
        //HTTP版本
        std::string http_version;
        //对content使用智能指针进行引用计数
        std::shared_ptr<std::istream> content;
        //哈希容器，key-value字典
        std::unordered_map<std::string,std::string> header;
        //用正则表达式处理路径匹配
        std::smatch path_match;
    };
    
    //使用typedef简化资源类型的表示方式
    //resource_type是一个std::map，其键为一个字符串，值为一个无序容器std::unordered_map，这个无序容器的键依然是一个字符串，其值接受一个返回类型为空，参数为ostream和Request的函数
    //std::map用于存储请求路径的正则表达式，而std::unordered_map用于存储请求方法，最后一个匿名lambda函数来保存处理方法。有了资源类型，我们仅仅只是定义了当他人使用这套框架时的接口
    typedef std::map<std::string,
            std::unordered_map<std::string,
                std::function<void(std::shared_ptr<std::ostream>,
                        std::shared_ptr<Request>)>>> resource_type;

    //socket_type为HTTP or HTTPS
    template <typename socket_type> class ServerBase{
    public:
        resource_type resource;
        resource_type default_resource;

        ServerBase(unsigned short port,size_t num_threads=1):
            endpoint(boost::asio::ip::tcp::v4(),port),
            acceptor(m_io_service,endpoint),
            num_threads(num_threads) {}
       
        void start();

    protected:
        boost::asio::io_service m_io_service;

        boost::asio::ip::tcp::endpoint endpoint;

        boost::asio::ip::tcp::acceptor acceptor;
        std::vector<resource_type::iterator> all_resources;

        virtual void accept() {}
        std::shared_ptr<Request> parse_request(std::istream& stream) const;

        void process_request_and_respond(std::shared_ptr<socket_type> socket) const;
        size_t num_threads;
        void respond(std::shared_ptr<socket_type> socket,std::shared_ptr<Request> request) const;
        std::vector<std::thread> threads;
    };

    template<typename socket_type> class Server : public ServerBase<socket_type> {};

 //   template<typename socket_type>
 //   void WebServer::ServerBase<socket_type>::stop() {
 //       m_io_service.stop();
 //   }

    template<typename socket_type> void ServerBase<socket_type>::start(){
        for(auto it=resource.begin();it!=resource.end();it++){
            all_resources.push_back(it);
        }

        for(auto it=default_resource.begin();it!=default_resource.end();it++){
            all_resources.push_back(it);
        }

        accept();
        for(size_t c=1;c<num_threads;c++){
            threads.emplace_back([this](){
                m_io_service.run();
            });
        }
        //主线程
        m_io_service.run();

        //等待其他线程，如果有的话，等待这些线程的结束
        for(auto& t :threads){
            t.join();
        }
    }
    template<typename socket_type>
    std::shared_ptr<Request> ServerBase<socket_type>:: parse_request(std::istream& stream) const{
        std::shared_ptr<Request> request=std::make_shared<Request>(Request());

        //使用正则表达式对请求报头进行解析，通过下面的正则表达式
        //可以解析出请求方法（GET/POST）、请求路径以及HTTP版本
        static std::regex regex_header=std::regex("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

        static std::regex regex_body = std::regex("^([^:]*): ?(.*)$");

        std::smatch sub_match;

        //从第一行中解析请求方法、路径和HTTP版本
        std::string line;
        std::getline(stream,line);
        line.pop_back();
        if(std::regex_match(line,sub_match,regex_header)){
            request->method = sub_match[1];
            request->path = sub_match[2];
            request->http_version = sub_match[3];

            if((request->path).find('?') != std::string::npos){
                request->path = (request->path).substr(0,(request->path).find('?'));
            }
            bool matched=false;
            //e="^([^:]*): ?(.*)$";
            do{
                getline(stream,line);
                line.pop_back();
                matched=std::regex_match(line,sub_match,regex_body);
                if(matched){
                    request->header[sub_match[1]]=sub_match[2];
                }
            }while(matched==true);
        }
        return request;
    }

    template<typename socket_type>
    void ServerBase<socket_type>::process_request_and_respond(std::shared_ptr<socket_type> socket) const{
        //为async_read_untile()创建新的读缓存
        //shared_ptr用于传递临时对象给匿名函数
        //会被推导为std::shared_ptr<boost::asio::streambuf>
        auto read_buffer = std::make_shared<boost::asio::streambuf>();

        boost::asio::async_read_until(*socket, *read_buffer,"\r\n\r\n",
        [this,socket,read_buffer](const boost::system::error_code& ec, size_t bytes_transferred){
            if(!ec){
            /*
             * 注意:read_buffer->size()的大小并不一定与bytes_transferred相等
             * Boost的文档中指出：
             * 在async_read_until操作成功之后，streambuf在界定符之外可能包含一些额外的数据
             * 所以较好的做法是直接从流中提取并解析当前read_buffer左边的报头，再拼接async_read后面的内容*/
                size_t total = read_buffer->size();

                //转换到istream
                std::istream stream(read_buffer.get());

                //被推导为std::shared_ptr<Request>类型
                //将stream中的请求信息进行解析，然后保存到request对象中
                std::shared_ptr<Request> request = parse_request(stream);

                size_t num_additional_bytes = total-bytes_transferred;

                //如果满足，同样读取
                //stoull转译字符串 str 中的无符号整数值。
                if(request->header.count("Content-length")>0){
                    boost::asio::async_read(*socket,*read_buffer,
                    boost::asio::transfer_exactly(stoull(request->header["Content-Length"])-num_additional_bytes),
                    [this,socket,read_buffer,request](const boost::system::error_code& ec,size_t bytes_transferred){
                        if(!ec){
                            //将指针作为istream对象存储到read_buffer中
                            request->content = std::make_shared<std::istream>(read_buffer.get());
                            respond(socket,request);
                        }
                    });
                }
                else{
                    respond(socket,request);
                }
            }
            //接下来要将stream中的请求信息进行解析，然后保存到request对象中
        });
    }

    //应答
    template<typename socket_type>
    void ServerBase<socket_type>::respond(std::shared_ptr<socket_type> socket,std::shared_ptr<Request> request) const{
        //对请求路径和方法进行匹配查找，并生成响应
        for(auto res_it: all_resources){
            std::regex e(res_it->first);
            std::smatch sm_res;
            if(std::regex_match(request->path,sm_res,e)){
                if(res_it->second.count(request->method)>0){
                    request->path_match=move(sm_res);

                    //会被推导为std::shared_ptr<boost::asio::streambuf>
                    std::shared_ptr<boost::asio::streambuf> write_buffer = std::make_shared<boost::asio::streambuf>();
                    std::shared_ptr<std::ostream> response = std::make_shared<std::ostream>(write_buffer.get());
                    res_it->second[request->method](response,request);

                    //在lambda中捕获write_buffer使其不会在async_write完成前被销毁
                    //lambda[]()，其中()中的内容为向lambda函数传递的参数
                    boost::asio::async_write(*socket,*write_buffer,
                    [this,socket,request,write_buffer](const boost::system::error_code& ec,size_t bytes_transferred){
                        //HTTP持久连接HTTP 1.1 递归调用
                        if(!ec && stof(request->http_version)>1.05){
                            process_request_and_respond(socket);
                        }
                    });
                    return;
                }
            }
        }
    }
}
