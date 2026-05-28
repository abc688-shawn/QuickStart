// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpectatorPawn.h"
#include "RotationActor.h"
#include "RotationGameMode.generated.h"


UCLASS()
class QUICKSTART_API ARotationGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARotationGameMode();

protected:
    virtual void BeginPlay() override;

    //public:
    //    void StartRotatingActor();  // 启动旋转
    //    void StopRotatingActor();   // 停止旋转

private:
    ARotationActor* RotatingActor;  // 存储生成的 RotationActor
};
