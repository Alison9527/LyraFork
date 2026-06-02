// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbility_RangedWeapon.h"
#include "Weapons/LyraRangedWeaponInstance.h"
#include "Physics/LyraCollisionChannels.h"
#include "LyraLogChannels.h"
#include "AIController.h"
#include "NativeGameplayTags.h"
#include "Weapons/LyraWeaponStateComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/LyraGameplayAbilityTargetData_SingleTargetHit.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_RangedWeapon)

namespace LyraConsoleVariables
{
    static float DrawBulletTracesDuration = 0.0f;
    static FAutoConsoleVariableRef CVarDrawBulletTraceDuraton(
       TEXT("lyra.Weapon.DrawBulletTraceDuration"),
       DrawBulletTracesDuration,
       TEXT("Should we do debug drawing for bullet traces (if above zero, sets how long (in seconds))"),
       ECVF_Default);

    static float DrawBulletHitDuration = 0.0f;
    static FAutoConsoleVariableRef CVarDrawBulletHits(
       TEXT("lyra.Weapon.DrawBulletHitDuration"),
       DrawBulletHitDuration,
       TEXT("Should we do debug drawing for bullet impacts (if above zero, sets how long (in seconds))"),
       ECVF_Default);

    static float DrawBulletHitRadius = 3.0f;
    static FAutoConsoleVariableRef CVarDrawBulletHitRadius(
       TEXT("lyra.Weapon.DrawBulletHitRadius"),
       DrawBulletHitRadius,
       TEXT("When bullet hit debug drawing is enabled (see DrawBulletHitDuration), how big should the hit radius be? (in uu)"),
       ECVF_Default);
}

// Weapon fire will be blocked/canceled if the player has this tag
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponFireBlocked, "Ability.Weapon.NoFiring");

//////////////////////////////////////////////////////////////////////

/** * 计算弹道散布方向的核心算法
 * 采用锥形正态分布，引入指数(Exponent)确保子弹更大概率密集在准星中心
 */
FVector VRandConeNormalDistribution(const FVector& Dir, const float ConeHalfAngleRad, const float Exponent)
{
    if (ConeHalfAngleRad > 0.f)
    {
       const float ConeHalfAngleDegrees = FMath::RadiansToDegrees(ConeHalfAngleRad);

       // consider the cone a concatenation of two rotations. one "away" from the center line, and another "around" the circle
       // apply the exponent to the away-from-center rotation. a larger exponent will cluster points more tightly around the center
       // 引入 Exponent（指数）。Exponent 越大，随机结果越倾向于 0，子弹越容易打在准星中心。
       const float FromCenter = FMath::Pow(FMath::FRand(), Exponent);
       const float AngleFromCenter = FromCenter * ConeHalfAngleDegrees; // 偏离中心轴的俯仰/偏航角
       const float AngleAround = FMath::FRand() * 360.0f;               // 绕着中心轴的滚转角 (0-360度)

       FRotator Rot = Dir.Rotation();
       FQuat DirQuat(Rot); // 原始瞄准方向
       FQuat FromCenterQuat(FRotator(0.0f, AngleFromCenter, 0.0f)); // 偏离中心的旋转
       FQuat AroundQuat(FRotator(0.0f, 0.0, AngleAround));          // 绕圆周的旋转
       
       // 四元数乘法组合旋转：先绕圈转，再偏离中心，最后对齐到武器当前朝向
       FQuat FinalDirectionQuat = DirQuat * AroundQuat * FromCenterQuat;
       FinalDirectionQuat.Normalize();

       // 还原为世界空间下的方向向量
       return FinalDirectionQuat.RotateVector(FVector::ForwardVector);
    }
    else
    {
       return Dir.GetSafeNormal();
    }
}


ULyraGameplayAbility_RangedWeapon::ULyraGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SourceBlockedTags.AddTag(TAG_WeaponFireBlocked);
}

ULyraRangedWeaponInstance* ULyraGameplayAbility_RangedWeapon::GetWeaponInstance() const
{
    return Cast<ULyraRangedWeaponInstance>(GetAssociatedEquipment());
}

