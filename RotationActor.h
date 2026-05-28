#pragma once
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "RotationComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "IMUComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "HighResScreenshot.h"
#include "RHICommandList.h"
#include "PipelineStateCache.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/FileHelper.h"
#include "OpenCVHelper.h"
#include "opencv2/opencv.hpp"
#include "opencv2/core.hpp"
#include "opencv2/video.hpp"
#include "opencv2/imgproc.hpp"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DrawDebugHelpers.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "ImageUtils.h"
#include "FrameStreamingComponent.h"
#include "PoseTelemetryComponent.h"
#include "RotationActor.generated.h"

class UCameraComponent;
class USceneComponent;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UUserWidget;

UCLASS()
class QUICKSTART_API ARotationActor : public AActor
{
    GENERATED_BODY()

public:
    ARotationActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // 设置目标位姿（位置 + 旋转）
    void SetTargetPose(FVector NewLocation, FRotator NewRotation);

    UFUNCTION(BlueprintCallable, Category = "Rotation")
    void StartRotating();

    UFUNCTION(BlueprintCallable, Category = "Rotation")
    void StopRotating();

    // 旋转组件
    UPROPERTY(VisibleAnywhere, Category = "Rotation")
    URotationComponent* RotationComponent;

    // 静态网格组件
    UPROPERTY(VisibleAnywhere, Category = "Mesh")
    UStaticMeshComponent* StaticMeshComponent;

    // IMU 传感器组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensors")
    UIMUComponent* IMUComponent;

    // 帧流媒体组件（TCP 双目图像传输）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Streaming")
    UFrameStreamingComponent* FrameStreamer;

    // 位姿遥测组件（TCP :8991 → Python 20 Hz 位姿推送）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    UPoseTelemetryComponent* PoseTelemetry;

private:
    // 碰撞根节点：胶囊体，启用扫掠碰撞后鱼体不能穿墙
    UPROPERTY(VisibleAnywhere, Category = "Collision")
    UCapsuleComponent* CollisionCapsule;

    // Actor 物理/传感器坐标系根节点，不承载鱼模型的导入修正
    UPROPERTY(VisibleAnywhere)
    USceneComponent* SceneRootComponent;

    // 机器鱼骨骼网格组件（视觉表现）
    UPROPERTY(VisibleAnywhere)
    USkeletalMeshComponent* FishMeshComponent;

    // 鱼模型导入姿态修正，仅影响外观，不影响相机/IMU 几何
    UPROPERTY(EditAnywhere, Category = "Mesh")
    FRotator MeshRelativeRotation = FRotator(0.f, 180.f, 90.f);

    // 机器鱼动画序列
    UPROPERTY(EditDefaultsOnly)
    UAnimSequence* FishAnimation;

    UPROPERTY(EditAnywhere, Category = "Spawn Parameters") // 目标位置
        FVector TargetLocation = FVector(0.0f, 0.0f, 200.0f);

    UPROPERTY(EditAnywhere, Category = "Spawn Parameters") // 目标旋转（物理坐标系）
        FRotator TargetRotation = FRotator(0.0f, 0.0f, 0.0f);

    float InterpSpeed;         // 位移插值速度
    float RotationInterpSpeed; // 旋转插值速度

    // 双目 rig 相对 actor 的安装位置。
    // X=62cm：补偿实测 +0.62m 系统性深度偏差（摄像机在 actor origin 后方 62cm 处）。
    UPROPERTY(EditAnywhere, Category = "Spawn Parameters")
    FVector CameraRelativeLocation = FVector(62.f, 0.f, 0.f);

    // 双目 rig 相对 actor 的安装旋转；默认朝向鱼头前方
    UPROPERTY(EditAnywhere, Category = "Spawn Parameters")
    FRotator CameraRelativeRotation = FRotator(0.f, 0.f, 0.f);

    // 双目 rig 根节点，保证左右相机共用同一物理坐标系
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    USceneComponent* StereoRigRoot;

    // 左目摄像机（显示用）
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* LeftCameraComponent;

    // 左目场景捕获组件
    UPROPERTY(VisibleAnywhere, Category = "Capture")
    USceneCaptureComponent2D* LeftSceneCaptureComponent;

    // 左目渲染目标（1920×1080 RGBA8）
    UPROPERTY(VisibleAnywhere, Transient, Category = "Capture")
    UTextureRenderTarget2D* LeftRenderTarget;

    // 双目基线距离 (cm)
    UPROPERTY(EditAnywhere, Category = "Stereo")
    float StereoBaseline = 3.0f;

    // 右目摄像机
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* RightCameraComponent;

    // 右目场景捕获组件
    UPROPERTY(VisibleAnywhere, Category = "Capture")
    USceneCaptureComponent2D* RightSceneCaptureComponent;

    // 右目渲染目标（1920×1080 RGBA8）
    UPROPERTY(VisibleAnywhere, Transient, Category = "Capture")
    UTextureRenderTarget2D* RightRenderTarget;

    // 画中画 Widget 实例
    UPROPERTY(EditAnywhere)
    UUserWidget* PIPWidgetInstance = nullptr;

    // 画中画 Widget 类（用于显示目标检测结果）
    UPROPERTY(EditAnywhere)
    TSubclassOf<UUserWidget> PIPWidget;

    // PIP 显示材质（在编辑器 Details 面板指定 M_PIPCamera）
    UPROPERTY(EditAnywhere, Category = "Camera")
    UMaterialInterface* PIPCameraMaterial = nullptr;

    cv::Ptr<cv::BackgroundSubtractor> fgbg;

    float TimeSinceLastCapture = 0.0f; // 帧捕获计时器
    float TimeElapsed;                 // 累计运行时间

    /** 轨迹绘制相关 */
    UPROPERTY(EditAnywhere, Category = "Trail")
    bool bDrawTrail = false;          // 是否绘制运动轨迹

    UPROPERTY(EditAnywhere, Category = "Trail")
    float TrailMinDistance = 20.f;    // 相邻采样点最小间距 (cm)

    FVector LastTrailPos;             // 上一次记录轨迹的位置
};
