#include "ZeroSumHUDWidget.h"
#include "ZeroSumGameState.h"
#include "ZeroSumPlayerState.h"
#include "FPSCharacter.h"
#include "WeaponBase.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/TextBlock.h"

void UZeroSumHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Time & Scores
	if (AZeroSumGameState* GS = GetWorld()->GetGameState<AZeroSumGameState>())
	{
		if (Txt_MatchTimer)
		{
			int32 Mins = GS->MatchTimeRemaining / 60;
			int32 Secs = GS->MatchTimeRemaining % 60;
			Txt_MatchTimer->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Mins, Secs)));
		}

		if (Txt_PlayerScore && Txt_EnemyScore)
		{
			APlayerState* MyPS = GetOwningPlayerState();
			if (!MyPS)
			{
				Txt_PlayerScore->SetText(FText::FromString(TEXT("0")));
				Txt_EnemyScore->SetText(FText::FromString(TEXT("0")));
			}
			else
			{
				for (APlayerState* PS : GS->PlayerArray)
				{
					if (AZeroSumPlayerState* ZSPS = Cast<AZeroSumPlayerState>(PS))
					{
						if (PS == MyPS)
						{
							Txt_PlayerScore->SetText(FText::FromString(FString::FromInt(ZSPS->Kills)));
						}
						else
						{
							Txt_EnemyScore->SetText(FText::FromString(FString::FromInt(ZSPS->Kills)));
						}
					}
				}
			}
		}
	}

	// Health & Ammo
	if (AFPSCharacter* MyPawn = Cast<AFPSCharacter>(GetOwningPlayerPawn()))
	{
		if (Txt_Health)
		{
			if (UHealthComponent* HC = MyPawn->GetHealthComponent())
			{
				Txt_Health->SetText(FText::FromString(FString::FromInt(FMath::RoundToInt(HC->GetHealth()))));
			}
		}

		if (Txt_Ammo)
		{
			if (UInventoryComponent* IC = MyPawn->GetInventoryComponent())
			{
				if (AWeaponBase* Weapon = IC->GetCurrentWeapon())
				{
					FRuntimeWeaponData* WData = Weapon->GetRuntimeWeaponData();
					if (WData)
					{
						Txt_Ammo->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), WData->ClipSize, WData->ClipCapacity)));
					}
				}
				else
				{
					Txt_Ammo->SetText(FText::FromString(TEXT("-- / --")));
				}
			}
		}
	}
	else
	{
		// Default states when dead/respawning
		if (Txt_Health) Txt_Health->SetText(FText::FromString(TEXT("0")));
		if (Txt_Ammo) Txt_Ammo->SetText(FText::FromString(TEXT("-- / --")));
	}
}