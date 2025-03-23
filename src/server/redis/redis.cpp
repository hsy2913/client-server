#include "redis.hpp"
#include <iostream>

Redis::Redis(): _subscribe_context(nullptr), _publish_context(nullptr) {
}
Redis::~Redis() {
    if (_subscribe_context != nullptr)
        redisFree(_subscribe_context);
    if (_publish_context != nullptr) {
        redisFree(_publish_context);
    }
}
bool Redis::connect() {
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr) {
        cerr << "connect redis failed!" << endl;
        return false;
    }
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr) {
        cerr << "connect redis failed!" << endl;
        return false;
    }
    thread t([&](){observer_channel_message();}
    );
    t.detach();
    cout << "connect redis-server success!" << endl;
    return true;
}

bool Redis::subscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "subscribe %d", channel)) {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    int done = 0;
    while(!done) {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done)) {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    cerr << "subscribe command success!" << endl;
    return true;
}

bool Redis::unsubscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "unsubscribe %d", channel)) {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    int done = 0;
    while(!done) {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done)) {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

bool Redis::publish(int channel, string message) {
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "publish %d %s", channel, message.c_str());
    if (reply == nullptr) {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    cerr << "publish command success!" << endl;
    return true;
}

void Redis::observer_channel_message() {
    redisReply* reply = nullptr;
    while (REDIS_OK == redisGetReply(_subscribe_context, (void**)&reply)) {
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr) {
            cerr << "handler success!" << endl;
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr << ">>>>>>>>>>>>>>>>>>>>>>>observer_channel_message quit <<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
}

void Redis::init_notify_handler(function<void(int, string)> fn) {
    this->_notify_message_handler = fn;
}