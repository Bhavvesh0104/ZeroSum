#include "ZeroSumLobbyGameMode.h"
#include "ZeroSumLobbyGameState.h"
#include "ZeroSumPlayerState.h"

AZeroSumLobbyGameMode::AZeroSumLobbyGameMode()
{
	GameStateClass = AZeroSumLobbyGameState::StaticClass();
	// reusing existing PlayerState to carry the RPCs
	PlayerStateClass = AZeroSumPlayerState::StaticClass(); 
}

void AZeroSumLobbyGameMode::SetClientReady(bool bReady)
{
	if (AZeroSumLobbyGameState* GS = GetGameState<AZeroSumLobbyGameState>())
	{
		GS->bIsClientReady = bReady;
		GS->OnRep_ClientReady(); // Force the multicast on the Listen Server
	}
}

void AZeroSumLobbyGameMode::UpdateSelectedMap(const FString& MapName)
{
	if (AZeroSumLobbyGameState* GS = GetGameState<AZeroSumLobbyGameState>())
	{
		GS->SelectedMapName = MapName;
		GS->OnRep_SelectedMap(); // Force the multicast on the Listen Server
	}
}

void AZeroSumLobbyGameMode::StartMatch()
{
	if (AZeroSumLobbyGameState* GS = GetGameState<AZeroSumLobbyGameState>())
	{
		FString MapURL = TEXT("/Game/ZeroSum/Maps/L_Arena?listen");
		
		// Map the dropdown string to actual directory paths
		if (GS->SelectedMapName == TEXT("Arena"))
		{
			MapURL = TEXT("/Game/ZeroSum/Maps/L_Arena?listen");
		}
		// More Maps for future updates

		GetWorld()->ServerTravel(MapURL);
	}
}