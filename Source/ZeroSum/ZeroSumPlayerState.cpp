#include "ZeroSumPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ZeroSumLobbyGameMode.h"
#include "ZeroSumGameInstance.h"

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

bool AZeroSumPlayerState::Server_SetLobbyReadyStatus_Validate(bool bIsReady)
{
	return true;
}

void AZeroSumPlayerState::Server_SetLobbyReadyStatus_Implementation(bool bIsReady)
{
	if (AZeroSumLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AZeroSumLobbyGameMode>())
	{
		GM->SetClientReady(bIsReady);
	}
}

bool AZeroSumPlayerState::Server_UpdateLobbyMapSelection_Validate(const FString& MapName)
{
	return true;
}

void AZeroSumPlayerState::Server_UpdateLobbyMapSelection_Implementation(const FString& MapName)
{
	// Only the Host is permitted to change the map
	if (HasAuthority()) 
	{
		if (AZeroSumLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AZeroSumLobbyGameMode>())
		{
			GM->UpdateSelectedMap(MapName);
		}
	}
}

void AZeroSumPlayerState::Client_SyncFinalScore_Implementation(const FString& WinnerName, int32 HostScore, int32 ClientScore, const FString& FinalHostName, const FString& FinalClientName)
{
	if (UZeroSumGameInstance* GI = Cast<UZeroSumGameInstance>(GetGameInstance()))
	{
		GI->bHasPreviousMatchData = true;
		GI->LastWinnerName = WinnerName;
		GI->LastHostScore = HostScore;
		GI->LastClientScore = ClientScore;
		
		// Save names for the Lobby reload
		GI->SavedHostName = FinalHostName;
		GI->SavedClientName = FinalClientName;
	}
}