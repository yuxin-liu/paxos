#ifndef CLIENT_H
#define CLIENT_H

#include <vector>
#include <string>
#include "global.h"

class Client {
    std::string ip_;
    std::vector<std::string> server_ip_vec_;
    std::vector<int> server_port_vec_;
    int id_;
    int port_;
    int curr_server_id_;
    int sock_;
    int seq_;
    const double p = 0.1;

    bool SendMessage(const std::string &ip, int port, const std::string &msg);

public:

    Client() {}
    Client(int id, const char *ip, int port);
    void InitServerAddr(const char * file);
    void SendRequest(const std::string &msg);



};







#endif