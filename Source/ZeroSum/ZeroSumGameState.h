#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ZeroSumGameState.generated.h"

UCLASS()
class ZEROSUM_API AZeroSumGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AZeroSumGameState();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "Match State")
	int32 MatchTimeRemaining;
};