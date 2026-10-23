// Copyright MazeDemo. All Rights Reserved.

#include "Maze/MazeGridLibrary.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Logging/StructuredLog.h"
#include "Maze/MazeGridLibrary.h"

void UMazeGridLibrary::GenerateMaze(FMazeGrid& OutGrid, int32 Seed, int32 CellsX, int32 CellsY, float LoopChance)
{
	OutGrid.Generate(Seed, CellsX, CellsY, LoopChance);
}

bool UMazeGridLibrary::VerifyDeterminism(int32 Seed, int32 CellsX, int32 CellsY, float LoopChance, int32 Iterations)
{
	if (Iterations < 2)
	{
		Iterations = 2;
	}

	FMazeGrid ReferenceGrid;
	ReferenceGrid.Generate(Seed, CellsX, CellsY, LoopChance);
	const uint32 ReferenceHash = ReferenceGrid.ComputeHash();

	bool bAllMatch = true;
	for (int32 i = 1; i < Iterations; ++i)
	{
		FMazeGrid CandidateGrid;
		CandidateGrid.Generate(Seed, CellsX, CellsY, LoopChance);

		const uint32 CandidateHash = CandidateGrid.ComputeHash();
		if (CandidateHash != ReferenceHash)
		{
			UE_LOGFMT(LogTemp, Error, "MazeDemo determinism check FAILED: run {Iteration} produced hash {Candidate}, expected {Reference}", i, CandidateHash, ReferenceHash);
			bAllMatch = false;
		}
	}

	UE_LOGFMT(LogTemp, Log, "MazeDemo determinism check [{Status}] Seed={Seed} Cells={CellsX}x{CellsY} LoopChance={LoopChance} Iterations={Iterations} Hash={Hash}",
		bAllMatch ? TEXT("PASS") : TEXT("FAIL"), Seed, CellsX, CellsY, LoopChance, Iterations, ReferenceHash);

	return bAllMatch;
}

bool UMazeGridLibrary::VerifySeedSensitivity(int32 SeedA, int32 SeedB, int32 CellsX, int32 CellsY, float LoopChance)
{
	FMazeGrid GridA;
	GridA.Generate(SeedA, CellsX, CellsY, LoopChance);

	FMazeGrid GridB;
	GridB.Generate(SeedB, CellsX, CellsY, LoopChance);

	const bool bDifferent = (GridA.ComputeHash() != GridB.ComputeHash());
	if (!bDifferent)
	{
		UE_LOGFMT(LogTemp, Error, "MazeDemo seed sensitivity check FAILED: Seed {SeedA} and {SeedB} produced identical layout", SeedA, SeedB);
	}
	else
	{
		UE_LOGFMT(LogTemp, Log, "MazeDemo seed sensitivity check PASS: Seed {SeedA} differs from {SeedB}", SeedA, SeedB);
	}

	return bDifferent;
}

void UMazeGridLibrary::DrawDebugMaze(const UObject* WorldContextObject, const FMazeGrid& Grid, float UnitSize, float WallHeight, float DurationSeconds)
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FVector HalfExtent(UnitSize * 0.5f, UnitSize * 0.5f, WallHeight * 0.5f);

	for (int32 Y = 0; Y < Grid.Height; ++Y)
	{
		for (int32 X = 0; X < Grid.Width; ++X)
		{
			if (!Grid.IsWall(X, Y))
			{
				continue;
			}

			const FVector Center = Grid.GetUnitCenterLocal(X, Y, UnitSize) + FVector(0.f, 0.f, WallHeight * 0.5f);
			DrawDebugBox(World, Center, HalfExtent, FColor::Orange, false, DurationSeconds, 0, 4.f);
		}
	}
#endif
}

FString UMazeGridLibrary::ToDebugString(const FMazeGrid& Grid)
{
	if (Grid.IsEmpty())
	{
		return TEXT("<empty grid>");
	}

	FString Result;
	Result.Reserve((Grid.Width + 1) * Grid.Height);

	for (int32 Y = 0; Y < Grid.Height; ++Y)
	{
		for (int32 X = 0; X < Grid.Width; ++X)
		{
			Result += Grid.IsWall(X, Y) ? TEXT('#') : TEXT('.');
		}
		Result += LINE_TERMINATOR;
	}

	return Result;
}
