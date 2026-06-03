// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/LyraAbilitySystemGlobals.h"
#include "AbilitySystem/LyraGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAbilitySystemGlobals)

struct FGameplayEffectContext;

ULyraAbilitySystemGlobals::ULyraAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayEffectContext* ULyraAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FLyraGameplayEffectContext();
}
