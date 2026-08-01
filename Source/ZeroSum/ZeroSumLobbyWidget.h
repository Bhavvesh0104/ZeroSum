#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZeroSumLobbyWidget.generated.h"

class UWidgetSwitcher;
class UComboBoxString;
class UButton;
class UTextBlock;
class UEditableTextBox;

UCLASS()
class ZEROSUM_API UZeroSumLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// SWITCHER 
	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* Switcher_Lobby;

	// INDEX 0: PRE-MATCH
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* Drop_MapSelect;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Ready;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_StartMatch;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Back;

	bool bIsStartingMatch = false;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EdTxt_HostName;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EdTxt_ClientName;

	// INDEX 1: POST-MATCH
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_WinnerText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_FinalScoreText;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_ReturnToLobby;

private:
	// Button Clicks
	UFUNCTION()
	void OnReadyClicked();

	UFUNCTION()
	void OnStartMatchClicked();

	UFUNCTION()
	void OnReturnToLobbyClicked();

	UFUNCTION()
	void OnBackClicked();

	// Dropdown & Text Commits
	UFUNCTION()
	void OnMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// GameState Delegate Handlers
	UFUNCTION()
	void OnClientReadyChanged(bool bIsReady);

	UFUNCTION()
	void OnMapChanged(const FString& MapName);

	bool bGameStateBound = false;
	bool bNamesSynced = false;
};