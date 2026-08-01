#include "ZeroSumLobbyGameState.h"
#include "Net/UnrealNetwork.h"

AZeroSumLobbyGameState::AZeroSumLobbyGameState()
{
	bIsClientReady = false;
	SelectedMapName = TEXT("Arena"); // Default Dropdown Map Name
}

void AZeroSumLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZeroSumLobbyGameState, bIsClientReady);
	DOREPLIFETIME(AZeroSumLobbyGameState, SelectedMapName);
}

void AZeroSumLobbyGameState::OnRep_ClientReady()
{
	OnClientReadyChanged.Broadcast(bIsClientReady);
}

void AZeroSumLobbyGameState::OnRep_SelectedMap()
{
	OnMapSelectionChanged.Broadcast(SelectedMapName);
}

