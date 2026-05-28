#include "RotationController.h"
#include "Engine/World.h"
#include "RotationGameMode.h"  

//void ARotationController::SetupInputComponent()
//{
//    Super::SetupInputComponent();
//
//    // 绑定 R 键按下事件，启动旋转
//    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ARotationController::StartRotation);
//
//    // 绑定 S 键按下事件，停止旋转
//    InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ARotationController::StopRotation);
//}

//void ARotationController::StartRotation()
//{
//    // 获取 GameMode 并启动旋转
//    ARotationGameMode* GameMode = GetWorld()->GetAuthGameMode<ARotationGameMode>();
//    if (GameMode)
//    {
//        GameMode->StartRotatingActor();  // 启动旋转
//    }
//}

//void ARotationController::StopRotation()
//{
//    // 获取 GameMode 并停止旋转
//    ARotationGameMode* GameMode = GetWorld()->GetAuthGameMode<ARotationGameMode>();
//    if (GameMode)
//    {
//        GameMode->StopRotatingActor();  // 停止旋转
//    }
//}