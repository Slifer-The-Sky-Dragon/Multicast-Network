#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <cstdlib>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <map>

#define EMPTY ""
#define SFD "$"

#define MAIN_MESSAGE 0

#define MAIN_TYPE "00"
#define MESSAGE_TYPE "01"
#define CONFIG_TYPE "10"

#define NEW_ROUTER "MyRouter"
#define NEW_SYSTEM "MySystem"
#define CONNECT_ROUTER_SYSTEM "ConnectRouterSystem"
#define CONNECT_ROUTER_ROUTER "ConnectRouterRouter"
#define SEND_MESSAGE "SendMessage"
#define SEND_FILE "SendFile"
#define RECV_FILE "RecieveFile"
#define CFG_STP "SpanningTreeProtocol"
#define SET_PAR "SetParents"
#define PRINT_INFO "PrintInfo"

#define SYSTEM_JOIN_GROUP "SystemJoinGroup"
#define SYSTEM_LEAVE_GROUP "SystemLeaveGroup"

char ROUTER_EXEC[] = { "./router" };
char SYSTEM_EXEC[] = { "./system" };

using namespace std;


const string FIFO_PREFIX = "fifo_";
typedef string ID;

vector<int> children;

void new_router_command_handler(stringstream& ss , vector<ID>& router_indexes , map < string , int >& fifo_to_fd){
    string port_cnt , router_id;
    ss >> port_cnt >> router_id;

    string fifo_name = FIFO_PREFIX + "ms" + router_id;
    mkfifo(fifo_name.c_str(), 0666);

    int pid = fork();

    if (pid == 0){
        char* args[] = { ROUTER_EXEC , (char*)port_cnt.c_str() ,
                        (char*)router_id.c_str() , NULL };
        
        execvp(ROUTER_EXEC, args);
    }

    else{
        int new_fd = open(fifo_name.c_str() , O_WRONLY);
        fifo_to_fd[fifo_name] = new_fd;
        router_indexes.push_back(router_id);
        children.push_back(pid);
    }
}

void new_system_command_handler(stringstream& ss , vector<ID>& system_indexes , map < string , int >& fifo_to_fd){
    string system_id;
    ss >> system_id;
    
    string fifo_name = FIFO_PREFIX + "mss" + system_id;
    mkfifo(fifo_name.c_str(), 0666);

    int pid = fork();

    if (pid == 0){
        char* args[] = {SYSTEM_EXEC , (char*)system_id.c_str() , NULL};
        
        execvp(SYSTEM_EXEC, args);
    }

    else{
        int new_fd = open(fifo_name.c_str() , O_WRONLY);
        fifo_to_fd[fifo_name] = new_fd;
        system_indexes.push_back(system_id);
        children.push_back(pid);
    }
}

string convert_to_message_type(int type){
    if(type == MAIN_MESSAGE){
        return "00";
    }
    else
        return "01";
}

string extend_string_length(string a , size_t final_length){
    string result;
    for(size_t i = 0 ; i < final_length - a.size() ; i++){
        result += "0";
    }
    result += a;
    return result;
}

string convert_to_ehternet_frame(int da , int sa , int type , string data){
    string result = EMPTY;
    string message_type = convert_to_message_type(type);

    result = SFD + extend_string_length(to_string(da) , 6) + extend_string_length(to_string(sa) , 6) +
                         message_type + data + "\n";
    return result;
}

void connect_router_system_command_handler(stringstream& ss , map < string , int >& fifo_to_fd){
    int system_id , router_id , port_number;
    ss >> system_id >> router_id >> port_number;

    string pipe_name1 = FIFO_PREFIX + "s" + to_string(router_id) + "ss" + to_string(system_id);
    string pipe_name2 = FIFO_PREFIX + "ss" + to_string(system_id) + "s" + to_string(router_id);
    
    mkfifo(pipe_name1.c_str() , 0666);
    mkfifo(pipe_name2.c_str() , 0666);

    string router_data = "CSS " + to_string(port_number) + " " + pipe_name1 + " " + pipe_name2;
    string system_data = "C " + pipe_name2 + " " + pipe_name1;

    string router_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , router_data);
    string system_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system_data);

    string router_pipe_name = FIFO_PREFIX + "ms" + to_string(router_id);
    string system_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id);
    
    if(fifo_to_fd.find(router_pipe_name) != fifo_to_fd.end() && fifo_to_fd.find(system_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[router_pipe_name] , router_message.c_str() , router_message.size());
        write(fifo_to_fd[system_pipe_name] , system_message.c_str() , system_message.size());
    }    
}