bool ULyraGameplayAbility_RangedWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    bool bResult = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

    if (bResult)
    {
       if (GetWeaponInstance() == nullptr)
       {
          UE_LOG(LogLyraAbilitySystem, Error, TEXT("Weapon ability %s cannot be activated because there is no associated ranged weapon (equipment instance=%s but needs to be derived from %s)"),
             *GetPathName(),
             *GetPathNameSafe(GetAssociatedEquipment()),
             *ULyraRangedWeaponInstance::StaticClass()->GetName());
          bResult = false;
       }
    }

    return bResult;
}

int32 ULyraGameplayAbility_RangedWeapon::FindFirstPawnHitResult(const TArray<FHitResult>& HitResults)
{
    for (int32 Idx = 0; Idx < HitResults.Num(); ++Idx)
    {
       const FHitResult& CurHitResult = HitResults[Idx];
       if (CurHitResult.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
       {
          // If we hit a pawn, we're good
          return Idx;
       }
       else
       {
          AActor* HitActor = CurHitResult.HitObjectHandle.FetchActor();
          if ((HitActor != nullptr) && (HitActor->GetAttachParentActor() != nullptr) && (Cast<APawn>(HitActor->GetAttachParentActor()) != nullptr))
          {
             // If we hit something attached to a pawn, we're good
             return Idx;
          }
       }
    }

    return INDEX_NONE;
}

void ULyraGameplayAbility_RangedWeapon::AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const
{
    if (AActor* Avatar = GetAvatarActorFromActorInfo())
    {
       // Ignore any actors attached to the avatar doing the shooting
       TArray<AActor*> AttachedActors;
       Avatar->GetAttachedActors(/*out*/ AttachedActors);
       TraceParams.AddIgnoredActors(AttachedActors);
    }
}

ECollisionChannel ULyraGameplayAbility_RangedWeapon::DetermineTraceChannel(FCollisionQueryParams& TraceParams, bool bIsSimulated) const
{
    return Lyra_TraceChannel_Weapon;
}

FHitResult ULyraGameplayAbility_RangedWeapon::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHitResults) const
{
    TArray<FHitResult> HitResults;
    
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), /*bTraceComplex=*/ true, /*IgnoreActor=*/ GetAvatarActorFromActorInfo());
    TraceParams.bReturnPhysicalMaterial = true;
    AddAdditionalTraceIgnoreActors(TraceParams);
    //TraceParams.bDebugQuery = true;

    const ECollisionChannel TraceChannel = DetermineTraceChannel(TraceParams, bIsSimulated);

    if (SweepRadius > 0.0f)
    {
       GetWorld()->SweepMultiByChannel(HitResults, StartTrace, EndTrace, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(SweepRadius), TraceParams);
    }
    else
    {
       GetWorld()->LineTraceMultiByChannel(HitResults, StartTrace, EndTrace, TraceChannel, TraceParams);
    }

    FHitResult Hit(ForceInit);
    if (HitResults.Num() > 0)
    {
       // Filter the output list to prevent multiple hits on the same actor;
       // this is to prevent a single bullet dealing damage multiple times to
       // a single actor if using an overlap trace
       // 过滤输出列表以防止对同一 Actor 多次命中（特别是使用了 Sweep 扫描时），防止单发子弹造成多次伤害
       for (FHitResult& CurHitResult : HitResults)
       {
          auto Pred = [&CurHitResult](const FHitResult& Other)
          {
             return Other.HitObjectHandle == CurHitResult.HitObjectHandle;
          };

          if (!OutHitResults.ContainsByPredicate(Pred))
          {
             OutHitResults.Add(CurHitResult);
          }
       }

       Hit = OutHitResults.Last();
    }
    else
    {
       Hit.TraceStart = StartTrace;
       Hit.TraceEnd = EndTrace;
    }

    return Hit;
}

FVector ULyraGameplayAbility_RangedWeapon::GetWeaponTargetingSourceLocation() const
{
    // Use Pawn's location as a base
    APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
    check(AvatarPawn);

    const FVector SourceLoc = AvatarPawn->GetActorLocation();
    const FQuat SourceRot = AvatarPawn->GetActorQuat();

    FVector TargetingSourceLocation = SourceLoc;

    //@TODO: Add an offset from the weapon instance and adjust based on pawn crouch/aiming/etc...

    return TargetingSourceLocation;
}

