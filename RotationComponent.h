#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RotationComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class QUICKSTART_API URotationComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URotationComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//设置旋转速率
	UFUNCTION(BlueprintCallable, Category = "Rotation")
	void SetRotationSpeed(float NewRotationSpeed);

private:
	// 旋转速率（度/秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed;

	// 当前旋转角度
	FRotator CurrentRotation;
};
