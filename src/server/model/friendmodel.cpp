#include "friendmodel.hpp"
#include "db.hpp"

void FriendModel::insert(int userid, int friendid) {
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend(userid, friendid) values (%d, %d)", userid, friendid);

    MySQL mysql;
    if (mysql.connect()) {
        mysql.update(sql);
    }
}

vector<User> FriendModel::query(int userid) {
    char sql[1024] = {0};
    // sprintf(sql, "select id, name, state from User where id= (select friendid from Friend where userid=%d)", userid);
    sprintf(sql, "select id, name, state from User inner join Friend on User.id = Friend.friendid where Friend.userid=%d", userid);
    vector<User> vec;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res) {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)) {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.emplace_back(user);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}