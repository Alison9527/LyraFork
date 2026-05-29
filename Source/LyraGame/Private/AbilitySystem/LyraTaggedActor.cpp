// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/LyraTaggedActor.h"

// Sets default values
ALyraTaggedActor::ALyraTaggedActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ALyraTaggedActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ALyraTaggedActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

