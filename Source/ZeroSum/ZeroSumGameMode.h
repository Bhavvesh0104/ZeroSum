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

protected:
	FTimerHandle MatchTimerHandle;
	
	void MatchTick();
	void EndMatch(AZeroSumPlayerState* Winner);
	void RespawnPlayer(AController* TargetController);
	
	void TriggerPostMatchWait();
	void ReturnToLobby();

	UFUNCTION()
	void OnPlayerHealthChanged(UHealthComponent* HealthComponent, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
};