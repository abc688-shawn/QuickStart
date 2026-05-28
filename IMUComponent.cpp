#include "IMUComponent.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/Actor.h"

// 初始化静态变量
bool UIMUComponent::bGaussianGenerate = false;
float UIMUComponent::GaussianU1 = 0.0f;
float UIMUComponent::GaussianU2 = 0.0f;


UIMUComponent::UIMUComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}


void UIMUComponent::BeginPlay()
{
    Super::BeginPlay();

    // 初始化
    LastVelocityUE = FVector::ZeroVector;
    LastRotationUE = GetOwner()->GetActorRotation();
}

void UIMUComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 获取拥有该组件的Actor
    AActor* Owner = GetOwner();
    if (!Owner || DeltaTime <= 0.f) return;

    // 1) 计算速度、加速度
    FVector CurrentVelocityUE = Owner->GetVelocity();
    FVector AccelerationUE = (CurrentVelocityUE - LastVelocityUE) / DeltaTime;
    LastVelocityUE = CurrentVelocityUE;

    // 2) 计算角速度
    FRotator CurrentRotationUE = Owner->GetActorRotation();
    // 先将它们转成欧拉角(度)
    FVector CurrentEulerUE = CurrentRotationUE.Euler();   // (pitchUE, yawUE, rollUE)
    FVector LastEulerUE = LastRotationUE.Euler();
    FVector DeltaEulerUE = (CurrentEulerUE - LastEulerUE);

    FVector AngularVelocityUE = DeltaEulerUE / DeltaTime; // (度/秒)
    LastRotationUE = CurrentRotationUE;

    // 把UE计算结果 -> 转换到“鱼坐标系”
    // 加速度(线性向量)
    Acceleration = ToFishCoords_Linear(AccelerationUE);

    // 角速度(欧拉角差分向量)
    AngularVelocity = ToFishCoords_Angular(AngularVelocityUE);

    // 姿态(绝对旋转)
    Orientation = ToFishCoords_Orientation(CurrentRotationUE);

    // 这里可以再对 Acceleration、AngularVelocity 加各种滤波/噪声模拟
    // 应用高斯噪声
    // 加速度噪声
    Acceleration.X += GaussianNoise(AccelerationNoiseStdDev.X);
    Acceleration.Y += GaussianNoise(AccelerationNoiseStdDev.Y);
    Acceleration.Z += GaussianNoise(AccelerationNoiseStdDev.Z);

    // 角速度噪声
    AngularVelocity.X += GaussianNoise(AngularVelocityNoiseStdDev.X);
    AngularVelocity.Y += GaussianNoise(AngularVelocityNoiseStdDev.Y);
    AngularVelocity.Z += GaussianNoise(AngularVelocityNoiseStdDev.Z);

    // 姿态噪声
    FRotator NoisyOrientation = Orientation;
    NoisyOrientation.Pitch += GaussianNoise(OrientationNoiseStdDev.X);
    NoisyOrientation.Yaw += GaussianNoise(OrientationNoiseStdDev.Y);
    NoisyOrientation.Roll += GaussianNoise(OrientationNoiseStdDev.Z);
    Orientation = NoisyOrientation.GetNormalized();
}

float UIMUComponent::GaussianNoise(float StdDev)
{
    if (StdDev <= 0.0f) return 0.0f;

    if (bGaussianGenerate) {
        bGaussianGenerate = false;
        return FMath::Sqrt(-2.0f * FMath::Loge(GaussianU1)) * FMath::Cos(2.0f * PI * GaussianU2) * StdDev;
    }

    float U1, U2;
    do { U1 = FMath::FRand(); } while (U1 <= 1e-6f);
    U2 = FMath::FRand();

    float Z0 = FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Sin(2.0f * PI * U2);
    GaussianU1 = U1;
    GaussianU2 = U2;
    bGaussianGenerate = true;

    return Z0 * StdDev;
}


// 坐标转换
FVector UIMUComponent::ToFishCoords_Linear(const FVector& UEVec) const
{
    return FVector(-UEVec.Y, UEVec.X, UEVec.Z);
}

FRotator UIMUComponent::ToFishCoords_Orientation(const FRotator& UERot) const
{
    float Pitch_fish = UERot.Pitch;
    float Yaw_fish = UERot.Yaw + 90.f;
    float Roll_fish = -UERot.Roll + 90.f;

    return FRotator(Pitch_fish, Yaw_fish, Roll_fish);
}

// 对角速度(ΔEuler/Δt)，只需对轴做交换 + 取负，不加 ±90 偏移（那是绝对旋转的偏移）
FVector UIMUComponent::ToFishCoords_Angular(const FVector& UEAngular) const
{
    return FVector(-UEAngular.Y, UEAngular.X, UEAngular.Z);
}