FTransform ULyraGameplayAbility_RangedWeapon::GetTargetingTransform(APawn* SourcePawn, ELyraAbilityTargetingSource Source) const
{
    check(SourcePawn);
    AController* SourcePawnController = SourcePawn->GetController(); 
    ULyraWeaponStateComponent* WeaponStateComponent = (SourcePawnController != nullptr) ? SourcePawnController->FindComponentByClass<ULyraWeaponStateComponent>() : nullptr;

    // The caller should determine the transform without calling this if the mode is custom!
    check(Source != ELyraAbilityTargetingSource::Custom);

    const FVector ActorLoc = SourcePawn->GetActorLocation();
    FQuat AimQuat = SourcePawn->GetActorQuat();
    AController* Controller = SourcePawn->GetController();
    FVector SourceLoc;

    double FocalDistance = 1024.0f;
    FVector FocalLoc;

    FVector CamLoc;
    FRotator CamRot;
    bool bFoundFocus = false;


    if ((Controller != nullptr) && ((Source == ELyraAbilityTargetingSource::CameraTowardsFocus) || (Source == ELyraAbilityTargetingSource::PawnTowardsFocus) || (Source == ELyraAbilityTargetingSource::WeaponTowardsFocus)))
    {
       // Get camera position for later
       bFoundFocus = true;

       APlayerController* PC = Cast<APlayerController>(Controller);
       if (PC != nullptr)
       {
          PC->GetPlayerViewPoint(/*out*/ CamLoc, /*out*/ CamRot);
       }
       else
       {
          SourceLoc = GetWeaponTargetingSourceLocation();
          CamLoc = SourceLoc;
          CamRot = Controller->GetControlRotation();
       }

       // Determine initial focal point
       // 基于摄像机朝向，向远方延伸 1024.0 个单位，作为初始瞄准焦点 (FocalLoc)
       FVector AimDir = CamRot.Vector().GetSafeNormal();
       FocalLoc = CamLoc + (AimDir * FocalDistance);

       // Move the start and focal point up in front of pawn
       // 核心投影运算：将射线起点从摄像机位置，平移投影到角色身前。避免摄像机射线被身后的墙壁等阻挡。
       if (PC)
       {
          const FVector WeaponLoc = GetWeaponTargetingSourceLocation();
          CamLoc = FocalLoc + (((WeaponLoc - FocalLoc) | AimDir) * AimDir);
          FocalLoc = CamLoc + (AimDir * FocalDistance);
       }
       //Move the start to be the HeadPosition of the AI
       else if (AAIController* AIController = Cast<AAIController>(Controller))
       {
          CamLoc = SourcePawn->GetActorLocation() + FVector(0, 0, SourcePawn->BaseEyeHeight);
       }

       if (Source == ELyraAbilityTargetingSource::CameraTowardsFocus)
       {
          // If we're camera -> focus then we're done
          return FTransform(CamRot, CamLoc);
       }
    }

    if ((Source == ELyraAbilityTargetingSource::WeaponForward) || (Source == ELyraAbilityTargetingSource::WeaponTowardsFocus))
    {
       SourceLoc = GetWeaponTargetingSourceLocation();
    }
    else
    {
       // Either we want the pawn's location, or we failed to find a camera
       SourceLoc = ActorLoc;
    }

    if (bFoundFocus && ((Source == ELyraAbilityTargetingSource::PawnTowardsFocus) || (Source == ELyraAbilityTargetingSource::WeaponTowardsFocus)))
    {
       // Return a rotator pointing at the focal point from the source
       return FTransform((FocalLoc - SourceLoc).Rotation(), SourceLoc);
    }

    // If we got here, either we don't have a camera or we don't want to use it, either way go forward
    return FTransform(AimQuat, SourceLoc);
}

