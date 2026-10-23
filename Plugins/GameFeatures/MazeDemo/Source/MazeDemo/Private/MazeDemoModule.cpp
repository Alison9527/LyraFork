// Copyright MazeDemo. All Rights Reserved.

#include "MazeDemoModule.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Maze/MazeGrid.h"
#include "Maze/MazeGridLibrary.h"

#define LOCTEXT_NAMESPACE "FMazeDemoModule"

namespace MazeDemoConsole
{
	/**
	 * 优先返回 PIE / 独立游戏世界，其次退回编辑器世界，
	 * 这样控制台命令在编辑器里直接敲也能立刻看到调试图形，不必先进 PIE。
	 */
	UWorld* ResolveDisplayWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		UWorld* FallbackWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
			if (Context.WorldType == EWorldType::Editor)
			{
				FallbackWorld = Context.World();
			}
		}
		return FallbackWorld;
	}

	void ParseArgs(const TArray<FString>& Args, int32& OutSeed, int32& OutCellsX, int32& OutCellsY, float& OutLoopChance)
	{
		OutSeed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1234;
		OutCellsX = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 17;
		OutCellsY = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : OutCellsX;
		OutLoopChance = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.15f;

		if (OutCellsX < 1)
		{
			OutCellsX = 1;
		}
		if (OutCellsY < 1)
		{
			OutCellsY = 1;
		}
	}

	void CmdDraw(const TArray<FString>& Args)
	{
		int32 Seed, CellsX, CellsY;
		float LoopChance;
		ParseArgs(Args, Seed, CellsX, CellsY, LoopChance);

		FMazeGrid Grid;
		if (!Grid.Generate(Seed, CellsX, CellsY, LoopChance))
		{
			UE_LOGFMT(LogTemp, Error, "Maze.Draw: invalid parameters");
			return;
		}

		UWorld* World = ResolveDisplayWorld();
		if (!World)
		{
			UE_LOGFMT(LogTemp, Warning, "Maze.Draw: no world available, open or start a level first");
			return;
		}

		UMazeGridLibrary::DrawDebugMaze(World, Grid, 300.f, 400.f, 30.f);
		UE_LOGFMT(LogTemp, Log, "Maze.Draw: {FloorCount} walkable units, hash {Hash}", Grid.CountFloorUnits(), Grid.ComputeHash());
	}

	void CmdAscii(const TArray<FString>& Args)
	{
		int32 Seed, CellsX, CellsY;
		float LoopChance;
		ParseArgs(Args, Seed, CellsX, CellsY, LoopChance);

		FMazeGrid Grid;
		Grid.Generate(Seed, CellsX, CellsY, LoopChance);
		UE_LOGFMT(LogTemp, Log, "\n{Ascii}", UMazeGridLibrary::ToDebugString(Grid));
	}

	void CmdVerify(const TArray<FString>& Args)
	{
		int32 Seed, CellsX, CellsY;
		float LoopChance;
		ParseArgs(Args, Seed, CellsX, CellsY, LoopChance);

		const bool bDeterministic = UMazeGridLibrary::VerifyDeterminism(Seed, CellsX, CellsY, LoopChance, 16);
		const bool bSeedSensitive = UMazeGridLibrary::VerifySeedSensitivity(Seed, Seed + 1, CellsX, CellsY, LoopChance);

		UE_LOGFMT(LogTemp, Log, "Maze.Verify: determinism={Determinism} seedSensitivity={SeedSensitivity}", bDeterministic, bSeedSensitive);
	}
}

static FAutoConsoleCommand GMazeDrawCommand(
	TEXT("Maze.Draw"),
	TEXT("Generate a maze grid and draw it with debug boxes. Usage: Maze.Draw [Seed] [CellsX] [CellsY] [LoopChance]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&MazeDemoConsole::CmdDraw));

static FAutoConsoleCommand GMazeAsciiCommand(
	TEXT("Maze.Ascii"),
	TEXT("Print a maze grid as ASCII art. Usage: Maze.Ascii [Seed] [CellsX] [CellsY] [LoopChance]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&MazeDemoConsole::CmdAscii));

static FAutoConsoleCommand GMazeVerifyCommand(
	TEXT("Maze.Verify"),
	TEXT("Run the determinism self-check. Usage: Maze.Verify [Seed] [CellsX] [CellsY] [LoopChance]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&MazeDemoConsole::CmdVerify));

void FMazeDemoModule::StartupModule()
{
	UE_LOGFMT(LogTemp, Log, "MazeDemo GameFeature module startup");
}

void FMazeDemoModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMazeDemoModule, MazeDemo)

#undef LOCTEXT_NAMESPACE
