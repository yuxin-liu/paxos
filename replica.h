#ifndef REPLICA_H
#define REPLICA_H

#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>

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

class Replica {
public:
    bool primary_id_;
    int id_;
    int replica_num_;
    int receive_sock;
    std::vector<std::string> ip_vec_;
    std::vector<int> port_vec_;
    std::vector<int> hb_port_vec_;
    



    Replica(const std::string &config) {
        std::ifstream in(config.c_str());
        if (!in) {
            std::cout << "open file failed\n";
            exit(1);
        }
        in >> id_ >> replica_num_;
        ip_vec_.resize(replica_num_);
        port_vec_.resize(replica_num_);
        hb_port_vec_.resize(replica_num_);
        for (int i = 0; i < replica_num_; ++i) {
            in >> ip_vec_[i] >> port_vec_[i] >> hb_port_vec_[i];
        }
        primary_id_ = -1;


        int server_fd, new_socket, valread; 
        struct sockaddr_in address; 
        int opt = 1; 
        int addrlen = sizeof(address); 
        char buffer[1024] = {0}; 
        char *hello = "Hello from server"; 
           
        // Creating socket file descriptor 
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
        { 
            perror("socket failed"); 
            exit(EXIT_FAILURE); 
        } 
           
        address.sin_family = AF_INET; 
        address.sin_addr.s_addr = INADDR_ANY; 
        address.sin_port = htons( PORT ); 
           
        // Forcefully attaching socket to the port 8080 
        if (bind(server_fd, (struct sockaddr *)&address,  
                                     sizeof(address))<0) 
        { 
            perror("bind failed"); 
            exit(EXIT_FAILURE); 
        } 
        if (listen(server_fd, SOMAXCONN) < 0) 
        { 
            perror("listen"); 
            exit(EXIT_FAILURE); 
        }

        //FINISH INITIALIZATION, START RECEIVING HERE
        int rval = 0;
        char buf = '6';
        std::string msg = "";
        std::string msg_type = "";
        bool first = true;
        while (true) {
            int msg_fd = accept(server_fd,nullptr, nullptr);
            msg_type.clear();
            msg.clear();
            while(rval = recv(msg_fd,buf, 1, MSG_WAITALL) == 1)
            {

                if(buf == '\0')
                {
                    //end of an message
                    break;
                }
                if(first)
                {
                    if(buf == '%')
                        first = false;
                    else
                        msg_type += buf;
                }
                else
                {
                    msg += buf;
                }
            }//receive

            //get sender ID
            std::string sender_ID_ = msg.substr(0,msg.find(','));
            msg.erase(msg.begin() + msg.find(','));
            if(msg_type == "C")
            {
                if (primary_id_ != -1 && primary_id_ != id_) {
                    // I'm not leader. ignore client's request
                    continue;
                }
                primary_id_ = id_;
                //client request
                //e.g.: 0,170.01.01.234,8080,ABC,1\0
                //sender_ID,IP,PORT,sequence number,request
                
                //get client IP
                std::string Client_IP = msg.substr(0,msg.find(','));
                msg.erase(msg.begin() + msg.find(','));
                //get client port
                std::string Client_Port = msg.substr(0,msg.find(','));
                msg.erase(msg.begin() + msg.find(',')); 
                //get request 
                std::string Client_Request = msg.substr(0,msg.find(','));
                msg.erase(msg.begin() + msg.find(',')); 
                //get sequence number 
                std::string Request_Seq_Num_Str = msg.substr(0,msg.find('\0'));
                int Req_Seq_Num = std::stoi(Request_Seq_Num_Str);
            }
            else if(msg_type == "I")
            {
                //Replica IAMLEADER
                //e.g.: 0,0,IAMLEADER
                //sender_ID,VIEW NUMBER ,IAMLEADER
                std::string View_Num_Str = msg.substr(0,msg.find(','));
                int View_Num = std::stoi(View_Num_Str);

            }
            else if(msg_type == "Y")
            {
                //Replica YOUARELEADER
                //e.g.: 1,ABC,0%
                //sender_ID,{current message hold by replica,view number%}
                std::vector<string> Msgs;
                std::vector<int> Views;
                while()
                {
                    std::string Message_Hold = msg.substr(0,msg.find(','));
                    msg.erase(msg.begin() + msg.find(','));
                    Msgs.push_back(Message_Hold); 
                    std::string Message_View_Num_Str = msg.substr(0,msg.find('%'));
                    msg.erase(msg.begin() + msg.find('%'));
                    int Message_View_Num = std::stoi(Message_View_Num_Str);
                }

            }
            else if(msg_type == "D")
            {
                //Replica Decree
                //e.g.:0,ABC%
                //Sender_ID, Slot Numnber, Decree message%
                std::string Decree_Msg = msg.substr(0,msg.find('%'));



            }
            else if(msg_type == "A")
            {
                //Replica Accept
                //e.g.: 1,
            }
            else
            {
                std::cout<<"Wrong message type \n";
                exit(1);
            }
        }

        StartRun();

    }


    void Heartbeat() {
        std::vector<int> sockets;
        for (int i = 0; i < replica_num_; ++i) {
            if (i == id_)
                continue;
            struct sockaddr_in address; 
            int sock = 0, valread; 
            struct sockaddr_in serv_addr;  
            char buffer[1024] = {0}; 
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
            { 
                printf("\n Socket creation error \n"); 
                return -1; 
            } 

            memset(&serv_addr, '0', sizeof(serv_addr)); 

            serv_addr.sin_family = AF_INET; 
            serv_addr.sin_port = htons(hb_port_vec_[i]); 
               
            // Convert IPv4 and IPv6 addresses from text to binary form 
            if(inet_pton(AF_INET, ip_vec_[i], &serv_addr.sin_addr)<=0)  
            { 
                printf("\nInvalid address/ Address not supported \n"); 
                return -1; 
            } 

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
            { 
                printf("\nConnection Failed \n"); 
                continue; 
            }
            sockets.push_back(sock);
        }
        char *hb = "heartbeat";
        while(true) {
            for (int i = 0; i < sockets.size(); ++i) {
                send(sockets[i] ,hb , strlen(hb) , 0 );
            }
            sleep(1);
        }
    }

    



};

#endif;