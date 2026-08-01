#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ZeroSumPlayerState.generated.h"

UCLASS()
class ZEROSUM_API AZeroSumPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	AZeroSumPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Kills, Transient, BlueprintReadOnly, Category = "Score")
	int32 Kills;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths, Transient, BlueprintReadOnly, Category = "Score")
	int32 Deaths;

	UFUNCTION()
	void OnRep_Kills();

	UFUNCTION()
	void OnRep_Deaths();

	void AddKill();
	void AddDeath();

	// New Lobby RPCs
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetLobbyReadyStatus(bool bIsReady);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateLobbyMapSelection(const FString& MapName);

	// Force-feeds the final score to the local GameInstance
	UFUNCTION(Client, Reliable)
	void Client_SyncFinalScore(const FString& WinnerName, int32 HostScore, int32 ClientScore, const FString& FinalHostName, const FString& FinalClientName);
};