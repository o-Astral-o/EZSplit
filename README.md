An Unreal Engine 5 plugin for splitting/merging static meshes

HOW TO USE:

SPLIT
Right click any static mesh in a level and select Split in the EZSplit options.
This will split the mesh by loose parts and create a new static mesh for each part.
Each new mesh will be placed into a folder "/OriginalMeshName/Split" in both the level and the content browser relative to the original mesh location.

Merge
Right click any static mesh in a level and select Merge Selected in the EZSplit options.
This will merge all selected meshes and create a new merged static mesh.
The resulting mesh will be placed into a folder "/Merged" in the content browser relative to the original location of the first selected actor.
If the split meshes are contained inside of a "/Split" folder in the content browser then it will create the "/Merged" folder beside this folder rather than inside it.