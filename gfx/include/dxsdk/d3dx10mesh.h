//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) Microsoft Corporation.  All Rights Reserved.
//
//  File:       d3dx10mesh.h
//  Content:    D3DX10 mesh types and functions
//
//////////////////////////////////////////////////////////////////////////////

#include "d3dx10.h"

#ifndef __D3DX10MESH_H__
#define __D3DX10MESH_H__

// {7ED943DD-52E8-40b5-A8D8-76685C406330}
DEFINE_GUID(IID_ID3DX10BaseMesh,
0x7ed943dd, 0x52e8, 0x40b5, 0xa8, 0xd8, 0x76, 0x68, 0x5c, 0x40, 0x63, 0x30);

// {04B0D117-1041-46b1-AA8A-3952848BA22E}
DEFINE_GUID(IID_ID3DX10MeshBuffer,
0x4b0d117, 0x1041, 0x46b1, 0xaa, 0x8a, 0x39, 0x52, 0x84, 0x8b, 0xa2, 0x2e);

// {4020E5C2-1403-4929-883F-E2E849FAC195}
DEFINE_GUID(IID_ID3DX10Mesh,
0x4020e5c2, 0x1403, 0x4929, 0x88, 0x3f, 0xe2, 0xe8, 0x49, 0xfa, 0xc1, 0x95);

// {8875769A-D579-4088-AAEB-534D1AD84E96}
DEFINE_GUID(IID_ID3DX10PMesh,
0x8875769a, 0xd579, 0x4088, 0xaa, 0xeb, 0x53, 0x4d, 0x1a, 0xd8, 0x4e, 0x96);

// {667EA4C7-F1CD-4386-B523-7C0290B83CC5}
DEFINE_GUID(IID_ID3DX10SPMesh,
0x667ea4c7, 0xf1cd, 0x4386, 0xb5, 0x23, 0x7c, 0x2, 0x90, 0xb8, 0x3c, 0xc5);

// {3CE6CC22-DBF2-44f4-894D-F9C34A337139}
DEFINE_GUID(IID_ID3DX10PatchMesh,
0x3ce6cc22, 0xdbf2, 0x44f4, 0x89, 0x4d, 0xf9, 0xc3, 0x4a, 0x33, 0x71, 0x39);

// Mesh options - lower 3 bytes only, upper byte used by _D3DX10MESHOPT option flags
enum _D3DX10_MESH {
    D3DX10_MESH_32_BIT                  = 0x001, // If set, then use 32 bit indices, if not set use 16 bit indices.
    D3DX10_MESH_GS_ADJACENCY			= 0x004, // If set, mesh contains GS adjacency info. Not valid on input.

};

typedef struct _D3DX10_ATTRIBUTE_RANGE
{
    UINT  AttribId;
    UINT  FaceStart;
    UINT  FaceCount;
    UINT  VertexStart;
    UINT  VertexCount;
} D3DX10_ATTRIBUTE_RANGE;

typedef D3DX10_ATTRIBUTE_RANGE* LPD3DX10_ATTRIBUTE_RANGE;

typedef enum _D3DX10_MESH_DISCARD_FLAGS
{
    D3DX10_MESH_DISCARD_ATTRIBUTE_BUFFER = 0x01,
    D3DX10_MESH_DISCARD_ATTRIBUTE_TABLE = 0x02,
    D3DX10_MESH_DISCARD_POINTREPS = 0x04,
    D3DX10_MESH_DISCARD_ADJACENCY = 0x08,
    D3DX10_MESH_DISCARD_DEVICE_BUFFERS = 0x10,

} D3DX10_MESH_DISCARD_FLAGS;

typedef struct _D3DX10_WELD_EPSILONS
{
    FLOAT Position;                 // NOTE: This does NOT replace the epsilon in GenerateAdjacency
                                            // in general, it should be the same value or greater than the one passed to GeneratedAdjacency
    FLOAT BlendWeights;
    FLOAT Normal;
    FLOAT PSize;
    FLOAT Specular;
    FLOAT Diffuse;
    FLOAT Texcoord[8];
    FLOAT Tangent;
    FLOAT Binormal;
    FLOAT TessFactor;
} D3DX10_WELD_EPSILONS;

typedef D3DX10_WELD_EPSILONS* LPD3DX10_WELD_EPSILONS;

typedef struct _D3DX10_INTERSECT_INFO
{
    UINT  FaceIndex;                // index of face intersected
    FLOAT U;                        // Barycentric Hit Coordinates
    FLOAT V;                        // Barycentric Hit Coordinates
    FLOAT Dist;                     // Ray-Intersection Parameter Distance
} D3DX10_INTERSECT_INFO, *LPD3DX10_INTERSECT_INFO;

// ID3DX10MeshBuffer is used by D3DX10Mesh vertex and index buffers
#undef INTERFACE
#define INTERFACE ID3DX10MeshBuffer

DECLARE_INTERFACE_(ID3DX10MeshBuffer, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX10MeshBuffer
    STDMETHOD(Map)(THIS_ void **ppData, SIZE_T *pSize) PURE;
    STDMETHOD(Unmap)(THIS) PURE;
    STDMETHOD_(SIZE_T, GetSize)(THIS) PURE;
};

