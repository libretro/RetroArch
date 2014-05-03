#if !defined(_H_CG_BINARY_H_)
#define _H_CG_BINARY_H_

#ifdef __cplusplus
extern "C" {
#endif

   /*************************************************************************/
   /*** CgBinaryGL binary data/file format                                ***/
   /*************************************************************************/

#define CG_BINARY_FORMAT_REVISION   0x00000006

   // we don't encode pointers in the binary file so cross compiling
   // with differnt pointer sizes works.
   typedef unsigned int                    CgBinaryOffset;
   typedef CgBinaryOffset                  CgBinaryEmbeddedConstantOffset;
   typedef CgBinaryOffset                  CgBinaryFloatOffset;
   typedef CgBinaryOffset                  CgBinaryStringOffset;
   typedef CgBinaryOffset                  CgBinaryParameterOffset;

   // a few typedefs
   typedef struct CgBinaryParameter        CgBinaryParameter;
   typedef struct CgBinaryEmbeddedConstant CgBinaryEmbeddedConstant;
   typedef struct CgBinaryVertexProgram    CgBinaryVertexProgram;
   typedef struct CgBinaryFragmentProgram  CgBinaryFragmentProgram;
   typedef struct CgBinaryProgram          CgBinaryProgram;

   // fragment programs have their constants embedded in the microcode
   struct CgBinaryEmbeddedConstant
   {
      unsigned int ucodeCount;       // occurances
      unsigned int ucodeOffset[1];   // offsets that need to be patched follow
   };

   // describe a binary program parameter (CgParameter is opaque)
   struct CgBinaryParameter
   {
      CGtype                          type;          // cgGetParameterType()
      CGresource                      res;           // cgGetParameterResource()
      CGenum                          var;           // cgGetParameterVariability()
      int                             resIndex;      // cgGetParameterResourceIndex()
      CgBinaryStringOffset            name;          // cgGetParameterName()
      CgBinaryFloatOffset             defaultValue;  // default constant value
      CgBinaryEmbeddedConstantOffset  embeddedConst; // embedded constant information
      CgBinaryStringOffset            semantic;      // cgGetParameterSemantic()
      CGenum                          direction;     // cgGetParameterDirection()
      int                             paramno;       // 0..n: cgGetParameterIndex() -1: globals
      CGbool                          isReferenced;  // cgIsParameterReferenced()
      CGbool                          isShared;	   // cgIsParameterShared()
   };

   // attributes needed for vshaders
   struct CgBinaryVertexProgram
   {
      unsigned int  instructionCount;         // #instructions
      unsigned int  instructionSlot;          // load address (indexed reads!)
      unsigned int  registerCount;            // R registers count
      unsigned int  attributeInputMask;       // attributes vs reads from
      unsigned int  attributeOutputMask;      // attributes vs writes (uses SET_VERTEX_ATTRIB_OUTPUT_MASK bits)
   };

   typedef enum {
      CgBinaryPTTNone = 0,
      CgBinaryPTT2x16 = 1,
      CgBinaryPTT1x32 = 2,
   } CgBinaryPartialTexType;

   // attributes needed for pshaders
   struct CgBinaryFragmentProgram
   {
      unsigned int  instructionCount;         // #instructions
      unsigned int  attributeInputMask;       // attributes fp reads (uses SET_VERTEX_ATTRIB_OUTPUT_MASK bits)
      unsigned int  partialTexType;           // texid 0..15 use two bits each marking whether the texture format requires partial load: see CgBinaryPartialTexType
      unsigned short texCoordsInputMask;      // tex coords used by frag prog. (tex<n> is bit n)
      unsigned short texCoords2D;             // tex coords that are 2d        (tex<n> is bit n)
      unsigned short texCoordsCentroid;       // tex coords that are centroid  (tex<n> is bit n)
      unsigned char registerCount;            // R registers count
      unsigned char outputFromH0;             // final color from R0 or H0
      unsigned char depthReplace;             // fp generated z epth value
      unsigned char pixelKill;                // fp uses kill operations
   };

   // defines a binary program -- *all* address/offsets are relative to the begining of CgBinaryProgram
   struct CgBinaryProgram
   {
      // vertex/pixel shader identification (BE/LE as well)
      CGprofile profile;

      // binary revision (used to verify binary and driver structs match)
      unsigned int binaryFormatRevision;

      // total size of this struct including profile and totalSize field!
      unsigned int totalSize;

      // parameter usually queried using cgGet[First/Next]LeafParameter
      unsigned int parameterCount;
      CgBinaryParameterOffset parameterArray;

      // depending on profile points to a CgBinaryVertexProgram or CgBinaryFragmentProgram struct
      CgBinaryOffset program;

      // raw ucode data
      unsigned int    ucodeSize;
      CgBinaryOffset  ucode;

      // variable length data follows
      unsigned char data[1];
   };

#ifdef __cplusplus
}
#endif

#endif // !_H_CG_BINARY_H_
