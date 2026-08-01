#include "ZeroSumLobbyWidget.h"
#include "ZeroSumGameInstance.h"
#include "ZeroSumLobbyGameMode.h"
#include "ZeroSumLobbyGameState.h"
#include "ZeroSumPlayerState.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"

void UZeroSumLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// Force BOTH text boxes to be completely read-only for everyone
	if (EdTxt_HostName) EdTxt_HostName->SetIsReadOnly(true);
	if (EdTxt_ClientName) EdTxt_ClientName->SetIsReadOnly(true);

	// Grab the local player's name first
	FString MyLocalName = TEXT("Player");
	UZeroSumGameInstance* GI = Cast<UZeroSumGameInstance>(GetGameInstance());
	if (GI)
	{
		MyLocalName = GI->PlayerUsername;
	}

	// Establish base UI visibility based on network authority
	if (PC->HasAuthority())
	{
		if (Btn_Ready) Btn_Ready->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_StartMatch)
		{
			Btn_StartMatch->SetVisibility(ESlateVisibility::Visible);
			Btn_StartMatch->SetIsEnabled(false);
		}
		if (Drop_MapSelect) Drop_MapSelect->SetIsEnabled(true);
	}
	else
	{
		if (Btn_StartMatch) Btn_StartMatch->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_Ready) Btn_Ready->SetVisibility(ESlateVisibility::Visible);
		if (Drop_MapSelect) Drop_MapSelect->SetIsEnabled(false);
	}

	// Evaluate persistent match data and configure text states
	if (GI && GI->bHasPreviousMatchData)
	{
		if (Switcher_Lobby) Switcher_Lobby->SetActiveWidgetIndex(1); 

		if (EdTxt_HostName) EdTxt_HostName->SetText(FText::FromString(GI->SavedHostName));
		if (EdTxt_ClientName) EdTxt_ClientName->SetText(FText::FromString(GI->SavedClientName));

		if (Txt_WinnerText) Txt_WinnerText->SetText(FText::FromString(FString::Printf(TEXT("Winner : %s"), *GI->LastWinnerName)));
		if (Txt_FinalScoreText) Txt_FinalScoreText->SetText(FText::FromString(FString::Printf(TEXT("Final Score : %d | %d"), GI->LastHostScore, GI->LastClientScore)));
	}
	else
	{
		if (Switcher_Lobby) Switcher_Lobby->SetActiveWidgetIndex(0); 

		if (PC->HasAuthority())
		{
			if (EdTxt_HostName) EdTxt_HostName->SetText(FText::FromString(MyLocalName));
			if (EdTxt_ClientName) EdTxt_ClientName->SetText(FText::FromString(TEXT("Waiting for player...")));
		}
		else
		{
			if (EdTxt_ClientName) EdTxt_ClientName->SetText(FText::FromString(MyLocalName));
			if (EdTxt_HostName) EdTxt_HostName->SetText(FText::FromString(TEXT("Fetching Host...")));
		}
	}

	// Bind Button execution and data clearance
	if (Btn_Ready) Btn_Ready->OnClicked.AddDynamic(this, &UZeroSumLobbyWidget::OnReadyClicked);
	if (Btn_StartMatch) Btn_StartMatch->OnClicked.AddDynamic(this, &UZeroSumLobbyWidget::OnStartMatchClicked);
	if (Btn_ReturnToLobby) Btn_ReturnToLobby->OnClicked.AddDynamic(this, &UZeroSumLobbyWidget::OnReturnToLobbyClicked);
	if (Btn_Back) Btn_Back->OnClicked.AddDynamic(this, &UZeroSumLobbyWidget::OnReturnToLobbyClicked); 
	if (Btn_Back) Btn_Back->OnClicked.AddDynamic(this, &UZeroSumLobbyWidget::OnBackClicked);
	
	if (Drop_MapSelect) 
	{
		Drop_MapSelect->OnSelectionChanged.AddDynamic(this, &UZeroSumLobbyWidget::OnMapSelectionChanged);
		Drop_MapSelect->SetSelectedOption(TEXT("Arena")); 
	}
}

void UZeroSumLobbyWidget::OnBackClicked()
{
	if (UZeroSumGameInstance* GI = Cast<UZeroSumGameInstance>(GetGameInstance()))
	{
		GI->LeaveGame();
	}
}

void UZeroSumLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsStartingMatch) return; 

	if (AZeroSumLobbyGameState* GS = GetWorld()->GetGameState<AZeroSumLobbyGameState>())
	{
		if (!bGameStateBound)
		{
			GS->OnClientReadyChanged.AddDynamic(this, &UZeroSumLobbyWidget::OnClientReadyChanged);
			GS->OnMapSelectionChanged.AddDynamic(this, &UZeroSumLobbyWidget::OnMapChanged);
			bGameStateBound = true;

			// Enforce initial button state based on network authority
			if (APlayerController* PC = GetOwningPlayer())
			{
				if (PC->HasAuthority() && Btn_StartMatch) Btn_StartMatch->SetIsEnabled(GS->bIsClientReady);
			}
		}

		// Enforce client UI visibility
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (!PC->HasAuthority())
			{
				if (Btn_StartMatch) Btn_StartMatch->SetVisibility(ESlateVisibility::Collapsed);
				if (Btn_Ready) Btn_Ready->SetVisibility(ESlateVisibility::Visible);
			}
		}

		// Synchronize display names upon client connection
		if (!bNamesSynced && GS->PlayerArray.Num() == 2)
		{
			if (APlayerController* PC = GetOwningPlayer())
			{
				if (AZeroSumPlayerState* MyPS = PC->GetPlayerState<AZeroSumPlayerState>())
				{
					for (APlayerState* PS : GS->PlayerArray)
					{
						if (PS != MyPS) 
						{
							FString OpponentName = PS->GetPlayerName();
							
							// Exclude engine hardware ID assignments
							if (OpponentName.Contains(TEXT("-")) && OpponentName.Len() > 15) continue;

							if (PC->HasAuthority())
							{
								if (EdTxt_ClientName) EdTxt_ClientName->SetText(FText::FromString(OpponentName));
							}
							else
							{
								if (EdTxt_HostName) EdTxt_HostName->SetText(FText::FromString(OpponentName));
							}
							
							bNamesSynced = true;
							break;
						}
					}
				}
			}
		}
		
		// Reset state if client disconnects
		if (bNamesSynced && GS->PlayerArray.Num() < 2)
		{
			bNamesSynced = false;
			if (APlayerController* PC = GetOwningPlayer())
			{
				if (PC->HasAuthority())
				{
					if (EdTxt_ClientName) EdTxt_ClientName->SetText(FText::FromString(TEXT("Waiting for player...")));
					if (Btn_StartMatch) Btn_StartMatch->SetIsEnabled(false);
				}
			}
		}
	}
}

void UZeroSumLobbyWidget::OnReadyClicked()
{
	if (Btn_Ready) Btn_Ready->SetIsEnabled(false); 
	if (Btn_Back) Btn_Back->SetIsEnabled(false); 

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AZeroSumPlayerState* PS = PC->GetPlayerState<AZeroSumPlayerState>())
		{
			PS->Server_SetLobbyReadyStatus(true);
		}
	}
}

void UZeroSumLobbyWidget::OnStartMatchClicked()
{
	bIsStartingMatch = true; // Lock the UI

	if (AZeroSumLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AZeroSumLobbyGameMode>())
	{
		GM->StartMatch();
	}
}

void UZeroSumLobbyWidget::OnReturnToLobbyClicked()
{
	if (UZeroSumGameInstance* GI = Cast<UZeroSumGameInstance>(GetGameInstance()))
	{
		GI->ClearMatchData();
	}
	if (Switcher_Lobby)
	{
		Switcher_Lobby->SetActiveWidgetIndex(0);
	}
}

void UZeroSumLobbyWidget::OnMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct) // Only trigger if a human clicked it
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (AZeroSumPlayerState* PS = PC->GetPlayerState<AZeroSumPlayerState>())
			{
				PS->Server_UpdateLobbyMapSelection(SelectedItem);
			}
		}
	}
}

void UZeroSumLobbyWidget::OnClientReadyChanged(bool bIsReady)
{
	// Update Host button state
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (PC->HasAuthority())
		{
			if (Btn_StartMatch) Btn_StartMatch->SetIsEnabled(bIsReady);
		}
	}
}

void UZeroSumLobbyWidget::OnMapChanged(const FString& MapName)
{
	// Force the UI dropdown to match the Server's replicated variable
	if (Drop_MapSelect && Drop_MapSelect->GetSelectedOption() != MapName)
	{
		Drop_MapSelect->SetSelectedOption(MapName);
	}
}

