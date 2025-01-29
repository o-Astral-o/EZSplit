#include "EZSplit.h"
#include "LevelEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "Editor/UnrealEd/Public/Selection.h"
#include "MeshDescription.h"
#include "Logging/LogMacros.h"
#include "ToolMenus.h"
#include "MeshUtilities.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/StaticMeshActor.h"         // For AStaticMeshActor
//#include "MeshDescriptionOperations.h"     // For FStaticMeshOperations::AppendMeshDescription
#include "Materials/MaterialInterface.h"   // For UMaterialInterface
#include "StaticMeshDescription.h"         // For FMeshDescription

#include "MeshMergeModule.h"
#include "IMeshMergeUtilities.h"
#include "Modules/ModuleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SavePackage.h"

#include "ObjectTools.h"

DEFINE_LOG_CATEGORY_STATIC(LogEZSplit, Log, All);

void FEZSplitModule::StartupModule()
{
    UE_LOG(LogEZSplit, Log, TEXT("EZSplit: StartupModule called."));

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        // Extend the Level Editor's Actor Context Menu
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
        FToolMenuSection& Section = Menu->AddSection("EZSplitSection", FText::FromString("EZSplit"));

        Section.AddMenuEntry(
            "SplitMesh",
            FText::FromString("Split Mesh"),
            FText::FromString("Splits the selected static mesh actor into separate parts by poly group."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FEZSplitModule::OnSplitMeshButtonClicked))
        );

        Section.AddMenuEntry(
            "MergeMesh",
            FText::FromString("Merge Selected"),
            FText::FromString("Merge the selected static meshes into a single static mesh."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FEZSplitModule::OnMergeMeshButtonClicked))
        );

        UE_LOG(LogEZSplit, Log, TEXT("EZSplit: Actor context menu extended successfully."));
    }
}

void FEZSplitModule::ShutdownModule()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnregisterOwner(this);
        UE_LOG(LogEZSplit, Log, TEXT("EZSplit: Unregistered menus."));
    }
}

void FEZSplitModule::AddMenuEntry(FMenuBuilder& MenuBuilder)
{
    UE_LOG(LogEZSplit, Log, TEXT("EZSplit: AddMenuEntry called."));

    bool bHasStaticMeshActor = false;
    for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
    {
        AActor* SelectedActor = Cast<AActor>(*It);
        if (SelectedActor && SelectedActor->FindComponentByClass<UStaticMeshComponent>())
        {
            bHasStaticMeshActor = true;
            break;
        }
    }

    if (bHasStaticMeshActor)
    {
        UE_LOG(LogEZSplit, Log, TEXT("EZSplit: Adding Split Mesh menu entry for selected static mesh actor."));
        MenuBuilder.AddMenuEntry(
            FText::FromString("Split Mesh"),
            FText::FromString("Splits the selected static mesh into separate parts by poly group."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FEZSplitModule::OnSplitMeshButtonClicked))
        );

        UE_LOG(LogEZSplit, Log, TEXT("EZSplit: Adding Merge Mesh menu entry for selected static mesh actor."));
        MenuBuilder.AddMenuEntry(
            FText::FromString("Merge Selected"),
            FText::FromString("Merge the selected static meshes into a single static mesh."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FEZSplitModule::OnMergeMeshButtonClicked))
        );
    }
    else
    {
        UE_LOG(LogEZSplit, Warning, TEXT("EZSplit: No Static Mesh Actor selected. Menu entry not added."));
    }
}

void FEZSplitModule::OnSplitMeshButtonClicked()
{
    // Access the selected actor
    TArray<AActor*> SelectedActors;
    if (GEditor && GEditor->GetSelectedActors())
    {
        GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
    }

    for (AActor* Actor : SelectedActors)
    {
        SplitMesh(Actor);
    }
}

void FEZSplitModule::OnMergeMeshButtonClicked()
{
    MergeSelectedStaticMeshes();
}


