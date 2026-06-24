#include "ZeroSumGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"

void UZeroSumGameInstance::Init()
{
	Super::Init();

	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UZeroSumGameInstance::OnCreateSessionComplete);
		}
	}
}

void UZeroSumGameInstance::HostGame()
{
	if (SessionInterface.IsValid())
	{
		FOnlineSessionSettings SessionSettings;
		SessionSettings.bIsLANMatch = true; // True while testing with Null subsystem
		SessionSettings.NumPublicConnections = 2; // Strict 1v1 environment
		SessionSettings.bShouldAdvertise = true;
		SessionSettings.bUsesPresence = true;
		SessionSettings.bUseLobbiesIfAvailable = true;
		SessionInterface->CreateSession(0, FName("ZeroSumSession"), SessionSettings);
	}
}

void UZeroSumGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{ 
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel("/Game/ZeroSum/Maps/L_Arena?listen");
		}
	}
}