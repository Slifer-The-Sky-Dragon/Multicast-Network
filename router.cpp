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
#include <string.h>
#include <map>
#include <algorithm>

using namespace std;

#define EMPTY ""
#define MAIN_TYPE "00"
#define MESSAGE_TYPE "01"
#define CHILD_TYPE "10"
#define CONFIG_TYPE "11"

#define GROUP_MESSAGE_TYPE "MG"
#define JOIN_QUERY_TYPE "JQ"

#define FILE_START_TYPE "FS"
#define FILE_LINE_TYPE "FL"
#define FILE_END_TYPE "FE"
#define RECEIVE_TYPE "RE"

#define SFD "$"

#define NOT_DEFINED -1
#define DA_IND 1
#define SA_IND 7
#define DA_SA_LEN 6

const int DELAY = 10000;
const int MESSAGE_SIZE = 1500;
const int MAX_ROUTER = 100;
const int MAX_PORT_CNT = 100;

typedef enum {EXEC_NAME , PORT_CNT, ROUTER_ID} Argv_Indexes;

typedef map < int , int > Sys2Port;
typedef map < int , int > Port2Fd;
typedef vector < int > FdVec;

int router_id;
vector < int > router_children;

struct system {
    int id, read_fd, write_fd;
};

void packet_decoder(string message, int received_fd, int main_router_fd, Port2Fd& port_to_write_fd ,
                            Sys2Port& system_to_port , FdVec& all_read_fd , FdVec& all_write_fd , FdVec& all_system_fd,
                            map < int , int >& read_fd_to_write_fd,
                            int& root_id , int& root_fd , int& root_dst);

int find_int_value(string a , int st , int length){
    string temp = EMPTY;
    for(int i = st ; i < st + length ; i++){
        temp += a[i];
    }
    return atoi(temp.c_str());
}

string find_message_data(string message){
    string result = EMPTY;
    for(size_t i = SA_IND + DA_SA_LEN + 2 ; i < message.size() ; i++){
        result += message[i];
    }
    return result;
}

string router_info() {
    return "ROUTER(ID = " + to_string(router_id) + ")";
}

string extend_string_length(string a , size_t final_length){
    string result;
    for(size_t i = 0 ; i < final_length - a.size() ; i++){
        result += "0";
    }
    result += a;
    return result;
}

string clear_new_line(string in){
    string res = "";
    for (size_t i = 0 ; i < in.size() ; i++)
        if(in[i] != '\n')
            res += in[i];

    return res;
}

string convert_to_packet(int da , int sa , string message_type , string data){
    string result = EMPTY;

    result = SFD + extend_string_length(to_string(da) , 6) + extend_string_length(to_string(sa) , 6) +
                         message_type + data + "\n";
    return result;
}

void broadcast_root_message(string root_message , FdVec& all_read_fd , 
                            FdVec& all_write_fd , map < int , int >& read_fd_to_write_fd){
    for(int i = 0 ; i < all_write_fd.size() ; i++){
        write(all_write_fd[i] , root_message.c_str() , root_message.size());
        string res = EMPTY;   
    }
    usleep(DELAY);
}


void main_config_message_handler(FdVec& all_read_fd , FdVec& all_write_fd , map < int , int >& read_fd_to_write_fd){
    string root_data = to_string(router_id) + " " + to_string(router_id) + " 0 ";
    string root_message = convert_to_packet(0 , 0 , CONFIG_TYPE , root_data);
    broadcast_root_message(root_message , all_read_fd , all_write_fd , read_fd_to_write_fd);
}

void main_setpar_message_handler(FdVec& all_read_fd , FdVec& all_write_fd , FdVec& all_system_fd,
                                    map < int , int >& read_fd_to_write_fd , int root_fd){
    all_write_fd.clear();
    for(int i = 0 ; i < all_system_fd.size() ; i++)
        all_write_fd.push_back(all_system_fd[i]);

    if(root_fd != -1){
        all_write_fd.push_back(read_fd_to_write_fd[root_fd]);
        string root_data = to_string(router_id) + " ";
        string root_message = convert_to_packet(0 , 0 , CHILD_TYPE , root_data);
        write(read_fd_to_write_fd[root_fd] , root_message.c_str() , root_message.size());
        usleep(DELAY);
    }
}

