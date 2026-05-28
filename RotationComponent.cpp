#include "RotationComponent.h"

URotationComponent::URotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	RotationSpeed = 30.f;  // 默认旋转速率为30度/秒
	CurrentRotation = FRotator::ZeroRotator;
}


void URotationComponent::BeginPlay()
{
	Super::BeginPlay();
}


void URotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 计算旋转增量
	float DeltaRotation = RotationSpeed * DeltaTime;

	// 更新当前旋转
	CurrentRotation.Yaw += DeltaRotation;  // 在Yaw轴上旋转

	//// 设置Actor的旋转
	//GetOwner()->SetActorRotation(CurrentRotation);
}

// 设置旋转速率
void URotationComponent::SetRotationSpeed(float NewRotationSpeed)
{
	RotationSpeed = NewRotationSpeed;
}

