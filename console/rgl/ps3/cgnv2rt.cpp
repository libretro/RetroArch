#include "cgnv2rt.h"

#include <cg/cgGL.h>
#include <cg/cgc.h>
#include <cg/cgBinary.h>

#include "cgbio.hpp"
#include "cg.h"

#include <stdlib.h>

using namespace cgc::bio;

class CgBaseType
{
public:
	CgBaseType() {_type = 0;}
	virtual ~CgBaseType() {}
	unsigned short _type;
	short _resourceIndex;
	unsigned short _resource;
};

class CgArrayType : public CgBaseType
{
public:
	CgArrayType():_elementType(NULL) { _type = (unsigned char)(CG_ARRAY + 128); }
	virtual ~CgArrayType()
	{
		if (_elementType)
			delete _elementType;
	}
	CgBaseType *_elementType;
	unsigned char _dimensionCount;
	unsigned short _dimensionItemCountsOffset;
};

class CgStructureType : public CgBaseType
{
public:
	class CgStructuralElement
	{
	public:
		char _name[256];
		char _semantic[256];
		CgBaseType *_type;
		unsigned short _flags;
		unsigned short _index;
	};

	CgStructureType()
	{
		_type = (unsigned char)(CG_STRUCT + 128);
		_root = false;
	}

	virtual ~CgStructureType()
	{
		int i=0;
		int count = (int)_elements.size();
		for (i=0;i<count;i++)
		{
			if (_elements[i]._type)
				delete _elements[i]._type;
		}
	}

	std::vector<CgStructuralElement> _elements;
	bool _insideArray;
	bool _root;
};

typedef struct
{
	const char *name;
	int resIndex;
} BINDLOCATION;

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
	classname ,
static int classnames[] = {
#include "Cg/cg_datatypes.h"
};

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
	nrows ,
static int rows[] = {
#include "Cg/cg_datatypes.h"
};

typedef float _float4[4];

typedef struct
{
   std::vector<short> _resources;
   std::vector<unsigned short> _defaultValuesIndices;
   std::vector<unsigned short> _elfDefaultsIndices;
   std::vector<short> _dimensions;
   std::vector<CgParameterSemantic> _semanticIndices;
} _CGNVCONTAINERS;

static bool bIsVertexProgram = true;


static int getStride(CgBaseType *type);
static int getSizeofSubArray(_CGNVCONTAINERS &containers, int dimensionIndex, int dimensionCount, int endianness);

static unsigned int stringTableFind( std::vector<char> &stringTable, const char* str  )
{
   const char* data = &stringTable[0];
   size_t size = stringTable.size();
   const char *end = data + size;

   size_t length = strlen(str);

   if (length+1 > size)
	   return 0;

   data += length;

   const char *p = (char*)memchr(data,'\0',end-data);
   while (p && (end-data)>0)
   {
	   if (!memcmp(p - length, str, length))
              return (unsigned int)(p - length - &stringTable[0]);

	   data = p+1;	
	   p = (char*)memchr(data,'\0',end-data);
   }
   return 0;
}

static unsigned int stringTableAdd( std::vector<char> &stringTable, const char* str )
{
   unsigned int ret = (unsigned int)stringTable.size();

   if ( ret == 0 )
   {
      stringTable.push_back('\0');
      ret = 1;
   }

   size_t stringLength = strlen(str) + 1;
   stringTable.resize(ret + stringLength);
   memcpy(&stringTable[0] + ret,str,stringLength);

   return ret;
}

static unsigned int stringTableAddUnique( std::vector<char> &stringTable, const char* str )
{
   if ( stringTable.size() == 0 )
      stringTable.push_back('\0');

   unsigned int ret = stringTableFind(stringTable, str);

   if (ret == 0 && str[0] != '\0')
      ret = stringTableAdd(stringTable, str);

   return ret;
}

template<class Type> static size_t array_size(std::vector<Type> &array);
template<class Type> static void array_push(char* &parameterOffset, std::vector<Type> &array);
inline static unsigned int swap16(const unsigned int v);
static unsigned short getFlags(CGenum var, CGenum dir, int no,	bool is_referenced, bool is_shared, int paramIndex);

static void fillStructureItems(_CGNVCONTAINERS &containers, CgStructureType *structure,
							   int endianness,
							   std::vector<CgParameterEntry> &parameterEntries,
							   std::vector<char> &parameterResources, std::vector<char> &stringTable, unsigned short *arrayResourceIndex = NULL,
							   unsigned short *arrayDefaultValueIndex = NULL);


