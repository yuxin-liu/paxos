# paxos

message format
client request
    C%<client_id>,<client_ip>,<client_port>,<seq_num>,<message>\0
client timeout
    S%<client_id>,<client_ip>,<client_port>,<leader_id>\0
replicas->replica new leader
    L%<replica_id>,<client_ip>,<client_port>\0
server ack
    A%<seq_num>\0
leader->replicas IAMLEADER
    I%<leader_id>,<view_num>\0
replicas->leader YOUARELEADER report executable holes and history propoals
    Y%<replica_id>,{<slot>,<view_num>,<value>,<E/D/A>,}\0
leader->replicas Decree
    D%<leader_id>,<view_num>,<slot_num>,<value>\0
replicas->replicas Accept
    A%<replica_id>,<view_num>,<slot_num>,<value>\0
leader->replicas Ask for exe when timeout
    E%<leader_id>,<chat_log_length>\0
replica->leader Ask for exe when timeout
    E%<replica_id>,<chat_log_length>\0
leader->replica / replicas->leader reply exe
    R%<replica_id>,{<slot>,<value>,}\0

server.config format
<skip slot mode flag>
<message loss mode flag>
<ip_1=0> <port_number_0>
<ip_1=1> <port_number_1>
<ip_1=2> <port_number_2>
<ip_1=3> <port_number_3>
...


