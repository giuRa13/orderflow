//#include <ixwebsocket/IXNetSystem.h>
//#include <ixwebsocket/IXWebSocket.h>
//#include <iostream>
//#include <mutex>
//#include <vector>
//#include <nlohmann/json.hpp>
#include <application.h>

int main()
{
    // init Network (Crucial for Windows)
    //ix::initNetSystem();

    //ix::WebSocket web_socket;
    //std::string url = "wss://stream.binance.com:9443/ws/btcusdt@trade";
    //std::string url = "wss://stream.binance.com:9443/ws/ethusdt@kline_1m";
    //web_socket.setUrl(url);

    /*web_socket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg){
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::cout << "Data: " << msg->str << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Error)
        {
            std::cerr << "Error: " << msg->errorInfo.reason << std::endl;
        }
    });
    
    web_socket.start();*/

    // UI loop
    Application app;
    app.init();
    app.run();


    //web_socket.stop();
    //ix::uninitNetSystem();
    return 0;

}