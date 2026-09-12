#ifndef DEFINITIONS_H
#define DEFINITIONS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

typedef unsigned int uint32_t;
typedef int int32_t;

#ifdef __LP64__
typedef unsigned long uint64_t;
typedef long int64_t;
typedef uint64_t size_t;
typedef size_t uintptr_t;
#else
typedef uint32_t size_t;
typedef size_t uintptr_t;
#endif
typedef unsigned long tid_t;

/************************ ERRORS *************************/
typedef unsigned int error_code;
#define ERROR_CODE_OK               0x00000000
#define ERROR_CODE_NO_LIBRARY       0xf1000001
#define ERROR_CODE_NO_FUNCTION      0xf1000002
#define ERROR_CODE_NO_SECTION 		0xf1000003
#define ERROR_CODE_INVALID_PARAMS   0xf4000000
#define ERROR_CODE_NO_MEMORY        0xff000000
#define ERROR_CODE_MPROTECT_FAIL	0xff000001
#define ERROR_CODE_PTRACE_DENIED	0xff100000
#define ERROR_CODE_PTRACE_FAIL		0xff100001
#define ERROR_CODE_UNKNOWN          0xffffffff	

#define OK(err) (err == ERROR_CODE_OK)
/**********************************************************/

/************************* ELF ****************************/

#define EI_MAG0		0		/* File identification byte 0 index */
#define ELFMAG0		0x7f		/* Magic number byte 0 */

#define EI_MAG1		1		/* File identification byte 1 index */
#define ELFMAG1		'E'		/* Magic number byte 1 */

#define EI_MAG2		2		/* File identification byte 2 index */
#define ELFMAG2		'L'		/* Magic number byte 2 */

#define EI_MAG3		3		/* File identification byte 3 index */
#define ELFMAG3		'F'		/* Magic number byte 3 */

#define EI_DATA		5		/* Data encoding byte index */
#define ELFDATANONE	0		/* Invalid data encoding */
#define ELFDATA2LSB	1		/* 2's complement, little endian */
#define ELFDATA2MSB	2		/* 2's complement, big endian */
#define ELFDATANUM	3

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define EI_NIDENT (16)
#define DT_NULL		0		/* Marks end of dynamic section */
#define DT_PLTRELSZ   2
#define DT_NEEDED	1		/* Name of needed library */
#define DT_HASH		4		/* Address of symbol hash table */
#define DT_STRTAB	5		/* Address of string table */
#define DT_SYMTAB	6		/* Address of symbol table */
#define DT_RELA       7
#define DT_RELASZ     8
#define DT_RELAENT    9

#define DT_REL        17
#define DT_RELSZ      18
#define DT_RELENT     19

#define DT_PLTREL     20
#define DT_JMPREL     23
#define	DT_INIT_ARRAY	25		/* Array with addresses of init fct */
#define	DT_FINI_ARRAY	26		/* Array with addresses of fini fct */
#define	DT_INIT_ARRAYSZ	27		/* Size in bytes of DT_INIT_ARRAY */
#define	DT_FINI_ARRAYSZ	28		/* Size in bytes of DT_FINI_ARRAY */

#define DT_GNU_HASH	0x6ffffef5	/* GNU-style hash table.  */

#define SHT_DYNSYM	  11		/* Dynamic linker symbol table */

#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */

#define ET_DYN		3		/* Shared object file */

#define STT_FUNC	2		/* Symbol is a code object */

#define SHN_UNDEF	0		/* Undefined section */

#define R_X86_64_64		1	/* Direct 64 bit  */
#define R_X86_64_GLOB_DAT	6	/* Create GOT entry */
#define R_X86_64_JUMP_SLOT	7	/* Create PLT entry */
#define R_X86_64_RELATIVE	8	/* Adjust by program base */
#define R_386_RELATIVE	8	/* Adjust by program base */

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */

#define ELF32_ST_BIND(val)		(((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)		((val) & 0xf)
#define ELF64_ST_BIND(val)		ELF32_ST_BIND (val)
#define ELF64_ST_TYPE(val)		ELF32_ST_TYPE (val)

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffff)

#define ELF32_R_SYM(i)			((i) >> 16)
#define ELF32_R_TYPE(i)			((i) & 0xffff)


/* Type for a 16-bit quantity.  */
typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

/* Types for signed and unsigned 32-bit quantities.  */
typedef uint32_t Elf32_Word;
typedef	int32_t  Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef	int32_t  Elf64_Sword;

/* Types for signed and unsigned 64-bit quantities.  */
typedef uint64_t Elf32_Xword;
typedef	int64_t  Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef	int64_t  Elf64_Sxword;

/* Type of addresses.  */
typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

/* Type of file offsets.  */
typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

/* Type for version symbol information.  */
typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;


#ifdef __LP64__
/* Elf header */
typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
  Elf64_Half	e_type;			/* Object file type */
  Elf64_Half	e_machine;		/* Architecture */
  Elf64_Word	e_version;		/* Object file version */
  Elf64_Addr	e_entry;		/* Entry point virtual address */
  Elf64_Off	e_phoff;		/* Program header table file offset */
  Elf64_Off	e_shoff;		/* Section header table file offset */
  Elf64_Word	e_flags;		/* Processor-specific flags */
  Elf64_Half	e_ehsize;		/* ELF header size in bytes */
  Elf64_Half	e_phentsize;		/* Program header table entry size */
  Elf64_Half	e_phnum;		/* Program header table entry count */
  Elf64_Half	e_shentsize;		/* Section header table entry size */
  Elf64_Half	e_shnum;		/* Section header table entry count */
  Elf64_Half	e_shstrndx;		/* Section header string table index */
} Elf64_Ehdr;

/* Section header.  */
typedef struct
{
  Elf64_Word	sh_name;		/* Section name (string tbl index) */
  Elf64_Word	sh_type;		/* Section type */
  Elf64_Xword	sh_flags;		/* Section flags */
  Elf64_Addr	sh_addr;		/* Section virtual addr at execution */
  Elf64_Off	sh_offset;		/* Section file offset */
  Elf64_Xword	sh_size;		/* Section size in bytes */
  Elf64_Word	sh_link;		/* Link to another section */
  Elf64_Word	sh_info;		/* Additional section information */
  Elf64_Xword	sh_addralign;		/* Section alignment */
  Elf64_Xword	sh_entsize;		/* Entry size if section holds table */
} Elf64_Shdr;

/* Program segment header.  */
typedef struct
{
  Elf64_Word	p_type;			/* Segment type */
  Elf64_Word	p_flags;		/* Segment flags */
  Elf64_Off	p_offset;		/* Segment file offset */
  Elf64_Addr	p_vaddr;		/* Segment virtual address */
  Elf64_Addr	p_paddr;		/* Segment physical address */
  Elf64_Xword	p_filesz;		/* Segment size in file */
  Elf64_Xword	p_memsz;		/* Segment size in memory */
  Elf64_Xword	p_align;		/* Segment alignment */
} Elf64_Phdr;

/* Symbol table entry.  */
typedef struct
{
  Elf64_Word	st_name;		/* Symbol name (string tbl index) */
  unsigned char	st_info;		/* Symbol type and binding */
  unsigned char st_other;		/* Symbol visibility */
  Elf64_Section	st_shndx;		/* Section index */
  Elf64_Addr	st_value;		/* Symbol value */
  Elf64_Xword	st_size;		/* Symbol size */
} Elf64_Sym;

/* Dynamic section entry.  */
typedef struct
{
  Elf64_Sxword	d_tag;			/* Dynamic entry type */
  union
    {
      Elf64_Xword d_val;		/* Integer value */
      Elf64_Addr d_ptr;			/* Address value */
    } d_un;
} Elf64_Dyn;

