// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonPlayerController.h"
#include "LyraPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class LYRAGAME_API ALyraPlayerController : public ACommonPlayerController
{
	GENERATED_BODY()
	
};


// A player controller used for replay capture and playback
UCLASS()
class ALyraReplayPlayerController : public ALyraPlayerController
{
	GENERATED_BODY()
	
};
