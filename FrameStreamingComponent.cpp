#include "FrameStreamingComponent.h"

UFrameStreamingComponent::UFrameStreamingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

// 游戏开始时建立 TCP 发送/接收连接
void UFrameStreamingComponent::BeginPlay()
{
    Super::BeginPlay();

    bRunning = true;

    // 打开发送/接收 Socket（TCP，阻塞模式）
    ISocketSubsystem* Sub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

    // 后台线程：主动连接 Python 发送端（SendPort）
    {
        ConnectFuture = Async(EAsyncExecution::Thread, [this, Sub]
            {
                FIPv4Address Addr;
                FIPv4Address::Parse(RemoteIP, Addr);
                FIPv4Endpoint Ep(Addr, SendPort);

                SendSock = Sub->CreateSocket(NAME_Stream, TEXT("SendSock"), false);
                SendSock->SetNonBlocking(false);

                while (bRunning && !SendSock->Connect(*Ep.ToInternetAddr()))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[FrameStreamer] 重试连接 %s:%d"), *RemoteIP, SendPort);
                    FPlatformProcess::Sleep(0.2f);
                }

                UE_LOG(LogTemp, Log, TEXT("[FrameStreamer] 发送通道已建立"));
            });
    }

    // 后台线程：监听并等待 Python 接收端回连（RecvPort）
    {
        AcceptFuture = Async(EAsyncExecution::Thread, [this, Sub]
            {
                FIPv4Endpoint ListenEP(FIPv4Address::Any, RecvPort);
                Listening = FTcpSocketBuilder(TEXT("RecvListener"))
                    .AsReusable()
                    .BoundToEndpoint(ListenEP)
                    .Listening(1);

                if (!Listening)
                {
                    UE_LOG(LogTemp, Error, TEXT("[FrameStreamer] 监听端口 %d 失败"), RecvPort);
                    return;
                }
                Listening->SetNonBlocking(false);
                UE_LOG(LogTemp, Log, TEXT("[FrameStreamer] 正在监听端口 %d ..."), RecvPort);

                // 轮询等待连接（每次等待 1ms，以便响应 bRunning 变化）
                bool bPending = false;
                while (bRunning && !bPending)
                {
                    Listening->HasPendingConnection(bPending);
                    FPlatformProcess::Sleep(0.001f);
                }
                if (!bRunning) { Listening->Close(); return; }

                RecvSock = Listening->Accept(TEXT("RecvSock"));
                Listening->Close();
                Listening = nullptr;

                if (!RecvSock)
                {
                    UE_LOG(LogTemp, Error, TEXT("[FrameStreamer] Accept 失败"));
                    return;
                }

                RecvSock->SetNonBlocking(false);
                UE_LOG(LogTemp, Log, TEXT("[FrameStreamer] 接收通道已建立"));

                // 连接建立后在同一线程中启动接收循环
                ReceiveLoop();
            });
    }
}

void UFrameStreamingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bRunning = false;
    if (ConnectFuture.IsValid()) ConnectFuture.Wait();
    if (AcceptFuture.IsValid())  AcceptFuture.Wait();
    if (RecvFuture.IsValid())    RecvFuture.Wait();

    ISocketSubsystem* Sub = ISocketSubsystem::Get();
    if (SendSock) { SendSock->Close();  Sub->DestroySocket(SendSock);  SendSock = nullptr; }
    if (RecvSock) { RecvSock->Close();  Sub->DestroySocket(RecvSock);  RecvSock = nullptr; }
    if (Listening) // 仅在 Listen 失败或 Editor 提前关闭时非空
    {
        Listening->Close();
        Sub->DestroySocket(Listening);
        Listening = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UFrameStreamingComponent::TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*)
{
    Super::TickComponent(Dt, LEVELTICK_TimeOnly, nullptr);
    if (!LeftRT || !SendSock) return;

    // 限制发送帧率为 15 FPS
    static float Acc = 0;
    Acc += Dt;
    if (Acc < 1.f / 15.f) return;
    Acc = 0;

    IImageWrapperModule& Mod = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

    // 左目：读取像素 → 压缩 JPEG → 发送
    FTextureRenderTargetResource* LeftRes = LeftRT->GameThread_GetRenderTargetResource();
    if (!LeftRes) return;
    TArray<FColor> LeftPx;
    LeftRes->ReadPixels(LeftPx);
    auto LeftWrap = Mod.CreateImageWrapper(EImageFormat::JPEG);
    LeftWrap->SetRaw(LeftPx.GetData(), LeftPx.Num() * 4, LeftRT->SizeX, LeftRT->SizeY, ERGBFormat::BGRA, 8);
    const TArray64<uint8>& LeftJpeg = LeftWrap->GetCompressed(75);

    int32 Sent = 0;
    uint32 NetLen = htonl(static_cast<uint32>(LeftJpeg.Num()));
    SendSock->Send(reinterpret_cast<uint8*>(&NetLen), 4, Sent);
    Sent = 0;
    SendSock->Send(LeftJpeg.GetData(), LeftJpeg.Num(), Sent);

    if (!RightRT) return; // 单目模式，发完左目即退出

    // 右目：读取像素 → 压缩 JPEG → 发送
    FTextureRenderTargetResource* RightRes = RightRT->GameThread_GetRenderTargetResource();
    if (!RightRes) return;
    TArray<FColor> RightPx;
    RightRes->ReadPixels(RightPx);
    auto RightWrap = Mod.CreateImageWrapper(EImageFormat::JPEG);
    RightWrap->SetRaw(RightPx.GetData(), RightPx.Num() * 4, RightRT->SizeX, RightRT->SizeY, ERGBFormat::BGRA, 8);
    const TArray64<uint8>& RightJpeg = RightWrap->GetCompressed(75);

    NetLen = htonl(static_cast<uint32>(RightJpeg.Num()));
    Sent = 0;
    SendSock->Send(reinterpret_cast<uint8*>(&NetLen), 4, Sent);
    Sent = 0;
    SendSock->Send(RightJpeg.GetData(), RightJpeg.Num(), Sent);
}

// -------------------- 接收线程 --------------------
void UFrameStreamingComponent::ReceiveLoop()
{
    while (bRunning && RecvSock)
    {
        uint32 LenBE = 0;
        int32 Got = 0;
        if (!RecvSock->Recv((uint8*)&LenBE, 4, Got, ESocketReceiveFlags::WaitAll))
        {
            FPlatformProcess::Sleep(0.001f);
            continue;
        }
        uint32 Len = ntohl(LenBE);
        TArray64<uint8> Buf;
        Buf.SetNumUninitialized(Len);
        if (!RecvSock->Recv(Buf.GetData(), Len, Got, ESocketReceiveFlags::WaitAll)) continue;

        AsyncTask(ENamedThreads::GameThread, [this, Arr = MoveTemp(Buf)]() mutable
            {
                UpdateTexture(Arr);
            });
    }
}

// -------------------- JPEG → 纹理 & UI --------------------
void UFrameStreamingComponent::UpdateTexture(const TArray64<uint8>& Jpeg)
{
    UE_LOG(LogTemp, Log, TEXT("UpdateTexture: %d bytes"), Jpeg.Num());
    IImageWrapperModule& ImgMod = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> Wrap = ImgMod.CreateImageWrapper(EImageFormat::JPEG);
    if (!Wrap->SetCompressed(Jpeg.GetData(), Jpeg.Num())) return;

    TArray<uint8> Raw;
    Wrap->GetRaw(ERGBFormat::BGRA, 8, Raw);

    const int32 W = Wrap->GetWidth(), H = Wrap->GetHeight();
    if (!ReceivedTex || ReceivedTex->GetSizeX() != W || ReceivedTex->GetSizeY() != H)
    {
        ReceivedTex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
        ReceivedTex->UpdateResource();
    }

    void* Mip = ReceivedTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Mip, Raw.GetData(), Raw.Num());
    ReceivedTex->GetPlatformData()->Mips[0].BulkData.Unlock();
    ReceivedTex->UpdateResource();

    // ReceivedTex 已更新，可供其他用途使用（如叠加显示）
    // 注意：不在此处更新 PIPImage，避免覆盖已绑定的本地摄像机实时画面
}
