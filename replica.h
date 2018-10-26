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

    // static bool IsEqual(const Request& r1, const Request& r2) {
    //     if ()
    //     if (r1.client_ip.compare(r2.client_ip)) return false;
    //     if (r1.chat_msg.compare(r2.chat_msg)) return false;
    //     if (r1.client_port != r2.client_port) return false;
    //     if (r1.client_seq != r2.client_seq) return false;
    //     return true;
    // }

    void PrintRequestLog() {
        std::cout << GetStr() << std::endl;
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
public:
    int primary_id_; // current leader id
    int view_num_;
    int current_view_;
    int id_;
    int replica_num_;
    int receive_sock_;
    int chat_log_len_;
    int next_slot_num_;
    bool is_skip_mode_;
    bool is_loss_mode_;
    std::vector<std::string> ip_vec_;
    std::vector<int> port_vec_;
    std::map<int, ProposedValue *> propose_map_; // slot number -> propose info
    std::set<std::pair<int, int>> req_set_; // {clientID, seqNum} 
            // only contain executed req, drop if already executed and ack back.
    std::set<int> supporting_set_;
    std::ofstream os_;
    const double p = 0.005;


    void SendMessage(const std::string &ip, int port, const std::string &msg);

    Replica() {}
    Replica(int id, int port);
    void InitServerAddr(const char * file);
    void AskForFollow();
    void Decree(int inSlotNum);
    void StartRun();
};

#endif