FHitResult ULyraGameplayAbility_RangedWeapon::DoSingleBulletTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHits) const
{
#if ENABLE_DRAW_DEBUG
    if (LyraConsoleVariables::DrawBulletTracesDuration > 0.0f)
    {
       static float DebugThickness = 1.0f;
       DrawDebugLine(GetWorld(), StartTrace, EndTrace, FColor::Red, false, LyraConsoleVariables::DrawBulletTracesDuration, 0, DebugThickness);
    }
#endif // ENABLE_DRAW_DEBUG

    FHitResult Impact;

    // Trace and process instant hit if something was hit
    // First trace without using sweep radius
    // 第一步：执行完美的线性射线检测 (Line Trace，无宽度)
    if (FindFirstPawnHitResult(OutHits) == INDEX_NONE)
    {
       Impact = WeaponTrace(StartTrace, EndTrace, /*SweepRadius=*/ 0.0f, bIsSimulated, /*out*/ OutHits);
    }

    if (FindFirstPawnHitResult(OutHits) == INDEX_NONE)
    {
       // If this weapon didn't hit anything with a line trace and supports a sweep radius, try that
       // 第二步：如果线性检测没有打中玩家，且武器设置了 SweepRadius 容错半径，则执行球体检测 (Sphere Trace)
       if (SweepRadius > 0.0f)
       {
          TArray<FHitResult> SweepHits;
          Impact = WeaponTrace(StartTrace, EndTrace, SweepRadius, bIsSimulated, /*out*/ SweepHits);

          // If the trace with sweep radius enabled hit a pawn, check if we should use its hit results
          const int32 FirstPawnIdx = FindFirstPawnHitResult(SweepHits);
          if (SweepHits.IsValidIndex(FirstPawnIdx))
          {
             // If we had a blocking hit in our line trace that occurs in SweepHits before our
             // hit pawn, we should just use our initial hit results since the Pawn hit should be blocked
             // 容错验证逻辑：确保子弹在碰到玩家前，没有被墙壁挡住。
             // 如果在 SweepHits 中遇到玩家之前的阻挡物，在线性检测中也阻挡了射线，则不视为命中。
             bool bUseSweepHits = true;
             for (int32 Idx = 0; Idx < FirstPawnIdx; ++Idx)
             {
                const FHitResult& CurHitResult = SweepHits[Idx];

                auto Pred = [&CurHitResult](const FHitResult& Other)
                {
                   return Other.HitObjectHandle == CurHitResult.HitObjectHandle;
                };
                if (CurHitResult.bBlockingHit && OutHits.ContainsByPredicate(Pred))
                {
                   bUseSweepHits = false;
                   break;
                }
             }

             if (bUseSweepHits)
             {
                OutHits = SweepHits; // 覆盖结果，算作命中宽容度内的目标
             }
          }
       }
    }

    return Impact;
}

