// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/LyraGameMode.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
// #include "LyraLogChannels.h"
#include "Misc/CommandLine.h"
// #include "System/LyraAssetManager.h"
// #include "Character/LyraPawnExtensionComponent.h"
// #include "Character/LyraPawnData.h"
// #include "GameModes/LyraWorldSettings.h"
// #include "GameModes/LyraExperienceDefinition.h"
// #include "GameModes/LyraExperienceManagerComponent.h"
// #include "GameModes/LyraUserFacingExperienceDefinition.h"
#include "Kismet/GameplayStatics.h"
// #include "Development/LyraDeveloperSettings.h"
// #include "Player/LyraPlayerSpawningManagerComponent.h"
// #include "CommonUserSubsystem.h"
// #include "CommonSessionSubsystem.h"
#include "TimerManager.h"
#include "GameMapsSettings.h"
#include "Character/LyraCharacter.h"
#include "GameModes/LyraGameState.h"
#include "Player/LyraPlayerController.h"
#include "Player/LyraPlayerState.h"
#include "System/LyraGameSession.h"
#include "UI/LyraHUD.h"

ALyraGameMode::ALyraGameMode(const FObjectInitializer& ObjectInitializer)
{
	GameStateClass = ALyraGameState::StaticClass();
	GameSessionClass = ALyraGameSession::StaticClass();
	PlayerControllerClass = ALyraPlayerController::StaticClass();
	ReplaySpectatorPlayerControllerClass = ALyraReplayPlayerController::StaticClass();
	PlayerStateClass = ALyraPlayerState::StaticClass();
	DefaultPawnClass = ALyraCharacter::StaticClass();
	HUDClass = ALyraHUD::StaticClass();
}

//~AGameModeBase interface
void ALyraGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Wait for the next frame to give time to initialize startup settings
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

UClass* ALyraGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// if (const ULyraPawnData* PawnData = GetPawnDataForController(InController))
	// {
	// 	if (PawnData->PawnClass)
	// 	{
	// 		return PawnData->PawnClass;
	// 	}
	// }

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* ALyraGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;	// Never save the default player pawns into a map.
	SpawnInfo.bDeferConstruction = true;

	// if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	// {
	// 	if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
	// 	{
	// 		if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
	// 		{
	// 			if (const ULyraPawnData* PawnData = GetPawnDataForController(NewPlayer))
	// 			{
	// 				PawnExtComp->SetPawnData(PawnData);
	// 			}
	// 			else
	// 			{
	// 				UE_LOG(LogLyra, Error, TEXT("Game mode was unable to set PawnData on the spawned pawn [%s]."), *GetNameSafe(SpawnedPawn));
	// 			}
	// 		}
	//
	// 		SpawnedPawn->FinishSpawning(SpawnTransform);
	//
	// 		return SpawnedPawn;
	// 	}
	// 	else
	// 	{
	// 		UE_LOG(LogLyra, Error, TEXT("Game mode was unable to spawn Pawn of class [%s] at [%s]."), *GetNameSafe(PawnClass), *SpawnTransform.ToHumanReadableString());
	// 	}
	// }
	// else
	// {
	// 	UE_LOG(LogLyra, Error, TEXT("Game mode was unable to spawn Pawn due to NULL pawn class."));
	// }

	return nullptr;
}

bool ALyraGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	// We never want to use the start spot, always use the spawn management component.
	return false;
}

void ALyraGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Delay starting new players until the experience has been loaded
	// (players who log in prior to that will be started by OnExperienceLoaded)
	// if (IsExperienceLoaded())
	// {
	// 	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	// }
}

AActor* ALyraGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// if (ULyraPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<ULyraPlayerSpawningManagerComponent>())
	// {
	// 	return PlayerSpawningComponent->ChoosePlayerStart(Player);
	// }
	
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ALyraGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	// if (ULyraPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<ULyraPlayerSpawningManagerComponent>())
	// {
	// 	PlayerSpawningComponent->FinishRestartPlayer(NewPlayer, StartRotation);
	// }

	Super::FinishRestartPlayer(NewPlayer, StartRotation);
}

bool ALyraGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return ControllerCanRestart(Player);
}

void ALyraGameMode::InitGameState()
{
	Super::InitGameState();

	// Listen for the experience load to complete	
	// ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
	// check(ExperienceComponent);
	// ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

bool ALyraGameMode::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
	// Do nothing, we'll wait until PostLogin when we try to spawn the player for real.
	// Doing anything right now is no good, systems like team assignment haven't even occurred yet.
	return true;
}

void ALyraGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);

	// OnGameModePlayerInitialized.Broadcast(this, NewPlayer);
}

void ALyraGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	// If we tried to spawn a pawn and it failed, lets try again *note* check if there's actually a pawn class
	// before we try this forever.
	// if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	// {
	// 	if (APlayerController* NewPC = Cast<APlayerController>(NewPlayer))
	// 	{
	// 		// If it's a player don't loop forever, maybe something changed and they can no longer restart if so stop trying.
	// 		if (PlayerCanRestart(NewPC))
	// 		{
	// 			RequestPlayerRestartNextFrame(NewPlayer, false);			
	// 		}
	// 		else
	// 		{
	// 			UE_LOG(LogLyra, Verbose, TEXT("FailedToRestartPlayer(%s) and PlayerCanRestart returned false, so we're not going to try again."), *GetPathNameSafe(NewPlayer));
	// 		}
	// 	}
	// 	else
	// 	{
	// 		RequestPlayerRestartNextFrame(NewPlayer, false);
	// 	}
	// }
	// else
	// {
	// 	UE_LOG(LogLyra, Verbose, TEXT("FailedToRestartPlayer(%s) but there's no pawn class so giving up."), *GetPathNameSafe(NewPlayer));
	// }
}
//~End of AGameModeBase interface

bool ALyraGameMode::ControllerCanRestart(AController* Controller)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{	
		if (!Super::PlayerCanRestart_Implementation(PC))
		{
			return false;
		}
	}
	else
	{
		// Bot version of Super::PlayerCanRestart_Implementation
		if ((Controller == nullptr) || Controller->IsPendingKillPending())
		{
			return false;
		}
	}

	// if (ULyraPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<ULyraPlayerSpawningManagerComponent>())
	// {
	// 	return PlayerSpawningComponent->ControllerCanRestart(Controller);
	// }

	return true;
}


void ALyraGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
		FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	// Precedence order (highest wins)
	//  - Matchmaking assignment (if present)
	//  - URL Options override
	//  - Developer Settings (PIE only)
	//  - Command Line override
	//  - World Settings
	//  - Dedicated server
	//  - Default experience

	UWorld* World = GetWorld();

	// if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	// {
	// 	const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
	// 	ExperienceId = FPrimaryAssetId(FPrimaryAssetType(ULyraExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
	// 	ExperienceIdSource = TEXT("OptionsString");
	// }
	//
	// if (!ExperienceId.IsValid() && World->IsPlayInEditor())
	// {
	// 	ExperienceId = GetDefault<ULyraDeveloperSettings>()->ExperienceOverride;
	// 	ExperienceIdSource = TEXT("DeveloperSettings");
	// }
	//
	// // see if the command line wants to set the experience
	// if (!ExperienceId.IsValid())
	// {
	// 	FString ExperienceFromCommandLine;
	// 	if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
	// 	{
	// 		ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
	// 		if (!ExperienceId.PrimaryAssetType.IsValid())
	// 		{
	// 			ExperienceId = FPrimaryAssetId(FPrimaryAssetType(ULyraExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
	// 		}
	// 		ExperienceIdSource = TEXT("CommandLine");
	// 	}
	// }
	//
	// // see if the world settings has a default experience
	// if (!ExperienceId.IsValid())
	// {
	// 	if (ALyraWorldSettings* TypedWorldSettings = Cast<ALyraWorldSettings>(GetWorldSettings()))
	// 	{
	// 		ExperienceId = TypedWorldSettings->GetDefaultGameplayExperience();
	// 		ExperienceIdSource = TEXT("WorldSettings");
	// 	}
	// }
	//
	// ULyraAssetManager& AssetManager = ULyraAssetManager::Get();
	// FAssetData Dummy;
	// if (ExperienceId.IsValid() && !AssetManager.GetPrimaryAssetData(ExperienceId, /*out*/ Dummy))
	// {
	// 	UE_LOG(LogLyraExperience, Error, TEXT("EXPERIENCE: Wanted to use %s but couldn't find it, falling back to the default)"), *ExperienceId.ToString());
	// 	ExperienceId = FPrimaryAssetId();
	// }
	//
	// // Final fallback to the default experience
	// if (!ExperienceId.IsValid())
	// {
	// 	if (TryDedicatedServerLogin())
	// 	{
	// 		// This will start to host as a dedicated server
	// 		return;
	// 	}
	//
	// 	//@TODO: Pull this from a config setting or something
	// 	ExperienceId = FPrimaryAssetId(FPrimaryAssetType("LyraExperienceDefinition"), FName("B_LyraDefaultExperience"));
	// 	ExperienceIdSource = TEXT("Default");
	// }
	//
	// OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}
