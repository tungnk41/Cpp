#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <signal.hpp>


#define SERVER_PORT 5050 
#define BUFFER_SIZE 1024
#define MAX_CLIENT 256    

void handleSocketConnection(int socketClient);
void sendMessage(const std::string &msg);
void print(std::string msg);
bool startsWith(const std::string& str, const std::string& prefix);
void removePattern(std::string& str, const std::string& pattern);

void handleSignalCtrlZ(int socketServer) {
    close(socketServer);
    exit(0);
}

int totalClient = 0;
std::mutex mtx;
std::unordered_map<std::string, int> socketClients;

int main(int argc,const char **argv,const char **envp){
    Signal sig;
    sig.registerSignal(sig.getProcessID(),SIGINT,handleSignalCtrlZ);

    int socketServer,socketClient;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddress_size;

    socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketServer == -1){
        print("create socket failed!");
        exit(1);
    }
    memset(&serverAddress,0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    // serverAddress.sin_port=htons(atoi(argv[1]));
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(socketServer, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
        print("bind failed!");
        print("Please run : netstat -tulpn | grep LISTEN ");
        print("kill pid");
        exit(1);
    }
    printf("the server is running on port %d\n", SERVER_PORT);
    if (listen(socketServer, MAX_CLIENT) == -1){
        print("listen failed");
        exit(1);
    }

    while(1){  
        clientAddress_size = sizeof(clientAddress);
        socketClient = accept(socketServer, (struct sockaddr*)&clientAddress, &clientAddress_size);
        if (socketClient == -1){
            print("accept failed!");
            exit(1);
        }
        std::thread th(handleSocketConnection, socketClient);
        th.detach();
    }
    close(socketServer);
    return 0;
}

void handleSocketConnection(int socketClient){
    char msg[BUFFER_SIZE];
    std::string username = "";
    int ret = 0;
    while((ret = recv(socketClient, msg, sizeof(msg),0)) != 0){
        print(std::string("----> Receive: ").append(msg));
        std::string receivedData = std::string(msg);
        if(startsWith(receivedData,"[#]")) {
            removePattern(receivedData,"[#]");
            username = receivedData;
            print(std::string("----> received tranform: ").append(username));
            if(socketClients.count(username) > 0){
                std::string error_msg = std::string(username) + " exists already. Please quit and enter with another name!";
                send(socketClient, error_msg.c_str(), error_msg.length()+1, 0);
            }
            else {
                socketClients[username] = socketClient;
                std::string msg = username + " joined!";
                mtx.lock();
                totalClient++;
                mtx.unlock();
                sendMessage(msg);
            }
        }
        else {
            sendMessage(std::string(receivedData));
        }
    }
    if(ret == 0){
        std::string leave_msg;
        mtx.lock();
        if(socketClients.count(username) > 0) {
            socketClients.erase(username);
        }
        mtx.unlock();
        leave_msg = username + " has left the chat room";
        sendMessage(leave_msg);
        close(socketClient);
    }
    else {
        close(socketClient);
    }
}

void sendMessage(const std::string &msg){
    print(msg);
    for (auto const& it : socketClients) {
        send(it.second, msg.c_str(), msg.length()+1, 0);
    }
}

void print(std::string msg) {
    std::cout << msg << std::endl;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }
    return str.substr(0, prefix.length()) == prefix;
}

void removePattern(std::string& str, const std::string& pattern) {
    size_t pos = 0;
    while ((pos = str.find(pattern, pos)) != std::string::npos) {
        str.erase(pos, pattern.length());
    }
}