int convertNvToElfFromMemory(const void *sourceData, size_t size, int endianness, int constTableOffset, void **binaryShader, int *binarySize,
							 std::vector<char> &stringTable, std::vector<float> &defaultValues)
{
	_CGNVCONTAINERS containers;

	unsigned char elfEndianness = endianness;

	nvb_reader* nvbr = 0;
	bin_io::instance()->new_nvb_reader( &nvbr );
	CGBIO_ERROR err = nvbr->loadFromString((const char*)sourceData,size);
	if (err != CGBIO_ERROR_NO_ERROR)
		return -1;

	bool doSwap = !(nvbr->endianness() == (HOST_ENDIANNESS)elfEndianness);

	CGprofile NVProfile = nvbr->profile();
	if (NVProfile == (CGprofile)7005 )
		NVProfile = CG_PROFILE_SCE_VP_RSX;
	if (NVProfile == (CGprofile)7006 )
		NVProfile = CG_PROFILE_SCE_FP_RSX;
	if (NVProfile == CG_PROFILE_SCE_VP_TYPEB || NVProfile == CG_PROFILE_SCE_VP_RSX)
	{
		bIsVertexProgram = true;
	}
	else if (NVProfile == CG_PROFILE_SCE_FP_TYPEB || NVProfile == CG_PROFILE_SCE_FP_RSX)
	{
		bIsVertexProgram = false;
	}
	else
	{
		return -1;
	}

	CgProgramHeader cgShader;
	memset(&cgShader,0,sizeof(CgProgramHeader));
	cgShader.profile = CNV2END((unsigned short) NVProfile);
	cgShader.compilerVersion = 0;//TODO
	if (bIsVertexProgram)
	{
		const CgBinaryVertexProgram *nvVertex = nvbr->vertex_program();
		if (doSwap)
		{
			cgShader.instructionCount = ENDSWAP(nvVertex->instructionCount);
			cgShader.attributeInputMask = ENDSWAP(nvVertex->attributeInputMask);
			cgShader.vertexProgram.instructionSlot = ENDSWAP(nvVertex->instructionSlot);
			cgShader.vertexProgram.registerCount = ENDSWAP(nvVertex->registerCount);
			cgShader.vertexProgram.attributeOutputMask = ENDSWAP(nvVertex->attributeOutputMask);
		}
		else
		{
			cgShader.instructionCount = nvVertex->instructionCount;
			cgShader.attributeInputMask = nvVertex->attributeInputMask;
			cgShader.vertexProgram.instructionSlot = nvVertex->instructionSlot;
			cgShader.vertexProgram.registerCount = nvVertex->registerCount;
			cgShader.vertexProgram.attributeOutputMask = nvVertex->attributeOutputMask;
		}
	}
	else
	{
		const CgBinaryFragmentProgram *nvFragment = nvbr->fragment_program();
		unsigned char flags;
		if (doSwap)
		{
			cgShader.instructionCount = ENDSWAP(nvFragment->instructionCount);
			cgShader.attributeInputMask = ENDSWAP(nvFragment->attributeInputMask);
			cgShader.fragmentProgram.partialTexType = ENDSWAP(nvFragment->partialTexType);
			cgShader.fragmentProgram.texcoordInputMask = ENDSWAP(nvFragment->texCoordsInputMask);
			cgShader.fragmentProgram.texcoord2d = ENDSWAP(nvFragment->texCoords2D);
			cgShader.fragmentProgram.texcoordCentroid = ENDSWAP(nvFragment->texCoordsCentroid);
			cgShader.fragmentProgram.registerCount = ENDSWAP(nvFragment->registerCount);
			flags =
				(nvFragment->outputFromH0 ? CGF_OUTPUTFROMH0 : 0) |
				(nvFragment->depthReplace ? CGF_DEPTHREPLACE : 0) |
				(nvFragment->pixelKill ? CGF_PIXELKILL : 0);
		}
		else
		{
			cgShader.instructionCount = nvFragment->instructionCount;
			cgShader.attributeInputMask = nvFragment->attributeInputMask;
			cgShader.fragmentProgram.partialTexType = nvFragment->partialTexType;
			cgShader.fragmentProgram.texcoordInputMask = nvFragment->texCoordsInputMask;
			cgShader.fragmentProgram.texcoord2d = nvFragment->texCoords2D;
			cgShader.fragmentProgram.texcoordCentroid = nvFragment->texCoordsCentroid;
			cgShader.fragmentProgram.registerCount = nvFragment->registerCount;
			flags =
				(nvFragment->outputFromH0 ? CGF_OUTPUTFROMH0 : 0) |
				(nvFragment->depthReplace ? CGF_DEPTHREPLACE : 0) |
				(nvFragment->pixelKill ? CGF_PIXELKILL : 0);
		}
		cgShader.fragmentProgram.flags = CNV2END(flags);
	}

	unsigned int *tmp = (unsigned int *)nvbr->ucode();
	const char *ucode;
	unsigned int *buffer = NULL;
	if (doSwap)
	{
		int size = (int)nvbr->ucode_size()/sizeof(unsigned int);
		buffer = new unsigned int[size];
		for (int i=0;i<size;i++)
		{
			unsigned int val = ENDSWAP(tmp[i]);
			if (!bIsVertexProgram)
				val = swap16(val);
			buffer[i] = val;
		}
		ucode = (const char*)buffer;
	}
	else
	    {
		ucode = (const char*)tmp;
		int size = (int)nvbr->ucode_size()/sizeof(unsigned int);
		buffer = new unsigned int[size];
		for (int i=0;i<size;i++)
		{
			buffer[i] = tmp[i];
		}
		ucode = (const char*)buffer;
		// end workaround
	    }

	int ucodeSize = nvbr->ucode_size();

	CgStructureType root;
	root._insideArray = false;
	root._root = true;

	int paramIndex = -1;

	CgStructureType::CgStructuralElement *current = NULL;
	int embeddedConstants = 0;
	int rootChildIndex = -1;

	int i;
	for (i = 0; i < (int)nvbr->number_of_params(); i++)
	{
		CGtype type;
		CGresource res;
		CGenum var;
		int rin;
		const char *name;
		std::vector<float> dv;
		std::vector<unsigned int> ec;
		const char *sem;
		CGenum dir;
		int no;
		bool is_referenced;
		bool is_shared;
		nvbr->get_param( i, type, res, var, rin, &name, dv, ec, &sem, no, is_referenced, is_shared );


		if (strlen(sem)>=2 && sem[0] == 'C')
		{
			const char *szSem = sem+1;
			while (*szSem != '\0')
			{
				if ( (*szSem) < '0' || (*szSem) > '9')
					break;
				szSem++;
			}
			if (*szSem == '\0')
				is_shared = 1;
		}

		const char *parameterName = name;
		const char *structureEnd = NULL;
		CgStructureType *container = &root;
		int done = 0;
		const char * structureStart = parameterName;
		while (!done && *structureStart)
		{
			structureEnd = strpbrk(structureStart, ".[");

			if (structureEnd)
			{
				if (*structureEnd == '[' && type >= CG_SAMPLER1D && type <= CG_SAMPLERCUBE)
				{
					const char *closed = strchr(structureEnd, ']');
					const char *somethingElse = strpbrk(closed, ".[");
					if (!somethingElse)
						structureEnd = NULL;
				}
			}

			if (structureEnd == NULL)
			{
				structureEnd = structureStart + strlen(structureStart);
				done = 1;
			}

			char structName[256];
			int length = (int)(structureEnd - structureStart);
			strncpy(structName, structureStart, length);
			structName[length] = '\0';

			CgStructureType::CgStructuralElement *structuralElement = NULL;
			int j=0;
			int elementCount = (int)container->_elements.size();
			for (j=0;j<elementCount;j++)
			{
				structuralElement = &container->_elements[j];
				if (!strcmp(structuralElement->_name,structName))
				{
					if ( (no == -1 && (structuralElement->_flags & CGPF_GLOBAL)) ||
						 (no != -1 && !(structuralElement->_flags & CGPF_GLOBAL)))
						break;
				}
			}
			if (j==elementCount)
			{
				if (container == &root)
				{
					if (rootChildIndex != j)
						rootChildIndex = j;
				}

				container->_elements.resize(elementCount+1);
				structuralElement = &container->_elements[elementCount];
				strncpy(structuralElement->_name,structName,sizeof(structuralElement->_name));
				structuralElement->_name[sizeof(structuralElement->_name)-1] = '\0';
				structuralElement->_flags = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
				int dimensionCount = 0;

				if (strncmp(sem, "COLOR", 5) == 0 || strncmp(sem, "NORMAL", 6) == 0)
					structuralElement->_flags |= CGP_NORMALIZE;


				int isStructure = (*structureEnd == '.');
				if (*structureEnd == '[')
				{
					dimensionCount++;
					const char *arrayEnd = strchr(structureEnd,']')+1;
					while (*arrayEnd == '[')
					{
						arrayEnd = strchr(arrayEnd,']')+1;
						dimensionCount++;
					}
					if (*arrayEnd == '.')
						isStructure = true;
				}

				if (dimensionCount)
				{
					CgArrayType *arrayType = new CgArrayType;
					arrayType->_dimensionCount = dimensionCount;
					arrayType->_dimensionItemCountsOffset = (unsigned short)containers._dimensions.size();
					int k;
					for (k=0;k<dimensionCount;k++)
						containers._dimensions.push_back(CNV2END((short)0));
					structuralElement->_type = arrayType;
					if (isStructure)
					{
						container = new CgStructureType;
						container->_insideArray = true;
						arrayType->_elementType = container;
					}
					else
					{
						arrayType->_elementType = new CgBaseType;
						arrayType->_elementType->_type = type - CG_TYPE_START_ENUM;
					}
					arrayType->_elementType->_resource = res;
					arrayType->_elementType->_resourceIndex = -1;
					if (bIsVertexProgram && strlen(sem)>=2 && sem[0] == 'C')
					{
						const char *szSem = sem+1;
						while (*szSem != '\0')
						{
							if ( (*szSem) < '0' || (*szSem) > '9')
								break;
							szSem++;
						}
						if (*szSem == '\0')
						{
							is_shared = 1;
							int registerIndex = atoi(sem+1);
							structuralElement->_flags |= CGP_CONTIGUOUS;
							structuralElement->_flags |= CGPF_SHARED;
							structuralElement->_type->_resourceIndex = registerIndex;
						}
						else
							structuralElement->_type->_resourceIndex = (int)containers._resources.size();
					}
					else
						structuralElement->_type->_resourceIndex = (int)containers._resources.size();

					structuralElement->_type->_resource = res;
				}
				else
				{
					if (isStructure)
					{
						bool insideArray = container->_insideArray;
						container = new CgStructureType;
						container->_insideArray = insideArray;
						structuralElement->_type = container;
					}
					else
					{
						structuralElement->_type = new CgBaseType;
						structuralElement->_type->_type = type - CG_TYPE_START_ENUM;
						structuralElement->_type->_resource = res;
						if (classnames[structuralElement->_type->_type-1] == CG_PARAMETERCLASS_MATRIX)
						{
							if (bIsVertexProgram)
							{
								structuralElement->_type->_resourceIndex = (short)rin;
							}
							else
								structuralElement->_type->_resourceIndex = (int)containers._resources.size();
						}
						else
						{
							if (!container->_insideArray)
							{
								if (bIsVertexProgram)
									structuralElement->_type->_resourceIndex = rin;
								else
								{
									if (structuralElement->_flags & CGPV_VARYING)
										structuralElement->_type->_resourceIndex = -1;
									else
									{
										structuralElement->_type->_resourceIndex = (int)containers._resources.size();
										containers._resources.push_back(CNV2END((unsigned short)rin));
										int size = (int)ec.size();
										containers._resources.push_back(CNV2END((unsigned short)size));
										int k;
										for (k=0;k<size;k++)
											containers._resources.push_back(CNV2END((unsigned short)ec[k]));
									}
								}
							}
							else
							{
								structuralElement->_type->_resourceIndex = (short)-1;
								structuralElement->_type->_resource = (unsigned short)res;
							}
						}
					}
				}
			}
			else
			{
				if (structuralElement->_type->_type ==  CG_STRUCT+128)
				{
					container = (CgStructureType*)structuralElement->_type;
				}
				else if (structuralElement->_type->_type ==  CG_ARRAY+128)
				{
					CgArrayType *arrayType = (CgArrayType *)structuralElement->_type;
					if (arrayType->_elementType->_type >128 )
					{
						container = (CgStructureType*)arrayType->_elementType;
					}
				}
			}

			if (dv.size())
			{
				int  size = (int)containers._defaultValuesIndices.size();
				if (!size  || (containers._defaultValuesIndices[size-2] != CNV2END((unsigned short)(rootChildIndex))))
				{
					containers._defaultValuesIndices.push_back(CNV2END((unsigned short)(rootChildIndex)));
					containers._defaultValuesIndices.push_back(CNV2END((unsigned short)defaultValues.size()));
				}
			}

			if (container->_insideArray && done)
			{
				if (is_referenced)
				{
					bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
					structuralElement->_flags = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
					if (sharedContiguous)
						structuralElement->_flags |= ( CGP_CONTIGUOUS | CGPF_SHARED);
				}
				if (bIsVertexProgram)
				{
					if (!is_shared)
						containers._resources.push_back(CNV2END((unsigned short)rin));
				}
				else
				{
					if (structuralElement->_flags & CGPV_VARYING)
						containers._resources.push_back(CNV2END((unsigned short)rin));
					else
					{
						containers._resources.push_back(CNV2END((unsigned short)rin));
						int size = (int)ec.size();
						containers._resources.push_back(CNV2END((unsigned short)size));
						int k;
						for (k=0;k<size;k++)
							containers._resources.push_back(CNV2END((unsigned short)ec[k]));
					}
				}
			}

			CgArrayType *arrayType = NULL;
			if (*structureEnd == '[')
			{
				int arrayCellIndex = 0;
				const char *arrayStart = structureEnd;
				const char *arrayEnd = structureEnd;
				CgBaseType *itemType = structuralElement->_type;
				if (itemType->_type >= 128)
				{
					arrayType = (CgArrayType *)itemType;
					int dimensionCount = 0;
					while (*arrayStart == '[' && dimensionCount<arrayType->_dimensionCount)
					{
						arrayEnd = strchr(arrayStart+1,']');
						int length =(int)(arrayEnd - arrayStart - 1);
						char indexString[16];
						strncpy(indexString,arrayStart+1,length);
						indexString[length] = '\0';
						int index = atoi(indexString);
						int dim = CNV2END(containers._dimensions[arrayType->_dimensionItemCountsOffset + dimensionCount]);
						if ((index+1) > dim)
							containers._dimensions[arrayType->_dimensionItemCountsOffset + dimensionCount] = CNV2END((short)(index+1));
						arrayCellIndex += index*getSizeofSubArray(containers,arrayType->_dimensionItemCountsOffset + dimensionCount,arrayType->_dimensionCount - dimensionCount -1,endianness);
						arrayStart = arrayEnd+1;
						dimensionCount++;
					}
					structureEnd = arrayStart;

					itemType = arrayType->_elementType;
					if (itemType->_type<128)
					{
						int rowCount = rows[itemType->_type-1];
						if (!rowCount)
						{
							bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
							if (!bIsVertexProgram || !sharedContiguous)
							{
								containers._resources.push_back(CNV2END((unsigned short)rin));
							}

							if (!bIsVertexProgram)
							{
								int size = (int)ec.size();
								containers._resources.push_back(CNV2END((unsigned short)size));
								int k;
								for (k=0;k<size;k++)
									containers._resources.push_back(CNV2END((unsigned short)ec[k]));
							}
							done = 1;
						}
					}

					if (*arrayStart == '\0')
					{
						done = 1;
					}
				}

				if (*structureEnd == '[')
				{
					int dimensionCount = 0;
					while (*arrayStart == '[')
					{
						arrayEnd = strchr(arrayStart+1,']');
						if (itemType->_type <128)
						{
							if (structuralElement != current)
							{
								embeddedConstants = 0;
								current = structuralElement;
							}

							int length =(int)(arrayEnd - arrayStart - 1);
							char indexString[16];
							strncpy(indexString,arrayStart+1,length);
							indexString[length] = '\0';

							if (bIsVertexProgram)
							{
								if (arrayType)
								{
									bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
									if (!sharedContiguous)
									{
										containers._resources.push_back(CNV2END((unsigned short)rin));
									}
								}
							}
							else
							{
								containers._resources.push_back(CNV2END((unsigned short)rin));
								int size = (int)ec.size();
								containers._resources.push_back(CNV2END((unsigned short)size));
								int k;
								for (k=0;k<size;k++)
									containers._resources.push_back(CNV2END((unsigned short)ec[k]));
								embeddedConstants += k+1;
							}
							done = 1;
						}
						arrayStart = arrayEnd+1;
						dimensionCount++;
					}
					structureEnd = arrayEnd;
				}

				if (is_referenced)
				{
					bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);

					unsigned short flag = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
					structuralElement->_flags = flag;

					if (sharedContiguous)
						structuralElement->_flags |= ( CGP_CONTIGUOUS | CGPF_SHARED);

					structuralElement->_type->_resource = res;
					if (arrayType)
						arrayType->_elementType->_resource = res;
				}
			}


			if (done && dv.size())
			{
				for ( int jj = 0; jj < (int)dv.size(); ++jj )
				{
					defaultValues.push_back(dv[jj]);
				}
			}

			if (done)
			{
				if (strlen(sem))
				{
					strncpy(structuralElement->_semantic,sem,sizeof(structuralElement->_semantic));
				}
				else
					structuralElement->_semantic[0] = '\0';
			}
			structureStart = structureEnd+1;
		}
	}

	nvbr->release();
	bin_io::delete_instance();

	std::vector<CgParameterEntry> parameterEntries;
	std::vector<char> parameterResources;
	fillStructureItems(containers,&root,endianness,parameterEntries,parameterResources,stringTable);

	CgParameterTableHeader header;
	memset(&header,0,sizeof(CgParameterTableHeader));
	header.entryCount = (unsigned short)parameterEntries.size();
	header.resourceTableOffset = sizeof(CgParameterTableHeader) + (unsigned short)(parameterEntries.size()*sizeof(parameterEntries[0]) + parameterResources.size()*sizeof(parameterResources[0]));
	header.defaultValueIndexTableOffset = header.resourceTableOffset + (unsigned short)containers._resources.size() * sizeof(containers._resources[0]);
	header.defaultValueIndexCount = (unsigned short)containers._elfDefaultsIndices.size()/2;
	header.semanticIndexTableOffset = header.defaultValueIndexTableOffset + (unsigned short)containers._elfDefaultsIndices.size() * sizeof (containers._elfDefaultsIndices[0]);
	header.semanticIndexCount = (unsigned short)containers._semanticIndices.size();

	header.entryCount = CNV2END(header.entryCount);
	header.resourceTableOffset = CNV2END(header.resourceTableOffset);
	header.defaultValueIndexTableOffset = CNV2END(header.defaultValueIndexTableOffset);
	header.defaultValueIndexCount = CNV2END(header.defaultValueIndexCount);
	header.semanticIndexTableOffset = CNV2END(header.semanticIndexTableOffset);
	header.semanticIndexCount = CNV2END(header.semanticIndexCount);

	size_t parameterTableSize = sizeof(CgParameterTableHeader);
	parameterTableSize += array_size(parameterEntries);
	parameterTableSize += array_size(parameterResources);
	parameterTableSize += array_size(containers._resources);
	parameterTableSize += array_size(containers._elfDefaultsIndices);
	parameterTableSize += array_size(containers._semanticIndices);

	int ucodeOffset = (((sizeof(CgProgramHeader)-1)/16)+1)*16;
	size_t programSize = ucodeOffset + ucodeSize + parameterTableSize;
	char *program = new char[programSize];

	memcpy(program,&cgShader,sizeof(CgProgramHeader));
	if (ucodeOffset-sizeof(CgProgramHeader))
		memset(program+sizeof(CgProgramHeader),0,ucodeOffset-sizeof(CgProgramHeader));

	memcpy(program + ucodeOffset,ucode,ucodeSize);
	if (!bIsVertexProgram && doSwap)
		delete[] buffer;
	else
	    delete[] buffer;

	char *parameterOffset = program + ucodeOffset + ucodeSize;

	memcpy(parameterOffset,&header,sizeof(CgParameterTableHeader));
	parameterOffset += sizeof(CgParameterTableHeader);
	array_push(parameterOffset, parameterEntries);
	array_push(parameterOffset, parameterResources);
	array_push(parameterOffset, containers._resources);
	array_push(parameterOffset, containers._elfDefaultsIndices);
	array_push(parameterOffset, containers._semanticIndices);

	*binarySize = (int)programSize;
	*binaryShader = program;

	return 0;
}

