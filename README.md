# 演示流程
+ 启动
  + make all (c file)
  + ./server & python3 server.py
  + ./client & python3 client.py
+ 服务器互发文本消息
  + select server can be 1 or 2
    + 1: 与本机的server通信
    + 2: 与别机的server通信
+ 将文件从客户端传给本机服务器
  + 注意：文件不能传输给别机服务器，因为python的多线程接收文件代码我没写，我只写了c
+ 将文件从服务器传给别机客户端
  + 注意：只可传给别机，原因与上一个类似（懒了
+ 在select server选项输入0即可断开连接