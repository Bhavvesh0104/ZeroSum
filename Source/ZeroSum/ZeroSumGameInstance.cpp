#include "ZeroSumGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
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
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UZeroSumGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UZeroSumGameInstance::OnJoinSessionComplete);
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

void UZeroSumGameInstance::FindGames()
{
	if (SessionInterface.IsValid())
	{
		SessionSearch = MakeShareable(new FOnlineSessionSearch());
		SessionSearch->bIsLanQuery = true;
		SessionSearch->MaxSearchResults = 10000;
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}

void UZeroSumGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	TArray<FZeroSumSessionInfo> SessionList;

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			if (Result.IsValid())
			{
				FZeroSumSessionInfo SessionInfo;
				SessionInfo.SessionIndex = i;
				SessionInfo.Ping = Result.PingInMs;
				SessionInfo.SessionName = Result.Session.OwningUserName.IsEmpty() ? "ZeroSum Match" : Result.Session.OwningUserName;

				SessionList.Add(SessionInfo);
			}
		}
	}

	OnSessionsFound.Broadcast(SessionList);
}

void UZeroSumGameInstance::JoinGame(int32 SessionIndex)
{
	if (SessionInterface.IsValid() && SessionSearch.IsValid())
	{
		if (SessionSearch->SearchResults.IsValidIndex(SessionIndex))
		{
			FOnlineSessionSearchResult& TargetResult = SessionSearch->SearchResults[SessionIndex];
			SessionInterface->JoinSession(0, FName("ZeroSumSession"), TargetResult);
		}
	}
}

void UZeroSumGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success && SessionInterface.IsValid())
	{
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
		{
			if (APlayerController* PC = GetFirstLocalPlayerController())
			{
				PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
			}
		}
	}
}