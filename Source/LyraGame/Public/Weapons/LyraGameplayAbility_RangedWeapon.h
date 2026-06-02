// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Equipment/LyraGameplayAbility_FromEquipment.h"
#include "LyraGameplayAbility_RangedWeapon.generated.h"

enum ECollisionChannel : int;

class APawn;
class ULyraRangedWeaponInstance;
class UObject;
struct FCollisionQueryParams;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;
struct FGameplayTag;
struct FGameplayTagContainer;

/** * Defines where an ability starts its trace from and where it should face 
 * 定义了技能（开火）的追踪起点和朝向
 */
UENUM(BlueprintType)
enum class ELyraAbilityTargetingSource : uint8
{
    // From the player's camera towards camera focus
    // 经典TPS做法：从摄像机位置，射向屏幕准星焦点
    CameraTowardsFocus,
    
    // From the pawn's center, in the pawn's orientation
    // 从角色中心，朝向角色正前方（少用，多用于2D或特定技能）
    PawnForward,
    
    // From the pawn's center, oriented towards camera focus
    // 从角色中心，射向摄像机准星焦点
    PawnTowardsFocus,
    
    // From the weapon's muzzle or location, in the pawn's orientation
    // 从武器枪口位置，朝向角色正前方（不考虑准星）
    WeaponForward,
    
    // From the weapon's muzzle or location, towards camera focus
    // 从武器枪口位置，射向摄像机准星焦点（硬核FPS常见做法，考虑掩体遮挡）
    WeaponTowardsFocus,
    
    // Custom blueprint-specified source location
    // 自定义蓝图位置
    Custom
};

/**
 * ULyraGameplayAbility_RangedWeapon
 *
 * An ability granted by and associated with a ranged weapon instance
 * 远程武器的核心开火技能，通常由远程武器实例赋予玩家
 */
UCLASS()
class ULyraGameplayAbility_RangedWeapon : public ULyraGameplayAbility_FromEquipment
{
    GENERATED_BODY()

public:

    ULyraGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintCallable, Category="Lyra|Ability")
    ULyraRangedWeaponInstance* GetWeaponInstance() const;

    //~UGameplayAbility interface
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    //~End of UGameplayAbility interface

protected:
    /** 单次开火的输入数据结构 */
    struct FRangedWeaponFiringInput
    {
       // Start of the trace
       // 射线检测的物理起点
       FVector StartTrace;

       // End of the trace if aim were perfect
       // 完美瞄准情况下（无散布）的终点位置
       FVector EndAim;

       // The direction of the trace if aim were perfect
       // 完美瞄准情况下的核心方向向量
       FVector AimDir;

       // The weapon instance / source of weapon data
       // 武器实例指针，用于获取射程、散布、伤害等数据驱动的参数
       ULyraRangedWeaponInstance* WeaponData = nullptr;

       // Can we play bullet FX for hits during this trace
       // 是否可以播放子弹特效（通常在 Dedicated Server 上为 false 以节省性能）
       bool bCanPlayBulletFX = false;

       FRangedWeaponFiringInput()
          : StartTrace(ForceInitToZero)
          , EndAim(ForceInitToZero)
          , AimDir(ForceInitToZero)
       {
       }
    };

protected:
    static int32 FindFirstPawnHitResult(const TArray<FHitResult>& HitResults);

    // Does a single weapon trace, either sweeping or ray depending on if SweepRadius is above zero
    // 执行单次武器射线检测，根据 SweepRadius 决定是执行线射线(LineTrace)还是球体扫描(SweepTrace)
    FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHitResults) const;

    // Wrapper around WeaponTrace to handle trying to do a ray trace before falling back to a sweep trace if there were no hits and SweepRadius is above zero 
    // 核心射线包装器：优先尝试线射线检测，如果没打中且设置了SweepRadius，则回退到带有容错范围的球体扫描检测，用于提升射击手感
    FHitResult DoSingleBulletTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHits) const;

    // Traces all of the bullets in a single cartridge
    // 追踪一个弹药筒（例如散弹枪的一发弹药包含多枚弹丸）中的所有子弹，计算散布并执行射线
    void TraceBulletsInCartridge(const FRangedWeaponFiringInput& InputData, OUT TArray<FHitResult>& OutHits);

    virtual void AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const;

    // Determine the trace channel to use for the weapon trace(s)
    virtual ECollisionChannel DetermineTraceChannel(FCollisionQueryParams& TraceParams, bool bIsSimulated) const;

    // 本地执行目标检测的核心函数，生成开火输入数据并调用 TraceBulletsInCartridge
    void PerformLocalTargeting(OUT TArray<FHitResult>& OutHits);

    FVector GetWeaponTargetingSourceLocation() const;
    
    // 计算并获取瞄准的 Transform（起点与朝向），处理摄像机到枪口的射线偏移逻辑
    FTransform GetTargetingTransform(APawn* SourcePawn, ELyraAbilityTargetingSource Source) const;

    // 命中数据准备完毕的回调：处理 GAS 目标数据，打包发给服务器，并触发蓝图效果
    void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

    UFUNCTION(BlueprintCallable)
    void StartRangedWeaponTargeting();

    // Called when target data is ready
    // 蓝图可实现事件：用于在蓝图层应用 Gameplay Effect（造成伤害等）
    UFUNCTION(BlueprintImplementableEvent)
    void OnRangedWeaponTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

private:
    FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
};