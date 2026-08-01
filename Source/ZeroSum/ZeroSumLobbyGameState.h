#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ZeroSumLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClientReadyChangedSignature, bool, bIsReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMapSelectionChangedSignature, const FString&, MapName);

UCLASS()
class ZEROSUM_API AZeroSumLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AZeroSumLobbyGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_ClientReady, Transient, BlueprintReadOnly, Category = "Lobby State")
	bool bIsClientReady;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMap, Transient, BlueprintReadOnly, Category = "Lobby State")
	FString SelectedMapName;

	UFUNCTION()
	void OnRep_ClientReady();

	UFUNCTION()
	void OnRep_SelectedMap();

	UPROPERTY(BlueprintAssignable, Category = "Lobby State")
	FOnClientReadyChangedSignature OnClientReadyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby State")
	FOnMapSelectionChangedSignature OnMapSelectionChanged;
};