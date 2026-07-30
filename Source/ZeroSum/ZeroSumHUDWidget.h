#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZeroSumHUDWidget.generated.h"

class UTextBlock;

UCLASS()
class ZEROSUM_API UZeroSumHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_MatchTimer;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_PlayerScore;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_EnemyScore;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_Health;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_Ammo;
};