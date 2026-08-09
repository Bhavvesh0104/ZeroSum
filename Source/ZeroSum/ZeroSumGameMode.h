#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ZeroSumGameMode.generated.h"

class UHealthComponent;
class AZeroSumPlayerState;

UCLASS()
class ZEROSUM_API AZeroSumGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AZeroSumGameMode();
	
	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

public:
	// Populate this in the Blueprint child class with the available weapon classes
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules Loadout")
	TArray<TSubclassOf<class AWeaponBase>> AvailableWeapons;

protected:
	FTimerHandle MatchTimerHandle;
	
	TSubclassOf<class AWeaponBase> MatchPrimaryWeapon;
	TSubclassOf<class AWeaponBase> MatchSecondaryWeapon;
	
	void MatchTick();
	void EndMatch(AZeroSumPlayerState* Winner);
	void RespawnPlayer(AController* TargetController);
	
	void TriggerPostMatchWait();
	void ReturnToLobby();

	UFUNCTION()
	void OnPlayerHealthChanged(UHealthComponent* HealthComponent, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
};