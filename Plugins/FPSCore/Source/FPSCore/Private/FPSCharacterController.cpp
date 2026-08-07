// Copyright 2022 Ellie Kelemen. All Rights Reserved.

#include "FPSCharacterController.h"

AFPSCharacterController::AFPSCharacterController()
{
	AmmoMap.Add(EAmmoType::Rifle, 1000000);
	AmmoMap.Add(EAmmoType::Pistol, 1000000);
	AmmoMap.Add(EAmmoType::Shotgun, 1000000);
	AmmoMap.Add(EAmmoType::Special, 1000000);
}