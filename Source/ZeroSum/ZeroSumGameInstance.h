#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
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

	UFUNCTION(BlueprintCallable, Category = "Network")
	void HostGame();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void FindGames();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinGame(int32 SessionIndex);

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnSessionsFoundDelegate OnSessionsFound;

protected:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void CreateSessionInternal();

	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};