void ULyraGameplayAbility_RangedWeapon::PerformLocalTargeting(OUT TArray<FHitResult>& OutHits)
{
    // 1. 从当前的技能 Actor 信息中获取拥有该技能的 Pawn。
    APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

    // 2. 获取当前关联的远程武器实例数据（包含了射程、散布、伤害等数据驱动配置）。
    ULyraRangedWeaponInstance* WeaponData = GetWeaponInstance();
    
    // 3. 【核心网络检测】确保 Pawn 存在，确保该 Pawn 是本地控制的（IsLocallyControlled），且武器数据有效。
    // 这意味着：如果是其他玩家（模拟代理 Simulated Proxy）开火，或者 Dedicated Server 自身，不会走进这个分支。
    // 真正的射线检测是由“按下鼠标”的那个客户端在本地立刻执行的。
    if (AvatarPawn && AvatarPawn->IsLocallyControlled() && WeaponData)
    {
       // 4. 初始化单次开火的输入参数结构体
       FRangedWeaponFiringInput InputData;
       InputData.WeaponData = WeaponData; // 绑定数据源
       
       // 5. 性能优化：如果是 Dedicated Server（纯逻辑服务器，没有渲染），则禁止播放子弹相关的视觉特效（如枪口火焰、曳光弹）。
       InputData.bCanPlayBulletFX = (AvatarPawn->GetNetMode() != NM_DedicatedServer);

       //@TODO: Epic 留下的拓展接口说明。比如当玩家贴脸靠近墙壁时，枪管可能会穿模，
       // 这里可以做额外的判断逻辑，把开火点往后移或者强制抬枪（High Ready 姿态）。

       // 6. 获取瞄准的初始 Transform（包含起点坐标和朝向）。
       // 策略 ELyraAbilityTargetingSource::CameraTowardsFocus 表示：
       // 射线理论上从摄像机出发，射向屏幕正中心的准星焦点。同时底层逻辑会将这个射线的物理起点平移投射到角色身前，防止打到玩家自己的后脑勺。
       const FTransform TargetTransform = GetTargetingTransform(AvatarPawn, ELyraAbilityTargetingSource::CameraTowardsFocus);
       
       // 7. 提取完美瞄准（无散布）情况下的基准方向向量（X轴正方向）。
       InputData.AimDir = TargetTransform.GetUnitAxis(EAxis::X);
       
       // 8. 设置物理射线检测的绝对起点。
       InputData.StartTrace = TargetTransform.GetTranslation();

       // 9. 计算完美瞄准情况下的射线终点 = 起点 + (方向向量 * 武器最大射程)
       InputData.EndAim = InputData.StartTrace + InputData.AimDir * WeaponData->GetMaxDamageRange();

// 10. 编译期宏控制的 Debug 绘制逻辑，仅在非 Shipping 版本生效。
#if ENABLE_DRAW_DEBUG
       // 如果控制台变量（CVar）设置了绘制持续时间大于 0
       if (LyraConsoleVariables::DrawBulletTracesDuration > 0.0f)
       {
          static float DebugThickness = 2.0f;
          // 在世界中画一条黄色的线，展示瞄准的基准方向（只画前 100 单位长度示意一下）
          DrawDebugLine(GetWorld(), InputData.StartTrace, InputData.StartTrace + (InputData.AimDir * 100.0f), FColor::Yellow, false, LyraConsoleVariables::DrawBulletTracesDuration, 0, DebugThickness);
       }
#endif

       // 11. 将组装好的 InputData 传递给下一步，由它来处理多发弹丸（如散弹枪）、弹道散布锥角，并执行真正的 GetWorld()->LineTraceMultiByChannel。
       // 结果将填充到传出参数 OutHits 中。
       TraceBulletsInCartridge(InputData, /*out*/ OutHits);
    }
}

