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

