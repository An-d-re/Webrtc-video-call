#include <muduo/net/EventLoop.h>
#include <muduo/net/Server.h>
#include <muduo/base/Logging.h>
#include <set>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sys/stat.h>
#include <sstream>
#include <iostream>
#include <cstring>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>

using namespace muduo;
using namespace muduo::net;

const int PORT = 8888;
std::set<TcpConnectionPtr> g_ws_clients;

const std::unordered_map<std::string, std::string> CONTENT_TYPE = {
    {".html", "text/html; charset=UTF-8"},
    {".css", "text/css; charset=UTF-8"},
    {".js", "application/javascript; charset=UTF-8"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"}
};

std::string base64Encode(const unsigned char* buffer, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);
    return std::string(bufferPtr->data, bufferPtr->length);
}

std::string calcWsAcceptKey(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)combined.c_str(), combined.size(), hash);
    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string parseHttpPath(const std::string& req) {
    size_t path_start = req.find("GET ");
    if (path_start == std::string::nops) return "/";
    path_start += 4;
    size_t path_end = req.find(" HTTP/", path_start);
    if (path_end == std::string::npos) return "/";
    return req.substr(path_start, path_end - path_start);
}

std::string parseWsFrame(const std::string& msg) {
    const uint8_t* data = (const uint8_t*)msg.data();
    int len = msg.size();
    if (len < 2) return "";

    int payload_len = data[1] & 0x7F;
    bool masked = (data[1] & 0x80) != 0;
    int offset = 2;

    if (payload_len == 126) {
        payload_len = (data[2] << 8) | data[3];
        offset = 4;
    } else if (payload_len == 127) {
        if (len < 10) return "";
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        offset = 10;
    }

    uint8_t mask[4] = {0};
    if (masked) {
        if (len < offset + 4) return "";
        std::memcpy(mask, data + offset, 4);
        offset += 4;
    }

    std::string decoded;
    decoded.reserve(payload_len);
    for (int i = 0; i < payload_len; ++i) {
        decoded += data[offset + i] ^ mask[i % 4];
    }

    return decoded;
}

std::string wrapWsFrame(const std::string& data) {
    std::string frame;
    frame += (char)0x81;

    int len = data.size();
    if (len <= 125) {
        frame += (char)len;
    } else if(len <= 65535) {
        frame += (char)126;
        frame += (char)((len >> 8) & 0xFF);
        frame += (char)(len & 0xFF);
    } else {
        return ""; // Too large
    }

    frame += data;
    return frame;
}

void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        LOG_INFO << "新客户端连接：" << conn->peerAddress().toIpPort(); 
        g_ws_clients.insert(conn);
    } else {
        LOG_INFO << "客户端断开：" << conn->peerAddress().toIpPort(); 
        g_ws_clients.erase(conn);
    }
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
    std::string msg = buf->retrieveAllAsString();

    if (msg.find("GET ") != std::string::nops && msg.find("Upgrade: websocket") != std::string::npos) {
        LOG_INFO << "收到WebSocket握手请求";
        // 解析 Sec-WebSocket-Key
        size_t key_pos = msg.find("Sec-WebSocket-Key:");
        std::string key;

        if(key_pos != std::string::npos) {
            // 跳过键名长度
            key_pos += 19;
            // 查找行尾
            size_t key_end = msg.find("\r\n", key_pos);
            if (key_end != std::string::npos) {
                // 截取 key
                key = msg.substr(key_pos, key_end - key_pos);
                // 去除首尾空白字符
                size_t start = key.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    key = key.substr(start);
                }
            }
        }

        // 密钥有效
        if (!key.empty()) {
            // 计算响应密钥
            std::string accept = calcWsAcceptKey(key);
            // 构造握手响应
            std::string response = 
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
            
            // 发送握手响应
            conn->send(response);
            // 加入客户端列表
            g_ws_clients.insert(conn);
            LOG_INFO << "WebSocket 握手成功！当前在线：" << g_ws_clients.size();
            return;
        } else {
            // 无效则关闭
            conn->shutdown();
            return;
        }
    }

    // websocket消息转发
    if (g_ws_clients.count(conn)) {
        // 解析消息
        std::string decoded = parseWsFrame(msg);

        if (!decoded.empty()) {
            LOG_INFO << "收到信令：" << decoded;
            // 封装成帧
            std::string wrapped = wrapWsFrame(decoded);

            // 广播给其他客户端
            for (const auto& client : g_ws_clients) {
                if (client != conn && !wrapped.empty()) {
                    client->send(wrapped);
                }
            }
        }
        return;
    }

    // HTTP请求处理
    if (msg.find("GET ") == 0) {
        std::string file_path = parseHttpPath(msg);

        // 安全检查，禁止访问上级目录
        if (file_path.find("..") != std::string::npos) {
            std::string content = "403 Forbidden";
            std::string response = 
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/plain; charset=UTF-8\r\n"
                "Content-Length: " + std::to_string(content.size()) + "\r\n";
                "Connection: close\r\n\r\n" + content;
            conn->send(response);
            conn->shutdown();
            return;
        } 

        // 默认访问主页
        if (file_path == "/") file_path = "/index.html";
        // 拼接静态文件路径
        file_path = "./static" + file_path;

        // 读取文件内容
        std::string file_content = fileExists(file_path) ? readFile(file_path) : "<h1>404 Not Found</h1>";
        
        // 获取文件后缀
        size_t dot_pos = file_path.find_last_of(".");
        std::string ext;
        if (dot_pos != std::string::npos) {
            ext = file_path.substr(dot_pos);
        }

        // 设置默认类型
        std::string content_type = "text/plain; charset=uft-8";
        // 查找对应类型
        if (CONTENT_TYPE.count(ext)) {
            content_type = CONTENT_TYPE.at(ext);
        }

        // 构造 HTTP 响应
        std::string response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + content_type + "\r\n"
            "Content-Length: " + std::to_string(content.size()) + "\r\n"
            "Connection: close\r\n\r\n" + file_content;

        // 发送给浏览器
        conn->send(resp);
        conn->shutdown();
        return;
    }
}


// 服务器入口
int main() {
    Logger::setLogLevel(Logger::INFO);
    EventLoop loop;
    InetAddress addr(PORT);

    TcpServer server(&loop, addr, "VideoCallServer");

    // 设置连接回调
    server.setConnectionCallback(onConnection);
    // 设置消息回调
    server.setMessageCallback(onMessage);
    // 设置 4 个工作线程
    server.setThreadNum(4);

    // 启动服务器
    server.start();

    // 打印启动信息
    LOG_INFO << "=====================================";
    LOG_INFO << "Muduo 视频通话服务器已启动！";
    LOG_INFO << "监听端口：" << PORT;
    LOG_INFO << "静态文件目录：./static/";
    LOG_INFO << "=====================================";

    // 启动事件循环
    loop.loop();
    return 0;
}
