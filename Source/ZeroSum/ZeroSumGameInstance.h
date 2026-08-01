#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "ZeroSumGameInstance.generated.h"

USTRUCT(BlueprintType)
struct FZeroSumSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Network")
	FString SessionName;

	UPROPERTY(BlueprintReadOnly, Category = "Network")
	int32 Ping;

	UPROPERTY(BlueprintReadOnly, Category = "Network")
	int32 SessionIndex;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionsFoundDelegate, const TArray<FZeroSumSessionInfo>&, SessionList);

UCLASS()
class ZEROSUM_API UZeroSumGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	FString PlayerUsername = TEXT("DefaultUser");

	UFUNCTION(BlueprintCallable, Category = "Network")
	void HostGame();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void FindGames();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinGame(int32 SessionIndex);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void PushUsernameToServer(APlayerController* PC);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void LeaveGame();

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnSessionsFoundDelegate OnSessionsFound;

	UPROPERTY(BlueprintReadOnly, Category = "Match Results Memory")
	bool bHasPreviousMatchData = false;

	UPROPERTY(BlueprintReadOnly, Category = "Match Results Memory")
	FString LastWinnerName;

	UPROPERTY(BlueprintReadOnly, Category = "Match Results Memory")
	int32 LastHostScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match Results Memory")
	int32 LastClientScore = 0;

	UFUNCTION(BlueprintCallable, Category = "Match Results Memory")
	void ClearMatchData() { bHasPreviousMatchData = false; SavedHostName = TEXT(""); SavedClientName = TEXT(""); }

	UPROPERTY(BlueprintReadWrite, Category = "Match Data")
	FString SavedHostName = TEXT("");

	UPROPERTY(BlueprintReadWrite, Category = "Match Data")
	FString SavedClientName = TEXT("");

protected:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;
	
	bool bWantsToHost = false;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void CreateSessionInternal();

	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};