void connect_router_router_command_handler(stringstream& ss , map < string , int >& fifo_to_fd){
    int router_id1, router_id2 , port_number1 , port_number2;
    ss >> router_id1 >> port_number1 >> router_id2 >> port_number2;

    string pipe_name1 = FIFO_PREFIX + "s" + to_string(router_id1) + "s" + to_string(router_id2);
    string pipe_name2 = FIFO_PREFIX + "s" + to_string(router_id2) + "s" + to_string(router_id1);   
    
    mkfifo(pipe_name1.c_str() , 0666);
    mkfifo(pipe_name2.c_str() , 0666);

    string router_data1 = "C " + to_string(port_number1) + " " + pipe_name1 + " " + pipe_name2;
    string router_data2 = "C " + to_string(port_number2) + " " + pipe_name2 + " " + pipe_name1;

    string router_message1 = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , router_data1);
    string router_message2 = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , router_data2);

    string router_pipe_name1 = FIFO_PREFIX + "ms" + to_string(router_id1);
    string router_pipe_name2 = FIFO_PREFIX + "ms" + to_string(router_id2);
    
    if(fifo_to_fd.find(router_pipe_name1) != fifo_to_fd.end() && fifo_to_fd.find(router_pipe_name2) != fifo_to_fd.end()){
        write(fifo_to_fd[router_pipe_name1] , router_message1.c_str() , router_message1.size());
        write(fifo_to_fd[router_pipe_name2] , router_message2.c_str() , router_message2.size());
    }    
}

void send_message_command_handler(stringstream& ss , map < string , int >& fifo_to_fd){
    int system_id1 , system_id2;
    string raw_message;

    ss >> system_id1 >> system_id2;
    getline(ss, raw_message);

    string system_data = "S " + to_string(system_id2) + " " + raw_message;
    string system_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system_data);
    string system_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id1);   

    if(fifo_to_fd.find(system_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[system_pipe_name] , system_message.c_str() , system_message.size());
    }
}

void send_file_command_handler(stringstream& ss, map < string , int >& fifo_to_fd){
    int system_id1 , system_id2;
    string file_name;

    ss >> system_id1 >> system_id2 >> file_name;

    string system1_data = "F " + to_string(system_id2) + " " + file_name;
    string system1_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system1_data);

    string system1_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id1);   

    if(fifo_to_fd.find(system1_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[system1_pipe_name] , system1_message.c_str() , system1_message.size());
    }
}

void recv_file_command_handler(stringstream& ss , map < string , int >& fifo_to_fd){
    int system_id1 , system_id2;
    string file_name;

    ss >> system_id1 >> system_id2 >> file_name;

    string system1_data = "R " + to_string(system_id2) + " " + file_name;
    string system1_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system1_data);

    string system1_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id1);   

    if(fifo_to_fd.find(system1_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[system1_pipe_name] , system1_message.c_str() , system1_message.size());
    }    
}

void cfg_stp_command_handler(vector<ID> router_indexes , map < string , int >& fifo_to_fd){
    string data = "CONFIG";
    string router_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , data);
    for(int i = 0 ; i < router_indexes.size() ; i++){
        string router_id = router_indexes[i];
        string router_pipe_name = FIFO_PREFIX + "ms" + router_id;
        write(fifo_to_fd[router_pipe_name] , router_message.c_str() , router_message.size());
    }
}