// Split
void SplitMesh(AActor* Actor)
{
    UStaticMesh* BaseMesh;
    
    UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
    if (MeshComponent)
    {
        UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
        if (StaticMesh)
        {
            BaseMesh = StaticMesh;

        }
        else
        {
            UE_LOG(LogEZSplit, Warning, TEXT("SplitMesh: BaseMesh is null!"));
            return;
        }
    }
    else
    {
        UE_LOG(LogEZSplit, Warning, TEXT("SplitMesh: BaseMesh is null!"));
        return;
    }

    // Get the LOD 0 data
    FStaticMeshRenderData* RenderData = BaseMesh->GetRenderData();
    if (!RenderData)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("SplitMesh: No RenderData found in BaseMesh!"));
        return;
    }

    FStaticMeshLODResources& LODResource = RenderData->LODResources[0];
    FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
    FIndexArrayView Indices = LODResource.IndexBuffer.GetArrayView();

    // Extract vertices
    TArray<FVector> Vertices;
    TArray<FVector> Normals, Tangents;
    TArray<FColor> Colors;
    TArray<TArray<FVector2D>> UVChannels;
    int32 NumUVChannels = LODResource.GetNumTexCoords();
    UVChannels.SetNum(NumUVChannels);

    for (uint32 i = 0; i < VertexBuffer.GetNumVertices(); ++i)
    {
        Vertices.Add(FVector(VertexBuffer.VertexPosition(i)));
        Normals.Add(FVector(LODResource.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i)));
        Tangents.Add(FVector(LODResource.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(i)));
        Colors.Add(LODResource.VertexBuffers.ColorVertexBuffer.VertexColor(i));

        for (int32 UVChannel = 0; UVChannel < NumUVChannels; ++UVChannel)
        {
            UVChannels[UVChannel].Add(FVector2D(LODResource.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, UVChannel)));
        }
    }

    // Extract triangle indices
    TArray<uint32> TriangleIndices;
    for (int32 i = 0; i < Indices.Num(); ++i)
    {
        TriangleIndices.Add(Indices[i]);
    }

    // Find connected components (loose parts)
    TArray<TArray<uint32>> Components;
    FindConnectedComponents(BaseMesh, Vertices, TriangleIndices, Components);

    // Create new static meshes for each component
    TArray<UStaticMesh*> SplitMesh = CreateNewStaticMeshes(BaseMesh, Vertices, TriangleIndices, Components, Normals, Tangents, Colors, UVChannels);

    HandleSplitMeshPlacement(Actor, SplitMesh, BaseMesh->GetName());
}

void FindConnectedComponents(
    UStaticMesh* BaseMesh,
    const TArray<FVector>& Vertices,
    const TArray<uint32>& TriangleIndices,
    TArray<TArray<uint32>>& OutComponents)
{
    TSet<int32> VisitedTriangles;
    auto AreTrianglesConnected = [&TriangleIndices](int32 TriA, int32 TriB) -> bool {
        int32 StartIndexA = TriA * 3;
        int32 StartIndexB = TriB * 3;

        for (int32 i = 0; i < 3; ++i)
        {
            for (int32 j = 0; j < 3; ++j)
            {
                if (TriangleIndices[StartIndexA + i] == TriangleIndices[StartIndexB + j])
                {
                    return true;
                }
            }
        }
        return false;
        };

    for (int32 i = 0; i < TriangleIndices.Num() / 3; ++i)
    {
        if (VisitedTriangles.Contains(i))
        {
            continue;
        }

        TArray<int32> Stack;
        TArray<uint32> Component;
        Stack.Push(i);

        while (Stack.Num() > 0)
        {
            int32 CurrentTri = Stack.Pop();
            if (VisitedTriangles.Contains(CurrentTri))
            {
                continue;
            }

            VisitedTriangles.Add(CurrentTri);
            Component.Add(CurrentTri);

            for (int32 j = 0; j < TriangleIndices.Num() / 3; ++j)
            {
                if (!VisitedTriangles.Contains(j) && AreTrianglesConnected(CurrentTri, j))
                {
                    Stack.Push(j);
                }
            }
        }

        OutComponents.Add(Component);
    }
}

