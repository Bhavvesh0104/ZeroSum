#include "ZeroSumGameMode.h"
#include "ZeroSumGameState.h"
#include "ZeroSumPlayerState.h"
#include "Components/HealthComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

AZeroSumGameMode::AZeroSumGameMode()
{
	GameStateClass = AZeroSumGameState::StaticClass();
	PlayerStateClass = AZeroSumPlayerState::StaticClass();
}

void AZeroSumGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AZeroSumGameMode::MatchTick, 1.0f, true);
}

void AZeroSumGameMode::MatchTick()
{
	if (AZeroSumGameState* GS = GetGameState<AZeroSumGameState>())
	{
		if (GS->MatchTimeRemaining > 0)
		{
			GS->MatchTimeRemaining--;
		}
		else
		{
			GetWorldTimerManager().ClearTimer(MatchTimerHandle);
			EndMatch(nullptr); 
		}
	}
}

void AZeroSumGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer && NewPlayer->GetPawn())
	{
		if (UHealthComponent* HC = NewPlayer->GetPawn()->FindComponentByClass<UHealthComponent>())
		{
			HC->OnHealthChanged.RemoveDynamic(this, &AZeroSumGameMode::OnPlayerHealthChanged);
			HC->OnHealthChanged.AddDynamic(this, &AZeroSumGameMode::OnPlayerHealthChanged);
		}
	}
}

void AZeroSumGameMode::OnPlayerHealthChanged(UHealthComponent* HealthComponent, float Health, float HealthDelta, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Health <= 0.0f)
	{
		AActor* DeadActor = HealthComponent->GetOwner();
		if (!DeadActor) return;

		APawn* VictimPawn = Cast<APawn>(DeadActor);
		if (!VictimPawn) return;

		AController* VictimController = VictimPawn->GetController();
		if (!VictimController) return;

		bool bMatchWon = false;
		AZeroSumPlayerState* WinnerPS = nullptr;

		if (InstigatedBy && InstigatedBy != VictimController)
		{
			if (AZeroSumPlayerState* KillerPS = InstigatedBy->GetPlayerState<AZeroSumPlayerState>())
			{
				KillerPS->AddKill();
				
				// Win Condition: Set to 10
				if (KillerPS->Kills >= 10)
				{
					bMatchWon = true;
					WinnerPS = KillerPS;
				}
			}
		}

		if (AZeroSumPlayerState* VictimPS = VictimController->GetPlayerState<AZeroSumPlayerState>())
		{
			VictimPS->AddDeath();
		}

		// Clear Attached Weapons & Destroy Character
		TArray<AActor*> AttachedActors;
		VictimPawn->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor)
			{
				AttachedActor->Destroy();
			}
		}
		
		VictimPawn->Destroy();

		// Either end the match, or respawn the victim
		if (bMatchWon)
		{
			EndMatch(WinnerPS);
		}
		else
		{
			FTimerHandle RespawnHandle;
			FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this, &AZeroSumGameMode::RespawnPlayer, VictimController);
			GetWorldTimerManager().SetTimer(RespawnHandle, RespawnDelegate, 3.0f, false);
		}
	}
}

void AZeroSumGameMode::RespawnPlayer(AController* TargetController)
{
	if (TargetController)
	{
		RestartPlayer(TargetController);
	}
}

void AZeroSumGameMode::EndMatch(AZeroSumPlayerState* Winner)
{
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);

	if (!Winner)
	{
		int32 HighestKills = -1;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AZeroSumPlayerState* PS = PC->GetPlayerState<AZeroSumPlayerState>())
				{
					if (PS->Kills > HighestKills)
					{
						HighestKills = PS->Kills;
						Winner = PS;
					}
				}
			}
		}
	}

	if (Winner)
	{
		UE_LOG(LogTemp, Warning, TEXT("ZERO SUM MATCH ENDED. WINNER: %s"), *Winner->GetPlayerName());
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->DisableInput(PC);

				// Clear Attached Weapons & Destroy Character for all remaining players
				TArray<AActor*> AttachedActors;
				Pawn->GetAttachedActors(AttachedActors);
				for (AActor* AttachedActor : AttachedActors)
				{
					if (AttachedActor)
					{
						AttachedActor->Destroy();
					}
				}
				Pawn->Destroy();
			}
		}
	}

	FTimerHandle PostMatchTimer;
	GetWorldTimerManager().SetTimer(PostMatchTimer, this, &AZeroSumGameMode::ReturnToMainMenu, 2.0f, false);
}

void AZeroSumGameMode::ReturnToMainMenu()
{
	GetWorld()->ServerTravel("/Game/ZeroSum/Maps/L_MainMenu");
}