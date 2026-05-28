#include "RotationActor.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Math/Vector.h"

// 构造函数，初始化各组件
ARotationActor::ARotationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    InterpSpeed = 1.0f;
    RotationInterpSpeed = 1.0f;

    // 胶囊体碰撞根节点：作为 RootComponent，sweep=true 时阻止鱼体穿墙
    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    CollisionCapsule->SetCapsuleHalfHeight(50.f);          // 鱼体长轴方向半高 50cm
    CollisionCapsule->SetCapsuleRadius(30.f);              // 鱼体截面半径 30cm
    CollisionCapsule->SetCollisionProfileName(TEXT("BlockAll"));
    RootComponent = CollisionCapsule;

    // 创建 actor 物理根节点；相机和 IMU 都基于该坐标系工作
    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRootComponent->SetupAttachment(CollisionCapsule);

    // 创建骨骼网格组件，仅负责视觉表现
    FishMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FishMesh"));
    FishMeshComponent->SetupAttachment(SceneRootComponent);
    FishMeshComponent->SetRelativeRotation(MeshRelativeRotation);
    FishMeshComponent->SetSimulatePhysics(false);                    // 禁用物理模拟
    FishMeshComponent->SetEnableGravity(false);                      // 禁用重力
    FishMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 禁用碰撞

    // 加载机器鱼骨骼网格资源
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> FishMeshAsset(TEXT("SkeletalMesh'/Game/Robotfish/fish2_0/Geometries/SKM_fish2_0_Mesh'"));
    if (FishMeshAsset.Succeeded())
    {
        FishMeshComponent->SetSkeletalMesh(FishMeshAsset.Object);
    }

    // 加载机器鱼动画序列
    static ConstructorHelpers::FObjectFinder<UAnimSequence> FishAnimAsset(TEXT("AnimSequence'/Game/Robotfish/fish2_0/Geometries/All_SK_fish2_0_Mesh_Sequence1'"));
    if (FishAnimAsset.Succeeded())
    {
        FishAnimation = FishAnimAsset.Object;
    }

    // 创建 IMU 组件并附加到根组件
    IMUComponent = CreateDefaultSubobject<UIMUComponent>(TEXT("IMUComponent"));
    IMUComponent->SetupAttachment(SceneRootComponent);

    // 创建双目 rig 根节点，使相机安装姿态与鱼模型外观修正解耦
    StereoRigRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StereoRigRoot"));
    StereoRigRoot->SetupAttachment(SceneRootComponent);
    StereoRigRoot->SetRelativeLocation(CameraRelativeLocation);
    StereoRigRoot->SetRelativeRotation(CameraRelativeRotation);

    // 创建左目摄像机
    LeftCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("LeftCamera"));
    LeftCameraComponent->SetupAttachment(StereoRigRoot);
    LeftCameraComponent->SetRelativeLocation(FVector::ZeroVector);
    LeftCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);

    // 创建左目场景捕获组件
    LeftSceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftSceneCapture"));
    LeftSceneCaptureComponent->SetupAttachment(LeftCameraComponent);
    LeftSceneCaptureComponent->FOVAngle = LeftCameraComponent->FieldOfView;

    // 创建右目摄像机
    RightCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("RightCamera"));
    RightCameraComponent->SetupAttachment(StereoRigRoot);
    RightCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, StereoBaseline));
    RightCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);

    // 创建右目场景捕获组件
    RightSceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RightSceneCapture"));
    RightSceneCaptureComponent->SetupAttachment(RightCameraComponent);
    RightSceneCaptureComponent->FOVAngle = RightCameraComponent->FieldOfView;

    // 创建帧流媒体组件（渲染目标在 BeginPlay 中创建）
    FrameStreamer = CreateDefaultSubobject<UFrameStreamingComponent>(TEXT("FrameStreamer"));

    // 创建位姿遥测组件（BeginPlay 时自动监听 :8991）
    PoseTelemetry = CreateDefaultSubobject<UPoseTelemetryComponent>(TEXT("PoseTelemetry"));
}

