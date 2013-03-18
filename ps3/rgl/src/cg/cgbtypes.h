#ifndef CGBTYPES_HEADER
#define CGBTYPES_HEADER

// parameter structure
typedef struct _Elf32_cgParameter {
	uint32_t cgp_name; // index of name in strtab
	uint32_t cgp_semantic; // index of semantic string in strtab
	uint16_t cgp_default;		// index of default data in const //Reduced to half
	uint16_t cgp_reloc;		// index of reloc indices in rel 
	uint16_t cgp_resource;		// index of hardware resource assigned
	uint16_t cgp_resource_index;		// index of hardware resource assigned
	unsigned char cgp_type;
	uint16_t cgp_info;
	unsigned char unused;
} Elf32_cgParameter; //20 bytes

#define CGF_OUTPUTFROMH0 0x01
#define CGF_DEPTHREPLACE 0x02
#define CGF_PIXELKILL 0x04

#endif