void ULyraGameplayAbility_RangedWeapon::TraceBulletsInCartridge(const FRangedWeaponFiringInput& InputData, OUT TArray<FHitResult>& OutHits)
{
    // 1. 从传入的开火输入数据中提取武器实例指针
    ULyraRangedWeaponInstance* WeaponData = InputData.WeaponData;
    
    // 2. 安全检查，确保武器数据有效（开发期断言，避免空指针崩溃）
    check(WeaponData);

    // 3. 【核心配置】获取当前“一个弹药筒”包含多少发子弹
    // 例如：常规步枪这里是 1，而霰弹枪这里可能是 8（一次开火射出 8 发弹丸）
    const int32 BulletsPerCartridge = WeaponData->GetBulletsPerCartridge();

    // 4. 遍历当前弹药筒中的每一发子弹
    for (int32 BulletIndex = 0; BulletIndex < BulletsPerCartridge; ++BulletIndex)
    {
       // 5. 获取武器的基础散布角度（可能是由于玩家跑动、开火连发等导致的动态基础散布）
       const float BaseSpreadAngle = WeaponData->GetCalculatedSpreadAngle();
       
       // 6. 获取散布角度的乘数（可能来自天赋、配件系统、或者瞄准状态的修正）
       const float SpreadAngleMultiplier = WeaponData->GetCalculatedSpreadAngleMultiplier();
       
       // 7. 计算最终的实际散布角度
       const float ActualSpreadAngle = BaseSpreadAngle * SpreadAngleMultiplier;

       // 8. 数学运算：将全角转换为半角，并从角度转换为弧度，准备传递给圆锥随机函数
       const float HalfSpreadAngleInRadians = FMath::DegreesToRadians(ActualSpreadAngle * 0.5f);

       // 9. 【计算散布方向】调用之前解析过的锥形正态分布算法
       // 传入完美瞄准方向 (AimDir)、散布半角、以及散布指数 (Exponent，用于让子弹更集中于中心)
       // 返回这颗特定弹丸在散布圆锥内的最终绝对飞行方向
       const FVector BulletDir = VRandConeNormalDistribution(InputData.AimDir, HalfSpreadAngleInRadians, WeaponData->GetSpreadExponent());

       // 10. 计算这发子弹的物理射线终点 = 起点 + (最终散布方向 * 武器最大射程)
       const FVector EndTrace = InputData.StartTrace + (BulletDir * WeaponData->GetMaxDamageRange());
       
       // 11. 初始化一个命中位置，默认为射线的理论终点
       FVector HitLocation = EndTrace;

       // 12. 准备一个局部数组，用于接收这一发子弹可能穿透/命中的多个目标结果
       TArray<FHitResult> AllImpacts;

       // 13. 【执行物理检测】调用核心包装函数 DoSingleBulletTrace
       // 它内部会处理 LineTrace 和 SweepRadius（宽子弹）的双重容错检测逻辑
       FHitResult Impact = DoSingleBulletTrace(InputData.StartTrace, EndTrace, WeaponData->GetBulletTraceSweepRadius(), /*bIsSimulated=*/ false, /*out*/ AllImpacts);

       // 14. 检查射线检测返回的最终 HitResult 中是否包含有效的 Actor
       const AActor* HitActor = Impact.GetActor();

       if (HitActor) // 如果子弹确实打中了一个实体
       {
// 15. 如果开启了 Debug 绘制宏
#if ENABLE_DRAW_DEBUG
          // 并且通过控制台变量（CVar）设置了命中点的绘制持续时间大于 0
          if (LyraConsoleVariables::DrawBulletHitDuration > 0.0f)
          {
             // 在世界空间中，子弹命中的位置画一个红色的点
             DrawDebugPoint(GetWorld(), Impact.ImpactPoint, LyraConsoleVariables::DrawBulletHitRadius, FColor::Red, false, LyraConsoleVariables::DrawBulletHitRadius);
          }
#endif

          // 16. 如果局部数组中有命中结果（防止 DoSingleBulletTrace 内部逻辑修改导致为空）
          if (AllImpacts.Num() > 0)
          {
             // 将这发子弹的所有命中结果追加到函数对外的总输出数组 OutHits 中
             OutHits.Append(AllImpacts);
          }

          // 17. 更新实际命中位置
          HitLocation = Impact.ImpactPoint;
       }

       // 18. 【核心特效支持逻辑】确保 OutHits 中始终至少有一条关于这发子弹的记录，哪怕它打向了天空
       // 原因：客户端表现层需要方向数据来生成曳光弹 (Tracers) 或其他飞行弹道特效
       if (OutHits.Num() == 0) 
       {
          // 19. 如果子弹没有发生阻挡碰撞 (bBlockingHit == false)，即打到了虚空里
          if (!Impact.bBlockingHit)
          {
             // Locate the fake 'impact' at the end of the trace
             // 伪造一个“命中”，将命中位置强行设置在射程最远端的理论终点
             Impact.Location = EndTrace;
             Impact.ImpactPoint = EndTrace;
          }

          // 20. 将这个伪造的（或未造成实质影响的）结果添加到 OutHits，供下游蓝图提取终点坐标画射线特效
          OutHits.Add(Impact);
       }
    }
}

void ULyraGameplayAbility_RangedWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Bind target data callback
    UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
    check(MyAbilityComponent);

    OnTargetDataReadyCallbackDelegateHandle = MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

    // Update the last firing time
    ULyraRangedWeaponInstance* WeaponData = GetWeaponInstance();
    check(WeaponData);
    WeaponData->UpdateFiringTime();

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void ULyraGameplayAbility_RangedWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (IsEndAbilityValid(Handle, ActorInfo))
    {
       if (ScopeLockCount > 0)
       {
          WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
          return;
       }

       UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
       check(MyAbilityComponent);

       // When ability ends, consume target data and remove delegate
       MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).Remove(OnTargetDataReadyCallbackDelegateHandle);
       MyAbilityComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

       Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    }
}