int convertNvToElfFreeBinaryShader(void *binaryShader)
{
	char *program = (char *)binaryShader;
	delete[] program;
	return 0;
}

static void pushbackUnsignedShort(std::vector<char> &parameterResources, unsigned short value)
{
	size_t size = parameterResources.size();
	parameterResources.resize(size + 2);
	*((unsigned short*)&parameterResources[size]) = value;
}

static void fillStructureItems(_CGNVCONTAINERS &containers, CgStructureType *structure, int endianness,
						std::vector<CgParameterEntry> &parameterEntries,std::vector<char> &parameterResources,
						std::vector<char> &stringTable, unsigned short *arrayResourceIndex, unsigned short *arrayDefaultValueIndex)
{
	unsigned char elfEndianness = endianness;

	int currentDefaultIndex = 0;
	int count = (int)structure->_elements.size();
	int i;
	for (i=0;i<count;i++)
	{
		CgStructureType::CgStructuralElement *structuralElement = &structure->_elements[i];
		size_t size = parameterEntries.size();
		parameterEntries.resize(size+1);
		CgParameterEntry *parameterEntry = &parameterEntries[size];
		parameterEntry->nameOffset = CNV2END((int)stringTableAddUnique(stringTable, structuralElement->_name));
		if (structuralElement->_semantic[0])
		{
			CgParameterSemantic semantic;
			semantic.entryIndex = CNV2END((unsigned short)size);
			semantic.reserved = 0;
			semantic.semanticOffset = CNV2END((int)stringTableAddUnique(stringTable, structuralElement->_semantic));
			containers._semanticIndices.push_back(semantic);
		}
		parameterEntry->flags = CNV2END(structuralElement->_flags);
		unsigned short typeIndex = ((unsigned short)parameterResources.size());
		parameterEntry->typeIndex = CNV2END(typeIndex);

		CgBaseType *itemType = structuralElement->_type;
		unsigned short _resource = itemType->_resource;
		unsigned short _resourceIndex = itemType->_resourceIndex;

		int parameterEntryIndex;

		if (itemType->_type-128 == CG_ARRAY)
		{
			CgArrayType *arrayType = (CgArrayType *)structuralElement->_type;
			int arraySize = getSizeofSubArray(containers,arrayType->_dimensionItemCountsOffset,arrayType->_dimensionCount,endianness);
			itemType = arrayType->_elementType;
			unsigned short arrayFlag = CGP_ARRAY;
			parameterEntry->flags |= CNV2END(arrayFlag);
			parameterResources.resize(typeIndex+sizeof(CgParameterArray));

			CgParameterArray *parameterArray = (CgParameterArray *)(&parameterResources[typeIndex]);
			if (itemType->_type-128 == CG_STRUCT )
				parameterArray->arrayType = CNV2END((unsigned short)CG_STRUCT);
			else
				parameterArray->arrayType = CNV2END(itemType->_type+128);
			parameterArray->dimensionCount = CNV2END((unsigned short)arrayType->_dimensionCount);
			int j;
			for (j=0;j<arrayType->_dimensionCount;j++)
			{
				pushbackUnsignedShort(parameterResources,(unsigned short)containers._dimensions[arrayType->_dimensionItemCountsOffset+j]);
			}
			if (arrayType->_dimensionCount&1)
				pushbackUnsignedShort(parameterResources,CNV2END((unsigned short)0));

			if (itemType->_type-128 == CG_STRUCT )
			{
				unsigned short unrolledFlag  = CGP_UNROLLED;
				parameterEntry->flags |= CNV2END(unrolledFlag);
				CgStructureType *structureType = (CgStructureType*)itemType;
				int k;

				unsigned short _arrayResourceIndex = (unsigned short)(arrayType->_resourceIndex);
				unsigned short _arrayDefaultValueIndex = 0;

				bool hasDefaults = false;
				if (structure->_root && containers._defaultValuesIndices.size())
				{
					if (containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
					{
						hasDefaults = true;
						_arrayDefaultValueIndex = containers._defaultValuesIndices[currentDefaultIndex*2+1];
					}
				}

				for (k=0;k<arraySize;k++)
				{
					size_t size = parameterEntries.size();
					parameterEntries.resize(size+1);

					CgParameterEntry &parameterArrayEntry = parameterEntries[size];

					char buffer[256];
					snprintf(buffer, sizeof(buffer), "%s[%i]",structuralElement->_name,k);
					parameterArrayEntry.nameOffset = CNV2END((int)stringTableAddUnique(stringTable, buffer));
					parameterArrayEntry.flags = CNV2END(structuralElement->_flags);
					unsigned short structureFlag = CGP_STRUCTURE;
					parameterArrayEntry.flags |= CNV2END(structureFlag);

					unsigned short arrayEntryTypeIndex = (unsigned short)parameterResources.size();
					parameterResources.resize(arrayEntryTypeIndex+sizeof(CgParameterStructure));
					parameterArrayEntry.typeIndex = CNV2END(arrayEntryTypeIndex);

					CgParameterStructure *parameterStructure = (CgParameterStructure*)(&parameterResources[arrayEntryTypeIndex]);
					parameterStructure->memberCount = CNV2END((unsigned short)structureType->_elements.size());
					parameterStructure->reserved = CNV2END((unsigned short)0);

					if (hasDefaults)
						fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable,&_arrayResourceIndex,&_arrayDefaultValueIndex);
					else
						fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable,&_arrayResourceIndex);
				}

				if (hasDefaults)
				{
					currentDefaultIndex++;
				}
				continue;
			}
			else
			{
				size_t size = parameterEntries.size();
				parameterEntries.resize(size+1);
				parameterEntry = &parameterEntries[size];
				parameterEntry->nameOffset = CNV2END(0);
				parameterEntry->flags = CNV2END(structuralElement->_flags);

				typeIndex = ((unsigned short)parameterResources.size());
				parameterEntry->typeIndex = CNV2END(typeIndex);

				parameterEntryIndex = (int)size - 1;
			}
		}
		else
		{
			unsigned short contiguousFlag = CGP_CONTIGUOUS;
			parameterEntry->flags |= CNV2END(contiguousFlag);
			size_t size = parameterEntries.size();
			parameterEntryIndex = (int)size - 1;
		}

		if (itemType->_type<128)
		{
			parameterResources.resize(typeIndex+sizeof(CgParameterResource));
			CgParameterResource *parameterResource = (CgParameterResource*)(&parameterResources[typeIndex]);

			if (itemType->_type + CG_TYPE_START_ENUM == CG_BOOL)
			    {
				parameterResource->type = CNV2END((unsigned short)CGP_SCF_BOOL);
			    }
			else
			    {
				parameterResource->type = CNV2END((unsigned short)(itemType->_type + CG_TYPE_START_ENUM));
			    }

			if ((structuralElement->_flags & CGPV_MASK) == CGPV_UNIFORM || (structuralElement->_flags & CGPV_MASK) == CGPV_CONSTANT)
			{
				if (itemType->_type +CG_TYPE_START_ENUM >= CG_SAMPLER1D && itemType->_type +CG_TYPE_START_ENUM<= CG_SAMPLERCUBE)
					parameterResource->resource = CNV2END(_resource);
				else
				{
					if (arrayResourceIndex)
					{
						unsigned short tmp = *arrayResourceIndex;
						unsigned short localflags = CNV2END(parameterEntry->flags);
						if (!bIsVertexProgram)
						{
							parameterResource->resource = CNV2END(tmp);
							int embeddedConstantCount = CNV2END(containers._resources[tmp+1]);
							(*arrayResourceIndex) = tmp+1+1+embeddedConstantCount;
							if (embeddedConstantCount == 0 && (CNV2END(containers._resources[tmp]) == 0))
							{
								if (parameterResource->resource == 0xffff)
									localflags &= ~CGPF_REFERENCED;
							}
						}
						else
						{

							if (structuralElement->_flags & CGPF_SHARED)
							{
								int stride = getStride(itemType);
								parameterResource->resource = *arrayResourceIndex;
								(*arrayResourceIndex) = tmp+stride;
							}
							else
							{
								parameterResource->resource = containers._resources[tmp];
								(*arrayResourceIndex) = tmp+1;
							}

							if (parameterResource->resource == 0xffff)
								localflags &= ~CGPF_REFERENCED;
						}
						parameterEntry->flags = CNV2END(localflags);
					}
					else
						parameterResource->resource = CNV2END(_resourceIndex);
				}
			}
			else
			{
				parameterResource->resource = CNV2END(itemType->_resource);
			}

			if (containers._defaultValuesIndices.size())
			{
				if (structure->_root)
				{
					if (currentDefaultIndex < (int)(containers._defaultValuesIndices.size()/2) && containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
					{
						containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
						containers._elfDefaultsIndices.push_back(containers._defaultValuesIndices[currentDefaultIndex*2+1]);
						currentDefaultIndex++;
					}
				}
				else if (arrayDefaultValueIndex)
				{
					containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
					containers._elfDefaultsIndices.push_back(*arrayDefaultValueIndex);

					int typeRegisterCount = getStride(itemType);
					*arrayDefaultValueIndex = CNV2END( (unsigned short)((CNV2END((*arrayDefaultValueIndex)))+typeRegisterCount*4));

				}
			}
		}
		else if (itemType->_type == CG_STRUCT + 128)
		{
			unsigned short structureFlag  = CGP_STRUCTURE;
			parameterEntry->flags |= CNV2END(structureFlag);

			CgStructureType *structureType = (CgStructureType*)itemType;
			parameterResources.resize(typeIndex+sizeof(CgParameterStructure));
			CgParameterStructure *parameterStructure = (CgParameterStructure*)(&parameterResources[typeIndex]);
			parameterStructure->memberCount = CNV2END((unsigned short)structureType->_elements.size());
			parameterStructure->reserved = CNV2END((unsigned short)0);

			fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable);

			if (containers._defaultValuesIndices.size() && structure->_root)
			{
				if (currentDefaultIndex < (int)(containers._defaultValuesIndices.size()/2) && containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
				{
						containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
						containers._elfDefaultsIndices.push_back(containers._defaultValuesIndices[currentDefaultIndex*2+1]);
						currentDefaultIndex++;
				}
			}
		}
	}
}


