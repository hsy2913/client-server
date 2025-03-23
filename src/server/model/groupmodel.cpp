#include "groupmodel.hpp"
#include "db.hpp"

bool GroupModel::createGroup(Group& group) {
    char sql[1024] = {0};
    sprintf(sql, "insert into AllGroup(groupname, groupdesc) values('%s', '%s')", group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

void GroupModel::addGroup(int userid, int groupid, string role) {
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser(groupid, userid, grouprole) values(%d, %d, '%s')", groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect()) {
        mysql.update(sql);
    }
}

vector<Group> GroupModel::queryGroups(int userid) {
    char sql[1024] = {0};
    sprintf(sql, "select * from AllGroup inner join GroupUser on AllGroup.id = GroupUser.groupid where GroupUser.userid = %d", userid);
    vector<Group> groupVec;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res) {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)) {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.emplace_back(group);
            }
            mysql_free_result(res);
        } 
    }   

    for (Group &group : groupVec) {
        sprintf(sql, "select id, name, state, grouprole from User inner join GroupUser on id = userid where groupid=%d", group.getId());
        MYSQL_RES* res = mysql.query(sql);
        if (res) {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)) {
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().emplace_back(user);
            }
            mysql_free_result(res);
        }         
    }
    return groupVec;
}

vector<int> GroupModel::queryGroupUsers(int userid, int groupid) {
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid != %d", groupid, userid);
    vector<int> vec;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res) {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)) {
                vec.emplace_back(atoi(row[0]));
                    
            }
            mysql_free_result(res);
        } 
    }   
    return vec; 
}