TArray<UStaticMesh*> CreateNewStaticMeshes(
    UStaticMesh* BaseMesh,
    const TArray<FVector>& Vertices,
    const TArray<uint32>& TriangleIndices,
    const TArray<TArray<uint32>>& Components,
    const TArray<FVector>& Normals,
    const TArray<FVector>& Tangents,
    const TArray<FColor>& Colors,
    const TArray<TArray<FVector2D>>& UVChannels)
{
    // Get base mesh details
    FString BaseMeshName = BaseMesh->GetName();
    FString BaseMeshPath = FPackageName::GetLongPackagePath(BaseMesh->GetOutermost()->GetName());
    FString DestinationFolder = BaseMeshPath + TEXT("/") + BaseMeshName;
    FString SplitFolderPath = DestinationFolder + TEXT("/Split");

    // Load the Asset Registry and Asset Tools
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().AddPath(SplitFolderPath);

    // Move the original static mesh asset to the new destination folder
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    FAssetRenameData RenameData(BaseMesh, DestinationFolder, BaseMeshName);
    TArray<FAssetRenameData> RenameDataArray;
    RenameDataArray.Add(RenameData);

    bool bRenameSuccess = AssetToolsModule.Get().RenameAssets(RenameDataArray);
    if (!bRenameSuccess)
    {
        //UE_LOG(LogEZSplit, Warning, TEXT("Failed to move original static mesh: %s to %s"), *OriginalAssetPath, *NewBaseMeshPath);
    }
    else
    {
        //UE_LOG(LogEZSplit, Log, TEXT("Moved original static mesh: %s to %s"), *OriginalAssetPath, *NewBaseMeshPath);
    }

    // Create new split meshes in the "/Split" folder
    TArray<UStaticMesh*> SplitMeshes;
    for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
    {
        const TArray<uint32>& Component = Components[ComponentIndex];

        FString NewMeshName = FString::Printf(TEXT("%s_%d"), *BaseMeshName, ComponentIndex);
        FString NewMeshPath = SplitFolderPath + TEXT("/") + NewMeshName;

        UObject* NewAsset = AssetToolsModule.Get().DuplicateAsset(NewMeshName, SplitFolderPath, BaseMesh);
        UStaticMesh* NewStaticMesh = Cast<UStaticMesh>(NewAsset);

        if (NewStaticMesh)
        {
            // Prepare MeshDescription for the split mesh
            FMeshDescription* MeshDescription = NewStaticMesh->CreateMeshDescription(0);
            if (!MeshDescription)
            {
                UE_LOG(LogEZSplit, Warning, TEXT("Failed to create MeshDescription for %s"), *NewMeshName);
                continue;
            }

            FStaticMeshAttributes Attributes(*MeshDescription);
            Attributes.Register();

            // Reserve space for mesh elements
            MeshDescription->ReserveNewVertices(Component.Num() * 3);
            MeshDescription->ReserveNewVertexInstances(Component.Num() * 3);
            MeshDescription->ReserveNewPolygons(Component.Num());
            MeshDescription->ReserveNewEdges(Component.Num() * 3);

            if (MeshDescription->PolygonGroups().Num() == 0)
            {
                MeshDescription->CreatePolygonGroup();
            }

            // Add vertices, normals, tangents, and UVs to the new mesh
            TMap<int32, FVertexID> VertexIDMap;
            for (int32 i = 0; i < Component.Num(); ++i)
            {
                int32 TriIndex = Component[i];
                TArray<FVertexInstanceID> TriangleVertexInstances;

                for (int32 Corner = 0; Corner < 3; ++Corner)
                {
                    uint32 VertexIndex = TriangleIndices[TriIndex * 3 + Corner];

                    if (!VertexIDMap.Contains(VertexIndex))
                    {
                        FVertexID VertexID = MeshDescription->CreateVertex();
                        VertexIDMap.Add(VertexIndex, VertexID);
                        Attributes.GetVertexPositions()[VertexID] = FVector3f(Vertices[VertexIndex]);
                    }

                    FVertexInstanceID VertexInstanceID = MeshDescription->CreateVertexInstance(VertexIDMap[VertexIndex]);
                    TriangleVertexInstances.Add(VertexInstanceID);

                    // Copy UVs, normals, and tangents
                    int32 NumUVChannels = UVChannels.Num();
                    Attributes.GetVertexInstanceUVs().SetNumChannels(NumUVChannels);
                    for (int32 UVChannel = 0; UVChannel < NumUVChannels; ++UVChannel)
                    {
                        Attributes.GetVertexInstanceUVs().Set(VertexInstanceID, UVChannel, FVector2f(
                            UVChannels[UVChannel][VertexIndex].X,
                            UVChannels[UVChannel][VertexIndex].Y
                        ));
                    }

                    Attributes.GetVertexInstanceColors()[VertexInstanceID] = FVector4f(Colors[VertexIndex]);
                    Attributes.GetVertexInstanceNormals()[VertexInstanceID] = FVector3f(Normals[VertexIndex]);
                    Attributes.GetVertexInstanceTangents()[VertexInstanceID] = FVector3f(Tangents[VertexIndex]);
                }

                // Create polygon for the triangle
                MeshDescription->CreatePolygon(FPolygonGroupID(0), TriangleVertexInstances);
            }

            // Commit and finalize the new mesh
            NewStaticMesh->CommitMeshDescription(0);
            NewStaticMesh->CreateBodySetup();
            NewStaticMesh->SetLightingGuid();
            NewStaticMesh->Build(false);
            NewStaticMesh->InitResources();
            NewStaticMesh->PostEditChange();

            FString MeshFileName = FPackageName::ObjectPathToPackageName(NewMeshPath);
            SavePackage(NewStaticMesh->GetOutermost(), MeshFileName);

            SplitMeshes.Add(NewStaticMesh);

            UE_LOG(LogEZSplit, Log, TEXT("Created new static mesh: %s"), *NewMeshPath);
        }
        else
        {
            UE_LOG(LogEZSplit, Warning, TEXT("Failed to create new static mesh: %s"), *NewMeshName);
        }
    }

    return SplitMeshes;
}