void main_print_message_handler(){
    string res = EMPTY;
    res += router_info();
    res += ": Children's Router ID = ";
    for(int i = 0 ; i < router_children.size() ; i++){
        res += to_string(router_children[i]) + " ";
    }
    if(router_children.size() == 0)
        res += "No Children";
    cout << res << endl;
}

void main_command_handler(string message_data , Port2Fd& port_to_write_fd ,
                        Sys2Port& system_to_port , FdVec& all_read_fd , FdVec& all_write_fd , FdVec& all_system_fd,
                        map < int , int >& read_fd_to_write_fd , 
                        int& root_id , int& root_fd , int& root_dst){

    stringstream ss(message_data);
    string command;
    ss >> command;

    if (command == "C" || command == "CSS"){
        int port_number;
        string pipe_name1 , pipe_name2;
        ss >> port_number >> pipe_name1 >> pipe_name2;

        int router_read_fd = open(pipe_name2.c_str(), O_RDWR);
        int router_write_fd = open(pipe_name1.c_str(), O_RDWR);

        if(router_write_fd < 0 || router_read_fd < 0){
            perror("open");
            abort();
        }

        port_to_write_fd[port_number] = router_write_fd;
        all_write_fd.push_back(router_write_fd);
        all_read_fd.push_back(router_read_fd);
        if(command == "CSS")
            all_system_fd.push_back(router_write_fd);

        read_fd_to_write_fd[router_read_fd] = router_write_fd;

        string res = "ROUTER";
        res += "(ID = " + to_string(router_id) + ")";
        res += ": Added connection ";
        res += "(port = " + to_string(port_number);
        res += ", read_fd = " + to_string(router_read_fd);
        res += ", write_fd = " + to_string(router_write_fd);
        res += ")";

        cout << res << endl;
    }

    else if(command == "CONFIG"){
        string res = "ROUTER";
        res += "(ID = " + to_string(router_id) + ")";
        res += ": CONFIG MESSAGE received!";
        cout << res << endl;

        main_config_message_handler(all_read_fd , all_write_fd , read_fd_to_write_fd);
    }

    else if(command == "SETPAR"){
        string res = "ROUTER";
        res += "(ID = " + to_string(router_id) + ")";
        res += ": SETPAR MESSAGE received!";
        cout << res << endl; 

        main_setpar_message_handler(all_read_fd , all_write_fd , all_system_fd , read_fd_to_write_fd , root_fd);      
    }

    else if(command == "PRINT"){
        main_print_message_handler();
    }
}

void message_command_handler(string raw_message , int received_fd, int main_router_fd, int da, int sa , string message_data , 
                            Port2Fd& port_to_write_fd , Sys2Port& system_to_write_port , FdVec& all_read_fd,
                            FdVec& all_write_fd , map < int , int >& read_fd_to_write_fd){

    system_to_write_port[sa] = read_fd_to_write_fd[received_fd];

    if(system_to_write_port.find(da) != system_to_write_port.end()){
        int dest_write_fd = system_to_write_port[da];
        write(dest_write_fd , raw_message.c_str() , raw_message.size());
        string res = EMPTY;
        res += router_info();
        res += ": Sent packet ";
        res += "(source = " + to_string(sa);
        res += ", dest = " + to_string(da);
        res += ")";
        cout << res << '\n';
    }
    else{
        for(size_t i = 0 ; i < all_write_fd.size() ; i++){
            if(all_write_fd[i] == read_fd_to_write_fd[received_fd])
                continue;
            write(all_write_fd[i] , raw_message.c_str() , raw_message.size());
            string res = EMPTY;
            res += router_info();
            res += ": Broadcasted packet ";
            res += "(source = " + to_string(sa);
            res += ", dest = " + to_string(da);
            res += ", fd = " + to_string(all_write_fd[i]);
            res += ")";
            cout << res << '\n';
        }
    }
}

void group_message_command_handler(string raw_message , int received_fd, int main_router_fd, int da, int sa , string message_data , 
                            Port2Fd& port_to_write_fd , Sys2Port& system_to_write_port , FdVec& all_read_fd,
                            FdVec& all_write_fd , map < int , int >& read_fd_to_write_fd,
                            int& root_id , int& root_fd , int& root_dst){
    // system_to_write_port[sa] = read_fd_to_write_fd[received_fd];
    for(size_t i = 0 ; i < all_write_fd.size() ; i++){
        if(all_write_fd[i] == read_fd_to_write_fd[received_fd])
            continue;
        string message_data = "";
        write(all_write_fd[i] , raw_message.c_str() , raw_message.size());
        string res = EMPTY;
        // res += router_info();
        // res += ": Broadcasted packet ";
        // res += "(source = " + to_string(sa);
        // res += ", dest = " + to_string(da);
        // res += ", fd = " + to_string(all_write_fd[i]);
        // res += ")";
        // cout << res << '\n';
    }
}

