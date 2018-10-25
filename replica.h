#ifndef REPLICA_H
#define REPLICA_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <iostream>
#include <set>
// #include <atomic>

class Request {
public:
    std::string client_ip;
    std::string chat_msg;
    int client_port;
    int client_seq;
    int client_id;

    Request() {}
    Request(const std::string &inMsg) {
        std::string msg = inMsg;
        client_id = std::stoi(msg.substr(6,msg.find('@')));
        msg.erase(msg.begin() + msg.find('@'));
        client_ip = msg.substr(0,msg.find(':'));
        msg.erase(msg.begin() + msg.find(':'));
        client_port = std::stoi(msg.substr(0,msg.find(':')));
        msg.erase(msg.begin() + msg.find(':'));
        client_seq = std::stoi(msg.substr(0,msg.find(':')));
        msg.erase(msg.begin() + msg.find(':'));
        chat_msg = msg;

    }

    static bool IsEqual(const Request& r1, const Request& r2) {
        if (r1.client_ip.compare(r2.client_ip)) return false;
        if (r1.chat_msg.compare(r2.chat_msg)) return false;
        if (r1.client_port != r2.client_port) return false;
        if (r1.client_seq != r2.client_seq) return false;
        return true;
    }

    void PrintRequestLog() {
        std::cout << GetStr() << std::endl;
    }

    std::string GetStr() const {
        std::string m = "CLIENT" + std::to_string(client_id) + "@" + client_ip
            + ":" + std::to_string(client_port) + ":" + std::to_string(client_seq)
            + ":" + chat_msg;
        return m;
    }

};

class ProposedValue {
public:
    Request * value;
    int view_num; // leader id
    int slot_id;
    std::set<int> vote_set;
};

class Replica {
public:
    int primary_id_; // current leader id
    bool is_supported_; // true if I'm supported by majority and can skip phase 1
    int id_;
    int replica_num_;
    int receive_sock_;
    std::vector<std::string> ip_vec_;
    std::vector<int> port_vec_;
    std::vector<int> hb_port_vec_;
    std::map<int, ProposedValue *> propose_map_; // slot number -> propose info
    std::map<std::pair<int, int>, int> req_map_; // {clientID, seqNum} -> slot number
            // only contain executed req, drop if already executed and ack back.
    std::map<int, Request *> decided_map_; // slot -> reqest info
    std::mutex m_; // used to atomically decided if I'm leader
    std::set<int> supporting_set_;
    int chat_log_len_;
    int next_slot_num_;
    int highest_view_num_;
    int view_num_;

    void SendMessage(const std::string &ip, int port, const std::string &msg);

    Replica() {}
    Replica(int id, int port);
    void InitServerAddr(const char * file);
    void AskForFollow();
    void Decree(int inSlotNum);
    void StartRun();


    // void Heartbeat() {
    //     std::vector<int> sockets;
    //     for (int i = 0; i < replica_num_; ++i) {
    //         if (i == id_)
    //             continue;
    //         struct sockaddr_in address; 
    //         int sock = 0, valread; 
    //         struct sockaddr_in serv_addr;  
    //         char buffer[1024] = {0}; 
    //         if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    //         { 
    //             printf("\n Socket creation error \n"); 
    //             return -1; 
    //         } 

    //         memset(&serv_addr, '0', sizeof(serv_addr)); 

    //         serv_addr.sin_family = AF_INET; 
    //         serv_addr.sin_port = htons(hb_port_vec_[i]); 
               
    //         // Convert IPv4 and IPv6 addresses from text to binary form 
    //         if(inet_pton(AF_INET, ip_vec_[i], &serv_addr.sin_addr)<=0)  
    //         { 
    //             printf("\nInvalid address/ Address not supported \n"); 
    //             return -1; 
    //         } 

    //         if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    //         { 
    //             printf("\nConnection Failed \n"); 
    //             continue; 
    //         }
    //         sockets.push_back(sock);
    //     }
    //     char *hb = "heartbeat";
    //     while(true) {
    //         for (int i = 0; i < sockets.size(); ++i) {
    //             send(sockets[i] ,hb , strlen(hb) , 0 );
    //         }
    //         sleep(1);
    //     }
    // }

    



};

#endif