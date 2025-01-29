// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StaticMeshDescription.h"

class FEZSplitModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void AddMenuEntry(FMenuBuilder& MenuBuilder);
	void OnSplitMeshButtonClicked();
	void OnMergeMeshButtonClicked();
};

// Split
void SplitMesh(AActor* Actor);

void FindConnectedComponents(
	UStaticMesh* BaseMesh,
	const TArray<FVector>& Vertices,
	const TArray<uint32>& TriangleIndices,
	TArray<TArray<uint32>>& OutComponents);

TArray<UStaticMesh*> CreateNewStaticMeshes(
	UStaticMesh* BaseMesh,
	const TArray<FVector>& Vertices,
	const TArray<uint32>& TriangleIndices,
	const TArray<TArray<uint32>>& Components,
	const TArray<FVector>& Normals,
	const TArray<FVector>& Tangents,
	const TArray<FColor>& Colors,
	const TArray<TArray<FVector2D>>& UVChannels);

void HandleSplitMeshPlacement(
	AActor* OriginalActor,
	const TArray<UStaticMesh*>& SplitMeshes,
	const FString& BaseMeshName);

// Merge
void MergeSelectedStaticMeshes();

void HandleMergedMeshPlacement(
	UStaticMesh* MergedMesh,
	const TArray<AActor*>& SelectedActors);

// Save
void SavePackage(UPackage* Package, const FString& FileName);