/* Relocation table entry with addend (in section of type SHT_RELA).  */
typedef struct
{
  Elf64_Addr	r_offset;		/* Address */
  Elf64_Xword	r_info;			/* Relocation type and symbol index */
  Elf64_Sxword	r_addend;		/* Addend */
} Elf64_Rela;

typedef struct {
  Elf64_Addr r_offset;   /* Address of relocation */
  Elf64_Xword r_info;    /* Symbol index and type */
} Elf64_Rel;

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym Elf64_Sym
#define Elf_Dyn Elf64_Dyn
#define Elf_Rela Elf64_Rela
#define Elf_Rel Elf64_Rel

# define REL_TYPE_RELATIVE R_X86_64_RELATIVE

#define ELF_R_SYM(x) ELF64_R_SYM(x)
#define ELF_R_TYPE(x) ELF64_R_TYPE(x)

#else
/* Elf header */
typedef struct
{
  Elf32_Word	sh_name;		/* Section name (string tbl index) */
  Elf32_Word	sh_type;		/* Section type */
  Elf32_Word	sh_flags;		/* Section flags */
  Elf32_Addr	sh_addr;		/* Section virtual addr at execution */
  Elf32_Off	sh_offset;		/* Section file offset */
  Elf32_Word	sh_size;		/* Section size in bytes */
  Elf32_Word	sh_link;		/* Link to another section */
  Elf32_Word	sh_info;		/* Additional section information */
  Elf32_Word	sh_addralign;		/* Section alignment */
  Elf32_Word	sh_entsize;		/* Entry size if section holds table */
} Elf32_Shdr;
/* Section header.  */
typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
  Elf32_Half	e_type;			/* Object file type */
  Elf32_Half	e_machine;		/* Architecture */
  Elf32_Word	e_version;		/* Object file version */
  Elf32_Addr	e_entry;		/* Entry point virtual address */
  Elf32_Off	e_phoff;		/* Program header table file offset */
  Elf32_Off	e_shoff;		/* Section header table file offset */
  Elf32_Word	e_flags;		/* Processor-specific flags */
  Elf32_Half	e_ehsize;		/* ELF header size in bytes */
  Elf32_Half	e_phentsize;		/* Program header table entry size */
  Elf32_Half	e_phnum;		/* Program header table entry count */
  Elf32_Half	e_shentsize;		/* Section header table entry size */
  Elf32_Half	e_shnum;		/* Section header table entry count */
  Elf32_Half	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

/* Program segment header.  */
typedef struct
{
  Elf32_Word	p_type;			/* Segment type */
  Elf32_Off	p_offset;		/* Segment file offset */
  Elf32_Addr	p_vaddr;		/* Segment virtual address */
  Elf32_Addr	p_paddr;		/* Segment physical address */
  Elf32_Word	p_filesz;		/* Segment size in file */
  Elf32_Word	p_memsz;		/* Segment size in memory */
  Elf32_Word	p_flags;		/* Segment flags */
  Elf32_Word	p_align;		/* Segment alignment */
} Elf32_Phdr;

/* Symbol table entry.  */
typedef struct
{
  Elf32_Word	st_name;		/* Symbol name (string tbl index) */
  Elf32_Addr	st_value;		/* Symbol value */
  Elf32_Word	st_size;		/* Symbol size */
  unsigned char	st_info;		/* Symbol type and binding */
  unsigned char	st_other;		/* Symbol visibility */
  Elf32_Section	st_shndx;		/* Section index */
} Elf32_Sym;

/* Dynamic section entry.  */
typedef struct
{
  Elf32_Sword	d_tag;			/* Dynamic entry type */
  union
    {
      Elf32_Word d_val;			/* Integer value */
      Elf32_Addr d_ptr;			/* Address value */
    } d_un;
} Elf32_Dyn;

