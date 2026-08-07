#include "ZeroSumGameMode.h"
#include "ZeroSumGameState.h"
#include "ZeroSumPlayerState.h"
#include "ZeroSumGameInstance.h"
#include "Components/HealthComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/InventoryComponent.h"
#include "EngineUtils.h"
#include "FPSCharacter.h"
#include "WeaponBase.h"

AZeroSumGameMode::AZeroSumGameMode()
{
	GameStateClass = AZeroSumGameState::StaticClass();
	PlayerStateClass = AZeroSumPlayerState::StaticClass();
}

void AZeroSumGameMode::BeginPlay()
{
	Super::BeginPlay();
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

void AZeroSumGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (GetNumPlayers() >= 2)
	{
		if (!GetWorldTimerManager().IsTimerActive(MatchTimerHandle))
		{
			GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AZeroSumGameMode::MatchTick, 1.0f, true);
		}
	}
}

void AZeroSumGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	// Initialisation of the match loadout to guarantee execution before the first pawn injection
	// Listen Servers fire RestartPlayer for the Host prior to BeginPlay
	if (MatchPrimaryWeapon == nullptr && AvailableWeapons.Num() > 0)
	{
		MatchPrimaryWeapon = AvailableWeapons[FMath::RandRange(0, AvailableWeapons.Num() - 1)];

		if (AvailableWeapons.Num() > 1)
		{
			int32 SecIndex = FMath::RandRange(0, AvailableWeapons.Num() - 1);
			while (AvailableWeapons[SecIndex] == MatchPrimaryWeapon)
			{
				SecIndex = FMath::RandRange(0, AvailableWeapons.Num() - 1);
			}
			MatchSecondaryWeapon = AvailableWeapons[SecIndex];
		}
		else
		{
			MatchSecondaryWeapon = MatchPrimaryWeapon;
		}
	}

	if (NewPlayer && NewPlayer->GetPawn())
	{
		if (UHealthComponent* HC = NewPlayer->GetPawn()->FindComponentByClass<UHealthComponent>())
		{
			HC->OnHealthChanged.RemoveDynamic(this, &AZeroSumGameMode::OnPlayerHealthChanged);
			HC->OnHealthChanged.AddDynamic(this, &AZeroSumGameMode::OnPlayerHealthChanged);
		}

		// Inject authoritative loadout directly into the fresh Pawn
		if (UInventoryComponent* InvComp = NewPlayer->GetPawn()->FindComponentByClass<UInventoryComponent>())
		{
			InvComp->InitializeLoadout(MatchPrimaryWeapon, MatchSecondaryWeapon);
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
		int32 HScore = 0;
		int32 CScore = 0;
		
		if (GS->PlayerArray.IsValidIndex(0))
		{
			if (AZeroSumPlayerState* HostPS = Cast<AZeroSumPlayerState>(GS->PlayerArray[0])) HScore = HostPS->Kills;
		}
		if (GS->PlayerArray.IsValidIndex(1))
		{
			if (AZeroSumPlayerState* ClientPS = Cast<AZeroSumPlayerState>(GS->PlayerArray[1])) CScore = ClientPS->Kills;
		}

		if (!Winner)
		{
			if (HScore > CScore && GS->PlayerArray.IsValidIndex(0))
			{
				Winner = Cast<AZeroSumPlayerState>(GS->PlayerArray[0]);
			}
			else if (CScore > HScore && GS->PlayerArray.IsValidIndex(1))
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

void AZeroSumGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		for (TActorIterator<AWeaponBase> WeaponIt(GetWorld()); WeaponIt; ++WeaponIt)
		{
			AFPSCharacter* WeaponOwner = Cast<AFPSCharacter>(WeaponIt->GetOwner());
			
			if (!WeaponOwner || WeaponOwner->GetController() == Exiting || WeaponOwner->GetController() == nullptr)
			{
				WeaponIt->Destroy();
			}
		}
		for (TActorIterator<AFPSCharacter> PawnIt(GetWorld()); PawnIt; ++PawnIt)
		{
			if (PawnIt->GetController() == Exiting || PawnIt->GetController() == nullptr)
			{
				PawnIt->Destroy();
				break; 
			}
		}
	}

	if (AZeroSumGameState* GS = GetGameState<AZeroSumGameState>())
	{
		if (!GS->bMatchEnded)
		{
			AZeroSumPlayerState* RemainingPlayer = nullptr;

			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (PC != Exiting)
					{
						RemainingPlayer = PC->GetPlayerState<AZeroSumPlayerState>();
						break;
					}
				}
			}

			if (RemainingPlayer)
			{
				EndMatch(RemainingPlayer);
			}
		}
	}

	Super::Logout(Exiting);
}