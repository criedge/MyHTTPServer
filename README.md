# MyHTTPServer
一个在TinyWebServer的基础上大干快上边抄边学的项目

本项目是学习TinyWebServer( https://github.com/qinguoyi/TinyWebServer/ )的产物
一个在TinyWebServer的基础上大干快上边抄边学的项目

基于Linux的多线程HTTP服务器
整体使用reactor设计模式，工作分配线程和工作处理线程分离。
IO模块使用非阻塞epoll+多路复用
事件处理模块使用状态机处理HTTP报文，支持GET和POST请求。
服务器业务模块实现web端用户注册、登录功能，可以请求图片和视频。
实现异步日志系统，日志系统和工作线程分离，记录服务器运行状态。
