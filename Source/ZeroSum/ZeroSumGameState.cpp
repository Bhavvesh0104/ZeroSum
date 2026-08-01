#include "ZeroSumGameState.h"
#include "ZeroSumPlayerState.h"
#include "ZeroSumGameInstance.h"
#include "Net/UnrealNetwork.h"

AZeroSumGameState::AZeroSumGameState()
{
	MatchTimeRemaining = 300; // 5 minutes default
	bMatchEnded = false;
	MatchWinner = nullptr;
}

void AZeroSumGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZeroSumGameState, MatchTimeRemaining);
	DOREPLIFETIME(AZeroSumGameState, bMatchEnded);
	DOREPLIFETIME(AZeroSumGameState, MatchWinner);
}

void AZeroSumGameState::OnRep_MatchEnded()
{
	if (bMatchEnded)
	{
		OnMatchEndedCallback.Broadcast(MatchWinner);
	}
}