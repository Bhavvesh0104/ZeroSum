#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZeroSumLobbyGameMode.generated.h"

UCLASS()
class ZEROSUM_API AZeroSumLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZeroSumLobbyGameMode();

	void SetClientReady(bool bReady);
	void UpdateSelectedMap(const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "Lobby Flow")
	void StartMatch();
};