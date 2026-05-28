#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "PoseTelemetryComponent.generated.h"

/**
 * 位姿遥测组件：监听 TCP 端口，以 ~20 Hz 向 Python 推送 actor 位姿。
 * 协议（换行分隔）：Pose:<x>,<y>,<z>,<pitch>,<yaw>,<roll>\n
 * 单位：cm / 度；yaw 为 UE actor 原始值（Python 侧加 90° 还原 nav yaw）。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class QUICKSTART_API UPoseTelemetryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPoseTelemetryComponent();

    UPROPERTY(EditAnywhere, Category = "Telemetry")
    int32 ListenPort = 8991;

    UPROPERTY(EditAnywhere, Category = "Telemetry")
    float SendRateHz = 20.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    FSocket* Listening = nullptr;
    FSocket* ClientSock = nullptr;
    FThreadSafeBool bRunning = false;

    // 游戏线程写，后台线程读 —— 互斥保护
    FCriticalSection PoseMutex;
    FVector   CachedLoc  = FVector::ZeroVector;
    FRotator  CachedRot  = FRotator::ZeroRotator;

    void SendLoop();

    TFuture<void> AcceptFuture;
    TFuture<void> SendFuture;
};