bool is_better_root(int root_id , int root_dst , int recv_root_id , int recv_dst){
    if(recv_root_id > root_id)
        return false;
    else if(recv_root_id == root_id)
        return recv_dst < root_dst;
    else
        return true;
}

void check_for_other_messages(stringstream& ss , int received_fd , int main_router_fd , Port2Fd& port_to_write_fd,
                                Sys2Port& system_to_write_port, FdVec& all_read_fd, FdVec& all_write_fd,
                                FdVec& all_system_fd , map < int , int >& read_fd_to_write_fd,
                                int& root_id , int& root_fd , int& root_dst){
    string command_part;
    while(ss >> command_part){
        string a , b;
        ss >> a >> b;
        string message = command_part + " " + a + " " + b + "\n";
        packet_decoder(message, received_fd , main_router_fd , port_to_write_fd , 
                                                system_to_write_port , all_read_fd , all_write_fd , all_system_fd, 
                                                read_fd_to_write_fd , root_id , root_fd , root_dst);
    }
}

void config_command_handler(string raw_message , int received_fd, int main_router_fd, int da, int sa , string message_data , 
                            Port2Fd& port_to_write_fd , Sys2Port& system_to_write_port , FdVec& all_read_fd,
                            FdVec& all_write_fd , FdVec& all_system_fd , map < int , int >& read_fd_to_write_fd,
                            int& root_id , int& root_fd , int& root_dst){

    stringstream ss(message_data);
    int recv_sender , recv_root_id , recv_dst;
    ss >> recv_sender >> recv_root_id >> recv_dst;

    string res = EMPTY;
    res += router_info();
    res += ": ROOT MESSAGE = ";
    res += to_string(recv_sender) + " " + to_string(recv_root_id) + " " + to_string(recv_dst);
    res += ", fd = " + to_string(received_fd);
    res += ")";
    cout << res << '\n';

    if(is_better_root(root_id , root_dst , recv_root_id , recv_dst)){
        root_id = recv_root_id;
        root_dst = recv_dst;
        root_fd = received_fd;

        string root_data = to_string(router_id) + " " + to_string(root_id) + " " + to_string(recv_dst + 1) + " ";
        string root_message = convert_to_packet(0 , 0 , CONFIG_TYPE , root_data);
        broadcast_root_message(root_message , all_read_fd , all_write_fd , read_fd_to_write_fd);
    }

    check_for_other_messages(ss , received_fd , main_router_fd , port_to_write_fd, system_to_write_port, all_read_fd,
                                all_write_fd, all_system_fd , read_fd_to_write_fd, root_id , root_fd , root_dst);
}

void child_command_handler(string raw_message , int received_fd, int main_router_fd, int da, int sa , string message_data , 
                            Port2Fd& port_to_write_fd , Sys2Port& system_to_write_port , FdVec& all_read_fd,
                            FdVec& all_write_fd , map < int , int >& read_fd_to_write_fd,
                            int& root_id , int& root_fd , int& root_dst){
    
    stringstream ss(message_data);
    int children_id;
    ss >> children_id;

    string res = EMPTY;
    res += router_info();
    res += ": CHILD MESSAGE received ";
    res += " child id = " + to_string(children_id);
    res += ", fd = " + to_string(received_fd);
    res += ")";
    cout << res << '\n';

    router_children.push_back(children_id);
    all_write_fd.push_back(read_fd_to_write_fd[received_fd]);
}

