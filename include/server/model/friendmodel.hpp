#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H
#include "user.hpp"
#include <vector>
using namespace std;

class FriendModel {
public:
    // TODO 好友列表多，一般记录在客户端
    void insert(int userid, int friendid);

    vector<User> query(int userid); 
};

#endif