// D3DX10 Mesh interfaces
#undef INTERFACE
#define INTERFACE ID3DX10Mesh

DECLARE_INTERFACE_(ID3DX10Mesh, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX10Mesh
    STDMETHOD_(UINT, GetFaceCount)(THIS) PURE;
    STDMETHOD_(UINT, GetVertexCount)(THIS) PURE;
    STDMETHOD_(UINT, GetVertexBufferCount)(THIS) PURE;
    STDMETHOD_(UINT, GetFlags)(THIS) PURE;
    STDMETHOD(GetVertexDescription)(THIS_ CONST D3D10_INPUT_ELEMENT_DESC **ppDesc, UINT *pDeclCount) PURE;

    STDMETHOD(SetVertexData)(THIS_ UINT iBuffer, CONST void *pData) PURE;
    STDMETHOD(GetVertexBuffer)(THIS_ UINT iBuffer, ID3DX10MeshBuffer **ppVertexBuffer) PURE;

    STDMETHOD(SetIndexData)(THIS_ CONST void *pData, UINT cIndices) PURE;
    STDMETHOD(GetIndexBuffer)(THIS_ ID3DX10MeshBuffer **ppIndexBuffer) PURE;

    STDMETHOD(SetAttributeData)(THIS_ CONST UINT *pData) PURE;
    STDMETHOD(GetAttributeBuffer)(THIS_ ID3DX10MeshBuffer **ppAttributeBuffer) PURE;

    STDMETHOD(SetAttributeTable)(THIS_ CONST D3DX10_ATTRIBUTE_RANGE *pAttribTable, UINT  cAttribTableSize) PURE;
    STDMETHOD(GetAttributeTable)(THIS_ D3DX10_ATTRIBUTE_RANGE *pAttribTable, UINT  *pAttribTableSize) PURE;

    STDMETHOD(GenerateAdjacencyAndPointReps)(THIS_ FLOAT Epsilon) PURE;
    STDMETHOD(GenerateGSAdjacency)(THIS) PURE;

    STDMETHOD(SetAdjacencyData)(THIS_ CONST UINT *pAdjacency) PURE;
    STDMETHOD(GetAdjacencyBuffer)(THIS_ ID3DX10MeshBuffer **ppAdjacency) PURE;

    STDMETHOD(SetPointRepData)(THIS_ CONST UINT *pPointReps) PURE;
    STDMETHOD(GetPointRepBuffer)(THIS_ ID3DX10MeshBuffer **ppPointReps) PURE;

    STDMETHOD(Discard)(THIS_ D3DX10_MESH_DISCARD_FLAGS dwDiscard) PURE;
    STDMETHOD(CloneMesh)(THIS_ UINT Flags, LPCSTR pPosSemantic, CONST D3D10_INPUT_ELEMENT_DESC *pDesc, UINT  DeclCount, ID3DX10Mesh** ppCloneMesh) PURE;

    STDMETHOD(Optimize)(THIS_ UINT Flags, UINT * pFaceRemap, LPD3D10BLOB *ppVertexRemap) PURE;
    STDMETHOD(GenerateAttributeBufferFromTable)(THIS) PURE;

	STDMETHOD(Intersect)(THIS_ D3DXVECTOR3 *pRayPos, D3DXVECTOR3 *pRayDir,
                                        UINT *pHitCount, UINT *pFaceIndex, float *pU, float *pV, float *pDist, ID3D10Blob **ppAllHits);
    STDMETHOD(IntersectSubset)(THIS_ UINT AttribId, D3DXVECTOR3 *pRayPos, D3DXVECTOR3 *pRayDir,
                                        UINT *pHitCount, UINT *pFaceIndex, float *pU, float *pV, float *pDist, ID3D10Blob **ppAllHits);

    // ID3DX10Mesh - Device functions
    STDMETHOD(CommitToDevice)(THIS) PURE;
    STDMETHOD(DrawSubset)(THIS_ UINT AttribId) PURE;
    STDMETHOD(DrawSubsetInstanced)(THIS_ UINT AttribId, UINT InstanceCount, UINT StartInstanceLocation) PURE;

    STDMETHOD(GetDeviceVertexBuffer)(THIS_ UINT iBuffer, ID3D10Buffer **ppVertexBuffer) PURE;
    STDMETHOD(GetDeviceIndexBuffer)(THIS_ ID3D10Buffer **ppIndexBuffer) PURE;
};

// Flat API
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

HRESULT WINAPI
    D3DX10CreateMesh(
        ID3D10Device *pDevice,
        CONST D3D10_INPUT_ELEMENT_DESC *pDeclaration,
        UINT  DeclCount,
        LPCSTR pPositionSemantic,
        UINT  VertexCount,
        UINT  FaceCount,
        UINT  Options,
        ID3DX10Mesh **ppMesh);

#ifdef __cplusplus
}
#endif //__cplusplus