/* Relocation table entry with addend (in section of type SHT_RELA).  */
typedef struct
{
  Elf32_Addr	r_offset;		/* Address */
  Elf32_Word	r_info;			/* Relocation type and symbol index */
  Elf32_Sword	r_addend;		/* Addend */
} Elf32_Rela;

typedef struct {
  Elf32_Addr r_offset;   /* Address of relocation */
  Elf32_Word r_info;     /* Symbol index and type */
} Elf32_Rel;

#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym Elf32_Sym
#define Elf_Dyn Elf32_Dyn
#define Elf_Rela Elf32_Rela
#define Elf_Rel Elf32_Rel

# define REL_TYPE_RELATIVE R_386_RELATIVE

#define ELF_R_SYM(x) ELF32_R_SYM(x)
#define ELF_R_TYPE(x) ELF32_R_TYPE(x)

#endif


typedef struct {
  unsigned long int   *ti_module;
  unsigned long int    ti_offset;
} tls_index_t;

/* Type for the dtv.  */
typedef union dtv
{
  size_t counter;
  void *pointer;
} dtv_t;

typedef struct
{
  void *tcb;            /* Pointer to the TCB.  Not necessary the
                           thread descriptor used by libpthread.  */
  dtv_t *dtv;
  void *self;           /* Pointer to the thread descriptor.  */
  int multiple_threads;
} tcbhead_t;


typedef void* (*tls_get_addr_t)(void *ti);
typedef void (*dtor_func) (void *);
typedef int (*cxa_thread_atexit_impl_t)(dtor_func func, void *obj, void *dso_symbol);
//Thread control structure
typedef struct {
  tid_t tid;
  unsigned int is_detached; //0 if joinable, 1 if detached
  volatile int child_finished;
  void* result; //written by child on exit
  void *(*start_routine)(void*);
  void* arg;
  //thread block limits
  void* tls_start_addr;
  void* stack_start_addr;
} pthread_tcb_t;

typedef struct { 
/* ELF Header and Baseaddr */
	Elf_Ehdr *header;
	void *baseaddr;

/* ELF Section Headers */
	Elf_Shdr *secdynsym;
	Elf_Shdr *secdynamic;
	Elf_Shdr *secrelaplt;
	Elf_Shdr *secreladyn;
	Elf_Shdr *secdynstr;	
	Elf_Shdr *sections;
	Elf_Phdr *segments;

/* ELF Sections */
	Elf_Dyn *dynamic;
	Elf_Sym *dynsym;
	char *dynstr;
	char *sh_strtab;
	void *gnu_hash; // DT_GNU_HASH
    void *hash; // DT_HASH
	Elf_Rela *relaplt;
	Elf_Rela *reladyn;

/*so name*/
	const char* name;

/* Counters of "things" */
	unsigned int dynsymcount;

  /*TLS*/
  unsigned char has_tls_section;
  tls_get_addr_t __tls_get_addr;
  cxa_thread_atexit_impl_t __cxa_thread_atexit_impl;
} elf_file_t;

typedef struct {
  size_t tls_memsz;
  size_t tls_filesz;
  void*  tls_initimage;
  size_t tls_align;
  size_t total_size;
  size_t stack_guard_size;
  size_t module_id;
  tid_t tid;
} thread_block_info_t;


struct _dtor_list_t
{
  dtor_func func;
  void *obj;
  struct _dtor_list_t *next;
};
typedef struct _dtor_list_t dtor_list_t;

typedef struct {
  thread_block_info_t tls_info;
  void* th_block_addr;
  dtor_list_t* list;
} tls_runtime_t;

typedef struct{
  elf_file_t elf;
  tls_runtime_t tls;
} loader_runtime_t;

/**********************************************************/

/************************ DEBUG ***************************/

#ifndef NDEBUG
    #define debug(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
    #define debug(format, ...) nop
    #define nop
#endif

/**********************************************************/

#endif