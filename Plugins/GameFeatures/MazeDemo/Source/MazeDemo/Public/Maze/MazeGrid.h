// Copyright MazeDemo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/IntPoint.h"

#include "MazeGrid.generated.h"

/**
 * FMazeGrid —— 纯数据层。
 *
 * 迷宫用「单元网格（unit grid）」表示：一张 Width x Height 的格子图，
 * 每个格子非墙即地。奇数坐标代表房间单元，偶数坐标代表它们之间的墙，
 * 因此 Width / Height 恒为奇数：
 *
 *      Width  = CellsX * 2 + 1
 *      Height = CellsY * 2 + 1
 *
 * 这样表示的好处是渲染层不需要任何额外换算：
 * 遍历 Units，凡是 1 就摆一根墙体实例，凡是 0 就是地面。
 *
 * 确定性要求（这是整个「只同步种子」方案的地基）：
 *   - 只能使用 FRandomStream(InSeed)，禁用 FMath::Rand / FMath::FRand
 *   - 禁止依赖迭代顺序不稳定的容器（TSet / TMap），本类一律使用定长数组与 TArray
 *   - 禁止引入时间、DeltaTime、并行结果
 * 违反任何一条，服务端与客户端就会生成出不一样的地形。
 */
USTRUCT(BlueprintType)
struct FMazeGrid
{
	GENERATED_BODY()

	/** 单元网格宽度（CellsX * 2 + 1） */
	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	int32 Width = 0;

	/** 单元网格高度（CellsY * 2 + 1） */
	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	int32 Height = 0;

	/** 逻辑房间数（横向），Width = CellsX * 2 + 1 */
	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	int32 CellsX = 0;

	/** 逻辑房间数（纵向），Height = CellsY * 2 + 1 */
	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	int32 CellsY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	int32 Seed = 0;

	/**
	 * 环路比例 [0,1]。0 = 完美迷宫（全是死胡同，被追必死）；
	 * 0.15 左右最耐玩，保留死胡同作为埋点，同时给出绕环甩脱的空间。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Maze")
	float LoopChance = 0.f;

	/** 0 = 地面, 1 = 墙。行主序，Index(X, Y) = Y * Width + X */
	UPROPERTY()
	TArray<uint8> Units;

	// ------------------------------------------------------------------
	// 索引与查询
	// ------------------------------------------------------------------

	int32 Index(int32 X, int32 Y) const { return Y * Width + X; }

	bool IsValidCoord(int32 X, int32 Y) const { return (X >= 0) && (X < Width) && (Y >= 0) && (Y < Height); }

	/** 越界一律视为墙，避免调用方到处写边界检查 */
	bool IsWall(int32 X, int32 Y) const { return !IsValidCoord(X, Y) || (Units[Index(X, Y)] != 0); }

	bool IsFloor(int32 X, int32 Y) const { return IsValidCoord(X, Y) && (Units[Index(X, Y)] == 0); }

	void ResetAllWalls();
	int32 NumUnits() const { return Units.Num(); }
	bool IsEmpty() const { return Units.Num() == 0; }

	int32 CountFloorUnits() const;

	/** 房间坐标 -> 单元坐标 */
	static int32 CellToUnitCoord(int32 CellCoord) { return CellCoord * 2 + 1; }

	FIntPoint CellToUnit(int32 CellX, int32 CellY) const { return FIntPoint(CellToUnitCoord(CellX), CellToUnitCoord(CellY)); }

	// ------------------------------------------------------------------
	// 生成
	// ------------------------------------------------------------------

	/**
	 * 用确定性算法生成迷宫。同一组 (Seed, CellsX, CellsY, LoopChance) 在任何端、
	 * 任何时刻都必须产出 ComputeHash() 完全一致的网格。
	 * @return 参数非法时返回 false
	 */
	bool Generate(int32 InSeed, int32 InCellsX, int32 InCellsY, float InLoopChance = 0.15f);

	// ------------------------------------------------------------------
	// 派生数据
	// ------------------------------------------------------------------

	/**
	 * 以某个可通行单元为起点做四邻域 BFS，得到全图的步数距离场。
	 * 不可达（或被墙隔开）的单元为 INDEX_NONE。
	 * 玩法的「刷在最远的死胡同」就是靠它实现的，不需要额外的寻路查询。
	 * @return 起点本身不是地面时返回 false
	 */
	bool ComputeDistanceField(int32 StartUnitX, int32 StartUnitY, TArray<int32>& OutDistances) const;

	/** 收集所有可通行单元坐标，按 (Y, X) 升序遍历 —— 顺序确定 */
	void GetFloorUnits(TArray<FIntPoint>& OutFloorUnits) const;

	/** FNV-1a 哈希，用于校验两端生成结果是否一致 */
	uint32 ComputeHash() const;

	/**
	 * 某个单元中心在本地空间的坐标，网格整体以原点为中心。
	 * 生成 Actor 时把 Actor 的位置加上这个值即可。
	 */
	FVector GetUnitCenterLocal(int32 UnitX, int32 UnitY, float UnitSize) const;

	/** 网格整体占地尺寸的 X / Y 分量 */
	FVector2D GetLocalExtent(float UnitSize) const { return FVector2D(Width * UnitSize, Height * UnitSize); }
};
