#ifndef REPLICA_H
#define REPLICA_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <set>

class Request {
public:
    int client_port;
    int client_seq;
    int client_id;
    std::string client_ip;
    std::string chat_msg;

    Request() {}
    Request(const std::string &inMsg) {
        if (inMsg == "NO-OP") {
            client_id = -1;
        }
        else {
            std::string msg = inMsg;
            client_id = std::stoi(msg.substr(6,msg.find('@')));
            msg.erase(0, msg.find('@') + 1);
            client_ip = msg.substr(0,msg.find(':'));
            msg.erase(0, msg.find(':') + 1);
            client_port = std::stoi(msg.substr(0,msg.find(':')));
            msg.erase(0, msg.find(':') + 1);
            client_seq = std::stoi(msg.substr(0,msg.find(':')));
            msg.erase(0, msg.find(':') + 1);
            chat_msg = msg;
        }
    }

    std::string GetStr() const {
        if (client_id == -1) {
            return "NO-OP";
        }
        std::string m = "CLIENT" + std::to_string(client_id) + "@" + client_ip
            + ":" + std::to_string(client_port) + ":" + std::to_string(client_seq)
            + ":" + chat_msg;
        return m;
    }
};

class ProposedValue {
public:
    Request * value;
    int view_num;
    int slot_id;
    std::set<int> vote_set; // set of replicas that accpet this value.
};

class Replica {
private:
    int primary_id_; // current leader id
    int current_view_; // current view id
    int id_;
    int replica_num_;
    int receive_sock_;
    int chat_log_len_; // upperbound of executed slots
    int next_slot_num_; // slot number for next coming request from client
    bool is_skip_mode_;
    bool is_loss_mode_;
    std::vector<std::string> ip_vec_;
    std::vector<int> port_vec_;
    std::map<int, ProposedValue *> propose_map_; // slot number -> propose info
    std::set<std::pair<int, int>> req_set_; // executed {clientID, seqNum} 
    std::set<int> supporting_set_;
    std::ofstream os_;
    const double p = 0.005;

    void SendMessage(const std::string &ip, int port, const std::string &msg);
    void AskForFollow();
    void Decree(int inSlotNum);

public:
    Replica() {}
    Replica(int id, int port);
    void InitServerAddr(const char * file);
    void StartRun();
};

#endif