void HandleSplitMeshPlacement(
    AActor* OriginalActor,
    const TArray<UStaticMesh*>& SplitMeshes,
    const FString& BaseMeshName)
{
    if (!OriginalActor || SplitMeshes.Num() == 0)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("HandleSplitMeshPlacement: Invalid input!"));
        return;
    }

    // Get the world and original actor's folder path
    UWorld* World = OriginalActor->GetWorld();
    if (!World)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("HandleSplitMeshPlacement: No valid world!"));
        return;
    }

    FString OriginalActorFolderPath = OriginalActor->GetFolderPath().ToString();
    FString NewFolderPath = FPaths::Combine(OriginalActorFolderPath, BaseMeshName, TEXT("Split"));

    // Delete the original actor
    OriginalActor->Destroy();

    // Spawn new actors for each split mesh
    for (UStaticMesh* SplitMesh : SplitMeshes)
    {
        if (!SplitMesh) continue;

        // Spawn a new static mesh actor
        AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (!NewActor) continue;

        // Assign the split mesh to the new actor
        UStaticMeshComponent* MeshComponent = NewActor->GetStaticMeshComponent();
        if (MeshComponent)
        {
            MeshComponent->SetStaticMesh(SplitMesh);
        }

        // Place the new actor in the /Split folder
        NewActor->SetFolderPath(FName(*NewFolderPath));
        NewActor->SetActorLabel(SplitMesh->GetName());

        UE_LOG(LogEZSplit, Log, TEXT("Spawned new actor: %s in folder %s"), *SplitMesh->GetName(), *NewFolderPath);
    }

    // Log completion
    UE_LOG(LogEZSplit, Log, TEXT("Successfully handled split mesh placement under folder: %s"), *NewFolderPath);
}


