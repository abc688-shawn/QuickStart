#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "IMUComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class QUICKSTART_API UIMUComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UIMUComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 获取到的加速度（本地或世界坐标，可根据需求调整）
    UPROPERTY(BlueprintReadOnly, Category = "IMU")
    FVector Acceleration;

    // 获取到的角速度（Euler角度变化率，或其他形式）
    UPROPERTY(BlueprintReadOnly, Category = "IMU")
    FVector AngularVelocity;

    // 当前的朝向（旋转）
    UPROPERTY(BlueprintReadOnly, Category = "IMU")
    FRotator Orientation;

    // 噪声参数
    UPROPERTY(EditAnywhere, Category = "IMU Noise")
    FVector AccelerationNoiseStdDev = FVector(0.05f, 0.05f, 0.05f);

    UPROPERTY(EditAnywhere, Category = "IMU Noise")
    FVector AngularVelocityNoiseStdDev = FVector(0.1f, 0.1f, 0.1f);

    UPROPERTY(EditAnywhere, Category = "IMU Noise")
    FVector OrientationNoiseStdDev = FVector(0.05f, 0.05f, 0.05f);

private:
    // 用于计算加速度的上帧速度和旋转
    FVector LastVelocityUE;
    FRotator LastRotationUE;

    // 将UE向量转换到鱼坐标系的实用函数
    FVector ToFishCoords_Linear(const FVector& UEVec) const;

    // 将UE的绝对旋转转换到鱼坐标系 (带偏移的Pitch/Yaw/Roll)
    FRotator ToFishCoords_Orientation(const FRotator& UERot) const;

    // 将UE的旋转增量(Delta Euler) 视为向量并映射到鱼坐标(不含偏移，只调轴向)
    FVector ToFishCoords_Angular(const FVector& UEAngular) const;

    // 高斯噪声生成器
    float GaussianNoise(float StdDev);
    static bool bGaussianGenerate;
    static float GaussianU1;
    static float GaussianU2;
};