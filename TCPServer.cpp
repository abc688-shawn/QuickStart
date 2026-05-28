#include "TCPServer.h"
#include "TCPServerWorker.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Common/TcpListener.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ATCPServer::ATCPServer()
{
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;  //表示尚未创建套接字
    bIsRunning = false;
    TCPServerWorker = nullptr;  // 表示尚未创建工作线程 
}

ATCPServer::~ATCPServer()
{
    // 确保在服务器销毁时停止工作线程
    if (TCPServerWorker)
    {
        TCPServerWorker->Stop();
        if (TCPServerThread)
        {
            TCPServerThread->WaitForCompletion();  //等待线程完成后再删除线程对象
            delete TCPServerThread;  // 确保删除线程
        }
        delete TCPServerWorker;
    }
    UE_LOG(LogTemp, Log, TEXT("TCPServer destroyed, sockets closed."));
}


void ATCPServer::StartServer()
{
    if (bIsRunning)
    {
        return; // 防止多次启动服务器
    }

    // 预先缓存Actor引用（在游戏线程中执行）
    CacheActorReference();

    bIsRunning = true;

    // 创建工作线程
    TCPServerWorker = new FTCPServerWorker(this);
    TCPServerThread = FRunnableThread::Create(TCPServerWorker, TEXT("TCPServerWorker"), TPri_Normal);

    UE_LOG(LogTemp, Log, TEXT("The server was started in a separate thread."));
}

void ATCPServer::ProcessCommands()
{
    if (!ConnectionSocket) {
        UE_LOG(LogTemp, Error, TEXT("ConnectionSocket is nullptr"));
        return;
    }

    TArray<uint8> Data;
    uint32 Size = 0;

    // 使用阻塞的方式接收数据
    while (ConnectionSocket->HasPendingData(Size)) {
        Data.SetNumUninitialized(FMath::Min(Size, 65507u));
        int32 BytesRead = 0;

        if (ConnectionSocket->Recv(Data.GetData(), Data.Num(), BytesRead)) {
            if (BytesRead > 0) {
                Data.Add(0);  // 确保数据以空字符结尾
                FString Command = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data.GetData())));

                UE_LOG(LogTemp, Log, TEXT("Received command: %s"), *Command);

                // 解析命令并控制 Actor
                if (Command.Contains("SetPose:")) {
                    FVector NewLocation = ParseLocationFromCommand(Command);
                    FRotator NewRotation = ParseRotationFromCommand(Command);
                    ARotationActor* Actor = GetActorToControl();
                    if (Actor) {
                        Actor->SetTargetPose(NewLocation, NewRotation);
                    }
                    else {
                        UE_LOG(LogTemp, Warning, TEXT("ARotationActor not found"));
                    }
                }
            }
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("Failed to receive data"));
            ConnectionSocket->Close();
            ConnectionSocket = nullptr;
            break;
        }
    }
}

FVector ATCPServer::ParseLocationFromCommand(const FString& Command)
{
    TArray<FString> Params;
    Command.ParseIntoArray(Params, TEXT(":"));
    if (Params.Num() < 2) return FVector::ZeroVector;

    TArray<FString> LocationData;
    Params[1].ParseIntoArray(LocationData, TEXT(","));
    if (LocationData.Num() < 3) return FVector::ZeroVector;

    // 修正坐标轴映射
    float X = FCString::Atof(*LocationData[0]);
    float Y = FCString::Atof(*LocationData[1]);
    float Z = FCString::Atof(*LocationData[2]);

    // 调整轴向映射
    return FVector(X, Y, Z);
}

FRotator ATCPServer::ParseRotationFromCommand(const FString& Command)
{
    TArray<FString> Params;
    Command.ParseIntoArray(Params, TEXT(":"));
    if (Params.Num() < 2) return FRotator::ZeroRotator;

    TArray<FString> Components;
    Params[1].ParseIntoArray(Components, TEXT(","));
    if (Components.Num() < 6) return FRotator::ZeroRotator;
    /*===============================================================
    float Roll = FCString::Atof(*Components[3]);
    float Yaw = FCString::Atof(*Components[4]);
    float Pitch = FCString::Atof(*Components[5]);

    // 修正旋转轴映射（根据模型实际方向调整）
    FRotator CorrectedRotation = FRotator(
        Pitch , // 绿（俯仰Pitch)
        Yaw , // 蓝（偏航Yaw）
        Roll// 红（横滚Roll）

    );
    ==========================================================================*/

    // 提取原始旋转值（假设传入顺序为 Pitch, Yaw, Roll）
    float Pitch = FCString::Atof(*Components[3]);
    float Yaw = FCString::Atof(*Components[4]);
    float Roll = FCString::Atof(*Components[5]);

    // 修正旋转轴映射（根据模型实际方向调整）
    // Roll 不叠加偏移：FishMesh 的视觉修正已由 MeshRelativeRotation(Roll=90) 处理，
    // Actor 根节点本身应保持 Roll=0，否则整个 Actor（含相机/IMU）会被掰倒 90°
    FRotator CorrectedRotation = FRotator(
        Pitch,  // 俯仰角
        Yaw,    // 偏航角（直接映射，与 Python nav_yaw 一致）
        0.0f    // 横滚角：始终为 0
    );

    return CorrectedRotation;
}

ARotationActor* ATCPServer::GetActorToControl()
{
    return CachedActor; // 直接返回缓存的引用
}

void ATCPServer::CacheActorReference()
{
    if (CachedActor)
    {
        return;
    }

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARotationActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        CachedActor = Cast<ARotationActor>(FoundActors[0]);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ARotationActor not found in the scene!"));
    }
}

void ATCPServer::EndPlay(const EEndPlayReason::Type EndPlayReason)  // 在游戏结束时停止服务器
{
    Super::EndPlay(EndPlayReason);

    // 如果服务器正在运行，则停止工作线程
    if (bIsRunning)
    {
        if (TCPServerWorker)
        {
            TCPServerWorker->Stop();
            if (TCPServerThread)
            {
                TCPServerThread->WaitForCompletion();  // 等待线程完成
                delete TCPServerThread;
                TCPServerThread = nullptr;
            }
            delete TCPServerWorker;
            TCPServerWorker = nullptr;
        }
        bIsRunning = false;
    }

    UE_LOG(LogTemp, Log, TEXT("The TCP server has stopped."));
}