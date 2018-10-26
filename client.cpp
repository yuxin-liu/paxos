#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <cerrno>
#include <stdlib.h> 
#include <time.h> 

#include <fstream>
#include <iostream>

#include "client.h"


Client::Client(int id, const char *ip, int port) : id_(id), ip_(ip), 
                        curr_server_id_(0), port_(port), seq_(0) {
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
    int temp;
    in >> temp >> temp;
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

bool Client::SendMessage(const std::string &ip, int port, const std::string &msg) {
    std::cout << "send to " << port << ":" << msg << std::endl;
    // int rand_num = rand() % (int)(1.0/p);
    // if (rand_num == 0)
    //     return true;

    int send_sock = 0; 
    struct sockaddr_in serv_addr;  
    if ((send_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        exit(1); 
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr)<=0) { 
        printf("\nInvalid address/ Address not supported \n"); 
        exit(1); 
    } 
    std::cout << "send to " << ip << ":" << port << std::endl;

    if (connect(send_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
        printf("\nConnection Failed, send msg failed \n"); 
        return false; 
    }

    send(send_sock, msg.c_str(), strlen(msg.c_str()), 0);
    close(send_sock);

    std::cout << "send msg done" << std::endl;
    return true;
}

void Client::SendRequest(const std::string &msg) {
    std::string send_msg = "C%" + std::to_string(id_) + ",";
    //e.g.: 0,170.01.01.234,8080,ABC,1\0
    //sender_ID,IP,PORT,sequence number,request
    send_msg += ip_ + ",";
    send_msg += std::to_string(port_) + ",";
    send_msg += std::to_string(seq_) + ",";
    send_msg += msg;
    std::cout << "client " << id_ << " start sending:" << send_msg << std::endl;
    int fail_count = 0;
    bool acked = false;
    while(true) {
        if (acked)
            break;
        // send request to current server leader
        if (fail_count >= 3) {
            // change leader
            std::string send_msg_die = "S%" + std::to_string(id_) + ",";
            send_msg_die += ip_ + ",";
            send_msg_die += std::to_string(port_) + ",";
            send_msg_die += std::to_string(curr_server_id_);
            while (true) {
                for (int i = 0; i < server_ip_vec_.size(); ++i) {
                    SendMessage(server_ip_vec_[i], server_port_vec_[i], send_msg_die);
                }
                int second_die = 5;
                fd_set fd_die;
                timeval tv_die;
                FD_ZERO(&fd_die);
                FD_SET(sock_, &fd_die);
                tv_die.tv_sec = second_die;
                tv_die.tv_usec = 0;
                int rv_die = select(sock_ + 1, &fd_die, NULL, NULL, &tv_die);
                if (rv_die == -1) {
                    printf("socket error");
                    exit(1);
                }
                else if (rv_die == 0) {
                    std::cout << "waiting for new leader timeout" << std::endl;
                    continue;
                }
                else {
                    char buf_die = '6';
                    std::string msg_die = "";

                    int msg_fd_die = accept(sock_, nullptr, nullptr);

                    while(recv(msg_fd_die, &buf_die, 1, MSG_WAITALL) == 1) {
                        if(buf_die == '\0') {
                            //end of an message
                            break;
                        }
                        else {
                            msg_die += buf_die;
                        }
                    }//receive
                    std::cout << "recieved msg:" << msg_die << std::endl;
                    if (msg_die[0] == 'L') {
                        int id = std::stoi(msg_die.substr(2));
                        curr_server_id_ = id;
                        std::cout << "change leader to "<< id <<std::endl;
                        break;
                    }
                } 
            }
            fail_count = 0;

        }
        if (!SendMessage(server_ip_vec_[curr_server_id_], server_port_vec_[curr_server_id_], send_msg)) {
            fail_count++;
            continue;
        }


        while (true) {
            int second = 30;
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
                std::cout << "waiting for ack timeout" << std::endl;
                fail_count++;
                if (fail_count >= 3)
                    break;
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
                std::cout << "recieved msg:" << msg << std::endl;

                if (msg[0] == 'A') {
                    int seq = std::stoi(msg.substr(2));
                    if (seq == seq_) {
                        seq_++;
                        std::cout << "seq " << seq << "acked" << std::endl;
                        acked = true;
                        break;
                    }
                }

            }
        }


    }

}
   
int main(int argc, char const *argv[]) 
{ 
    if (argc < 4) {
        std::cout << "client usage: ./client <id> <ip> <recieve_port>" << std::endl;
        exit(1);
    }
    srand (time(NULL));

    Client client(std::atoi(argv[1]), argv[2], std::atoi(argv[3]));
    client.InitServerAddr("server.config");

    std::string chat;
    while(std::cin >> chat) {
        client.SendRequest(chat);
        chat.clear();
    }
    return 0; 
} 