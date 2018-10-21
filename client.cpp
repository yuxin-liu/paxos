#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <cerrno>

#include <fstream>
#include <iostream>

#include "client.h"


Client::Client(const char *ip, int port) : ip_(ip), curr_server_id_(0), 
                                 port_(port), seq_(0) {
    // init sock
    struct sockaddr_in address; 
       
    // Creating socket file descriptor 
    if ((sock_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( port_ ); 
       
    // Forcefully attaching socket to the port 8080 
    if (bind(sock_, (struct sockaddr *)&address, sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(sock_, SOMAXCONN) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }
}

void Client::InitServerAddr(const char * file) {
    std::ifstream in(file);
    if (!in) {
        std::cout << "open file failed\n";
        exit(1);
    }
    while (in) {
        std::string ip;
        int port;
        in >> ip >> port;
        if (!in)
            break;
        server_ip_vec_.push_back(ip);
        server_port_vec_.push_back(port);
    }
    in.close();
}

void Client::SendRequest(const std::string &msg) {
    std::string send_msg = "";
    //e.g.: 0,170.01.01.234,8080,ABC,1\0
    //sender_ID,IP,PORT,sequence number,request
    send_msg += "0,";
    send_msg += ip_ + ",";
    send_msg += std::to_string(port_) + ",";
    send_msg += std::to_string(seq_) + ",";
    send_msg += msg;
    while(true) {
        // send request to current server leader
        int send_sock = 0; 
        struct sockaddr_in serv_addr;  
        if ((send_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        { 
            printf("\n Socket creation error \n"); 
            exit(1); 
        } 

        memset(&serv_addr, '0', sizeof(serv_addr)); 

        serv_addr.sin_family = AF_INET; 
        serv_addr.sin_port = htons(server_port_vec_[curr_server_id_]); 
           
        // Convert IPv4 and IPv6 addresses from text to binary form 
        if(inet_pton(AF_INET, server_ip_vec_[curr_server_id_].c_str(), 
                                    &serv_addr.sin_addr)<=0) { 
            printf("\nInvalid address/ Address not supported \n"); 
            exit(1); 
        } 

        if (connect(send_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
            printf("\nConnection Failed \n"); 
            curr_server_id_++;
            if (curr_server_id_ == server_ip_vec_.size())
                curr_server_id_ = 0;
            continue; 
        }

        send(send_sock, send_msg.c_str(), strlen(send_msg.c_str()), 0);

        // TODO: wait on sock for ack with timeout using select
        // receive ack: break
        // if timeout: increment curr_server_id_ and continue
        int second = 10;
        fd_set fd;
        timeval tv;
        FD_ZERO(&fd);
        FD_SET(sock_, &fd);
        tv.tv_sec = second;
        tv.tv_usec = 0;
        int rv = select(sock_ + 1, &fd, NULL, NULL, &tv);
        if (rv == -1) {
            printf("socket error");
            exit(1);
        }
        else if (rv == 0) {
            curr_server_id_++;
            if (curr_server_id_ == server_ip_vec_.size())
                curr_server_id_ = 0;
            continue;
        }
        else {
            char buf = '6';
            std::string msg = "";

            int msg_fd = accept(sock_, nullptr, nullptr);

            while(recv(msg_fd, &buf, 1, MSG_WAITALL) == 1) {
                if(buf == '\0') {
                    //end of an message
                    break;
                }
                else {
                    msg += buf;
                }
            }//receive

            int seq = std::stoi(msg);
            if (seq == seq_) {
                // success, ok to send next request
                seq_++;
                break;
            }
            else {
                // receive again
                continue;
            }
        }

    }

}
   
int main(int argc, char const *argv[]) 
{ 
    if (argc < 4) {
        std::cout << "not enough arg" << std::endl;
        exit(1);
    }

    Client client(argv[1], std::atoi(argv[2]));
    client.InitServerAddr(argv[3]);

    client.SendRequest("chat0");
    client.SendRequest("chat1");

    return 0; 
} 