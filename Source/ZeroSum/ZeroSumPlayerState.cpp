#include "ZeroSumPlayerState.h"
#include "Net/UnrealNetwork.h"

AZeroSumPlayerState::AZeroSumPlayerState()
{
	Kills = 0;
	Deaths = 0;
	NetUpdateFrequency = 10.0f; // Fast network priority for UI updates
}

void AZeroSumPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZeroSumPlayerState, Kills);
	DOREPLIFETIME(AZeroSumPlayerState, Deaths);
}

void AZeroSumPlayerState::OnRep_Kills() {}
void AZeroSumPlayerState::OnRep_Deaths() {}

void AZeroSumPlayerState::AddKill()
{
	if (HasAuthority()) Kills++;
}

void AZeroSumPlayerState::AddDeath()
{
	if (HasAuthority()) Deaths++;
}