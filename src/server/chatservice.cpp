#include "chatservice.hpp"
#include "public.hpp"

// #include <string>
#include <muduo/base/Logging.h>
using namespace muduo;
// using namespace std;
using namespace placeholders;
ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

// register handler
ChatService::ChatService() {
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    if (_redis.connect()) {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgid) {
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        // LOG_ERROR << "msgid" << msgid << " cannot find handler!";
        // string errstr = "msgid:" + msgid + " cannot find handler!";
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp) {
            LOG_ERROR << "msgid" << msgid << " cannot find handler!";
        };
    }
    else
        return _msgHandlerMap[msgid];
}

void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);
    // printf("%d %s", user.getId(), user.getPwd().c_str());
    if (user.getId() == id && user.getPwd() == pwd) {
        if (user.getState() == "online") {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "The account is logging.";
            conn->send(response.dump());
        }
        else { 
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(), conn});
            }
            _redis.subscribe(user.getId());
            // Q: if server down? query but not remove?
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            vector<string> vec = _offlineMsgModel.query(user.getId());
            if (!vec.empty()) {
                response["offlinemsg"] = vec;
                _offlineMsgModel.remove(user.getId());
            }            
            vector<User> userVec = _friendModel.query(id);
            // TODO 1 to 2 can find but 2 to 1 cannot find friend pass the info and two direction can check
            if (!userVec.empty()) {
                vector<string> friendVec;
                for (User &userInfo: userVec) {
                    json friendInfo;
                    friendInfo["id"] = userInfo.getId();
                    friendInfo["name"] = userInfo.getName();
                    friendInfo["state"] = userInfo.getState();
                    friendVec.emplace_back(friendInfo.dump());
                }
                response["friend"] = friendVec;
            }
            vector<Group> groupVec = _groupModel.queryGroups(id);
            if (!groupVec.empty()) {
                vector<string> groupStr;
                for (Group& group: groupVec) {
                    json groupInfo;
                    groupInfo["groupid"] = group.getId();
                    groupInfo["groupname"] = group.getName();
                    groupInfo["groupdesc"] = group.getDesc();
                    vector<string> groupAllUser;
                    for (GroupUser &groupUser: group.getUsers()) {
                        json groupUserJs;
                        groupUserJs["id"] = groupUser.getId();
                        groupUserJs["name"] = groupUser.getName();
                        groupUserJs["state"] = groupUser.getState();
                        groupUserJs["role"] = groupUser.getRole();
                        groupAllUser.emplace_back(groupUserJs.dump());
                    }
                    groupInfo["users"] = groupAllUser;
                    groupStr.emplace_back(groupInfo.dump());
                }
                response["group"] = groupStr;

            }
            conn->send(response.dump());
        }

    }
    else {
        // TODO: detail
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "The account or the password is wrong!";
        conn->send(response.dump());
    }
}

void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    string name = js["name"];
    string pwd = js ["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state) {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());

    }
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn) {
    User user;
    { 
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it) {
            if (it->second == conn) {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }

    }
    _redis.unsubscribe(user.getId());
    if (user.getId() != -1) {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

void ChatService::reset() {
    _userModel.resetState();
}

void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int toid = js["to"];
    // string msg = js["msg"];
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            it->second->send(js.dump());
            return;
        }
        // else {
            
        // }
    }
    User user = _userModel.query(toid);
    if (user.getState() == "online") {
        _redis.publish(toid, js.dump());
        return;
    }

    // TOPO: 集群，登录到其他电脑
    _offlineMsgModel.insert(toid, js.dump());
}

void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    // 转发好友消息 curr_oneChat
    // 发送好友同意消息 ack
    // add to the databases
    int userid = js["id"];
    int friendid = js["friendid"];

    _friendModel.insert(userid, friendid);
    
}

void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    Group group;
    group.setName(js["groupname"]);
    group.setDesc(js["groupdesc"]);
    bool flag = _groupModel.createGroup(group);
    if (flag) {
        _groupModel.addGroup(userid, group.getId(), "creator");
        // json msg;
        // msg["msgid"] = CREATE_GROUP_MSG;
        // msg["groupid"] = group.getId();
        // msg["errno"] = 0;
        // conn->send(msg.dump());
    }
    else {
        // json msg;
        // msg["msgid"] = CREATE_GROUP_MSG;
        // msg["errno"] = 1;
        // msg["errmsg"] = "The action of creating the group fails!";
        // conn->send(msg.dump());
    }
    
}

void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    int groupid = js["groupid"];
    // string role = js["role"];
    _groupModel.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    int groupid = js["groupid"];
    // string msg = js["msg"];
    vector<int> users = _groupModel.queryGroupUsers(userid, groupid);
    {
        lock_guard<mutex> lock(_connMutex);
        for (int user: users) {
            auto it = _userConnMap.find(user);
            if (it != _userConnMap.end())
                it->second->send(js.dump());
            else {
                User currUser = _userModel.query(user);
                if (currUser.getState() == "online") {
                    _redis.publish(user, js.dump());
                }
                else 
                    _offlineMsgModel.insert(user, js.dump());
            }
            
        }
    }
}

void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    User user;
    int userid = js["id"];
    // string msg = js["msg"];
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()) {
            // user.setId(userid);
            _userConnMap.erase(it);
        }    
    }
    // if (user.getId() != -1) {
    _redis.unsubscribe(userid);
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
    // }
}

void ChatService::handleRedisSubscribeMessage(int channel, string message) {
    json js = json::parse(message.c_str());
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(channel);
        if (it != _userConnMap.end()) {
            it->second->send(message);
            return;
        }
    }
    _offlineMsgModel.insert(channel, message);
}