void set_parents_command_handler(vector<ID> router_indexes , map < string , int >& fifo_to_fd){
    string data = "SETPAR";
    string router_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , data);
    for(int i = 0 ; i < router_indexes.size() ; i++){
        string router_id = router_indexes[i];
        string router_pipe_name = FIFO_PREFIX + "ms" + router_id;
        write(fifo_to_fd[router_pipe_name] , router_message.c_str() , router_message.size());
    }    
}

void print_info_command_handler(vector<ID> router_indexes , map < string , int >& fifo_to_fd){
    string data = "PRINT";
    string router_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , data);
    for(int i = 0 ; i < router_indexes.size() ; i++){
        string router_id = router_indexes[i];
        string router_pipe_name = FIFO_PREFIX + "ms" + router_id;
        write(fifo_to_fd[router_pipe_name] , router_message.c_str() , router_message.size());
    }    
}

void system_join_group_command_handler(stringstream& ss , vector<ID>& router_indexes,
                                vector<ID>& system_indexes , 
                                map < string , int >& fifo_to_fd){
    
    int system_id, group_id;
    ss >> system_id >> group_id;

    string system_data = "J " + to_string(group_id);
    string system_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system_data);

    string system_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id);   

    if(fifo_to_fd.find(system_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[system_pipe_name] , system_message.c_str() , system_message.size());
    }
}

void system_leave_group_command_handler(stringstream& ss , vector<ID>& router_indexes,
                                vector<ID>& system_indexes , 
                                map < string , int >& fifo_to_fd){
    
    int system_id, group_id;
    ss >> system_id >> group_id;

    string system_data = "L " + to_string(group_id);
    string system_message = convert_to_ehternet_frame(0 , 0 , MAIN_MESSAGE , system_data);

    string system_pipe_name = FIFO_PREFIX + "mss" + to_string(system_id);   

    if(fifo_to_fd.find(system_pipe_name) != fifo_to_fd.end()){
        write(fifo_to_fd[system_pipe_name] , system_message.c_str() , system_message.size());
    }
}

void command_handler(string command , 
                     vector<ID>& router_indexes ,
                     vector<ID>& system_indexes , 
                     map < string , int >& fifo_to_fd){
    stringstream ss(command);
    string command_type;
    ss >> command_type;

    if(command_type == NEW_ROUTER)
        new_router_command_handler(ss, router_indexes , fifo_to_fd);
    else if(command_type == NEW_SYSTEM)
        new_system_command_handler(ss, system_indexes , fifo_to_fd);
    else if(command_type == CONNECT_ROUTER_SYSTEM)
        connect_router_system_command_handler(ss , fifo_to_fd);
    else if(command_type == CONNECT_ROUTER_ROUTER)
        connect_router_router_command_handler(ss , fifo_to_fd);
    else if(command_type == SEND_MESSAGE)
        send_message_command_handler(ss , fifo_to_fd);
    else if(command_type == SEND_FILE)
        send_file_command_handler(ss, fifo_to_fd);
    else if(command_type == RECV_FILE)
        recv_file_command_handler(ss , fifo_to_fd);
    else if(command_type == CFG_STP)
        cfg_stp_command_handler(router_indexes , fifo_to_fd);
    else if(command_type == SET_PAR)
        set_parents_command_handler(router_indexes , fifo_to_fd);
    else if(command_type == PRINT_INFO)
        print_info_command_handler(router_indexes , fifo_to_fd);
    else if(command_type == SYSTEM_JOIN_GROUP)
        system_join_group_command_handler(ss , router_indexes, system_indexes , fifo_to_fd);
    else if(command_type == SYSTEM_LEAVE_GROUP)
        system_leave_group_command_handler(ss , router_indexes , system_indexes , fifo_to_fd);

}

int main(){
    vector<ID> router_indexes , system_indexes;
    map < string , int > fifo_to_fd;
    string command;
    
    while(getline(cin , command)){
        command_handler(command, router_indexes, system_indexes , fifo_to_fd);
    }

    for (auto c: children)
        kill(c, SIGKILL);
}