// Copyright 2022 Ellie Kelemen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "GameFramework/PlayerController.h"
#include "FPSCharacterController.generated.h"

class FPSCORE_API AWeaponBase;

UCLASS()
class AFPSCharacterController : public APlayerController
{
	GENERATED_BODY()

public:
	AFPSCharacterController();

	// Ammo handling removed for Infinite Reserve Ammo Refactor
};
