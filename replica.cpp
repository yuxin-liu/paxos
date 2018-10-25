#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include <thread>


#include "replica.h"

Replica::Replica(int id, int port) : id_(id), chat_log_len_(0), view_num_(0),
        primary_id_(0), is_supported_(0), next_slot_num_(0), highest_view_num_(-1) {
    std::cout << "start init replica " << id_ <<std::endl;
    struct sockaddr_in address; 
    // Creating socket file descriptor 
    if ((receive_sock_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( port ); 
       
    // Forcefully attaching socket to the port
    if (bind(receive_sock_, (struct sockaddr *)&address, sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(receive_sock_, SOMAXCONN) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }
    std::cout << "end init replica " << id_ <<std::endl;

}

void Replica::SendMessage(const std::string &ip, int port, const std::string &msg) {
    std::cout << "send msg:" << msg << ":to " << ip << ":" << port << std::endl;
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

    if (connect(send_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
        printf("\nConnection Failed \n");  
    }

    send(send_sock, msg.c_str(), strlen(msg.c_str()), 0);
    std::cout << "end send msg:" << msg << ":to " << ip << ":" << port << std::endl;
}

void Replica::AskForFollow() {
    std::string send_msg = "I%" + std::to_string(id_) + "," 
                    + std::to_string(id_);
    // check for holes
    for (int i = chat_log_len_; i < next_slot_num_; ++i) {
        if (propose_map_.find(i) == propose_map_.end()) {
            send_msg += "," + std::to_string(i);
        }
    }

    for (int i = 0; i < replica_num_; ++i) {
        if (i == id_)
            continue;
        SendMessage(ip_vec_[i], port_vec_[i], send_msg);
    }
}

void Replica::Decree(int inSlotNum) {
    std::string send_msg = "D%" + std::to_string(id_) + "," + std::to_string(id_)
                    + std::to_string(inSlotNum) + ",";
    Request * r = propose_map_[inSlotNum]->value;
    propose_map_[inSlotNum]->vote_set.insert(id_);
    send_msg += r->GetStr();
    for (int i = 0; i < replica_num_; ++i) {
        if (id_ == i)
            continue;
        SendMessage(ip_vec_[i], port_vec_[i], send_msg);
    }
}


void Replica::InitServerAddr(const char * file) {
    std::cout << "start init server addr for replica " << id_ <<std::endl;
    std::ifstream in(file);
    if (!in) {
        std::cout << "open file failed\n";
        exit(1);
    }
    while (in) {
        std::string ip;
        int port, hb_port;
        in >> ip >> port >> hb_port;
        if (!in)
            break;
        ip_vec_.push_back(ip);
        port_vec_.push_back(port);
        hb_port_vec_.push_back(hb_port);
    }
    in.close();
    replica_num_ = ip_vec_.size();
    std::cout << "end init server addr for replica " << id_ <<std::endl;
}

void Replica::StartRun() {
    std::cout << "start run" << std::endl;
    char buf = '6';
    std::string msg = "";
    std::string msg_type = "";
    bool first = true;
    while (true) {

        m_.lock();
            int primary_id = primary_id_;
            int my_view_num = view_num_;
        m_.unlock();

        // check for execution, exe (and send ack to client if I'm leader)
        while (decided_map_.find(chat_log_len_) != decided_map_.end()) {
            Request * r = decided_map_[chat_log_len_];
            if (primary_id == id_) {
                std::string send_msg = "A%" + std::to_string(r->client_seq);
                SendMessage(r->client_ip, r->client_port, send_msg);
            }
            std::pair<int, int> req = std::make_pair(r->client_id, r->client_seq);
            if (req_map_.find(req) != req_map_.end()) {
                // if the client,seq exed before, exe a no-op
                decided_map_[chat_log_len_] = nullptr;
                delete(r);
            }
            chat_log_len_++;
        }

        if (primary_id == id_) {
            // send IAMLEADER, ask for holes
            if (!is_supported_) {
                AskForFollow();
            }
            else {
                // if there are still holes, propose no-op for them
                for (int i = chat_log_len_; i < next_slot_num_; ++i) {
                    if (propose_map_.find(i) == propose_map_.end()) {
                        propose_map_[i] = new ProposedValue();
                        propose_map_[i]->slot_id = i;
                        propose_map_[i]->value = nullptr;
                        propose_map_[i]->view_num = my_view_num;
                    }
                }
                for (auto it = propose_map_.begin(); it != propose_map_.end(); ++it) {
                    Decree(it->first);
                }
            }
        }

        // timeout
        int second = 5;
        fd_set fd;
        timeval tv;
        FD_ZERO(&fd);
        FD_SET(receive_sock_, &fd);
        tv.tv_sec = second;
        tv.tv_usec = 0;
        int rv = select(receive_sock_ + 1, &fd, NULL, NULL, &tv);
        if (rv == -1) {
            printf("socket error");
            exit(1);
        }
        else if (rv == 0) {
            continue;
        }

        msg_type.clear();
        msg.clear();
        int msg_fd = accept(receive_sock_, nullptr, nullptr);
        
        while(recv(msg_fd, &buf, 1, MSG_WAITALL) == 1) {
            if(buf == '\0') {
                //end of an message
                break;
            }
            if(first) {
                if(buf == '%')
                    first = false;
                else
                    msg_type += buf;
            }
            else {
                msg += buf;
            }
        }//receive

        //get sender ID
        int sender_ID = std::stoi(msg.substr(0,msg.find(',')));
        msg.erase(msg.begin() + msg.find(','));

        m_.lock();
            primary_id = primary_id_;
        m_.unlock();

        if(msg_type == "C")
        {
            std::string client_ip = msg.substr(0,msg.find(','));
            msg.erase(msg.begin() + msg.find(','));
            int client_port = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            if (primary_id != id_) {
                // I'm not leader. ignore client's request
                // tell client current leader
                std::string send_msg = "L" + std::to_string(primary_id);
                SendMessage(client_ip, client_port, send_msg);
                continue;
            }

            Request * r = new Request();
            r->client_ip = client_ip;
            r->client_port = client_port;
            r->client_seq = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            r->chat_msg = msg;

            int slot_num = next_slot_num_++;

            ProposedValue * p = new ProposedValue();
            p->value = r;
            p->view_num = id_;

            propose_map_[slot_num] = p;

        }
        else if(msg_type == "I")
        {
            if (sender_ID != primary_id)
                continue;
            int view_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            if (view_num < highest_view_num_)
                continue;
            highest_view_num_ = view_num;

            
            std::vector<int> asked_hole_vec;
            while (msg.find(',') != std::string::npos) {
                int hole_slot = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(msg.begin() + msg.find(','));
                asked_hole_vec.push_back(hole_slot);
            }
            if (!msg.empty()) {
                asked_hole_vec.push_back(std::stoi(msg));
            }

            std::string send_msg = "Y%" + std::to_string(id_) + ",E";

            for (int i = 0; i < asked_hole_vec.size(); ++i) {
                if (decided_map_.find(asked_hole_vec[i]) != decided_map_.end()) {
                    // hole slot already decided, report to leader
                    send_msg += "," + std::to_string(asked_hole_vec[i])
                        + "," + decided_map_[asked_hole_vec[i]]->GetStr();
                }
            }

            send_msg += ",A";

            for (auto it = propose_map_.begin(); it != propose_map_.end(); ++it) {
                send_msg += "," + std::to_string(it->first) + "," 
                    + std::to_string(it->second->view_num) + ","
                    + it->second->value->GetStr();
            }
            SendMessage(ip_vec_[primary_id], port_vec_[primary_id], send_msg);


        }
        else if(msg_type == "Y")
        {
            if (is_supported_)
                continue;
            if (supporting_set_.find(sender_ID) != supporting_set_.end())
                continue;
            supporting_set_.insert(sender_ID);
            if (supporting_set_.size() > replica_num_ / 2)
                is_supported_ = true;
            msg.erase(msg.begin());
            msg.erase(msg.begin());
            while(msg[0] != 'A')
            {
                // decided holes
                int slot = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(msg.begin() + msg.find(','));
                if (slot >= chat_log_len_ && decided_map_.find(slot) == decided_map_.end()) {
                    Request * r = new Request(msg.substr(0,msg.find(',')));
                    decided_map_[slot] = r;
                } 
                msg.erase(msg.begin() + msg.find(','));
            }
            msg.erase(msg.begin());
            msg.erase(msg.begin());

            while (!msg.empty()) {
                // history proposed values
                int slot = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(msg.begin() + msg.find(','));
                int view_num = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(msg.begin() + msg.find(','));

                if (decided_map_.find(slot) != decided_map_.end()) {
                    msg.erase(msg.begin() + msg.find(','));
                    continue;
                }

                Request * r;
                if (msg.find(',') != std::string::npos) {
                    r = new Request(msg.substr(0,msg.find(',')));
                    msg.erase(msg.begin() + msg.find(','));
                }
                else {
                    r = new Request(msg);
                    msg.clear();
                }

                if (propose_map_.find(slot) == propose_map_.end()) {
                    propose_map_[slot] = new ProposedValue();
                    propose_map_[slot]->value = r;
                    propose_map_[slot]->slot_id = slot;
                    propose_map_[slot]->view_num = view_num;
                }
                else if (view_num > propose_map_[slot]->view_num) {
                    propose_map_[slot]->view_num = view_num;
                    propose_map_[slot]->value = r;
                }
                else {
                    delete(r);
                }
                if (slot >= next_slot_num_)
                    next_slot_num_ = slot + 1;
            }
        }
        else if(msg_type == "D")
        {
            int view_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            int slot_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            if (slot_num < chat_log_len_)
                continue;
            if (view_num < highest_view_num_)
                continue;
            if (decided_map_.find(slot_num) != decided_map_.end())
                continue;
            highest_view_num_ = view_num;
            if (slot_num >= next_slot_num_)
                next_slot_num_ = slot_num + 1;
            Request * r = new Request(msg);
            ProposedValue * p = new ProposedValue();
            p->value = r;
            p->view_num = view_num;
            p->vote_set.insert(sender_ID);
            p->vote_set.insert(id_);
            if (propose_map_.find(slot_num) != propose_map_.end()) {
                if (propose_map_[slot_num]->value) {
                    delete propose_map_[slot_num]->value;
                }
                delete propose_map_[slot_num];
            }
            propose_map_[slot_num] = p;
            std::string send_msg = "A%" + std::to_string(id_) + ","
                + std::to_string(view_num)+ "," + std::to_string(slot_num) + "," + msg;
            for (int i = 0; i < replica_num_; ++i) {
                if (id_ == i)
                    continue;
                SendMessage(ip_vec_[i], port_vec_[i], send_msg);
            }


        }
        else if(msg_type == "A")
        {
            int view_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));
            int slot_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(msg.begin() + msg.find(','));

            if (slot_num < chat_log_len_)
                continue;
            if (decided_map_.find(slot_num) != decided_map_.end())
                continue;
            // if (propose_map_.find(slot_num) == propose_map_.end()) {
            //     propose_map_[slot_num] = new ProposedValue();
            //     propose_map_[slot_num]->slot_id = slot_num;
            //     propose_map_[slot_num]->view_num = view_num;
            //     propose_map_[slot_num]->value = new Request(msg);

            // }
            if (propose_map_[slot_num]->view_num != view_num)
                continue;
            propose_map_[slot_num]->vote_set.insert(sender_ID);
            if (propose_map_[slot_num]->vote_set.size() > replica_num_ / 2) {
                decided_map_[slot_num] = propose_map_[slot_num]->value;
                delete(propose_map_[slot_num]);
                propose_map_.erase(slot_num);
            }
        }
        else
        {
            std::cout<<"Wrong message type \n";
            exit(1);
        }
    }
}


int main(int argc, char const *argv[]) 
{ 
    Replica replica(0, 0);
    replica.InitServerAddr("server.config");
    replica.StartRun();
    // int server_fd, new_socket, valread; 
    // struct sockaddr_in address; 
    // int opt = 1; 
    // int addrlen = sizeof(address); 
    // char buffer[1024] = {0}; 
    // char *hello = "Hello from server"; 
       
    // // Creating socket file descriptor 
    // if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    // { 
    //     perror("socket failed"); 
    //     exit(EXIT_FAILURE); 
    // } 
       
    // address.sin_family = AF_INET; 
    // address.sin_addr.s_addr = INADDR_ANY; 
    // address.sin_port = htons( PORT ); 
       
    // // Forcefully attaching socket to the port 8080 
    // if (bind(server_fd, (struct sockaddr *)&address,  
    //                              sizeof(address))<0) 
    // { 
    //     perror("bind failed"); 
    //     exit(EXIT_FAILURE); 
    // } 
    // if (listen(server_fd, 3) < 0) 
    // { 
    //     perror("listen"); 
    //     exit(EXIT_FAILURE); 
    // } 
    // if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
    //                    (socklen_t*)&addrlen))<0) 
    // { 
    //     perror("accept"); 
    //     exit(EXIT_FAILURE); 
    // } 
    // // std::thread (Send);
    // valread = read( new_socket , buffer, 1024); 
    // printf("%s\n",buffer ); 
    // send(new_socket , hello , strlen(hello) , 0 ); 
    // printf("Hello message sent\n"); 



    return 0; 
} 