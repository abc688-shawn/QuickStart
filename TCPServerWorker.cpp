#include "TCPServerWorker.h"
#include "TCPServer.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

// 构造函数，接受 ATCPServer 实例
FTCPServerWorker::FTCPServerWorker(ATCPServer* InServer)
    : TCPServer(InServer)
{
    // 根据需要初始化其他变量
}

// 析构函数
FTCPServerWorker::~FTCPServerWorker()
{
    /*清理资源，确保在对象销毁时关闭所有打开的套接字。*/
    std::lock_guard<std::mutex> lock(TCPServer->ConnectionSocketMutex);

    if (TCPServer->ConnectionSocket)
    {
        TCPServer->ConnectionSocket->Close();
        TCPServer->ConnectionSocket = nullptr;
    }

    if (TCPServer->ListenerSocket)
    {
        TCPServer->ListenerSocket->Close();
        TCPServer->ListenerSocket = nullptr;
    }
}

// 初始化线程
bool FTCPServerWorker::Init()
{
    // 创建监听套接字并绑定，该套接字为流式套接字
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TCPServer->ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("TCPListener"), false);

    if (!TCPServer->ListenerSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create listening socket"));
        return false;
    }

    // 设置套接字为非阻塞模式
    TCPServer->ListenerSocket->SetNonBlocking(true);

    FIPv4Address Addr = FIPv4Address::Any;  // 监听所有网卡，允许跨机连接
    FIPv4Endpoint Endpoint(Addr, 12345);

    if (TCPServer->ListenerSocket->Bind(*Endpoint.ToInternetAddr()) == false)
    {
        ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
        UE_LOG(LogTemp, Error, TEXT("Failed to bind socket. Error code: %d"), (int32)LastError);
        return false;
    }

    if (!TCPServer->ListenerSocket->Listen(8))
    {
        UE_LOG(LogTemp, Error, TEXT("Listening socket failed"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("The server is listening %s:%d"), *Addr.ToString(), Endpoint.Port);
    return true;
}

// 线程执行循环
uint32 FTCPServerWorker::Run()
{
    if (!TCPServer) {
        UE_LOG(LogTemp, Error, TEXT("TCPServer is nullptr"));
        return 1; // 退出线程
    }

    while (TCPServer->bIsRunning)
    {
        // 如果没有连接，等待连接
        {
            if (!TCPServer->ConnectionSocket && TCPServer->ListenerSocket)
            {
                // UE_LOG(LogTemp, Log, TEXT("Waiting for client connection..."));

                TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
                TCPServer->ConnectionSocket = TCPServer->ListenerSocket->Accept(*RemoteAddr, TEXT("NewConnection"));

                if (TCPServer->ConnectionSocket)
                {
                    UE_LOG(LogTemp, Log, TEXT("Establishing connection with %s"), *RemoteAddr->ToString(true));
                }
                else
                {
                    // UE_LOG(LogTemp, Error, TEXT("Failed to accept client connection"));
                    continue; // 继续等待连接
                }
            }
        }

        // 如果已连接，处理命令
        {
            std::lock_guard<std::mutex> lock(TCPServer->ConnectionSocketMutex); //当 std::lock_guard 对象离开其作用域时（例如代码块结束或发生异常），会自动释放锁。
            if (TCPServer->ConnectionSocket)
            {
                // UE_LOG(LogTemp, Log, TEXT("Connection established, processing commands"));
                TCPServer->ProcessCommands(); // 处理命令
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Connection lost, waiting for new connection..."));
            }
        }

        FPlatformProcess::Sleep(0.02f); // 20ms for 50Hz
    }
    return 0;
}

// 停止工作线程并清理
void FTCPServerWorker::Stop()
{
    UE_LOG(LogTemp, Log, TEXT("FTCPServerWorker::Stop() is called"));
    TCPServer->bIsRunning = false;

    // 关闭套接字以解除任何阻塞
    std::lock_guard<std::mutex> lock(TCPServer->ConnectionSocketMutex);
    if (TCPServer->ListenerSocket)
    {
        TCPServer->ListenerSocket->Close();
        TCPServer->ListenerSocket = nullptr;
        UE_LOG(LogTemp, Log, TEXT("ListenerSocket is closed"));
    }

    if (TCPServer->ConnectionSocket)
    {
        TCPServer->ConnectionSocket->Close();
        TCPServer->ConnectionSocket = nullptr;
        UE_LOG(LogTemp, Log, TEXT("ConnectionSocket is closed"));
    }
}