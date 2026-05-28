#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotationActor.h"
#include "TimerManager.h"
#include "TCPServerWorker.h"  
#include <mutex>
#include "TCPServer.generated.h"

UCLASS()
class QUICKSTART_API ATCPServer : public AActor
{
    GENERATED_BODY()
public:
    ATCPServer();
    ~ATCPServer();

    std::mutex ConnectionSocketMutex;  // 互斥锁成员

public:
    //TCP服务
    void StartServer();
    void ProcessCommands();
    void EndPlay(const EEndPlayReason::Type EndPlayReason);
    ARotationActor* GetActorToControl();
    void CacheActorReference();


    FSocket* ListenerSocket; // 用于监听的套接字
    FSocket* ConnectionSocket; // 用于接受连接的套接字
    FTCPServerWorker* TCPServerWorker;  // FTCPServerWorker 类型指针
    FRunnableThread* TCPServerThread;    // FTCPServerThread 类型指针
    bool bIsRunning;

private:
    ARotationActor* CachedActor = nullptr;

    FVector ParseLocationFromCommand(const FString& Command);
    FRotator ParseRotationFromCommand(const FString& Command);
};