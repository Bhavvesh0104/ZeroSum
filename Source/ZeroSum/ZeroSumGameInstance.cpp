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
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UZeroSumGameInstance::OnDestroySessionComplete);
		}
	}
}

void UZeroSumGameInstance::HostGame()
{
	if (SessionInterface.IsValid())
	{
		auto ExistingSession = SessionInterface->GetNamedSession(FName("ZeroSumSession"));
		if (ExistingSession != nullptr)
		{ 
			SessionInterface->DestroySession(FName("ZeroSumSession"));
		}
		else
		{
			CreateSessionInternal();
		}
	}
}

void UZeroSumGameInstance::CreateSessionInternal()
{
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.NumPublicConnections = 2;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;

	SessionInterface->CreateSession(0, FName("ZeroSumSession"), SessionSettings);
}

void UZeroSumGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		CreateSessionInternal();
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