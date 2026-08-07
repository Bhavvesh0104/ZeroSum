// Copyright 2022 Ellie Kelemen. All Rights Reserved.

#include "Components/InventoryComponent.h"
#include "EnhancedInputComponent.h"
#include "FPSCharacter.h"
#include "FPSCharacterController.h"
#include "WeaponBase.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	
	CurrentWeaponSlot = 0;
	TargetWeaponSlot = 0;
	bPerformingWeaponSwap = false;
}

// Swapping weapons with the scroll wheel
void UInventoryComponent::SwapToPrimary() { LocalSwap(0); }
void UInventoryComponent::SwapToSecondary() { LocalSwap(1); }

void UInventoryComponent::LocalSwap(int SlotId)
{
	if (CurrentWeaponSlot == SlotId || bPerformingWeaponSwap) return;
	
	// Allow Client to request swap even if weapon isn't mapped locally yet (fixes frozen Client)
	if (GetOwner()->HasAuthority() && !EquippedWeapons.Contains(SlotId)) return;

	bPerformingWeaponSwap = true;
	TargetWeaponSlot = SlotId;
	float UnequipTime = 0.2f;

	if (CurrentWeapon)
	{
		CurrentWeapon->CancelReload();
		CurrentWeapon->StopFire();
		CurrentWeapon->SetCanFire(false);
		CurrentWeapon->Client_StopFire();

		AFPSCharacter* Player = Cast<AFPSCharacter>(GetOwner());
		if (Player && Player->IsLocallyControlled())
		{
			if (UAnimInstance* AnimInst = Player->GetHandsMesh()->GetAnimInstance())
			{
				if (UAnimMontage* UnequipMontage = CurrentWeapon->GetStaticWeaponData()->FP_WeaponUnequip)
				{
					AnimInst->Montage_Play(UnequipMontage, 1.0f);
					UnequipTime = FMath::Max(0.1f, UnequipMontage->GetPlayLength() - (UnequipMontage->BlendOut.GetBlendTime() + 0.08f));
				}
			}
		}
	}

	if (GetOwner()->HasAuthority())
	{
		SwapWeapon(SlotId);
	}
	else
	{
		Server_SwapWeapon(SlotId);
		// Client strictly predicts the unequip delay
		GetWorld()->GetTimerManager().ClearTimer(WeaponSwapTimerHandle);
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UInventoryComponent::UnequipReturn);
		GetWorld()->GetTimerManager().SetTimer(WeaponSwapTimerHandle, TimerDelegate, UnequipTime, false);
	}
}

void UInventoryComponent::Server_UpdateTargetWeaponSlot_Implementation(const int SlotId)
{
	TargetWeaponSlot = SlotId;
}

