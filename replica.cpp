#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include <stdlib.h> 
#include <time.h> 

#include "replica.h"

Replica::Replica(int id, int port) : id_(id), chat_log_len_(0), current_view_(-1),
        primary_id_(0), next_slot_num_(0) {
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
    os_.open("replica" + std::to_string(id_) + ".log");
    if (!os_.is_open()) {
        printf("open output file failed");
        exit(1);
    }
    std::cout << "end init replica " << id_ <<std::endl;
}

void Replica::SendMessage(const std::string &ip, int port, const std::string &msg) {
    std::cout << ">>>>send msg:" << msg << ":to " << ip << ":" << port << std::endl;
    if (is_loss_mode_) {
        int rand_num = rand() % (int)(1.0/p);
        if (rand_num == 0)
            return;
    }
    int send_sock = 0; 
    struct sockaddr_in serv_addr;  
    if ((send_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
        printf("\n Socket creation error \n"); 
        std::cout << strerror(errno);
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
        printf("\nConnection Failed. Sending failed \n");  
        close(send_sock);
        return;
    }

    send(send_sock, msg.c_str(), strlen(msg.c_str()), 0);
    close(send_sock);
}

void Replica::AskForFollow() {
    std::cout << "replica " << id_ << " asking for follow" << std::endl;
    supporting_set_.insert(id_);
    std::string send_msg = "I%" + std::to_string(id_) + "," 
                    + std::to_string(current_view_);

    for (int i = 0; i < replica_num_; ++i) {
        if (i != id_)
            SendMessage(ip_vec_[i], port_vec_[i], send_msg);
    }
}

void Replica::Decree(int inSlotNum) {
    std::cout << "replica " << id_ << " dcree slot " << inSlotNum << std::endl;
    std::string send_msg = "D%" + std::to_string(id_) + ","
        + std::to_string(current_view_) + "," + std::to_string(inSlotNum) + ",";
    Request * r = propose_map_[inSlotNum]->value;
    propose_map_[inSlotNum]->vote_set.insert(id_);
    send_msg += r->GetStr();
    for (int i = 0; i < replica_num_; ++i) {
        if (id_ != i)
            SendMessage(ip_vec_[i], port_vec_[i], send_msg);
    }
}


void Replica::InitServerAddr(const char * file) {
    std::cout << "init server addr for replica " << id_ <<std::endl;
    std::ifstream in(file);
    if (!in) {
        std::cout << "open file " << file << " failed\n";
        exit(1);
    }
    in >> is_skip_mode_;
    in >> is_loss_mode_;
    while (in) {
        std::string ip;
        int port;
        in >> ip >> port;
        if (!in)
            break;
        ip_vec_.push_back(ip);
        port_vec_.push_back(port);
    }
    in.close();
    replica_num_ = ip_vec_.size();
}


void Replica::StartRun() {
    std::cout << "start run" << std::endl;
    char buf = '6';
    std::string msg = "";
    std::string msg_type = "";
    bool first = true;
    if (id_ == 0)
        current_view_++;
    int timeout_count = 0;

    while (true) {
        if (primary_id_ != id_)
            supporting_set_.clear();

        // check for execution, exe (and send ack to client if I'm leader)
        while (propose_map_.find(chat_log_len_) != propose_map_.end()) {
            if (propose_map_[chat_log_len_]->vote_set.size() <= replica_num_ / 2)
                break;
            std::cout << "execute " << chat_log_len_ << " ";
            Request * r = propose_map_[chat_log_len_]->value;
            if (primary_id_ == id_ && r->client_id != -1) {
                std::string send_msg = "A%" + std::to_string(r->client_seq);
                SendMessage(r->client_ip, r->client_port, send_msg);
            }
            if (r->client_id != -1) {
                std::pair<int, int> req = std::make_pair(r->client_id, r->client_seq);
                if (req_set_.find(req) != req_set_.end()) {
                    // if the client,seq exed before, change it to no-op
                    r->client_id = -1;
                    std::cout << "dupilicate execution, execute no-op" << std::endl;
                }
                else {
                    // put the executed {client, seq} to set
                    req_set_.insert(req);
                }
            }
            std::cout << r->GetStr() << std::endl;
            os_ << r->GetStr() << std::endl;
            chat_log_len_++;
        }

        if (primary_id_ == id_) {
            std::cout << "I am leader!" << std::endl;
            if (supporting_set_.size() <= replica_num_ / 2) {
                // not supported, ask for support
                AskForFollow();
            }
            else {
                // already supported, then decree
                // first check for skipped slots, make up values for them.
                for (int i = chat_log_len_; i < next_slot_num_; ++i) {
                    if (propose_map_.find(i) == propose_map_.end()) {
                        propose_map_[i] = new ProposedValue();
                        propose_map_[i]->slot_id = i;
                        propose_map_[i]->value = new Request("NO-OP");
                        propose_map_[i]->view_num = current_view_;
                    }
                }
                for (auto it = propose_map_.begin(); it != propose_map_.end(); ++it) {
                    if (it->second->vote_set.size() <= replica_num_ / 2)
                        Decree(it->first);
                }
            }
        }
        int second = 2;
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
            std::cout << "timeout";
            timeout_count++;
            // Ask for execution beyond chat_log.
            if (timeout_count < 5)
                continue;
            std::string send_msg = "E%" + std::to_string(id_) + "," + std::to_string(chat_log_len_);
            if (id_ == primary_id_) {
                for (int i = 0; i < replica_num_; ++i) {
                    if (i != id_) {
                        SendMessage(ip_vec_[i], port_vec_[i], send_msg);
                    }
                }
            }
            else {
                SendMessage(ip_vec_[primary_id_], port_vec_[primary_id_], send_msg);
            }
            timeout_count = 0;
            continue;
        }

        msg_type.clear();
        msg.clear();
        first = true;
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
        std::cout << "<<<<receive msg:" << msg_type << "@" << msg << std::endl;

        //get sender ID
        int sender_ID = std::stoi(msg.substr(0,msg.find(',')));
        msg.erase(0, msg.find(',') + 1);

        if(msg_type == "C") {
            std::string client_ip = msg.substr(0,msg.find(','));
            msg.erase(0, msg.find(',') + 1);
            int client_port = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);
            if (primary_id_ != id_) {
                // I'm not leader. ignore client's request
                std::cout << "but I'm not leader!" << std::endl;
                continue;
            }

            Request * r = new Request();
            r->client_id = sender_ID;
            r->client_ip = client_ip;
            r->client_port = client_port;
            r->client_seq = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);
            r->chat_msg = msg;

            int slot_num;
            if (is_skip_mode_) {
                slot_num = next_slot_num_ + 1;
                next_slot_num_ +=2;
            }
            else {
                slot_num = next_slot_num_++;
            }

            ProposedValue * p = new ProposedValue();
            p->value = r;
            p->view_num = current_view_;

            propose_map_[slot_num] = p;
        }
        else if(msg_type == "I")
        {
            int view_num = std::stoi(msg);
            if (view_num <= current_view_)
                continue;
            else {
                current_view_ = view_num;
                supporting_set_.clear();
            }

            std::string send_msg = "Y%" + std::to_string(id_) + ",";

            for (auto it = propose_map_.begin(); it != propose_map_.end(); ++it) {
                send_msg += std::to_string(it->first) + ",";
                send_msg += std::to_string(it->second->view_num) + ",";
                send_msg += it->second->value->GetStr() + ",";
                if (it->first < chat_log_len_)
                    send_msg += "E,";
                else if (it->second->vote_set.size() > replica_num_ / 2)
                    send_msg += "D,";
                else
                    send_msg += "A,";
            }
            SendMessage(ip_vec_[primary_id_], port_vec_[primary_id_], send_msg);
        }
        else if(msg_type == "Y") {
            if (supporting_set_.size() > replica_num_ / 2)
                continue;
            if (supporting_set_.find(sender_ID) != supporting_set_.end())
                continue;
            supporting_set_.insert(sender_ID);
            while(!msg.empty()) {
                int slot = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(0, msg.find(',') + 1);
                if (propose_map_.find(slot) != propose_map_.end()
                    && propose_map_[slot]->vote_set.size() > replica_num_ / 2) {
                    // slot already decided, ignore
                    msg.erase(0, msg.find(',') + 1);
                    msg.erase(0, msg.find(',') + 1);
                    msg.erase(0, msg.find(',') + 1);
                    continue;
                }
                int view_num = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(0, msg.find(',') + 1);
                std::string value = msg.substr(0,msg.find(','));
                msg.erase(0, msg.find(',') + 1);
                std::string type = msg.substr(0,msg.find(','));
                msg.erase(0, msg.find(',') + 1);

                if (type == "E" || type == "D") {
                    // force to decide this value
                    if (propose_map_.find(slot) != propose_map_.end()) {
                        if (propose_map_[slot]->value)
                            delete(propose_map_[slot]->value);
                        delete(propose_map_[slot]);
                    }
                    propose_map_[slot] = new ProposedValue();
                    propose_map_[slot]->slot_id = slot;
                    propose_map_[slot]->view_num = view_num;
                    propose_map_[slot]->value = new Request(value);
                    for (int i = 0; i < replica_num_ / 2 + 1; ++i) {
                        propose_map_[slot]->vote_set.insert(i);
                    }

                }
                else if (type == "A") {
                    if (propose_map_.find(slot) != propose_map_.end()) {
                        if (propose_map_[slot]->view_num >= view_num) {
                            // I have newer proposal, ingore
                            continue;
                        }
                        else {
                            // change to new value
                            propose_map_[slot]->view_num = view_num;
                            if (propose_map_[slot]->value)
                                delete(propose_map_[slot]->value);
                            propose_map_[slot]->value = new Request(value);
                            propose_map_[slot]->vote_set.clear();
                        }
                    }
                    else {
                        propose_map_[slot] = new ProposedValue();
                        propose_map_[slot]->slot_id = slot;
                        propose_map_[slot]->view_num = view_num;
                        propose_map_[slot]->value = new Request(value);
                    }
                }
                if (slot >= next_slot_num_)
                    next_slot_num_ = slot + 1;
            } 
        }
        else if(msg_type == "D")
        {
            int view_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);
            if (view_num > current_view_)
                current_view_ = view_num;
            int slot_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);
            if (slot_num < chat_log_len_ || sender_ID != primary_id_)
                continue;
            if (propose_map_.find(slot_num) != propose_map_.end()) {
                if (propose_map_[slot_num]->vote_set.size() > replica_num_ / 2)
                    continue;
                if (propose_map_[slot_num]->view_num >= view_num)
                    continue;
                if (propose_map_[slot_num]->value)
                    delete(propose_map_[slot_num]->value);
                delete(propose_map_[slot_num]);
            }
            supporting_set_.clear();
            propose_map_[slot_num] = new ProposedValue();
            propose_map_[slot_num]->slot_id = slot_num;
            propose_map_[slot_num]->view_num = current_view_;
            propose_map_[slot_num]->value = new Request(msg);
            propose_map_[slot_num]->vote_set.insert(id_);
            propose_map_[slot_num]->vote_set.insert(primary_id_);
            if (slot_num >= next_slot_num_)
                next_slot_num_ = slot_num + 1;
            
            std::string send_msg = "A%" + std::to_string(id_) + ","
                + std::to_string(view_num)+ "," + std::to_string(slot_num) + "," + msg;
            for (int i = 0; i < replica_num_; ++i) {
                if (id_ != i)
                    SendMessage(ip_vec_[i], port_vec_[i], send_msg);
            }
        }
        else if(msg_type == "A")
        {
            int view_num = std::stoi(msg.substr(0,msg.find(',')));
            if (view_num > current_view_)
                current_view_ = view_num;
            msg.erase(0, msg.find(',') + 1);
            int slot_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);

            if (slot_num < chat_log_len_)
                continue;
            if (propose_map_.find(slot_num) != propose_map_.end()) {
                if (propose_map_[slot_num]->vote_set.size() > replica_num_ / 2)
                    continue;
                else if (propose_map_[slot_num]->view_num > view_num) {
                    continue;
                }
                else if (propose_map_[slot_num]->view_num == view_num) {
                    propose_map_[slot_num]->vote_set.insert(sender_ID);
                    continue;
                }
                else {
                    if (propose_map_[slot_num]->value)
                        delete(propose_map_[slot_num]->value);
                    delete(propose_map_[slot_num]);
                }
            }
            propose_map_[slot_num] = new ProposedValue();
            propose_map_[slot_num]->slot_id = slot_num;
            propose_map_[slot_num]->view_num = view_num;
            propose_map_[slot_num]->value = new Request(msg);
            if (slot_num >= next_slot_num_)
                next_slot_num_ = slot_num + 1;
        }
        else if (msg_type == "S") {
            std::string temp_msg = msg;
            std::string client_ip = temp_msg.substr(0,temp_msg.find(','));
            temp_msg.erase(0, temp_msg.find(',') + 1);
            int client_port = std::stoi(temp_msg);
            temp_msg.erase(0, temp_msg.find(',') + 1);
            int curr_leader = std::stoi(temp_msg.substr(0,temp_msg.find(',')));
            if (curr_leader != primary_id_) {
                std::string send_msg = "L%" + std::to_string(primary_id_);
                SendMessage(client_ip, client_port, send_msg);
            }
            else {
                int new_primary = (primary_id_ + 1) % replica_num_;
                if (new_primary == id_)
                    continue;
                std::string send_msg = "L%" + std::to_string(id_) + "," + msg;
                SendMessage(ip_vec_[new_primary], port_vec_[new_primary], send_msg);
                primary_id_ = new_primary;
            }

        }
        else if (msg_type == "L") {
            if (primary_id_ != id_) {
                primary_id_ = id_;
                current_view_++;
            }
            std::string send_msg = "L%" + std::to_string(id_);
            std::string client_ip = msg.substr(0,msg.find(','));
            msg.erase(0, msg.find(',') + 1);
            int client_port = std::stoi(msg);
            SendMessage(client_ip, client_port, send_msg);
        }
        else if (msg_type == "E") {
            int slot_num = std::stoi(msg.substr(0,msg.find(',')));
            msg.erase(0, msg.find(',') + 1);
            int receive_log_len = std::stoi(msg.substr(0,msg.find(',')));
            if (receive_log_len >= chat_log_len_)
                continue;
            std::string send_msg = "R%" + std::to_string(id_) + ",";
            for (int i = receive_log_len; i < chat_log_len_; ++i) {
                send_msg += std::to_string(i) + ",";
                send_msg += propose_map_[i]->value->GetStr() + ",";
            }
            SendMessage(ip_vec_[sender_ID], port_vec_[sender_ID], send_msg);

        }
        else if (msg_type == "R") {
            while (!msg.empty()) {
                int slot_num = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(0, msg.find(',') + 1);
                if (propose_map_.find(slot_num) != propose_map_.end()) {
                    if (propose_map_[slot_num]->vote_set.size() > replica_num_ / 2)
                        continue;
                    if (propose_map_[slot_num]->value)
                        delete(propose_map_[slot_num]->value);
                    delete(propose_map_[slot_num]);
                }
                propose_map_[slot_num] = new ProposedValue();
                propose_map_[slot_num]->slot_id = slot_num;
                propose_map_[slot_num]->view_num = std::stoi(msg.substr(0,msg.find(',')));
                msg.erase(0, msg.find(',') + 1);
                propose_map_[slot_num]->value = new Request(msg.substr(0,msg.find(',')));
                msg.erase(0, msg.find(',') + 1);
                for (int i = 0; i < replica_num_ / 2 + 1; ++i) {
                    propose_map_[slot_num]->vote_set.insert(i);
                }
            }
        }
        else {
            std::cout<<"Wrong message type \n";
            exit(1);
        }
    }
}


int main(int argc, char const *argv[]) 
{ 
    srand (time(NULL));
    if (argc < 3) {
        std::cout << "replica usage: ./replica <id> <receive_port>" << std::endl;
        exit(1);
    }
    Replica replica(std::atoi(argv[1]), std::atoi(argv[2]));
    replica.InitServerAddr("server.config");
    replica.StartRun();
    return 0; 
} 