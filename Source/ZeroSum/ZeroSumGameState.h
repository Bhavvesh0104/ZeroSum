#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ZeroSumGameState.generated.h"

class AZeroSumPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEndedSignature, AZeroSumPlayerState*, Winner);

UCLASS()
class ZEROSUM_API AZeroSumGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AZeroSumGameState();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "Match State")
	int32 MatchTimeRemaining;

	UPROPERTY(ReplicatedUsing = OnRep_MatchEnded, Transient, BlueprintReadOnly, Category = "Match State")
	bool bMatchEnded;

	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "Match State")
	AZeroSumPlayerState* MatchWinner;

	UFUNCTION()
	void OnRep_MatchEnded();

	UPROPERTY(BlueprintAssignable, Category = "Match State")
	FOnMatchEndedSignature OnMatchEndedCallback;
};