void ULyraGameplayAbility_RangedWeapon::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
    // 1. 获取当前 Actor 的能力系统组件 (ASC)
    UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
    check(MyAbilityComponent);

    // 2. 确保当前技能实例 (AbilitySpec) 依然有效
    if (const FGameplayAbilitySpec* AbilitySpec = MyAbilityComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
    {
       // 3. 【核心预测机制】开启一个作用域级别的客户端预测窗口
       // 这告诉 GAS 引擎：“接下来的操作（如扣除弹药、触发特效）是在客户端本地预测执行的”。
       // 它会利用当前的 PredictionKey 将这些操作标记起来，等待服务器后续的确认或回滚。
       FScopedPredictionWindow ScopedPrediction(MyAbilityComponent);

       // 4. 【内存安全】接管 TargetData 的所有权
       // 使用 MoveTemp (类似 std::move) 获取传入数据的控制权。这非常关键，因为网络游戏环境复杂，
       // 这能防止其他潜在的游戏代码回调在底层突然清空或修改这份数据，导致指针悬空。
       FGameplayAbilityTargetDataHandle LocalTargetDataHandle(MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

       // 5. 【网络通信】判断当前是否是本地控制的客户端（且不是权威服务器）
       const bool bShouldNotifyServer = CurrentActorInfo->IsLocallyControlled() && !CurrentActorInfo->IsNetAuthority();
       if (bShouldNotifyServer)
       {
          // 6. 发送 RPC 给服务器，将本地预测的 TargetData（打中了谁、打中了哪里）同步过去
          MyAbilityComponent->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), LocalTargetDataHandle, ApplicationTag, MyAbilityComponent->ScopedPredictionKey);
       }

       const bool bIsTargetDataValid = true;
       bool bProjectileWeapon = false; // 当前默认当作即时命中（Hitscan）武器处理

// 7. 【服务器专属逻辑】使用宏隔绝代码，防止纯客户端包包含服务器的验证逻辑（防作弊与减小包体）
#if WITH_SERVER_CODE
       if (!bProjectileWeapon)
       {
          // 获取控制权，并判断当前执行环境是否为权威服务器 (ROLE_Authority)
          if (AController* Controller = GetControllerFromActorInfo())
          {
             if (Controller->GetLocalRole() == ROLE_Authority)
             {
                // 8. 【服务器确认命中标记】
                // 服务器收到客户端的 Hitscan 数据后，在这里进行校验并通知客户端的 UI 组件
                if (ULyraWeaponStateComponent* WeaponStateComponent = Controller->FindComponentByClass<ULyraWeaponStateComponent>())
                {
                   TArray<uint8> HitReplaces;
                   // 遍历所有命中数据，收集那些被服务器“替换/修正”过的命中记录（通常与延迟补偿/倒带系统有关）
                   for (uint8 i = 0; (i < LocalTargetDataHandle.Num()) && (i < 255); ++i)
                   {
                      if (FGameplayAbilityTargetData_SingleTargetHit* SingleTargetHit = static_cast<FGameplayAbilityTargetData_SingleTargetHit*>(LocalTargetDataHandle.Get(i)))
                      {
                         if (SingleTargetHit->bHitReplaced)
                         {
                            HitReplaces.Add(i);
                         }
                      }
                   }

                   // 9. 服务器调用 RPC 告诉客户端：“你传来的 UniqueId 对应的命中我验证过了，是真的，把灰准星变成红准星吧”
                   WeaponStateComponent->ClientConfirmTargetData(LocalTargetDataHandle.UniqueId, bIsTargetDataValid, HitReplaces);
                }
             }
          }
       }
#endif //WITH_SERVER_CODE

       // 10. 【技能执行】CommitAbility 会检查并应用技能消耗（比如：扣除备弹）和冷却时间
       // 只有 Commit 成功，才说明这枪真正开出来了
       if (bIsTargetDataValid && CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
       {
          // 11. 武器成功开火，增加枪械的后坐力/散布值
          ULyraRangedWeaponInstance* WeaponData = GetWeaponInstance();
          check(WeaponData);
          WeaponData->AddSpread();

          // 12. 【核心伤害触发】调用蓝图可实现事件
          // 这是 C++ 交接给策划蓝图的终点。蓝图会在这里接收 TargetData，并向目标应用 GameplayEffect（如扣血 25 点）。
          OnRangedWeaponTargetDataReady(LocalTargetDataHandle);
       }
       else
       {
          // 13. 如果 Commit 失败（例如没子弹了），则打印警告，并强制结束技能，取消预测表现
          UE_LOG(LogLyraAbilitySystem, Warning, TEXT("Weapon ability %s failed to commit (bIsTargetDataValid=%d)"), *GetPathName(), bIsTargetDataValid ? 1 : 0);
          K2_EndAbility();
       }
    }

    // 14. 【清理工作】我们已经处理完了这份 TargetData，通知 ASC 消费掉它，释放内存并移除对应的委托
    MyAbilityComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
}

