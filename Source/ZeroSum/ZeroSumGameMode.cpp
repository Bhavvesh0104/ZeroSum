#include "ZeroSumGameMode.h"
#include "ZeroSumGameState.h"
#include "ZeroSumPlayerState.h"
#include "ZeroSumGameInstance.h"
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
	// Lock the score and prevent deaths if the match timer has already ended
	if (AZeroSumGameState* GS = GetGameState<AZeroSumGameState>())
	{
		if (GS->bMatchEnded) return; 
	}

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

		// Destroy defeated pawn and associated attachments
		TArray<AActor*> AttachedActors;
		VictimPawn->GetAttachedActors(AttachedActors);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor) AttachedActor->Destroy();
		}
		VictimPawn->Destroy();
		
		// Evaluate match progression state
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

	if (AZeroSumGameState* GS = GetGameState<AZeroSumGameState>())
	{
		// Calculate final score from replicated states
		int32 HScore = 0;
		int32 CScore = 0;
		
		if (GS->PlayerArray.IsValidIndex(0)) HScore = Cast<AZeroSumPlayerState>(GS->PlayerArray[0])->Kills;
		if (GS->PlayerArray.IsValidIndex(1)) CScore = Cast<AZeroSumPlayerState>(GS->PlayerArray[1])->Kills;

		// Resolve timer expiration conditions
		if (!Winner)
		{
			if (HScore > CScore)
			{
				Winner = Cast<AZeroSumPlayerState>(GS->PlayerArray[0]);
			}
			else if (CScore > HScore)
			{
				Winner = Cast<AZeroSumPlayerState>(GS->PlayerArray[1]);
			}
		}

		GS->MatchWinner = Winner;
		GS->bMatchEnded = true;

		// Assign final match display string
		FString WName = Winner ? Winner->GetPlayerName() : TEXT("Match Tied!");

		// Retrieve authoritative participant names
		FString HName = TEXT("Host");
		FString CName = TEXT("Client");
		if (GS->PlayerArray.IsValidIndex(0)) HName = GS->PlayerArray[0]->GetPlayerName();
		if (GS->PlayerArray.IsValidIndex(1)) CName = GS->PlayerArray[1]->GetPlayerName();

		// Replicate final statistics to local instances before ServerTravel
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AZeroSumPlayerState* PS = PC->GetPlayerState<AZeroSumPlayerState>())
				{
					PS->Client_SyncFinalScore(WName, HScore, CScore, HName, CName);
				}
			}
		}

		GS->OnRep_MatchEnded(); 
	}

	// Winner gets 2 seconds to move and act
	FTimerHandle WinnerDanceTimer;
	GetWorldTimerManager().SetTimer(WinnerDanceTimer, this, &AZeroSumGameMode::TriggerPostMatchWait, 2.0f, false);
}

void AZeroSumGameMode::TriggerPostMatchWait()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				// Freeze player input state
				Pawn->DisableInput(PC);
			}
		}
	}

	// Trigger map transition delay
	FTimerHandle ReturnToLobbyTimer;
	GetWorldTimerManager().SetTimer(ReturnToLobbyTimer, this, &AZeroSumGameMode::ReturnToLobby, 2.0f, false);
}

void AZeroSumGameMode::ReturnToLobby()
{
	GetWorld()->ServerTravel("/Game/ZeroSum/Maps/L_Lobby?listen");
}