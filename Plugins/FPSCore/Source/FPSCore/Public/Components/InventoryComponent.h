// Copyright 2022 Ellie Kelemen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "WeaponBase.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Animation/AnimTypes.h"
#include "InventoryComponent.generated.h"

class UCameraComponent;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FHitActor, UInventoryComponent, EventHitActor, FHitResult, HitResult);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FFailedToReload, UInventoryComponent, EventFailedToReload);

UENUM(BlueprintType)
enum class EReloadFailedBehaviour : uint8
{
	Retry UMETA(DisplayName = "Retry until successful"),
	ChangeState UMETA(DisplayName = "Change movement state to be able to successfuly reload"),
	HandleInBP UMETA(DisplayName = "Handle in Blueprint"),
	Ignore UMETA(DisplayName = "Ignore unsuccessful reload")
};

UENUM(BlueprintType)
enum class EWeaponSwapBehaviour : uint8
{
	UseNewValue UMETA(DisplayName = "Swap to new value"),
	Ignore UMETA(DisplayName = "Ignore subsequent swaps")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPSCORE_API UInventoryComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Sets default values for this component's properties */
	UInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called to bind functionality to input */
	void SetupInputComponent(class UEnhancedInputComponent *PlayerInputComponent);

	// Spawning a new weapon directly into an inventory slot
	void SpawnWeapon(TSubclassOf<AWeaponBase> NewWeapon, const int InventoryPosition);

	/** Equipping a new weapon */
	void UpdateWeapon(AWeaponBase *SpawnedWeapon, const int InventoryPosition, bool bPlayAnim = true);

	// Forces immediate local synchronization of the weapon pointer before OnRep arrives
	void ForceCurrentWeapon(AWeaponBase* NewWeapon) { CurrentWeapon = NewWeapon; }

	void SetPerformingWeaponSwap(bool bIsSwapping) { bPerformingWeaponSwap = bIsSwapping; }

	/** Returns the number of weapon slots */
	int GetNumberOfWeaponSlots() const { return NumberOfWeaponSlots; }

	/** Returns the currently equipped weapon slot */
	int GetCurrentWeaponSlot() const { return CurrentWeaponSlot; }
	void SetCurrentWeaponSlot(int NewSlot) { CurrentWeaponSlot = NewSlot; }

	/** Returns the map of currently equipped weapons */
	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	TMap<int, AWeaponBase *> GetEquippedWeapons() const { return EquippedWeapons; }

	/** Returns an equipped weapon
	 *	@param WeaponID The ID of the weapon to get
	 *	@return The weapon with the given ID
	 */
	AWeaponBase *GetWeaponByID(const int WeaponID) const { return EquippedWeapons[WeaponID]; }

	/** Returns the current weapon equipped by the player */
	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	AWeaponBase *GetCurrentWeapon() const { return CurrentWeapon; }

	/**  Returns the amount of ammunition currently loaded into the weapon */
	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	FText GetCurrentWeaponLoadedAmmo() const
	{
		if (CurrentWeapon != nullptr)
		{
			return FText::AsNumber(CurrentWeapon->GetRuntimeWeaponData()->ClipSize);
		}
		UE_LOG(LogProfilingDebugging, Log, TEXT("Cannot find Current Weapon"));
		return FText::FromString("0");
	}

	/** Returns the amount of ammunition remaining for the current weapon */
	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	FText GetCurrentWeaponRemainingAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	FName GetCurrentWeaponDisplayName() const
	{
		if (CurrentWeapon != nullptr)
		{
			return CurrentWeapon->GetStaticWeaponData()->WeaponName;
		}
		UE_LOG(LogProfilingDebugging, Log, TEXT("Cannot find Current Weapon"));
		return TEXT("Currentweapon is nullptr!");
	}

	UFUNCTION(BlueprintCallable, Category = "Inventory Component")
	UTexture2D *GetCurrentWeaponDisplayImage() const
	{
		if (CurrentWeapon != nullptr)
		{
			return CurrentWeapon->GetStaticWeaponData()->WeaponIcon;
		}
		UE_LOG(LogProfilingDebugging, Log, TEXT("Cannot find Current Weapon"));
		return nullptr;
	}

	UPROPERTY(BlueprintAssignable, Category = "Inventory Component")
	FHitActor EventHitActor;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Component")
	FFailedToReload EventFailedToReload;

	/** The input actions implemented by this component */
	UPROPERTY()
	UInputAction *FiringAction;

	UPROPERTY()
	UInputAction *PrimaryWeaponAction;

	UPROPERTY()
	UInputAction *SecondaryWeaponAction;

	UPROPERTY()
	UInputAction *ReloadAction;

	UPROPERTY()
	UInputAction *ScrollAction;

	UPROPERTY()
	UInputAction *InspectWeaponAction;

	/** Reloads the weapon */
	void Reload();

	// Server authoritative loadout injection
	void InitializeLoadout(TSubclassOf<AWeaponBase> PrimaryClass, TSubclassOf<AWeaponBase> SecondaryClass);

	void UnequipReturn();
	void EquipReturn();

private:
	/** Spawns starter weapons */
	virtual void BeginPlay() override;

	/** Called automatically on the Client when the Server changes the weapon */
    UFUNCTION()
    void OnRep_CurrentWeapon();

    /** The player's currently equipped weapon */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
    AWeaponBase *CurrentWeapon;
	
	void SwapWeapon(int SlotId);

	void LocalSwap(int SlotId);
	void SwapToPrimary();
	void SwapToSecondary();

	UFUNCTION(Server, Reliable)
	void Server_UpdateTargetWeaponSlot(int SlotId);

	/** Swaps between weapons using the scroll wheel */
	void ScrollWeapon(const FInputActionValue &Value);

	/** Plays an inspect animation on the weapon */
	void Inspect();

	/** RPC of the stop fire function */
	UFUNCTION(Server, Reliable)
	void Server_SwapWeapon(const int SlotId);
	void Server_SwapWeapon_Implementation(const int SlotId);

	/** The distance at which pickups for old weapons spawn during a weapon swap */
	UPROPERTY(EditDefaultsOnly, Category = "Camera | Interaction")
	float WeaponSpawnDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapons | Behaviour")
	EReloadFailedBehaviour ReloadFailedBehaviour = EReloadFailedBehaviour::Ignore;

	UPROPERTY(EditDefaultsOnly, Category = "Weapons | Behaviour")
	EWeaponSwapBehaviour WeaponSwapBehaviour = EWeaponSwapBehaviour::UseNewValue;

	/** The integer that keeps track of which weapon slot ID is currently active */
	int CurrentWeaponSlot;

	/** The integer that keeps track of which weapon slot ID we are aiming to switch to while waiting for the unequip animation to play */
	int TargetWeaponSlot;

	bool bPerformingWeaponSwap;

	FTimerHandle ReloadRetry;
	FTimerHandle WeaponSwapTimerHandle;

public:
	// Returns the current swap state for animation and aiming interruption logic
	bool IsPerformingWeaponSwap() const { return bPerformingWeaponSwap; }

public:
	/** THe Number of slots for Weapons that this player has */
	UPROPERTY(EditDefaultsOnly, Category = "Weapons | Inventory")
	int NumberOfWeaponSlots = 2;

	/** A Map storing the player's current weapons and the slot that they correspond to */
	UPROPERTY()
	TMap<int, AWeaponBase *> EquippedWeapons;
};
