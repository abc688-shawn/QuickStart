#pragma once

#include "CoreMinimal.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Components/ActorComponent.h"
#include "Components/Image.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Blueprint/UserWidget.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "FrameStreamingComponent.generated.h"

class UTextureRenderTarget2D;
class UTexture2D;
class UUserWidget;
class UImage;

/**
 * 双目图像 TCP 流媒体组件。
 * 以 15 FPS 将左目（LeftRT）和右目（RightRT）帧压缩为 JPEG 后通过 TCP 发送给 Python。
 * 同时在 RecvPort 上监听 Python 回传的处理结果，更新画中画控件。
 *
 * TCP 双目帧格式（大端序）：
 *   [4B left_size][left JPEG][4B right_size][right JPEG]
 * 若 RightRT 为空，则仅发送左目单帧：[4B left_size][left JPEG]
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class QUICKSTART_API UFrameStreamingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFrameStreamingComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere) // Python 端 IP 地址
        FString RemoteIP = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere) // 发送端口（UE → Python 原始帧）
        int32 SendPort = 8989;

    UPROPERTY(EditAnywhere) // 接收端口（Python → UE 处理结果）
        int32 RecvPort = 8990;

    UPROPERTY(Transient) // 左目渲染目标（运行时绑定）
        UTextureRenderTarget2D* LeftRT = nullptr;

    UPROPERTY(Transient) // 右目渲染目标（运行时绑定）
        UTextureRenderTarget2D* RightRT = nullptr;

    // 设置画中画 Widget（用于显示 Python 回传的处理结果）
    void SetPIPWidget(UUserWidget* InWidget)
    {
        PIPWidgetInstance = InWidget;
        Img.Reset();
    }

private:
    FSocket* SendSock = nullptr; // 发送 Socket（连接 Python SendPort）
    FSocket* RecvSock = nullptr; // 接收 Socket（接受 Python 回连 RecvPort）
    FSocket* Listening = nullptr; // 监听 Socket（等待 Python 接收端回连）
    FThreadSafeBool bRunning = false;

    UPROPERTY()
    TWeakObjectPtr<UUserWidget> PIPWidgetInstance;

    UPROPERTY()
    UTexture2D* ReceivedTex = nullptr; // Python 回传图像的纹理对象

    UPROPERTY()
    TWeakObjectPtr<UImage> Img; // PIP 控件中的图像控件引用

    // 后台线程：循环接收 Python 回传数据
    void ReceiveLoop();

    // 游戏线程：将 JPEG 数据解码为 UTexture2D 并更新 PIP 控件
    void UpdateTexture(const TArray64<uint8>& Jpeg);

    TFuture<void> RecvFuture;
    TFuture<void> AcceptFuture;
    TFuture<void> ConnectFuture;
};