void ARotationActor::BeginPlay()
{
    // 渲染目标必须在 Super::BeginPlay()（即组件 BeginPlay）之前创建并绑定，
    // 否则 FrameStreamingComponent::BeginPlay 启动时 LeftRT 仍为 null
    LeftRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("LeftRenderTarget"));
    LeftRenderTarget->RenderTargetFormat = RTF_RGBA8;
    LeftRenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
    LeftRenderTarget->InitAutoFormat(1920, 1080);
    LeftRenderTarget->TargetGamma = 2.2f;
    LeftRenderTarget->UpdateResource();

    RightRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RightRenderTarget"));
    RightRenderTarget->RenderTargetFormat = RTF_RGBA8;
    RightRenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
    RightRenderTarget->InitAutoFormat(1920, 1080);
    RightRenderTarget->TargetGamma = 2.2f;
    RightRenderTarget->UpdateResource();

    // 提前绑定到 FrameStreamer，确保组件 BeginPlay 时已有效
    if (FrameStreamer)
    {
        FrameStreamer->LeftRT = LeftRenderTarget;
        FrameStreamer->RightRT = RightRenderTarget;
    }

    // 强制重置视觉和双目 rig 变换，防止 Blueprint 序列化值覆盖 C++ 默认值
    if (FishMeshComponent)
    {
        FishMeshComponent->SetRelativeLocation(FVector::ZeroVector);
        FishMeshComponent->SetRelativeRotation(MeshRelativeRotation);
    }
    if (StereoRigRoot)
    {
        StereoRigRoot->SetRelativeLocation(CameraRelativeLocation);
        StereoRigRoot->SetRelativeRotation(CameraRelativeRotation);
    }
    if (LeftCameraComponent)
    {
        LeftCameraComponent->SetRelativeLocation(FVector::ZeroVector);
        LeftCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
    }
    if (RightCameraComponent)
    {
        RightCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, StereoBaseline));
        RightCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
    }

    Super::BeginPlay();

    // 播放机器鱼动画（循环）
    if (FishAnimation && FishMeshComponent)
    {
        FishMeshComponent->PlayAnimation(FishAnimation, true);
    }

    // 创建并显示画中画 Widget
    if (PIPWidget && GetWorld())
    {
        // 如果是单机或主要客户端，获取 PlayerController
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PIPWidgetInstance = CreateWidget<UUserWidget>(PC, PIPWidget);
        }
        else
        {
            PIPWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), PIPWidget);
        }

        if (FrameStreamer) FrameStreamer->SetPIPWidget(PIPWidgetInstance);
        if (PIPWidgetInstance)
        {
            PIPWidgetInstance->AddToViewport(100); // 设置较高的 ZOrder 防止被其他 UI 遮挡
            PIPWidgetInstance->SetVisibility(ESlateVisibility::Visible);

            UE_LOG(LogTemp, Log, TEXT("[RotationActor] PIPWidgetInstance created and added to viewport."));

            // 将左目渲染目标直接绑定到 PIP 控件，无需 Python 连接即可显示摄像机画面
            UImage* PIPImage = Cast<UImage>(PIPWidgetInstance->GetWidgetFromName(TEXT("PIPImage")));
            if (PIPImage)
            {
                if (LeftRenderTarget)
                {
                    // 使用编辑器指定的 M_PIPCamera 材质（UI Domain + Opaque），绕过水下后处理写 alpha=0 问题
                    if (PIPCameraMaterial)
                    {
                        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PIPCameraMaterial, this);
                        MID->SetTextureParameterValue(TEXT("CameraTexture"), LeftRenderTarget);
                        PIPImage->SetBrushFromMaterial(MID);
                        UE_LOG(LogTemp, Log, TEXT("[RotationActor] Successfully bound LeftRenderTarget via M_PIPCamera MID."));
                    }
                    else
                    {
                        PIPImage->SetBrushResourceObject(LeftRenderTarget);
                        UE_LOG(LogTemp, Warning, TEXT("[RotationActor] PIPCameraMaterial not assigned in Details panel!"));
                    }
                    PIPImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));

                    // 固定 Canvas Slot 尺寸与位置
                    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PIPImage->Slot))
                    {
                        CanvasSlot->SetAutoSize(false);
                        CanvasSlot->SetSize(FVector2D(640.f, 360.f));
                        CanvasSlot->SetPosition(FVector2D(20.f, 20.f));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[RotationActor] LeftRenderTarget is null when trying to bind!"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[RotationActor] GetWidgetFromName('PIPImage') failed! Make sure the Image is named 'PIPImage' AND 'Is Variable' is CHECKED in the UMG editor."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[RotationActor] Failed to create PIPWidget instance."));
        }
    }
    else if (!PIPWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[RotationActor] PIPWidget 未赋值！请在编辑器 Details 面板中指定 Widget Blueprint 类。"));
    }


    // 设置左目场景捕获目标纹理
    if (LeftSceneCaptureComponent)
    {
        LeftSceneCaptureComponent->TextureTarget = LeftRenderTarget;
        // SCS_FinalColorLDR 会写 Alpha=1（SceneColorHDR 不写 Alpha，导致 RT Alpha=0 → Widget 透明）
        LeftSceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        LeftSceneCaptureComponent->bAlwaysPersistRenderingState = true;
    }

    // 运行时确认左目渲染目标绑定
    if (FrameStreamer && LeftRenderTarget)
    {
        FrameStreamer->LeftRT = LeftRenderTarget;
    }

    // 设置右目场景捕获目标纹理
    if (RightSceneCaptureComponent && RightRenderTarget)
    {
        RightSceneCaptureComponent->TextureTarget = RightRenderTarget;
        RightSceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        RightSceneCaptureComponent->bAlwaysPersistRenderingState = true;
    }

    // 运行时确认右目渲染目标绑定
    if (FrameStreamer && RightRenderTarget)
    {
        FrameStreamer->RightRT = RightRenderTarget;
    }

    // 初始化 OpenCV 背景减除器
    fgbg = cv::createBackgroundSubtractorMOG2(100, 8, false);

    LastTrailPos = GetActorLocation();
}

void ARotationActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 平滑移动到目标位置
    if (FVector::Dist(GetActorLocation(), TargetLocation) > 1.0f)
    {
        FVector NewLocation = FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaTime, InterpSpeed);
        SetActorLocation(NewLocation, true);  // sweep=true：碰到墙体自动停止，不穿墙
    }

    // 平滑旋转到目标角度
    if (!TargetRotation.Equals(GetActorRotation(), 1.0f))
    {
        FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, RotationInterpSpeed);
        SetActorRotation(NewRotation);
    }

    // 屏幕显示当前朝向（Key=1 保持覆盖同一行，Time=0 仅当前帧）
    if (IMUComponent && GEngine)
    {
        FVector   Acc = IMUComponent->Acceleration;
        FVector   Gyro = IMUComponent->AngularVelocity;
        FRotator  Ori = IMUComponent->Orientation;

        const FString DebugStr = FString::Printf(TEXT("Ori: %s"), *Ori.ToString());
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Green, DebugStr);
    }

    // 绘制运动轨迹
    if (bDrawTrail)
    {
        const FVector Current = GetActorLocation();
        if (FVector::DistSquared(Current, LastTrailPos) >= FMath::Square(TrailMinDistance))
        {
            DrawDebugLine(GetWorld(), LastTrailPos, Current, FColor::Cyan, true, 0.f, 0, 4.f);
            LastTrailPos = Current;
        }
    }
}

void ARotationActor::SetTargetPose(FVector NewLocation, FRotator NewRotation)
{
    TargetLocation = NewLocation;
    TargetRotation = NewRotation;
}

// 开始旋转
void ARotationActor::StartRotating()
{
    if (RotationComponent)
    {
        RotationComponent->SetRotationSpeed(45.f);
    }
}

// 停止旋转
void ARotationActor::StopRotating()
{
    if (RotationComponent)
    {
        RotationComponent->SetRotationSpeed(0.f);
    }
}