// ID3DX10Mesh::Optimize options - upper byte only, lower 3 bytes used from _D3DX10MESH option flags
enum _D3DX10_MESHOPT {
    D3DX10_MESHOPT_COMPACT       = 0x01000000,
    D3DX10_MESHOPT_ATTR_SORT     = 0x02000000,
    D3DX10_MESHOPT_VERTEX_CACHE   = 0x04000000,
    D3DX10_MESHOPT_STRIP_REORDER  = 0x08000000,
    D3DX10_MESHOPT_IGNORE_VERTS   = 0x10000000,  // optimize faces only, don't touch vertices
    D3DX10_MESHOPT_DO_NOT_SPLIT    = 0x20000000,  // do not split vertices shared between attribute groups when attribute sorting
    D3DX10_MESHOPT_DEVICE_INDEPENDENT = 0x00400000,  // Only affects VCache.  uses a static known good cache size for all cards

    // D3DX10_MESHOPT_SHAREVB has been removed, please use D3DX10MESH_VB_SHARE instead

};

//////////////////////////////////////////////////////////////////////////
// ID3DXSkinInfo
//////////////////////////////////////////////////////////////////////////

// {420BD604-1C76-4a34-A466-E45D0658A32C}
DEFINE_GUID(IID_ID3DX10SkinInfo,
0x420bd604, 0x1c76, 0x4a34, 0xa4, 0x66, 0xe4, 0x5d, 0x6, 0x58, 0xa3, 0x2c);

// scaling modes for ID3DX10SkinInfo::Compact() & ID3DX10SkinInfo::UpdateMesh()
#define D3DX10_SKININFO_NO_SCALING 0
#define D3DX10_SKININFO_SCALE_TO_1 1
#define D3DX10_SKININFO_SCALE_TO_TOTAL 2

typedef struct _D3DX10_SKINNING_CHANNEL
{
    UINT            SrcOffset;
    UINT            DestOffset;
    BOOL            IsNormal;
} D3DX10_SKINNING_CHANNEL;

#undef INTERFACE
#define INTERFACE ID3DX10SkinInfo

typedef struct ID3DX10SkinInfo *LPD3DX10SKININFO;

DECLARE_INTERFACE_(ID3DX10SkinInfo, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD_(UINT , GetNumVertices)(THIS) PURE;
    STDMETHOD_(UINT , GetNumBones)(THIS) PURE;
    STDMETHOD_(UINT , GetMaxBoneInfluences)(THIS) PURE;

    STDMETHOD(AddVertices)(THIS_ UINT  Count) PURE;
    STDMETHOD(RemapVertices)(THIS_ UINT  NewVertexCount, UINT  *pVertexRemap) PURE;

    STDMETHOD(AddBones)(THIS_ UINT  Count) PURE;
    STDMETHOD(RemoveBone)(THIS_ UINT  Index) PURE;
    STDMETHOD(RemapBones)(THIS_ UINT  NewBoneCount, UINT  *pBoneRemap) PURE;

    STDMETHOD(AddBoneInfluences)(THIS_ UINT  BoneIndex, UINT  InfluenceCount, UINT  *pIndices, float *pWeights) PURE;
    STDMETHOD(ClearBoneInfluences)(THIS_ UINT  BoneIndex) PURE;
    STDMETHOD_(UINT , GetBoneInfluenceCount)(THIS_ UINT  BoneIndex) PURE;
    STDMETHOD(GetBoneInfluences)(THIS_ UINT  BoneIndex, UINT  Offset, UINT  Count, UINT  *pDestIndices, float *pDestWeights) PURE;
    STDMETHOD(FindBoneInfluenceIndex)(THIS_ UINT  BoneIndex, UINT  VertexIndex, UINT  *pInfluenceIndex) PURE;
    STDMETHOD(SetBoneInfluence)(THIS_ UINT  BoneIndex, UINT  InfluenceIndex, float Weight) PURE;
    STDMETHOD(GetBoneInfluence)(THIS_ UINT  BoneIndex, UINT  InfluenceIndex, float *pWeight) PURE;

    STDMETHOD(Compact)(THIS_ UINT  MaxPerVertexInfluences, UINT  ScaleMode, float MinWeight) PURE;
    STDMETHOD(DoSoftwareSkinning)(UINT  StartVertex, UINT  VertexCount, void *pSrcVertices, UINT  SrcStride, void *pDestVertices, UINT  DestStride, D3DXMATRIX *pBoneMatrices, D3DXMATRIX *pInverseTransposeBoneMatrices, D3DX10_SKINNING_CHANNEL *pChannelDescs, UINT  NumChannels) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

HRESULT WINAPI
    D3DX10CreateSkinInfo(LPD3DX10SKININFO* ppSkinInfo);

#ifdef __cplusplus
}
#endif //__cplusplus

typedef struct _D3DX10_ATTRIBUTE_WEIGHTS
{
    FLOAT Position;
    FLOAT Boundary;
    FLOAT Normal;
    FLOAT Diffuse;
    FLOAT Specular;
    FLOAT Texcoord[8];
    FLOAT Tangent;
    FLOAT Binormal;
} D3DX10_ATTRIBUTE_WEIGHTS, *LPD3DX10_ATTRIBUTE_WEIGHTS;

#endif //__D3DX10MESH_H__
