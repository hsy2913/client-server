# ChatServer

在 Linux 环境下，基于 muduo 网络库开发的集群聊天服务器。该系统实现了新用户注册、用户登录、好友管理、群组管理、好友通信、群组聊天和离线消息存储等功能。

## 项目内容

- **高效通信**：基于 `muduo` 网络库构建核心通信模块，优化了高并发处理性能，确保系统在大规模用户场景下仍能高效运行。
- **数据序列化**：利用第三方 `JSON` 库实现高效的通信数据序列化与反序列化，减少了网络传输延迟。
- **负载均衡**：通过 `Nginx` 的TCP负载均衡功能，将客户端请求智能分配到多个服务器，提高系统的可扩展性和容错能力。
- **跨服务器通信**：基于 `Redis` 的发布-订阅模式，实现了跨服务器的实时消息传递，支持高效的消息路由。
- **数据持久化**：封装 `MySQL` 接口，将用户信息、聊天记录等重要数据存储至磁盘，实现数据持久化管理。
- **构建工具**：使用 `CMake` 进行项目自动化构建，确保跨平台编译与构建的兼容性。


## 构建项目

```sh
bash build.sh
```

## 运行服务端与客户端

### 启动服务端
```sh
cd ./bin
./ChatServer '127.0.0.1' 6000
```

### 启动客户端

```sh
./ChatClient '127.0.0.1' 8000
```

## nginx负载均衡配置

```sh
cd /usr/local/nginx/conf
sudo vim nginx.conf
```

```nginx.conf
stream {
    upstream MyServer {
        server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }
    server {
        proxy_connect_timeout 1s;
        #proxy_timeout 3s;
        listen 8000;
        proxy_pass MyServer;
        tcp_nodelay on;
    }
}
```