void router_command_handler(int received_fd, int main_router_fd, string raw_message, int da, int sa , string message_type ,
                            string message_data , Port2Fd& port_to_write_fd ,
                            Sys2Port& system_to_port , FdVec& all_read_fd , FdVec& all_write_fd, FdVec& all_system_fd, 
                            map < int , int >& read_fd_to_write_fd , 
                            int& root_id , int& root_fd , int& root_dst){
    if(message_type == MAIN_TYPE)
        main_command_handler(message_data, port_to_write_fd, system_to_port, all_read_fd , all_write_fd , all_system_fd,
                                read_fd_to_write_fd , root_id , root_fd , root_dst);
    
    else if(message_type == CONFIG_TYPE)
        config_command_handler(raw_message , received_fd , main_router_fd , da , sa , message_data , port_to_write_fd ,
                                 system_to_port , all_read_fd , all_write_fd , all_system_fd, read_fd_to_write_fd,
                                 root_id , root_fd , root_dst);
    else if(message_type == CHILD_TYPE)
        child_command_handler(raw_message , received_fd , main_router_fd , da , sa , message_data , port_to_write_fd ,
                                 system_to_port , all_read_fd , all_write_fd , read_fd_to_write_fd,
                                 root_id , root_fd , root_dst);
    else if(message_type == GROUP_MESSAGE_TYPE)
        group_message_command_handler(raw_message , received_fd , main_router_fd , da , sa , message_data , port_to_write_fd ,
                                 system_to_port , all_read_fd , all_write_fd , read_fd_to_write_fd,
                                 root_id , root_fd , root_dst);
    else
        message_command_handler(raw_message , received_fd , main_router_fd , da , sa , message_data , port_to_write_fd ,
                                 system_to_port , all_read_fd , all_write_fd , read_fd_to_write_fd);
}

void packet_decoder(string message, int received_fd, int main_router_fd, Port2Fd& port_to_write_fd ,
                            Sys2Port& system_to_port , FdVec& all_read_fd , FdVec& all_write_fd , FdVec& all_system_fd,
                            map < int , int >& read_fd_to_write_fd,
                            int& root_id , int& root_fd , int& root_dst){
    int da = find_int_value(message , DA_IND , DA_SA_LEN);
    int sa = find_int_value(message , SA_IND , DA_SA_LEN);

    string message_type = EMPTY;
    message_type += message[SA_IND + DA_SA_LEN];
    message_type += message[SA_IND + DA_SA_LEN + 1];

    string message_data = find_message_data(message);
    router_command_handler(received_fd , main_router_fd , message , da , sa , message_type , message_data ,
                            port_to_write_fd , system_to_port , all_read_fd , all_write_fd , all_system_fd ,
                            read_fd_to_write_fd , root_id , root_fd , root_dst);
}

int main(int argc , char* argv[]) {
    router_id = atoi(argv[ROUTER_ID]);
    cout << "New Router has been created..." << endl;

    int root_id = router_id , root_fd = -1 , root_dst = 0;
    Port2Fd port_to_write_fd;
    Sys2Port system_to_port;
    FdVec all_read_fd;
    FdVec all_write_fd;
    FdVec all_system_fd;
    map < int , int > read_fd_to_write_fd;

    string router_name_pipe = "fifo_ms" + to_string(router_id);
    int main_router_fd = open(router_name_pipe.c_str() , O_RDONLY | O_NONBLOCK);

    if(main_router_fd < 0){
        cout << "Error in opening name pipe..." << endl;
        exit(0);
    }
    all_read_fd.push_back(main_router_fd);

    fd_set read_fds;
    while(true){
        FD_ZERO(&read_fds);
        int max_fd = -1;
        for(size_t i = 0 ; i < all_read_fd.size() ; i++){
            FD_SET(all_read_fd[i] , &read_fds);
            max_fd = max(all_read_fd[i] , max_fd);
        }
        int res = select(max_fd + 1 , &read_fds , NULL , NULL , NULL);
        if(res < 0)
            cout << "Error occured in select..." << endl;
        
        for(size_t i = 0 ; i < all_read_fd.size() ; i++){
            if(FD_ISSET(all_read_fd[i] , &read_fds)){
                char message[MESSAGE_SIZE];
                bzero(message , MESSAGE_SIZE);
                int res = read(all_read_fd[i] , message , MESSAGE_SIZE);
                if (res == 0) {
                    cout << router_info() << ": PANIC! Read EOF on descriptor!" << endl;
                    abort();
                }
                packet_decoder(string(message), all_read_fd[i] , main_router_fd , port_to_write_fd , 
                                        system_to_port , all_read_fd , all_write_fd , all_system_fd , read_fd_to_write_fd , 
                                        root_id , root_fd , root_dst);
            }
        }
    }
}