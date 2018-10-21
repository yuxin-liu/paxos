#ifndef GLOBAL_H
#define GLOBAL_H

#include <iostream>
#include <string>



class Request {
public:
    std::string client_ip;
    std::string chat_msg;
    int client_port;

    Request() {}

    static bool IsEqual(const Request& r1, const Request& r2) {
        if (r1.client_ip.compare(r2.client_ip)) return false;
        if (r1.chat_msg.compare(r2.chat_msg)) return false;
        if (r1.client_port != r2.client_port) return false;
        return true;
    }

    void PrintRequestLog() {
        std::cout << "CLIENT@" << client_ip << ":" << client_port << ":" << chat_msg;
    }



};

#endif