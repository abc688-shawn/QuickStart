// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RotationGameMode.h"  // 引用 RotationGameMode 类
#include "RotationController.generated.h"

UCLASS()
class QUICKSTART_API ARotationController : public APlayerController
{
	GENERATED_BODY()

	//protected:
	//    virtual void SetupInputComponent() override;
	//
	//private:
	//    void StartRotation();
	//    void StopRotation();
};