void ULyraGameplayAbility_RangedWeapon::StartRangedWeaponTargeting()
{
    // 1. 获取并校验当前技能的 Actor 信息。check() 宏在非 Shipping 版本中如果失败会直接引发断言崩溃，
    // 这是保证底层指针安全的严谨 C++ 工程实践，比满篇的 if(ptr == nullptr) 更加高效和清晰。
    check(CurrentActorInfo);

    // 2. 获取施放该技能的 AvatarActor（通常是玩家操作的 Pawn）
    AActor* AvatarActor = CurrentActorInfo->AvatarActor.Get();
    check(AvatarActor);

    // 3. 获取能力系统组件 (ASC, Ability System Component)
    UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
    check(MyAbilityComponent);

    // 4. 获取控制器，并尝试从中找到负责管理武器状态的组件 (ULyraWeaponStateComponent)
    AController* Controller = GetControllerFromActorInfo();
    check(Controller);
    ULyraWeaponStateComponent* WeaponStateComponent = Controller->FindComponentByClass<ULyraWeaponStateComponent>();

    // 5. 【核心机制】开启 GAS 的预测窗口 (Scoped Prediction Window)
    // 这一步告诉 ASC：“接下来的所有操作（包括生成数据、触发事件）都是客户端预测的，请生成一个 PredictionKey，服务器稍后会用它来验证/回滚”。
    FScopedPredictionWindow ScopedPrediction(MyAbilityComponent, CurrentActivationInfo.GetActivationPredictionKey());

    // 6. 准备一个数组来接收射线检测的物理结果
    TArray<FHitResult> FoundHits;
    
    // 7. 执行本地射线检测（包含散布计算、双重射线容错等），结果填充到 FoundHits 中
    PerformLocalTargeting(/*out*/ FoundHits);

    // 8. 准备将物理检测结果 (FHitResult) 转换为 GAS 可网络传输的目标数据 (TargetData)
    FGameplayAbilityTargetDataHandle TargetData;
    
    // 9. 为这批 TargetData 分配一个唯一 ID，这个 ID 主要用于命中反馈 UI（Hit Marker）的服务器确认机制
    TargetData.UniqueId = WeaponStateComponent ? WeaponStateComponent->GetUnconfirmedServerSideHitMarkerCount() : 0;

    // 10. 如果射线打中了东西
    if (FoundHits.Num() > 0)
    {
       // 11. 【核心机制】生成一个随机的弹药筒 ID (CartridgeID)
       const int32 CartridgeID = FMath::Rand();

       // 12. 遍历所有打中的物理结果
       for (const FHitResult& FoundHit : FoundHits)
       {
          // 13. 创建 Lyra 自定义的单体命中数据结构
          FLyraGameplayAbilityTargetData_SingleTargetHit* NewTargetData = new FLyraGameplayAbilityTargetData_SingleTargetHit();
          NewTargetData->HitResult = FoundHit;           // 存入具体的物理碰撞信息（位置、法线、骨骼等）
          NewTargetData->CartridgeID = CartridgeID;      // 存入刚刚生成的弹药筒 ID

          // 14. 将单个命中数据添加到 TargetData 集合中
          TargetData.Add(NewTargetData);
       }
    }

    // 15. 【客户端表现】如果找到了武器状态组件，将这些命中数据记录为“未确认的命中标记”
    // 这会在玩家屏幕上立刻画出一个灰色的“X”命中提示，提供瞬时的打击反馈
    if (WeaponStateComponent != nullptr)
    {
       WeaponStateComponent->AddUnconfirmedServerSideHitMarkers(TargetData, FoundHits);
    }

    // 16. 将打包好的 TargetData 交给之前绑定的回调函数处理（包含发送给服务器、扣除子弹、触发蓝图伤害等逻辑）
    OnTargetDataReadyCallback(TargetData, FGameplayTag());
}