void UInventoryComponent::ScrollWeapon(const FInputActionValue &Value)
{
	int NewID;

	if (Value[0] < 0)
	{
		NewID = FMath::Clamp(CurrentWeaponSlot + 1, 0, NumberOfWeaponSlots - 1);
		if (CurrentWeaponSlot == NumberOfWeaponSlots - 1) NewID = 0;
	}
	else
	{
		NewID = FMath::Clamp(CurrentWeaponSlot - 1, 0, NumberOfWeaponSlots - 1);
		if (CurrentWeaponSlot == 0) NewID = NumberOfWeaponSlots - 1;
	}

	if (bPerformingWeaponSwap && WeaponSwapBehaviour == EWeaponSwapBehaviour::UseNewValue)
	{
		TargetWeaponSlot = NewID;
		if (!GetOwner()->HasAuthority()) Server_UpdateTargetWeaponSlot(NewID);
	}
	else if (!bPerformingWeaponSwap)
	{
		LocalSwap(NewID);
	}
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInventoryComponent::InitializeLoadout(TSubclassOf<AWeaponBase> PrimaryClass, TSubclassOf<AWeaponBase> SecondaryClass)
{
	if (!GetOwner()->HasAuthority()) return;

	if (PrimaryClass)
	{
		SpawnWeapon(PrimaryClass, 0);
	}
	if (SecondaryClass)
	{
		SpawnWeapon(SecondaryClass, 1);
	}

	// Force the primary weapon to be actively equipped after both are spawned
	if (EquippedWeapons.Contains(0))
	{
		// Explicitly disable the equip animation boolean to prevent AnimBP initialization errors on spawn
		UpdateWeapon(EquippedWeapons[0], 0, false);
	}
}

void UInventoryComponent::Server_SwapWeapon_Implementation(const int SlotId)
{
	SwapWeapon(SlotId);
}

void UInventoryComponent::SwapWeapon(const int SlotId)
{
	if (!GetOwner()->HasAuthority()) return;
	if (!EquippedWeapons.Contains(SlotId)) return;

	bPerformingWeaponSwap = true;
	TargetWeaponSlot = SlotId;
	float UnequipTime = 0.2f;

	if (CurrentWeapon)
	{
		CurrentWeapon->CancelReload();
		CurrentWeapon->StopFire();
		CurrentWeapon->SetCanFire(false);
		CurrentWeapon->Client_StopFire();

		CurrentWeapon->Multi_UnequipWeaponAnim();

		if (UAnimMontage* UnequipMontage = CurrentWeapon->GetStaticWeaponData()->FP_WeaponUnequip)
		{
			UnequipTime = FMath::Max(0.1f, UnequipMontage->GetPlayLength() - (UnequipMontage->BlendOut.GetBlendTime() + 0.08f));
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(WeaponSwapTimerHandle);
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UInventoryComponent::UnequipReturn);
	GetWorld()->GetTimerManager().SetTimer(WeaponSwapTimerHandle, TimerDelegate, UnequipTime, false);
}

void UInventoryComponent::SpawnWeapon(TSubclassOf<AWeaponBase> NewWeapon, const int InventoryPosition)
{
	AFPSCharacter* CurrentPlayer = Cast<AFPSCharacter>(GetOwner());
	if (!CurrentPlayer || !CurrentPlayer->HasAuthority()) return;

	if (EquippedWeapons.Contains(InventoryPosition) && EquippedWeapons[InventoryPosition])
	{
		if (CurrentWeapon == EquippedWeapons[InventoryPosition])
		{
			CurrentWeapon = nullptr;
			CurrentWeaponSlot = -1;
		}
		
		EquippedWeapons[InventoryPosition]->Destroy();
		EquippedWeapons.Remove(InventoryPosition);
	}

	AWeaponBase* SpawnedWeapon = GetWorld()->SpawnActorDeferred<AWeaponBase>(NewWeapon, FTransform::Identity, CurrentPlayer, CurrentPlayer, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (SpawnedWeapon)
	{
		SpawnedWeapon->FinishSpawning(FTransform::Identity);
		EquippedWeapons.Add(InventoryPosition, SpawnedWeapon);
		
		// Explicitly hide secondary weapons on spawn so they do not ghost in the Host's hands
		if (CurrentWeaponSlot != InventoryPosition)
		{
			SpawnedWeapon->PrimaryActorTick.bCanEverTick = false;
			SpawnedWeapon->SetActorHiddenInGame(true);
		}
	}
}

// Spawns a new weapon 
void UInventoryComponent::UpdateWeapon(AWeaponBase *SpawnedWeapon, const int InventoryPosition, bool bPlayAnim)
{
	AFPSCharacter *CurrentPlayer = Cast<AFPSCharacter>(GetOwner());
	if (CurrentPlayer == this->GetOwner())
	{
		if (CurrentWeapon)
		{
			CurrentWeapon->PrimaryActorTick.bCanEverTick = false;
			CurrentWeapon->SetActorHiddenInGame(true);
			CurrentWeapon->Client_StopFire();
		}

		CurrentWeapon = EquippedWeapons[InventoryPosition];
		CurrentWeaponSlot = InventoryPosition;

		if (CurrentWeapon)
		{
			CurrentWeapon->PrimaryActorTick.bCanEverTick = true;
			CurrentWeapon->SetActorHiddenInGame(false);

			if (CurrentPlayer && bPlayAnim)
			{
				if (CurrentPlayer->IsLocallyControlled() && CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip)
				{
					CurrentPlayer->GetHandsMesh()->GetAnimInstance()->Montage_Play(CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip, 1.0f);
				}
				else if (!CurrentPlayer->IsLocallyControlled() && CurrentWeapon->GetStaticWeaponData()->TP_WeaponEquip)
				{
					CurrentPlayer->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(CurrentWeapon->GetStaticWeaponData()->TP_WeaponEquip, 1.0f);
				}
				CurrentPlayer->UpdateMovementState(CurrentPlayer->GetMovementState());
			}
		}
	}
}

FText UInventoryComponent::GetCurrentWeaponRemainingAmmo() const
{
	if (const AFPSCharacter *FPSCharacter = Cast<AFPSCharacter>(GetOwner()))
	{
		AFPSCharacterController *CharacterController = Cast<AFPSCharacterController>(FPSCharacter->GetController());
		if (CharacterController)
		{
			if (CurrentWeapon != nullptr)
			{
				return FText::AsNumber(CharacterController->AmmoMap[CurrentWeapon->GetRuntimeWeaponData()->AmmoType]);
			}
			UE_LOG(LogProfilingDebugging, Log, TEXT("Cannot find Current Weapon"));
			return FText::AsNumber(0);
		}
		UE_LOG(LogProfilingDebugging, Error, TEXT("No character controller found in UInventoryComponent"))
		return FText::FromString("Err");
	}
	UE_LOG(LogProfilingDebugging, Error, TEXT("No player character found in UInventoryComponent"))
	return FText::FromString("Err");
}

void UInventoryComponent::Inspect()
{
	if (CurrentWeapon)
	{
		if (const AFPSCharacter *FPSCharacter = Cast<AFPSCharacter>(GetOwner()))
		{
			if (CurrentWeapon->GetStaticWeaponData()->HandsInspect)
			{
				FPSCharacter->GetHandsMesh()->GetAnimInstance()->Montage_Play(CurrentWeapon->GetStaticWeaponData()->HandsInspect, 1.0f);
			}
			if (CurrentWeapon->GetStaticWeaponData()->WeaponInspect)
			{
				CurrentWeapon->GetMainMeshComp()->PlayAnimation(CurrentWeapon->GetStaticWeaponData()->WeaponInspect, false);
			}
		}
	}
}

void UInventoryComponent::UnequipReturn()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->PrimaryActorTick.bCanEverTick = false;
		CurrentWeapon->SetActorHiddenInGame(true);
	}

	CurrentWeaponSlot = TargetWeaponSlot;
	// Safely retrieve weapon or assign null if Client has not mapped it via replication yet
	CurrentWeapon = EquippedWeapons.FindRef(TargetWeaponSlot);

	float EquipTime = 0.2f;

	if (CurrentWeapon)
	{
		CurrentWeapon->PrimaryActorTick.bCanEverTick = true;
		CurrentWeapon->SetActorHiddenInGame(false);
		CurrentWeapon->SetTPAttachment();

		if (CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip)
		{
			EquipTime = CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip->GetPlayLength();
		}

		if (AFPSCharacter* Player = Cast<AFPSCharacter>(GetOwner()))
		{
			Player->UpdateMovementState(Player->GetMovementState());

			if (Player->IsLocallyControlled())
			{
				if (UAnimInstance* AnimInst = Player->GetHandsMesh()->GetAnimInstance())
				{
					if (UAnimMontage* EquipMontage = CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip)
					{
						AnimInst->Montage_Play(EquipMontage, 1.0f);
					}
				}
			}
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(WeaponSwapTimerHandle);
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UInventoryComponent::EquipReturn);
	GetWorld()->GetTimerManager().SetTimer(WeaponSwapTimerHandle, TimerDelegate, EquipTime, false);
}

void UInventoryComponent::EquipReturn()
{
	bPerformingWeaponSwap = false;

	if (AFPSCharacter* Player = Cast<AFPSCharacter>(GetOwner()))
	{
		if (CurrentWeapon)
		{
			CurrentWeapon->SetCanFire(Player->CanFireInCurrentState());
		}

		if (Player->IsLocallyControlled())
		{
			Player->ResumeFireInput();
		}
		else
		{
			Player->Client_FinishWeaponSwap();
		}
	}
}

void UInventoryComponent::SetupInputComponent(UEnhancedInputComponent *PlayerInputComponent)
{
	if (PrimaryWeaponAction)
	{
		PlayerInputComponent->BindAction(PrimaryWeaponAction, ETriggerEvent::Started, this, &UInventoryComponent::SwapToPrimary);
	}

	if (SecondaryWeaponAction)
	{
		PlayerInputComponent->BindAction(SecondaryWeaponAction, ETriggerEvent::Started, this, &UInventoryComponent::SwapToSecondary);
	}

	if (ScrollAction)
	{
		// Scrolling through weapons
		PlayerInputComponent->BindAction(ScrollAction, ETriggerEvent::Started, this, &UInventoryComponent::ScrollWeapon);
	}

	if (InspectWeaponAction)
	{
		// Playing the inspect animation
		PlayerInputComponent->BindAction(InspectWeaponAction, ETriggerEvent::Started, this, &UInventoryComponent::Inspect);
	}
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Register the variable to sync from Server to Client
    DOREPLIFETIME(UInventoryComponent, CurrentWeapon);
}

void UInventoryComponent::OnRep_CurrentWeapon()
{
    if (CurrentWeapon)
    {
        EquippedWeapons.Add(CurrentWeaponSlot, CurrentWeapon);

        CurrentWeapon->PrimaryActorTick.bCanEverTick = true;
        CurrentWeapon->SetActorHiddenInGame(false);
        CurrentWeapon->SetTPAttachment();

        AFPSCharacter* CurrentPlayer = Cast<AFPSCharacter>(GetOwner());
        if (CurrentPlayer)
        {
            if (!CurrentPlayer->IsLocallyControlled())
            {
                if (CurrentWeapon->GetStaticWeaponData()->TP_WeaponEquip && CurrentPlayer->GetThirdPersonMesh()->GetAnimInstance())
                {
                    CurrentPlayer->GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(CurrentWeapon->GetStaticWeaponData()->TP_WeaponEquip, 1.0f);
                }
            }
            else 
            {
                // If the local swap dropped CurrentWeapon to null waiting for this RPC, play the Equip anim now
                if (CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip && CurrentPlayer->GetHandsMesh()->GetAnimInstance())
                {
                    CurrentPlayer->GetHandsMesh()->GetAnimInstance()->Montage_Play(CurrentWeapon->GetStaticWeaponData()->FP_WeaponEquip, 1.0f);
                }
            }
            CurrentPlayer->UpdateMovementState(CurrentPlayer->GetMovementState());
        }
    }
}
