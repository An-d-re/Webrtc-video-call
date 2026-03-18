# Webrtc-video-call
基于 Muduo + WebRTC + Nginx 的 1v1 局域网视频通话系统

## 项目介绍
本项目使用 C++ Muduo 网络库实现 WebSocket 信令服务器，结合 WebRTC 实现浏览器端 1v1 实时视频通话，支持摄像头、麦克风采集，支持局域网内多设备互通。

## 核心技术栈
- C++ Muduo 网络库（信令服务器）
- WebRTC（P2P 音视频传输）
- Nginx（HTTPS + 静态页面 + 反向代理）
- OpenSSL（HTTPS证书）

## 环境依赖（必须安装）
### 1. 安装 Muduo 网络库
```bash
# 安装依赖
sudo apt install cmake libboost-all-dev libssl-dev

# 下载并编译 muduo
git clone https://github.com/chenshuo/muduo.git
cd muduo
./build.sh
```
### 2. 安装Nginx + OpenSSL
```bash
sudo apt install nginx openssl
```
## 项目结构
```plaintext
  Webrtc-video-call/
  ├── server.cpp          # Muduo WebSocket 信令服务器
  ├── static/
  │   └── index.html      # WebRTC 前端页面
  ├── nginx.conf          # Nginx HTTPS + 代理配置
  └── README.md
```
## 编译与运行
### 1. 编译信令服务器
```bash
g++ server.cpp -o server -lmuduo_net -lmuduo_base -lpthread
```
### 2. 配置Nginx
将 nginx.conf 复制到 /etc/nginx/conf.d/
并生成SSL证书：
```bash
sudo mkdir -p /etc/nginx/cert
sudo openssl req -newkey rsa:2048 -nodes -keyout /etc/nginx/cert/server.key -x509 -days 365 -out /etc/nginx/cert/server.crt
```
### 3. 启动服务
```bash
# 启动 Nginx
sudo systemctl restart nginx

# 启动信令服务器
./server
```
## 局域网使用方法
1. 所有设备连接**同一个热点 / 局域网wifi**
2. 查看虚拟机局域网 IP（ip a）
3. 浏览器访问：
```plaintext
https://你的虚拟机ip
```
4. 忽略证书警告 -> 开启摄像头 -> 发起通话
## 功能说明
· 1v1 实时视频通话
· 摄像头 + 麦克风采集
· WebSocket 信令转发
· 局域网 P2P 直连
· 挂断 / 重连功能
## 注意事项
· 浏览器必须使用 HTTPS 才能开启摄像头
· 所有设备必须在同一局域网
· 虚拟机需使用桥接模式
· 需关闭虚拟机防火墙（或开放端口）
