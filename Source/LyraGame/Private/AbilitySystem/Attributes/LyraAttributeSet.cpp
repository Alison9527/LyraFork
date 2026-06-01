// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAttributeSet)

class UWorld;


ULyraAttributeSet::ULyraAttributeSet()
{
}

UWorld* ULyraAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

ULyraAbilitySystemComponent* ULyraAttributeSet::GetLyraAbilitySystemComponent() const
{
	return Cast<ULyraAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
