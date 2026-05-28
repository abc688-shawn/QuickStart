#pragma once
#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

class ATCPServer;  // 前向声明 ATCPServer

class FTCPServerWorker : public FRunnable
{
public:
    // 构造函数，传入 ATCPServer 实例
    FTCPServerWorker(ATCPServer* InServer);

    // 析构函数
    virtual ~FTCPServerWorker();

    // 初始化线程
    virtual bool Init() override;

    // 线程执行主循环
    virtual uint32 Run() override;

    // 停止线程
    virtual void Stop() override;

private:
    // TCP 服务器引用
    ATCPServer* TCPServer;
};