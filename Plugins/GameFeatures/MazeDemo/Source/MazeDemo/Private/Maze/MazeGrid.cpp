// Copyright MazeDemo. All Rights Reserved.

#include "Maze/MazeGrid.h"

#include "Math/RandomStream.h"
#include "Templates/TypeHash.h"

namespace
{
	// 固定顺序的四邻域：+X, +Y, -X, -Y
	const FIntPoint GMazeDirections[4] =
	{
		FIntPoint(1, 0),
		FIntPoint(0, 1),
		FIntPoint(-1, 0),
		FIntPoint(0, -1)
	};
}

void FMazeGrid::ResetAllWalls()
{
	Units.SetNumUninitialized(Width * Height);
	for (uint8& Unit : Units)
	{
		Unit = 1;
	}
}

int32 FMazeGrid::CountFloorUnits() const
{
	int32 Count = 0;
	for (const uint8 Unit : Units)
	{
		Count += (Unit == 0) ? 1 : 0;
	}
	return Count;
}

bool FMazeGrid::Generate(int32 InSeed, int32 InCellsX, int32 InCellsY, float InLoopChance)
{
	if ((InCellsX < 1) || (InCellsY < 1))
	{
		return false;
	}

	// 唯一允许的随机源。任何 FMath::Rand / FMath::FRand / 时间相关输入都会破坏确定性。
	FRandomStream Stream(InSeed);

	CellsX = InCellsX;
	CellsY = InCellsY;
	Seed = InSeed;
	LoopChance = FMath::Clamp(InLoopChance, 0.f, 1.f);

	Width = CellsX * 2 + 1;
	Height = CellsY * 2 + 1;
	ResetAllWalls();

	// ------------------------------------------------------------------
	// 1. 回溯DFS 生成完美迷宫（任意两点间唯一路径，全是死胡同）
	// ------------------------------------------------------------------
	TArray<uint8> Visited;
	Visited.AddZeroed(CellsX * CellsY);

	TArray<FIntPoint> Stack;

	const auto CarveUnit = [this](int32 UnitX, int32 UnitY)
	{
		Units[Index(UnitX, UnitY)] = 0;
	};

	Visited[0] = 1;
	CarveUnit(1, 1);
	Stack.Emplace(0, 0);

	while (Stack.Num() > 0)
	{
		const FIntPoint CurrentCell = Stack.Last();

		// 按固定顺序收集未访问的邻居（顺序确定 -> 结果确定）
		int32 CandidateDirs[4];
		int32 NumCandidates = 0;
		for (int32 DirIdx = 0; DirIdx < 4; ++DirIdx)
		{
			const int32 NeighborX = CurrentCell.X + GMazeDirections[DirIdx].X;
			const int32 NeighborY = CurrentCell.Y + GMazeDirections[DirIdx].Y;

			if ((NeighborX < 0) || (NeighborX >= CellsX) || (NeighborY < 0) || (NeighborY >= CellsY))
			{
				continue;
			}
			if (Visited[NeighborY * CellsX + NeighborX] != 0)
			{
				continue;
			}

			CandidateDirs[NumCandidates++] = DirIdx;
		}

		if (NumCandidates == 0)
		{
			// 死胡同，回溯
			Stack.Pop(EAllowShrinking::No);
			continue;
		}

		// Fisher-Yates 洗牌，只洗候选方向
		for (int32 i = NumCandidates - 1; i > 0; --i)
		{
			const int32 j = Stream.RandHelper(i + 1);
			Swap(CandidateDirs[i], CandidateDirs[j]);
		}

		const int32 ChosenDir = CandidateDirs[0];
		const FIntPoint NextCell(CurrentCell.X + GMazeDirections[ChosenDir].X, CurrentCell.Y + GMazeDirections[ChosenDir].Y);
		const FIntPoint CurrentUnit = CellToUnit(CurrentCell.X, CurrentCell.Y);
		const FIntPoint NextUnit = CellToUnit(NextCell.X, NextCell.Y);

		// 敲掉两格之间的那堵墙（坐标恰好是两者的中点）
		CarveUnit((CurrentUnit.X + NextUnit.X) / 2, (CurrentUnit.Y + NextUnit.Y) / 2);
		CarveUnit(NextUnit.X, NextUnit.Y);

		Visited[NextCell.Y * CellsX + NextCell.X] = 1;
		Stack.Emplace(NextCell);
	}

	// ------------------------------------------------------------------
	// 2. 开环：把完美迷宫里的一部分墙打通，制造绕行可能
	//    这里取「按数量精确切分」而不是逐墙掷骰，好处是环路总数稳定，
	//    不会因为地图变大而出现环路密度漂移。
	// ------------------------------------------------------------------
	if (LoopChance > 0.f)
	{
		TArray<FIntPoint> LoopCandidates;

		for (int32 Y = 1; Y < Height - 1; ++Y)
		{
			for (int32 X = 1; X < Width - 1; ++X)
			{
				if (Units[Index(X, Y)] == 0)
				{
					continue; // 已经是地面
				}

				const bool bEvenX = ((X % 2) == 0);
				const bool bEvenY = ((Y % 2) == 0);

				// 夹在两个房间之间的墙才可能是候选；棋盘格状的交点是承重结构，不能动
				if (bEvenX && !bEvenY)
				{
					// 竖墙：左右都是房间
					if (Units[Index(X - 1, Y)] == 0 && Units[Index(X + 1, Y)] == 0)
					{
						LoopCandidates.Emplace(X, Y);
					}
				}
				else if (!bEvenX && bEvenY)
				{
					// 横墙：上下都是房间
					if (Units[Index(X, Y - 1)] == 0 && Units[Index(X, Y + 1)] == 0)
					{
						LoopCandidates.Emplace(X, Y);
					}
				}
			}
		}

		for (int32 i = LoopCandidates.Num() - 1; i > 0; --i)
		{
			const int32 j = Stream.RandHelper(i + 1);
			LoopCandidates.Swap(i, j);
		}

		const int32 NumToOpen = FMath::RoundToInt(static_cast<float>(LoopCandidates.Num()) * LoopChance);
		for (int32 i = 0; i < NumToOpen; ++i)
		{
			const FIntPoint& Coord = LoopCandidates[i];
			Units[Index(Coord.X, Coord.Y)] = 0;
		}
	}

	return true;
}

