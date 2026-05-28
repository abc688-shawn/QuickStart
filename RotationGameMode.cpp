#include "RotationGameMode.h"
#include "Engine/World.h"  
#include "RotationActor.h"  
#include "TCPServer.h"
#include "RotationController.h"  

ARotationGameMode::ARotationGameMode()
{
    PlayerControllerClass = ARotationController::StaticClass();
    // 替换默认带球体碰撞的 ADefaultPawn，避免其出现在摄像机视野内干扰深度感知
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void ARotationGameMode::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("ARotationGameMode::BeginPlay() is be used"));

    /*===========================================机器鱼不需要动态生成==================================================================
    // 在场景中生成 机器鱼
    if (GetWorld())
    {
        // 初始化机器鱼的位置和姿态
        FVector SpawnLocation(0.0f, 0.0f, 100.0f);
        FRotator SpawnRotation(0.0f, 0.0f, 0.0f);

        RotatingActor = GetWorld()->SpawnActor<ARotationActor>(ARotationActor::StaticClass(), SpawnLocation, SpawnRotation);

        if (RotatingActor)
        {
            UE_LOG(LogTemp, Log, TEXT("ARotationActor is spawned at location: %s"), *RotatingActor->GetActorLocation().ToString());
            UE_LOG(LogTemp, Log, TEXT("ARotationActor is spawned at rotation: %s"), *RotatingActor->GetActorRotation().ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn ARotationActor"));
        }
    }
    =====================================================================================================================*/

    // 在关卡中动态创建 ATCPServer 实例
    if (ATCPServer* TCPServer = GetWorld()->SpawnActor<ATCPServer>(ATCPServer::StaticClass()))
    {
        // 服务器启动后，它会在后台运行
        TCPServer->StartServer();
        UE_LOG(LogTemp, Log, TEXT("TCPServer started"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn ATCPServer"));
    }
}

//void ARotationGameMode::StartRotatingActor()
//{
//    if (RotatingActor && RotatingActor->RotationComponent)
//    {
//        RotatingActor->RotationComponent->SetRotationSpeed(45.f);  // 启动旋转
//    }
//}

//void ARotationGameMode::StopRotatingActor()
//{
//    if (RotatingActor && RotatingActor->RotationComponent)
//    {
//        RotatingActor->RotationComponent->SetRotationSpeed(0.f);  // 停止旋转
//    }
//}