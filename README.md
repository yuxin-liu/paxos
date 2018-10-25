# paxos

when a new leader takes place, ask for all the holes.
execute when receive an executed value, decree when only receive proposed value, decree noop when receive no value

message format
client request
    C%<client_id>,<client_ip>,<client_port>,<seq_num>,<message>\0
server ack
    A%<seq_num>\0
server->client Please Switch To LeaderX
    L%<replica_id>\0
leader->replicas IAMLEADER
    I%<leader_id>,<view_num>{,<hole1_slot>,<hole2_slot>...}\0
replicas->leader YOUARELEADER report executable holes and history propoals
    Y%<replica_id>,E{,<slot1>,<value1>...},A{,<slot1>,<view_num1>,<value1>...}
leader->replicas Decree
    D%<leader_id>,<view_num>,<slot_num>,<value>
replicas->replicas Accept
    A%<replica_id>,<view_num>,<slot_num>,<value>