// Merge
void MergeSelectedStaticMeshes()
{
    // Get selected actors
    TArray<AActor*> SelectedActors;
    GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

    // Collect static mesh components
    TArray<UPrimitiveComponent*> ComponentsToMerge;
    for (AActor* Actor : SelectedActors)
    {
        if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
        {
            if (UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent())
            {
                ComponentsToMerge.Add(StaticMeshComponent);
            }
        }
    }

    if (ComponentsToMerge.Num() < 2)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("Select at least two static mesh actors to merge."));
        return;
    }

    // Get the path of the first split mesh
    UStaticMesh* FirstSplitMesh = nullptr;
    for (UPrimitiveComponent* Component : ComponentsToMerge)
    {
        if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
        {
            FirstSplitMesh = StaticMeshComponent->GetStaticMesh();
            break;
        }
    }

    if (!FirstSplitMesh)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("No valid static mesh found for merging."));
        return;
    }

    FString FirstMeshPath = FPackageName::GetLongPackagePath(FirstSplitMesh->GetOutermost()->GetName());

    // Move back one folder and append /Merged
    FString MergedFolderPath = FPaths::GetPath(FirstMeshPath) + TEXT("/Merged");
    UE_LOG(LogEZSplit, Log, TEXT("Merged folder path: %s"), *MergedFolderPath);

    // Ensure the folder exists in the content browser
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().AddPath(MergedFolderPath);

    // Generate the name for the merged mesh
    FString BaseName = FirstSplitMesh->GetName();
    int32 LastUnderscoreIndex;
    if (BaseName.FindLastChar(TEXT('_'), LastUnderscoreIndex))
    {
        BaseName = BaseName.Left(LastUnderscoreIndex);
    }

    // Find an available index for the merged mesh
    FString MergedMeshName;
    int32 Index = 0;
    do
    {
        MergedMeshName = FString::Printf(TEXT("%s_%d"), *BaseName, Index);
        FString TestPath = FString::Printf(TEXT("%s/%s"), *MergedFolderPath, *MergedMeshName);
        if (!AssetRegistryModule.Get().DoesPackageExistOnDisk(*TestPath))
        {
            break;
        }
        Index++;
    } while (true);

    // Create the package for the merged mesh
    FString PackageName = FString::Printf(TEXT("%s/%s"), *MergedFolderPath, *MergedMeshName);
    UPackage* Package = CreatePackage(*PackageName);

    // Get the Mesh Merge Utilities
    IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

    // Define merge settings
    FMeshMergingSettings MergeSettings;
    MergeSettings.bMergeMaterials = true;
    MergeSettings.bGenerateLightMapUV = true;
    MergeSettings.bPivotPointAtZero = true;
    MergeSettings.LODSelectionType = EMeshLODSelectionType::AllLODs;
    MergeSettings.bBakeVertexDataToMesh = true;

    // Merge the meshes
    TArray<UObject*> AssetsToMerge;
    FVector MergedActorLocation;
    MeshMergeUtilities.MergeComponentsToStaticMesh(
        ComponentsToMerge,
        ComponentsToMerge[0]->GetWorld(),
        MergeSettings,
        nullptr,          // Base material, optional
        Package,          // Destination package
        MergedMeshName,   // Name of the merged asset
        AssetsToMerge,    // Output assets
        MergedActorLocation,
        0.0f,             // Screen size for LOD determination
        false             // Verbosity flag
    );

    // Find the merged static mesh
    UStaticMesh* MergedStaticMesh = nullptr;
    for (UObject* Asset : AssetsToMerge)
    {
        if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
        {
            MergedStaticMesh = StaticMesh;
            break;
        }
    }

    if (MergedStaticMesh)
    {
        MergedStaticMesh->PostEditChange();
        FAssetRegistryModule::AssetCreated(MergedStaticMesh);
        Package->MarkPackageDirty();

        FString MergedFileName = FPackageName::ObjectPathToPackageName(PackageName);
        SavePackage(MergedStaticMesh->GetOutermost(), MergedFileName);

        // Handle placement of the merged mesh in the level
        HandleMergedMeshPlacement(MergedStaticMesh, SelectedActors);


        UE_LOG(LogEZSplit, Log, TEXT("Merged static mesh created at %s"), *MergedStaticMesh->GetPathName());
    }
    else
    {
        UE_LOG(LogEZSplit, Error, TEXT("Failed to merge static meshes."));
    }

    for (UPrimitiveComponent* Component : ComponentsToMerge)
    {
        if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
        {
            UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

            FString Path = FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName());

            TArray<UObject*> Objects;

            if (Path.EndsWith("/Merged"))
            {
                Objects.Add(StaticMesh);
                ObjectTools::DeleteObjects(Objects, /*bShowConfirmation=*/false);
            }
        }
    }
}

