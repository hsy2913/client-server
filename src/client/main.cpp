#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

User g_currentUser;
vector<User> g_currentUserFriendList;
vector<Group> g_currentUserGroupList;
bool isMainMenuRunning = false;
void showCurrentUserData();
void readTaskHandler(int clientfd);
string getCurrentTime();
void mainMenu(int cliendfd);

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if (connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)) == -1) {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }
    for (;;) {
        cout << "=========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get();

        switch(choice) {
            case 1: {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                cin.get();
                cout << "password:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1 , 0);
                if (len == -1)
                    cerr << "send login msg error:" << request << endl;
                else {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (len == -1)
                        cerr << "recv login msg error" << endl;
                    else {
                        json response = json::parse(buffer);
                        if (response["errno"].get<int>() != 0)
                            cerr << response["errmsg"] << endl;
                        else {
                            g_currentUser.setId(response["id"]);
                            g_currentUser.setName(response["name"]);
                            g_currentUserFriendList.clear();
                            if (response.contains("friend")) {
                                vector<string> vec = response["friend"];
                                for (string &str: vec) {
                                    json js = json::parse(str);
                                    User user;
                                    user.setId(js["id"]);
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }
                            g_currentUserGroupList.clear();
                            if (response.contains("group")) {
                                vector<string> vec = response["group"];
                                for (string &str:vec) {
                                    json js = json::parse(str);
                                    Group group;
                                    group.setId(js["groupid"].get<int>());
                                    group.setName(js["groupname"]);
                                    group.setDesc(js["groupdesc"]);
                                    vector<string> vecUser = js["users"];
                                    for (string& userStr: vecUser) {
                                        json userInfo = json::parse(userStr);
                                        GroupUser groupUser;
                                        groupUser.setId(userInfo["id"].get<int>());
                                        groupUser.setName(userInfo["name"]);
                                        groupUser.setState(userInfo["state"]);
                                        groupUser.setRole(userInfo["role"]);
                                        group.getUsers().emplace_back(groupUser);
                                    }
                                    g_currentUserGroupList.push_back(group);
                                }
                            }

                            showCurrentUserData();

                            if (response.contains("offlinemsg")) {
                                vector<string> vec = response["offlinemsg"];
                                for (string &str: vec) {
                                    json js = json::parse(str);
                                    int msgtype = js["msgid"].get<int>();
                                    if (msgtype == ONE_CHAT_MSG) {
                                        cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                            << " said:" << js["msg"].get<string>() << endl;
                                    }
                                    else if (msgtype == GROUP_CHAT_MSG) {
                                        cout << "group[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                            << " said:" << js["msg"].get<string>() << endl;
                                    }
                                }
                            }
                            static int threadnumber = 0;
                            if (threadnumber == 0) {
                                std::thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                ++threadnumber;
                            }
 
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }
            case 2: {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username:";
                cin.getline(name, 50);
                cout << "userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REG_MSG; 
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (len == -1) {
                    cerr << "send reg msg error:" << request << endl;
                }
                else {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (len == -1)
                        cerr << "recv reg response error" << endl;
                    else {
                        json response = json::parse(buffer);
                        if (response["errno"].get<int>() != 0)
                            cerr << name << " is already exist, register error!" << endl;
                        else 
                            cerr << name << " register success, userid is " << response["id"]
                                << ", do not forget it!" << endl;
                    }
                }
                break;

            }
            case 3: {
                close(clientfd);
                exit(0);
            }
            default: {
                cerr << "invalid input!" << endl;
                break;
            }
        }
    }
    return 0;
}

void showCurrentUserData() {
    cout << "========================login user=======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "------------------------friend list----------------------" << endl;
    if (!g_currentUserFriendList.empty()) {
        for (User &user: g_currentUserFriendList) {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl; 
        }
    }
    cout << "------------------------group list-----------------------" << endl;
    if (!g_currentUserGroupList.empty()) {
        for (Group &group: g_currentUserGroupList) {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser& user: group.getUsers()) {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << 
                user.getRole() << endl;
            }
        }
    }
    cout << "=========================================================" << endl;
}

void readTaskHandler(int clientfd) {
    for (;;) {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (len == -1 || len == 0) {
            close(clientfd);
            exit(-1);
        }
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (msgtype == ONE_CHAT_MSG) {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                << " said:" << js["msg"].get<string>() << endl;
            continue;
        }
        else if (msgtype == GROUP_CHAT_MSG) {
            cout << "group[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                << " said:" << js["msg"].get<string>() << endl;
            continue;
        }
        else {
            continue;
        }
    }

}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

void help(int = 0, string = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void logout(int, string);

unordered_map<string, string> commandMap = {
    {"help", "display supported command"},
    {"chat", "one-chat, format:{chat:friendid:message}"},
    {"addfriend", "add friend, format:{addfriend:friendid}"},
    {"creategroup", "create group, format:{creategroup:groupname:groupdesc}"},
    {"addgroup", "add group, format:{addgroup:groupid}"},
    {"groupchat", "group chat, format:{groupchat:groupid:message}"},
    {"logout", "logout"}
};

unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", logout}
};

void mainMenu(int clientfd) {
    help();
    char buffer[1024];
    while(isMainMenuRunning) {
        cin.getline(buffer, 1024);
        string commandBuf(buffer);
        string command;
        int idx = commandBuf.find(':');
        if (idx == -1)
            command = commandBuf;
        else
            command = commandBuf.substr(0, idx);
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) {
            cerr << "invalid input command!" << endl;
            continue;
        }
        it->second(clientfd, commandBuf.substr(idx + 1, commandBuf.size() - idx));
    }
}

void help(int clientfd, string str) {
    cout << "show command list >>> " << endl;
    for (auto& p:commandMap) {
        cout << p.first << ":" << p.second << endl; 
    } 
    cout << endl;
}

void addfriend(int clientfd, string str) {
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addfriend msg error ->" << buffer << endl;
}

void chat(int clientfd, string str) {
    int idx = str.find(':');
    if (idx == -1) {
        cerr << "command chat error ->" << str << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["to"] = friendid;
    js["msg"] = msg;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) 
        cerr << "send chat msg error ->" << buffer << endl;
    
}


void creategroup(int clientfd, string str) {
    int idx = str.find(':');
    if (idx == -1) {
        cerr << "command creategroup error ->" << str << endl;
        return;
    }
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = str.substr(0, idx);
    js["groupdesc"] = str.substr(idx + 1, str.size() - idx);
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send creategroup msg error ->" << buffer << endl;
}

void addgroup(int clientfd, string str) {
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = atoi(str.c_str());
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addgroup msg error ->" << buffer << endl;
}

void groupchat(int clientfd, string str) {
    int idx = str.find(':');
    if (idx == -1) {
        cerr << "command groupchat error ->" << str << endl;
        return;
    }
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = atoi(str.substr(0, idx).c_str());
    js["msg"] = str.substr(idx + 1, str.size() - idx);
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addgroup msg error ->" << buffer << endl;
}

void logout(int clientfd, string str) {
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addgroup msg error ->" << buffer << endl;
    else 
        isMainMenuRunning = false;
}