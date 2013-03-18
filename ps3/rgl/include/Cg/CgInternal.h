#ifndef _cg_internal_h
#define _cg_internal_h

#ifdef __cplusplus
extern "C" {
#endif

   //Hardware shader settings
#define CGF_OUTPUTFROMH0 0x01
#define CGF_DEPTHREPLACE 0x02
#define CGF_PIXELKILL 0x04

   //CgParameterEntry flags

   //variability
#define CGPV_MASK 0x03
#define CGPV_VARYING 0x00
#define CGPV_UNIFORM 0x01
#define CGPV_CONSTANT 0x02
#define CGPV_MIXED 0x03

   //direction
#define CGPD_MASK 0x0C
#define CGPD_IN 0x00
#define CGPD_OUT 0x04
#define CGPD_INOUT 0x08

   //is_referenced
#define CGPF_REFERENCED 0x10
   //is_shared
#define CGPF_SHARED 0x20
   //is_global
#define CGPF_GLOBAL 0x40
   //internal parameter
#define CGP_INTERNAL 0x80

   //type
#define CGP_INTRINSIC  0x0000
#define CGP_STRUCTURE  0x100
#define CGP_ARRAY  0x200
#define CGP_TYPE_MASK  (CGP_STRUCTURE + CGP_ARRAY)

   //storage
#define CGP_UNROLLED 0x400
#define CGP_UNPACKED 0x800
#define CGP_CONTIGUOUS 0x1000

#define CGP_NORMALIZE 0x2000 // (attrib) if the usual cgGLSetParameterPointer should normalize the attrib

#define CGP_RTCREATED 0x4000 // indicates that the parameter was created at runtime

   //static control flow boolean type
#define CGP_SCF_BOOL (CG_TYPE_START_ENUM + 1024)

   //data types
   typedef struct _CgParameterTableHeader
   {
      unsigned short entryCount;
      unsigned short resourceTableOffset;
      unsigned short defaultValueIndexTableOffset;
      unsigned short defaultValueIndexCount;
      unsigned short semanticIndexTableOffset;
      unsigned short semanticIndexCount;
   }
   CgParameterTableHeader;

   typedef struct _CgParameterEntry
   {
      unsigned int nameOffset;
      unsigned short typeIndex;
      unsigned short flags;
   }
   CgParameterEntry;

#ifdef MSVC
#pragma warning( push )
#pragma warning ( disable : 4200 )
#endif

   typedef struct _CgParameterArray
   {
      unsigned short arrayType;
      unsigned short dimensionCount;
      unsigned short dimensions[];
   }
   CgParameterArray; //padded to 4 bytes

#ifdef MSVC
#pragma warning( pop )
#endif

   typedef struct _CgParameterStructure
   {
      unsigned short memberCount;
      unsigned short reserved;
   }
   CgParameterStructure;

   typedef struct _CgParameterResource
   {
      unsigned short type;
      unsigned short resource;
   }
   CgParameterResource;

   typedef struct _CgParameterSemantic
   {
      unsigned short entryIndex;
      unsigned short reserved;
      unsigned int semanticOffset;
   }
   CgParameterSemantic;

   typedef struct _CgParameterDefaultValue
   {
      unsigned short entryIndex;
      unsigned short defaultValueIndex;
   }
   CgParameterDefaultValue;

   typedef struct CgProgramHeader
   {
      //28 bytes
      unsigned short profile; // Vertex / Fragment
      unsigned short compilerVersion;
      unsigned int instructionCount;
      unsigned int attributeInputMask;
      union
      {
         struct
         {
            //16 bytes
            unsigned int instructionSlot;
            unsigned int registerCount;
            unsigned int attributeOutputMask;
         }
         vertexProgram;
         struct
         {
            //12 bytes
            unsigned int partialTexType;
            unsigned short texcoordInputMask;
            unsigned short texcoord2d;
            unsigned short texcoordCentroid;
            unsigned char registerCount;
            unsigned char flags; //combination of CGF_OUTPUTFROMH0,CGF_DEPTHREPLACE,CGF_PIXELKILL
         }
         fragmentProgram;
      };
   }
   CgProgramHeader;

#ifdef __cplusplus
}
#endif

#endif