void HandleMergedMeshPlacement(
    UStaticMesh* MergedMesh,
    const TArray<AActor*>& SelectedActors)
{
    if (!MergedMesh || SelectedActors.Num() == 0)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("HandleMergedMeshPlacement: Invalid input!"));
        return;
    }

    // Get the world and the folder path of the first selected actor
    UWorld* World = SelectedActors[0]->GetWorld();
    if (!World)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("HandleMergedMeshPlacement: No valid world!"));
        return;
    }

    FString OriginalFolderPath = SelectedActors[0]->GetFolderPath().ToString();

    // Determine the new folder path by removing "/Split" if present
    FString NewFolderPath = OriginalFolderPath;
    if (OriginalFolderPath.EndsWith(TEXT("/Split")))
    {
        NewFolderPath = FPaths::GetPath(OriginalFolderPath); // Move back one folder
    }

    // Delete the original actors
    for (AActor* Actor : SelectedActors)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }

    // Spawn a new actor for the merged mesh
    AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!NewActor)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("HandleMergedMeshPlacement: Failed to spawn actor for merged mesh."));
        return;
    }

    // Assign the merged mesh to the new actor
    UStaticMeshComponent* MeshComponent = NewActor->GetStaticMeshComponent();
    if (MeshComponent)
    {
        MeshComponent->SetStaticMesh(MergedMesh);
    }

    // Place the new actor in the appropriate hierarchy folder
    NewActor->SetFolderPath(FName(*NewFolderPath));
    NewActor->SetActorLabel(MergedMesh->GetName());

    // Log success
    UE_LOG(LogEZSplit, Log, TEXT("Placed merged mesh actor: %s in folder: %s"), *MergedMesh->GetName(), *NewFolderPath);
}

// Save
void SavePackage(UPackage* Package, const FString& FileName)
{
    if (!Package)
    {
        UE_LOG(LogEZSplit, Warning, TEXT("SavePackage: Invalid package."));
        return;
    }

    FString FullFilePath = FPackageName::LongPackageNameToFilename(FileName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GError;

    if (!UPackage::SavePackage(Package, nullptr, *FullFilePath, SaveArgs))
    {
        UE_LOG(LogEZSplit, Warning, TEXT("Failed to save package: %s"), *FullFilePath);
    }
    else
    {
        UE_LOG(LogEZSplit, Log, TEXT("Saved package: %s"), *FullFilePath);
    }
}

IMPLEMENT_MODULE(FEZSplitModule, EZSplit)