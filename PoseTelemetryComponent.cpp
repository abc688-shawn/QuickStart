#include "PoseTelemetryComponent.h"
#include "Common/TcpSocketBuilder.h"
#include "GameFramework/Actor.h"

UPoseTelemetryComponent::UPoseTelemetryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPoseTelemetryComponent::BeginPlay()
{
    Super::BeginPlay();
    bRunning = true;

    AcceptFuture = Async(EAsyncExecution::Thread, [this]
    {
        FIPv4Endpoint EP(FIPv4Address::Any, ListenPort);
        Listening = FTcpSocketBuilder(TEXT("PoseTelemetryListener"))
            .AsReusable()
            .BoundToEndpoint(EP)
            .Listening(1);

        if (!Listening)
        {
            UE_LOG(LogTemp, Error, TEXT("[PoseTelemetry] 监听端口 %d 失败"), ListenPort);
            return;
        }
        Listening->SetNonBlocking(false);
        UE_LOG(LogTemp, Log, TEXT("[PoseTelemetry] 等待 Python 连接 port %d ..."), ListenPort);

        bool bPending = false;
        while (bRunning && !bPending)
        {
            Listening->HasPendingConnection(bPending);
            FPlatformProcess::Sleep(0.001f);
        }
        if (!bRunning) { Listening->Close(); return; }

        ClientSock = Listening->Accept(TEXT("PoseClient"));
        Listening->Close();
        Listening = nullptr;

        if (!ClientSock)
        {
            UE_LOG(LogTemp, Error, TEXT("[PoseTelemetry] Accept 失败"));
            return;
        }
        ClientSock->SetNonBlocking(false);
        UE_LOG(LogTemp, Log, TEXT("[PoseTelemetry] Python 已连接，开始推送位姿"));

        SendFuture = Async(EAsyncExecution::Thread, [this] { SendLoop(); });
    });
}

void UPoseTelemetryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bRunning = false;

    if (AcceptFuture.IsValid()) AcceptFuture.Wait();
    if (SendFuture.IsValid())   SendFuture.Wait();

    ISocketSubsystem* Sub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (ClientSock) { ClientSock->Close(); Sub->DestroySocket(ClientSock); ClientSock = nullptr; }
    if (Listening)  { Listening->Close();  Sub->DestroySocket(Listening);  Listening  = nullptr; }

    Super::EndPlay(EndPlayReason);
}

void UPoseTelemetryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 游戏线程：安全读取 owner 位姿并缓存供后台发送线程使用
    AActor* Owner = GetOwner();
    if (!Owner) return;

    FScopeLock Lock(&PoseMutex);
    CachedLoc = Owner->GetActorLocation();
    CachedRot = Owner->GetActorRotation();
}

void UPoseTelemetryComponent::SendLoop()
{
    const float Interval = (SendRateHz > 0.f) ? (1.f / SendRateHz) : 0.05f;

    while (bRunning && ClientSock)
    {
        FVector  Loc;
        FRotator Rot;
        {
            FScopeLock Lock(&PoseMutex);
            Loc = CachedLoc;
            Rot = CachedRot;
        }

        // 格式：Pose:x,y,z,pitch,yaw,roll\n
        FString Line = FString::Printf(
            TEXT("Pose:%.2f,%.2f,%.2f,%.4f,%.4f,%.4f\n"),
            Loc.X, Loc.Y, Loc.Z,
            Rot.Pitch, Rot.Yaw, Rot.Roll);

        FTCHARToUTF8 Utf8(*Line);
        int32 Sent = 0;
        if (!ClientSock->Send(
            reinterpret_cast<const uint8*>(Utf8.Get()),
            Utf8.Length(), Sent))
        {
            UE_LOG(LogTemp, Warning, TEXT("[PoseTelemetry] 发送失败，等待重连..."));
            break; // 连接断开，退出循环（AcceptFuture 会重新处理下一次连接）
        }

        FPlatformProcess::Sleep(Interval);
    }
}
