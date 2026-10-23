// Copyright MazeDemo. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Maze/MazeGrid.h"

#include "MazeGridLibrary.generated.h"

/**
 * 迷宫网格的蓝图/控制台入口。
 * 第一步（无美术资产、无 Actor）阶段，靠这里的函数就能完整验证和分析迷宫。
 */
UCLASS()
class UMazeGridLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** 确定性生成一个迷宫网格。默认 17x17 房间 / 15% 环路。 */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	static void GenerateMaze(FMazeGrid& OutGrid, int32 Seed, int32 CellsX = 17, int32 CellsY = 17, float LoopChance = 0.15f);

	/**
	 * 自检：同一个种子重复生成 Iterations 次，全部 Hash 一致才算通过。
	 * 这是「只同步种子」方案的地基，每次改动生成算法后都应跑一遍。
	 */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	static bool VerifyDeterminism(int32 Seed, int32 CellsX = 17, int32 CellsY = 17, float LoopChance = 0.15f, int32 Iterations = 16);

	/** 不同种子是否真的产出不同地形（防止 seed 被无意忽略） */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	static bool VerifySeedSensitivity(int32 SeedA = 1, int32 SeedB = 2, int32 CellsX = 17, int32 CellsY = 17, float LoopChance = 0.15f);

	/** 用调试方块把迷宫画出来，方便在没有墙体资产时先看清结构 */
	UFUNCTION(BlueprintCallable, Category = "Maze", meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugMaze(const UObject* WorldContextObject, const FMazeGrid& Grid, float UnitSize = 300.f, float WallHeight = 400.f, float DurationSeconds = 30.f);

	/** 转成 ASCII 字符画，用于日志。'#' = 墙, '.' = 地面 */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	static FString ToDebugString(const FMazeGrid& Grid);
};
