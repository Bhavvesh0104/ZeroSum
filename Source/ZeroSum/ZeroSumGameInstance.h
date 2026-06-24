#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "ZeroSumGameInstance.generated.h"

UCLASS()
class ZEROSUM_API UZeroSumGameInstance : public UGameInstance
{
	GENERATED_BODY()

public: 
	virtual void Init() override;
	UFUNCTION(BlueprintCallable, Category = "Network")
	void HostGame();

protected:
	IOnlineSessionPtr SessionInterface;
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void CreateSessionInternal();
};