bool FMazeGrid::ComputeDistanceField(int32 StartUnitX, int32 StartUnitY, TArray<int32>& OutDistances) const
{
	if (!IsFloor(StartUnitX, StartUnitY))
	{
		OutDistances.Reset();
		return false;
	}

	OutDistances.Init(INDEX_NONE, Width * Height);
	OutDistances[Index(StartUnitX, StartUnitY)] = 0;

	TArray<FIntPoint> Frontier;
	Frontier.Emplace(StartUnitX, StartUnitY);

	int32 CurrentDistance = 0;
	while (Frontier.Num() > 0)
	{
		TArray<FIntPoint> NextFrontier;
		++CurrentDistance;

		for (const FIntPoint& Coord : Frontier)
		{
			for (const FIntPoint& Dir : GMazeDirections)
			{
				const int32 NeighborX = Coord.X + Dir.X;
				const int32 NeighborY = Coord.Y + Dir.Y;
				if (!IsFloor(NeighborX, NeighborY))
				{
					continue;
				}

				const int32 NeighborIndex = Index(NeighborX, NeighborY);
				if (OutDistances[NeighborIndex] != INDEX_NONE)
				{
					continue;
				}

				OutDistances[NeighborIndex] = CurrentDistance;
				NextFrontier.Emplace(NeighborX, NeighborY);
			}
		}

		Frontier = MoveTemp(NextFrontier);
	}

	return true;
}

void FMazeGrid::GetFloorUnits(TArray<FIntPoint>& OutFloorUnits) const
{
	OutFloorUnits.Reset();
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (Units[Index(X, Y)] == 0)
			{
				OutFloorUnits.Emplace(X, Y);
			}
		}
	}
}

uint32 FMazeGrid::ComputeHash() const
{
	// FNV-1a
	uint32 Hash = 2166136261u;
	Hash = HashCombine(Hash, ::GetTypeHash(Width));
	Hash = HashCombine(Hash, ::GetTypeHash(Height));
	for (const uint8 Unit : Units)
	{
		Hash ^= static_cast<uint32>(Unit);
		Hash *= 16777619u;
	}
	return Hash;
}

FVector FMazeGrid::GetUnitCenterLocal(int32 UnitX, int32 UnitY, float UnitSize) const
{
	const float HalfWidth = static_cast<float>(Width) * 0.5f;
	const float HalfHeight = static_cast<float>(Height) * 0.5f;

	return FVector(
		(static_cast<float>(UnitX) + 0.5f - HalfWidth) * UnitSize,
		(static_cast<float>(UnitY) + 0.5f - HalfHeight) * UnitSize,
		0.f);
}
