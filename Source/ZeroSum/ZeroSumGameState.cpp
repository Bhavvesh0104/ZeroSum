#include "ZeroSumGameState.h"
#include "Net/UnrealNetwork.h"

AZeroSumGameState::AZeroSumGameState()
{
	MatchTimeRemaining = 300; // 5 minutes default
}

void AZeroSumGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZeroSumGameState, MatchTimeRemaining);
}