static int getStride(CgBaseType *type)
{
	if (type->_type <128)
	{
		if (classnames[type->_type-1] == CG_PARAMETERCLASS_MATRIX)
			return rows[type->_type-1];
		else
			return 1;
	}
	else
	{
		if (type->_type == CG_STRUCT + 128)
		{
			CgStructureType *structureType = (CgStructureType *)type;
			int res = 0;
			int i;
			int count = (int)structureType->_elements.size();
			for (i=0;i<count;i++)
				res += getStride(structureType->_elements[i]._type);
			return res;
		}
		else
		{
			return -9999999;
		}
	}
}

static int getSizeofSubArray(_CGNVCONTAINERS &containers, int dimensionIndex, int dimensionCount, int endianness)
{
	unsigned char elfEndianness = endianness;
	int res = 1;
	int i;
	for (i=0;i<dimensionCount;i++)
	{
		res *= (int)CNV2END(containers._dimensions[dimensionIndex + i]);
	}
	return res;
}

template<class Type> static size_t array_size(std::vector<Type> &array)
{
	return (unsigned int)array.size()*sizeof(array[0]);
}

template<class Type> static void array_push(char* &parameterOffset, std::vector<Type> &array)
{
	size_t dataSize = array.size()*sizeof(array[0]);
	memcpy(parameterOffset,&array[0],dataSize);
	parameterOffset += dataSize;
}

unsigned int inline static swap16(const unsigned int v)
{
	return (v>>16) | (v<<16);
}

unsigned short getFlags(CGenum var, CGenum dir, int no,	bool is_referenced, bool is_shared, int paramIndex)
{
	(void)paramIndex;
	unsigned short flags = 0;

	if (var == CG_VARYING)
		flags |= CGPV_VARYING;
	else if (var == CG_UNIFORM)
		flags |= CGPV_UNIFORM;
	else if (var == CG_CONSTANT)
		flags |= CGPV_CONSTANT;
	else if (var == CG_MIXED)
		flags |= CGPV_MIXED;

	if (is_referenced)
		flags |= CGPF_REFERENCED;
	if (is_shared)
		flags |= CGPF_SHARED;

	if (no == -1)
		flags |= CGPF_GLOBAL;
	else if (no == -2)
		flags |= CGP_INTERNAL;
	else
	{
		paramIndex = no;
	}
	return flags;
}
