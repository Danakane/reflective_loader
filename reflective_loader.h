#ifndef _REFLECTIVELOADER_H
#define _REFLECTIVELOADER_H


#include <sys/syscall.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>


#include "definitions.h"


/************************ CONSTANTS ***********************/
#define INFINITE 0xffffffff
#define MAX_PATH_LEN 512
/**********************************************************/

static loader_runtime_t* g_runtime;

/*********************** CRT SYSCALLS *********************/

static inline int crt_close(int fd)
{

	long ret;
	asm volatile ("syscall" : "=a" (ret) : "a" (__NR_close),
		      "D" (fd):
		      "cc", "memory", "rcx",
		      "r8", "r9", "r10", "r11" );
	if (ret < 0)
	{
		ret = -1;
	}
	return (int)ret;
}

static inline int crt_open (const char *pathname, unsigned long flags, unsigned long mode)
{
	long ret = 0;
	__asm__ volatile ("syscall" : "=a" (ret) : "a" (__NR_open),
		      "D" (pathname), "S" (flags), "d" (mode) :
		      "cc", "memory", "rcx",
		      "r8", "r9", "r10", "r11" );
	return (int) ret;
}

static inline void* crt_mmap(void *start, unsigned long length, int prot, int flags, int fd, unsigned long offset)
{
	void *ret;
	register long r10 asm("r10") = flags;
	register long r9 asm("r9") = offset;
	register long r8 asm("r8") = fd;

	__asm__ volatile ("syscall" : "=a" (ret) : "a" (__NR_mmap),
		      "D" (start), "S" (length), "d" (prot), "r" (r8), "r" (r9), "r" (r10) : 
		      "cc", "memory", "rcx", "r11");

	return ret;
}

static inline int crt_munmap(void *start, unsigned long length)
{

	long ret;
	asm volatile ("syscall" : "=a" (ret) : "a" (__NR_munmap),
		      "D" (start), "S" (length) :
		      "cc", "memory", "rcx",
		      "r8", "r9", "r10", "r11" );
	if (ret < 0)
	{
		ret = -1;
	}
	return (int)ret;
}

static inline int crt_read(int fd, char *buffer, unsigned long bufferlen)
{

	long ret;
	__asm__ volatile ("syscall" : "=a" (ret) : "a" (__NR_read),
		      "D" (fd), "S" (buffer), "d" (bufferlen) :
		      "cc", "memory", "rcx",
		      "r8", "r9", "r10", "r11" );
	if (ret < 0)
	{
		ret = -1;
	}
	return (int)ret;
}

static inline int crt_write(int fd, const char *buffer, unsigned long bufferlen)
{
    long ret;
    __asm__ volatile ("syscall" : "=a" (ret) : "a" (__NR_write),
              "D" (fd), "S" (buffer), "d" (bufferlen) :
              "cc", "memory", "rcx",
              "r8", "r9", "r10", "r11" );
    if (ret < 0)
    {
        ret = -1;
    }
    return (int)ret;
}

static inline int crt_stat(const char *path, void *buf)
{
	long ret;
	asm volatile ("syscall" :
		"=a" (ret) :
		"a" (__NR_stat), "D" (path), "S" (buf) :
		"memory"
	);
	if (ret < 0)
	{
		ret = -1;
	}
	return (int)ret;
}

static inline tid_t crt_gettid()
{
    tid_t tid;

    __asm__ volatile (
        "syscall"
        : "=a"(tid)
        : "a"(__NR_gettid)          // SYS_gettid
        : "rcx", "r11", "memory"
    );

    return tid;
}



static inline void* crt_memset(void *dest, char val, unsigned long n)
{
	unsigned long i;
	unsigned char *d = (unsigned char *)dest;

	for (i = 0; i < n; ++i)
		d[i] = val;

	return dest;
}


static inline void* crt_memcpy(void *dest, const void *src, unsigned long n)
{
	unsigned long i;
	unsigned char *d = (unsigned char *)dest;
	unsigned char *s = (unsigned char *)src;

	for (i = 0; i < n; ++i)
		d[i] = s[i];

	return dest;
}

static inline unsigned long crt_strlen(const char *str)
{
	unsigned long len = 0;
	for (; str && *str; ++str, ++len);
	return len;
}

static inline int crt_strcmp(const char *s1, const char *s2, int strict) 
{
	int len1 = crt_strlen(s1);
	int len2 = crt_strlen(s2);
	int len = 0;

    if(strict && len1 != len2) return -1;
	
	if(len1 > len2)
		len = len2;
	else
		len = len1;

	for(int i = 0; i < len; i++)
	{
		if(*(s1 + i) != *(s2 + i))
		{

			return -1;
		}	
	}

	return 0;
}


/**********************************************************/

/************************** UTILS *************************/

/*
Convert a hex string to the corresponding pointer value
*/
static inline uintptr_t hex2ptr(const char* hex)
{
    uintptr_t ptr = 0;
    unsigned int len = crt_strlen(hex);

    for (unsigned int i = 0; i < len; ++i) {
        uintptr_t value = (*hex >= 'a') ? (*hex - 'a' + 10) : (*hex - '0');
        ptr = (ptr * 16) + value;
        hex++;
    }

    return ptr;
}

/*
This function read the content of a file whose size can't be determined
It reallocate memory if the destination
return the memory buffer length
*/
static inline size_t read_dynamic_file(const char* file, void **bufaddr, size_t* file_len)
{
    size_t len = 0;

    int buffer_too_small = 1;
    int factor = 0;
    int chunk_len = 4096;
    void* buffer = NULL;

    while(buffer_too_small)
    {
        len = 0;

        int fd = crt_open(file, 0, 0);
        if(fd <= 0)
        {
            crt_close(fd);
            debug("crt_open %s failed\n", file);
            break;
        }
        // allocate the memory block
        factor++;
        int buffer_len = chunk_len * factor;
        buffer = crt_mmap(
            NULL, 
            buffer_len, 
            PROT_READ|PROT_WRITE, 
            MAP_PRIVATE|MAP_ANONYMOUS, 
            -1, 
            0
        );
        if(!buffer)
        {
            //debug("crt_mmap failed\n");
            break;
        }

        off_t offset = 0;
        char buf[1024];
		crt_memset(buf, 0, sizeof(buf));

        while (buffer_len >= offset)
        {
            int nb_read = crt_read(fd, buf, sizeof(buf));
            if(!nb_read) 
            {
                // The whole file has been read
                buffer_too_small = 0;
                *file_len = offset;
                *bufaddr = buffer;
                len = buffer_len;
                break;
            }
            else 
            {
                if(buffer_len >= nb_read + offset)
                {
                    uintptr_t dest = (uintptr_t)buffer + offset;
                    crt_memcpy((void*)dest, buf, nb_read);
                    offset += nb_read;
                }
                else 
                {
                    // the content exceed the buffer len we need allocate a bigger block
                    //debug("read_dynamic_file: the buffer is too small\n");
                    break;
                }
            }
        }

        if(buffer_too_small)
        {
            if(buffer)
            {
                crt_munmap(buffer, buffer_len);
                buffer = NULL;
            }
        }

        crt_close(fd);
    }

    return len;
}



// Function to detect the system's endianness
static inline int is_little_endian() {
    uint16_t num = 1;
    return (*(uint8_t *)&num == 1);  // If true, the system is little-endian
}

// Byte swap for 16-bit integers
static inline uint16_t swap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

// Byte swap for 32-bit integers
static inline uint32_t swap32(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

#ifdef __LP64__
// Byte swap for 64-bit integers
static inline uint64_t swap64(uint64_t val) {
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
}
#endif

// le16toh: Convert 16-bit little-endian to host byte order
static inline uint16_t _le16toh(uint16_t val) {
    if (is_little_endian()) {
        return val;  // Already in little-endian, no conversion needed
    }
    return swap16(val);  // Swap bytes if the system is big-endian
}

// be16toh: Convert 16-bit big-endian to host byte order
static inline uint16_t _be16toh(uint16_t val) {
    if (is_little_endian()) {
        return swap16(val);  // Swap bytes if the system is little-endian
    }
    return val;  // Already in big-endian, no conversion needed
}

// le32toh: Convert 32-bit little-endian to host byte order
static inline uint32_t _le32toh(uint32_t val) {
    if (is_little_endian()) {
        return val;  // Already in little-endian, no conversion needed
    }
    return swap32(val);  // Swap bytes if the system is big-endian
}

// be32toh: Convert 32-bit big-endian to host byte order
static inline uint32_t _be32toh(uint32_t val) {
    if (is_little_endian()) {
        return swap32(val);  // Swap bytes if the system is little-endian
    }
    return val;  // Already in big-endian, no conversion needed
}

#ifdef __LP64__
// le64toh: Convert 64-bit little-endian to host byte order
static inline uint64_t _le64toh(uint64_t val) {
    if (is_little_endian()) {
        return val;  // Already in little-endian, no conversion needed
    }
    return swap64(val);  // Swap bytes if the system is big-endian
}

// be64toh: Convert 64-bit big-endian to host byte order
static inline uint64_t _be64toh(uint64_t val) {
    if (is_little_endian()) {
        return swap64(val);  // Swap bytes if the system is little-endian
    }
    return val;  // Already in big-endian, no conversion needed
}
#endif

/**********************************************************/


/************************* ELF ***************************/




#ifdef __LP64__
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym Elf64_Sym
#define Elf_Dyn Elf64_Dyn
#define Elf_Rela Elf64_Rela
#define Elf_Half Elf64_Half
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym Elf32_Sym
#define Elf_Dyn Elf32_Dyn
#define Elf_Rela Elf32_Rela
#define Elf_Half Elf32_Half
#endif


// Define ELFDATANATIVE macro
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

// Structure of the GNU hash section
typedef struct {
    uint32_t nbuckets;
    uint32_t symndx;
    uint32_t maskwords;
    uint32_t shift2;
} gnu_hash_header_t;

// Structure of the legacy System V .hash section
typedef struct {
    uint32_t nbuckets;
    uint32_t nchains;
    // Followed by nbuckets bucket array and nchains chain array
    // but irrelevant for our use case
} hash_header_t;


typedef struct {
    int dlfunc_type; /* -1, DLFUNC_POSIX or DLFUNC_INTERNAL */
    FILE *fp;
    size_t libc_addr;
    size_t str_offset;
    size_t str_size;
    size_t sym_offset;
    size_t sym_num;
    size_t sym_entsize;
} param_t;

static inline error_code check_elf_magic(Elf_Ehdr *ehdr)
{
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr->e_type != ET_DYN
	) 
	{
        return ERROR_CODE_NO_LIBRARY;
    }

	return ERROR_CODE_OK;
}


// Define file16_to_cpu function
static inline uint16_t file16_to_cpu(void* so, uint16_t val)
{
    Elf_Ehdr* ehdr = (Elf_Ehdr*)so;
	if (ehdr->e_ident[EI_DATA] != ELFDATANATIVE)
		val = (ehdr->e_ident[EI_DATA] == ELFDATA2LSB) ? _le16toh(val) : _be16toh(val);
	return val;
}

// Define file32_to_cpu function
static inline uint32_t file32_to_cpu(void* so, uint32_t val)
{
	Elf_Ehdr* ehdr = (Elf_Ehdr*)so;
	if (ehdr->e_ident[EI_DATA] != ELFDATANATIVE)
		val = (ehdr->e_ident[EI_DATA] == ELFDATA2LSB) ? _le32toh(val) : _be32toh(val);
	return val;
}

// Define file64_to_cpu function
static inline uint64_t file64_to_cpu(void* so, uint64_t val)
{
	Elf_Ehdr* ehdr = (Elf_Ehdr*)so;
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		val = (ehdr->e_ident[EI_DATA] == ELFDATA2LSB) ? _le64toh(val) : _be64toh(val);
	return val;
}


static inline size_t elf_file_size(void *so)
{
    Elf_Ehdr* elf_header = (Elf_Ehdr*)so;
	// Convert values from file endianness to host endianness
	size_t shoff = file64_to_cpu(so, elf_header->e_shoff);
	size_t shentsize = file16_to_cpu(so, elf_header->e_shentsize);
	size_t shnum = file16_to_cpu(so, elf_header->e_shnum);

	// Calculate size of ELF file
	size_t size = shoff + (shentsize * shnum);

    return size;
}

static inline size_t elf_memory_size(void *so)
{
	Elf_Ehdr* header = (Elf_Ehdr*)so;
    unsigned int size = 0;
	unsigned int num_pages = 0;
	Elf64_Phdr *segments = (Elf64_Phdr *)(header->e_phoff + (char *)header);

	for(int i = 0; i < header->e_phnum; i++)
	{
		if(segments[i].p_type == PT_LOAD)
		{
			if(segments[i].p_memsz > segments[i].p_align)
			{
				num_pages = 1 + (segments[i].p_memsz - segments[i].p_memsz % segments[i].p_align) / segments[i].p_align;
			}			
			else
			{
				num_pages = 1;
			}				
		
			size += segments[i].p_align * num_pages;
		}
	}
	size += 0x2000; //padding

	return size;
}


/**********************************************************/

/********************* RESOLUTION *************************/

typedef struct {
	unsigned char *maps;
	unsigned char *pos;
    size_t maps_length;
	size_t memory_size;
} maps_file_t;

typedef struct {
	void *startaddr;
	void *endaddr;
	char *perms;
	char *name;
	char* path;
} map_entry_t;


typedef enum {
    LIBC_TYPE_UNKNOWN = 0,
    LIBC_TYPE_GNU,
    LIBC_TYPE_MUSL,
} libc_type_t;




static inline Elf_Ehdr* resolve_self() 
{

	char *ptr = NULL;

	__asm__("leaq (%%rip), %0;": "=r"(ptr));

	//Locate the ELF Header for this file
	while(1)
	{
		if(check_elf_magic((Elf_Ehdr *)ptr) == 0)
		{
			break;
		}	
		ptr--;
	}

	return (Elf_Ehdr*)ptr;
}

static inline map_entry_t get_next_maps_entry(maps_file_t *maps) {
    map_entry_t entry = {0};  // Initialize entry with zeros
    unsigned char *pos = maps->pos;
    int valid = 0;

    // Check if we have reached the end of the maps file
    if (pos >= (maps->maps + maps->maps_length)) {
        return entry;
    }

    // Get the start address
    char* hex_start_addr = (char*)pos;
    while (*pos != '-') {
        pos++;
    }
    *pos++ = '\0';  // Null-terminate the start address and move to the next char
    entry.startaddr = (void*)hex2ptr(hex_start_addr);

    // Get the end address
    char* hex_end_address = (char*)pos;
    while (*pos != ' ') {
        pos++;
    }
    *pos++ = '\0';  // Null-terminate the end address and move to the next char
    entry.endaddr = (void*)hex2ptr(hex_end_address);

    // Get permissions
    entry.perms = (char*)pos;
    while (*pos != ' ') {
        pos++;
    }
    *pos++ = '\0';  // Null-terminate the permissions string

    // Check for the shared library name (if any)
    while (*pos != '\n') {
        if (*pos == '/') {
            valid = 1;  // A valid entry contains a path to a shared object
            if(!entry.path)
                entry.path = (char*)pos;
        }
        pos++;
    }
    *pos = '\0';  // Null-terminate the line
    char* lib_name = (char*)pos;

    // If a valid shared library path is found, extract the name
    if (valid) {
        while (*lib_name != '/' && (unsigned char*)lib_name > maps->pos) {
            lib_name--;  // Move back to find the start of the library path
        }
        entry.name = lib_name + 1;  // Set the name to the path
    } else {
        entry.name = NULL;  // No valid name found
    }

    // Move to the next entry
    pos++;
    maps->pos = pos;  // Update the position in the maps file

    return entry;
}


static inline void* resolve_so(const char* so_name) 
{
	
	maps_file_t maps;
	map_entry_t entry = {0};
 
	/* Done this way to ensure relocations are not required 
	 * compiler generates a sequence of move instructions writing
	 * the string onto the stack. */

	char maps_path[16];	
	maps_path[0]  =  '/';   
	maps_path[1]  =  'p';   
	maps_path[2]  =  'r';
	maps_path[3]  =  'o';
	maps_path[4]  =  'c';
	maps_path[5]  =  '/';
	maps_path[6]  =  's';
	maps_path[7]  =  'e';
	maps_path[8]  =  'l';
	maps_path[9]  =  'f';
	maps_path[10] =  '/';
	maps_path[11] =  'm';
	maps_path[12] =  'a';
 	maps_path[13] =  'p';
	maps_path[14] =  's';
	maps_path[15] =  '\0'; 	

	char perms[5]; 
	perms[0] = 'r';
	perms[1] = '-';
	perms[2] = '-';
	perms[3] = 'p';
	perms[4] = '\0';
    
	maps.memory_size = read_dynamic_file(maps_path, (void**)&maps.maps, &maps.maps_length);

    if(!maps.memory_size)
    {
        return NULL;
    }
	maps.pos = maps.maps;

    void* lib_addr = NULL;
	do
	{
		entry = get_next_maps_entry(&maps);
	
		if(entry.name == NULL) /* Entry does not have a name */		
			continue;

		if(crt_strcmp(entry.name, so_name, 0) == 0)
		{
			if(crt_strcmp(entry.perms, perms, 1) == 0)
            {
                if(check_elf_magic((Elf_Ehdr*)entry.startaddr) == 0)
                {
				    lib_addr = entry.startaddr;
                }
            }
		}

	} while(entry.startaddr != NULL && !lib_addr);
	
	crt_munmap(maps.maps, maps.memory_size); //unmap maps file from memory

    return lib_addr;
}

static inline void* resolve_libc()
{
    char glibc_name[9];
	glibc_name[0] = 'l';
	glibc_name[1] = 'i';
	glibc_name[2] = 'b';
	glibc_name[3] = 'c';
	glibc_name[4] = '.';
    glibc_name[5] = 's';
	glibc_name[6] = 'o';
	glibc_name[7] = '.';
	glibc_name[8] = '\0';

    void* libc = resolve_so(glibc_name);
    if(!libc)
    {
        char msul_name[9];
        msul_name[0] = 'l';
        msul_name[1] = 'd';
        msul_name[2] = '-';
        msul_name[3] = 'm';
        msul_name[4] = 'u';
        msul_name[5] = 's';
        msul_name[6] = 'l';
        msul_name[7] = '-';
        msul_name[8] = '\0';
        libc = resolve_so(msul_name);
    }

    return libc;
}

static inline size_t count_dynsym(void* so)
{
	if(!so) return 0;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)so;
    Elf64_Shdr *shdrs = (Elf64_Shdr *)((char *)so + ehdr->e_shoff);

    // Find the .dynsym section
    size_t dynsym_count = 0;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_DYNSYM) {
            // Calculate the number of entries in .dynsym
            dynsym_count = shdrs[i].sh_size / sizeof(Elf64_Sym);
            break;
        }
    }

    return dynsym_count;
}

static inline size_t gnu_hash_count_dynsym(const uint32_t *gnu_hash, const Elf64_Sym *symtab)
{
    const uint32_t nbuckets    = gnu_hash[0];
    const uint32_t symoffset   = gnu_hash[1];
    const uint32_t bloom_size  = gnu_hash[2];
    const uint32_t bloom_shift = gnu_hash[3];

    const uint64_t *bloom = (const uint64_t *)(gnu_hash + 4);
    const uint32_t *buckets = (const uint32_t *)(bloom + bloom_size);
    const uint32_t *chains  = buckets + nbuckets;

    size_t max_sym = symoffset;

    // Step 1: find highest symbol index referenced by buckets
    for (uint32_t i = 0; i < nbuckets; i++) {
        uint32_t idx = buckets[i];
        if (idx > max_sym)
            max_sym = idx;
    }

    // Step 2: walk chains until termination bit (LSB = 1)
    for (uint32_t i = 0; i < nbuckets; i++) {
        uint32_t idx = buckets[i];
        if (idx == 0)
            continue;

        const uint32_t *chain = &chains[idx - symoffset];

        while (1) {
            uint32_t entry = *chain++;
            uint32_t sym_index = idx++;

            if (sym_index > max_sym)
                max_sym = sym_index;

            // termination bit => last symbol in chain
            if (entry & 1)
                break;
        }
    }

    // +1 because indices are 0-based
    return max_sym + 1;
}

static inline void *resolve_function(void* module, const char *symbol_name)
{
    Elf_Ehdr *ehdr = (Elf_Ehdr*)module;
	Elf_Phdr* phdrs = (Elf_Phdr*)((char*)module + ehdr->e_phoff);
	Elf_Half phnum = ehdr->e_phnum;

    Elf_Dyn *dynamic = NULL;

    for (Elf_Half i = 0; i < phnum; ++i) 
	{
        if (phdrs[i].p_type == PT_DYNAMIC) 
		{
            dynamic = (Elf_Dyn*)((char*)module + phdrs[i].p_vaddr);
            break;
        }
    }

    if (!dynamic) return NULL;

    Elf_Sym  *symtab = NULL;
    const char *strtab = NULL;
    uint32_t   *hash   = NULL;
	uint32_t   *gnu_hash   = NULL;

    for (Elf_Dyn *d = dynamic; d->d_tag != DT_NULL; ++d)
	{
        switch (d->d_tag) {
        case DT_SYMTAB:
            symtab = (Elf_Sym*)d->d_un.d_ptr;
            break;

        case DT_STRTAB:
            strtab = (const char *)d->d_un.d_ptr;
            break;
        case DT_HASH:
            hash = (uint32_t *)d->d_un.d_ptr;
            break;
        case DT_GNU_HASH:
            gnu_hash = (uint32_t *)d->d_un.d_ptr;
            break;
        }
    }

    if (!symtab || !strtab || !(hash || gnu_hash)) return NULL;


	if(gnu_hash != NULL)
	{
		size_t count = gnu_hash_count_dynsym(gnu_hash, symtab);
		for(int i = 0; i < count; ++i) 
		{
			size_t type = symtab[i].st_info & 0xf;
			if(type == STT_FUNC) 
			{
				const char *name = strtab + symtab[i].st_name;
				if (crt_strcmp(name, symbol_name, 1) == 0) 
				{
					return (void *)((char*)module + symtab[i].st_value);
				}
			}
		}
	}
	else 
	{
		for(size_t idx = 0; idx < hash[1]; ++idx)
		{
			size_t type = symtab[idx].st_info & 0xf;
			if(type == STT_FUNC) 
			{
				const char *name = strtab + symtab[idx].st_name;
				if (crt_strcmp(name, symbol_name, 1) == 0) 
				{
					return (void *)((char*)module + symtab[idx].st_value);
				}
			}
		}
	}

    return NULL;
}

static inline size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}


/**********************************************************/

/********************** REFLECTION ************************/
typedef void*(*libc_calloc_t)(size_t, size_t);
typedef void*(*libc_free_t)(void*);
typedef void*(*libc_dlsym_t)(void*, char*);
typedef void*(*libc_dlopen_t)(char*, int);
typedef void*(*libc_dlclose_t)(void*);

static inline int find_section(
    const char* name, 
    Elf64_Shdr *sections, 
    char *sh_strtab, 
    unsigned int num_sections
)
{
    error_code error = ERROR_CODE_NO_SECTION;
	for(int i = 0; i < num_sections; i++)
	{
        const char* section_name = sh_strtab + sections[i].sh_name;
		if(crt_strcmp(section_name, name, 1) == 0)
		{
			return i;
		}
	}

    debug("[!] ERROR could not find section\n");
	return -1;
}

#define R_X86_64_DTPMOD64 16  /* ID of module containing symbol */
#define R_X86_64_DTPOFF64 17  /* Offset in TLS block of module */
#define R_X86_64_TPOFF64  18  /* Offset in initial TLS block */

#define PT_TLS 7 /* Thread local storage segment */ 

#define NPTL_TCB_SIZE 1184 // sizeof (struct pthread)
#define NPTL_TCB_ALIGN sizeof(double)
#define NPTL_TCBHEAD_T_SIZE (sizeof(tcbhead_t))
//64KB stack, change to your taste...
#define CHILD_STACK_BITS 16
#define CHILD_STACK_SIZE (1 << CHILD_STACK_BITS)
static error_code rebase(void* so, loader_runtime_t* runtime)
{
	// This function crash if the so was linked with Zig
	// that why need a C wrapper for the reflective loader and let gcc do the final linking for stage1 and the stage2
    runtime->elf.header = (Elf_Ehdr*)so;

    size_t len = elf_memory_size(so);
	if(!len)
	{
		return ERROR_CODE_INVALID_PARAMS;
	}
    runtime->elf.baseaddr = crt_mmap(
        NULL, 
        len, 
        PROT_READ|PROT_WRITE, 
        MAP_PRIVATE|MAP_ANONYMOUS, 
        -1, 
        0
    ); // mmap respect the page boundary (4096 bytes aligned) so no need to round the address

	if(!runtime->elf.baseaddr)
	{
		return ERROR_CODE_NO_MEMORY;
	}

	
    runtime->elf.segments = (Elf_Phdr*)(runtime->elf.header->e_phoff + (char*)runtime->elf.header); // Program Segments 
	runtime->elf.sections = (Elf_Shdr*)(runtime->elf.header->e_shoff + (char*)runtime->elf.header); // Program Sections 


	runtime->elf.has_tls_section = 0;
    for(int i = 0; i < runtime->elf.header->e_phnum; i++)
	{
		//Copy loadable segments into memory
		if(runtime->elf.segments[i].p_type == PT_LOAD)
		{
			char msg1[] = {'O', 'K', '1', '\n', '\0'};
			uintptr_t seg_vaddr = runtime->elf.segments[i].p_vaddr;
            uintptr_t seg_offset = runtime->elf.segments[i].p_offset;
            size_t seg_filesz = runtime->elf.segments[i].p_filesz;
            size_t seg_memsz = runtime->elf.segments[i].p_memsz;  // Total memory size including .bss
            if ((seg_vaddr + (char*)runtime->elf.baseaddr) < (char*)runtime->elf.baseaddr || seg_memsz > len) {
                debug("[!] Segment bounds are invalid");
                crt_munmap(runtime->elf.baseaddr, len);
				crt_memset(runtime, 0, sizeof(*runtime));
				return ERROR_CODE_INVALID_PARAMS;
            }
			crt_memcpy(
                (void*)((char*)runtime->elf.baseaddr + runtime->elf.segments[i].p_vaddr), 
                (void *)((char*)runtime->elf.header + runtime->elf.segments[i].p_offset), 
                runtime->elf.segments[i].p_filesz
            );
			// Copy initialized data (.data, .text, etc.)
            crt_memcpy(
                (void *)((char*)runtime->elf.baseaddr + seg_vaddr), 
                (void *)((char*)runtime->elf.header + seg_offset), 
                seg_filesz
            );
            // Zero out the remaining memory for .bss (if any)
            if (seg_memsz > seg_filesz) {
                size_t bss_size = seg_memsz - seg_filesz;
                void* bss_start = (void *)((char*)runtime->elf.baseaddr + seg_vaddr + seg_filesz);
                crt_memset(bss_start, 0, bss_size);
                debug("[+] .bss section zeroed out: %p, size: %zu\n", bss_start, bss_size);
            }

            debug("[+] PT_LOAD Segment loaded at %p\n", (void *)(seg_vaddr + (char*)runtime->elf.baseaddr));
		}
		else if (runtime->elf.segments[i].p_type == PT_TLS) 
		{
			runtime->elf.has_tls_section = 1;
			runtime->tls.tls_info.tls_initimage = (char*)runtime->elf.baseaddr + runtime->elf.segments[i].p_offset;
			runtime->tls.tls_info.tls_memsz  = runtime->elf.segments[i].p_memsz;
			runtime->tls.tls_info.tls_filesz = runtime->elf.segments[i].p_filesz;
			runtime->tls.tls_info.tls_align  = runtime->elf.segments[i].p_align;
			
        }
	}

	if(runtime->elf.has_tls_section != 0)
	{
		// // https://github.com/gem5/m5threads/blob/master/pthread.c#L109
		// runtime->tls.tls_info.stack_guard_size = 2048; 
		// //Total thread block size -- this is what we'll request to mmap
		// size_t sz = 
		// 	sizeof(pthread_tcb_t) + 
		// 	runtime->tls.tls_info.tls_memsz + 
		// 	NPTL_TCBHEAD_T_SIZE + 
		// 	runtime->tls.tls_info.stack_guard_size + CHILD_STACK_SIZE;

		// //Align to multiple of CHILD_STACK_SIZE
  		// sz += CHILD_STACK_SIZE - 1;  
  		// runtime->tls.tls_info.total_size = (sz>>CHILD_STACK_BITS)<<CHILD_STACK_BITS;
		runtime->tls.tls_info.total_size = align_up(
			runtime->tls.tls_info.tls_memsz, runtime->tls.tls_info.tls_align);
		runtime->tls.tls_info.module_id = (size_t)runtime->elf.baseaddr;
		runtime->tls.tls_info.tid = crt_gettid(); // tls_index will be the loader thread id
	}


    //Find SH_STRTAB
	runtime->elf.sh_strtab = (char *)runtime->elf.header + runtime->elf.sections[runtime->elf.header->e_shstrndx].sh_offset;

    //find this files .dynamic section
    const char dynamic_section_name[] = ".dynamic";
	int idx_dynamic_section = find_section(
        dynamic_section_name, 
        runtime->elf.sections, 
        runtime->elf.sh_strtab, 
        runtime->elf.header->e_shnum
    );
	if(idx_dynamic_section != -1)
	{
		runtime->elf.secdynamic = (Elf64_Shdr *)&runtime->elf.sections[idx_dynamic_section];
		runtime->elf.dynamic = (Elf_Dyn*)(runtime->elf.secdynamic->sh_addr + (char*)runtime->elf.baseaddr);
	}

    //find this files .dynstr
    const char dynstr_section_name[] = ".dynstr";
	int idx_dynstr_section = find_section(
        dynstr_section_name, 
        runtime->elf.sections, 
        runtime->elf.sh_strtab, 
        runtime->elf.header->e_shnum
    );
	if(idx_dynstr_section)
	{
		runtime->elf.secdynstr = (Elf64_Shdr *)&runtime->elf.sections[idx_dynstr_section];
		runtime->elf.dynstr = (char*)(runtime->elf.secdynstr->sh_addr + (char*)runtime->elf.baseaddr);
	}

    //find this files .rela.plt section
	const char rela_plt_section_name[] = ".rela.plt";
	int idx_rela_plt_section = find_section(
        rela_plt_section_name, 
        runtime->elf.sections, 
        runtime->elf.sh_strtab, 
        runtime->elf.header->e_shnum
    );
	if(idx_rela_plt_section != -1)
	{
		runtime->elf.secrelaplt = (Elf64_Shdr *)&runtime->elf.sections[idx_rela_plt_section];
		runtime->elf.relaplt = (Elf_Rela*)(runtime->elf.secrelaplt->sh_addr + (char*)runtime->elf.baseaddr);
	}

	//find this files .rela.dyn section
	const char rela_dyn_section_name[] = ".rela.dyn";
	int idx_rela_dyn_section = find_section(
        rela_dyn_section_name, 
        runtime->elf.sections, 
        runtime->elf.sh_strtab, 
        runtime->elf.header->e_shnum
    );
	if(idx_rela_dyn_section != -1)
	{
		runtime->elf.secreladyn = (Elf64_Shdr *)&runtime->elf.sections[idx_rela_dyn_section];
		runtime->elf.reladyn = (Elf_Rela*)(runtime->elf.secreladyn->sh_addr + (char*)runtime->elf.baseaddr);
		debug("reladyn section index: %d\n", idx_rela_dyn_section);
		debug("reladyn section sh_addr: %ld\n", runtime->elf.secreladyn->sh_addr);
	}
	//find this files dynsym section
	const char dynsym_section_name[] = ".dynsym";
	int idx_dynsym_section = find_section(
        dynsym_section_name, 
        runtime->elf.sections, 
        runtime->elf.sh_strtab, 
        runtime->elf.header->e_shnum
    );
	if(idx_dynamic_section != -1)
	{
		runtime->elf.secdynsym = (Elf64_Shdr *)&runtime->elf.sections[idx_dynsym_section];
		runtime->elf.dynsym = (Elf_Sym*)(runtime->elf.secdynsym->sh_addr + (char*)runtime->elf.baseaddr);
	}

	
    return ERROR_CODE_OK;
	
}

static error_code build_thread_tls_block(loader_runtime_t* runtime)
{
	runtime->tls.th_block_addr = (char*)crt_mmap(
		NULL, 
		runtime->tls.tls_info.total_size, 
		PROT_READ|PROT_WRITE, 
		MAP_PRIVATE|MAP_ANONYMOUS, -1, 0
	);

	return runtime->tls.th_block_addr ? ERROR_CODE_OK : ERROR_CODE_NO_MEMORY;
}

static void release_thread_tls_block(loader_runtime_t* runtime)
{
	crt_munmap(
		runtime->tls.th_block_addr, 
		runtime->tls.tls_info.total_size
	);
	runtime->tls.th_block_addr = NULL;
}

# define roundup(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))
static error_code setup_thread_tls(loader_runtime_t* runtime)
{
	//DEBUG("Init TLS: Copying %d bytes from 0x%llx to 0x%llx\n", filesz, (uint64_t) initimage, (uint64_t) tls_start_ptr);
	crt_memcpy (
		(char*)runtime->tls.th_block_addr, 
		runtime->tls.tls_info.tls_initimage, 
		runtime->tls.tls_info.tls_filesz
	);

	//Rest of tls vars are already cleared (mmap returns zeroed memory)
		

	return ERROR_CODE_OK;
}
// static error_code setup_thread_tls(loader_runtime_t* runtime)
// {
// 	size_t tcb_offset = 0;
// 	void *tls_block = NULL;
// 	char *tls_start_ptr = NULL;

// 	/* Compute the (real) TCB offset */
// 	tcb_offset = roundup(runtime->tls.tls_info.tls_memsz, NPTL_TCB_ALIGN);


// 	/* Align the TLS block.  */
// 	tls_block = (void *) (((uintptr_t) runtime->tls.th_block_addr + runtime->tls.tls_info.tls_align - 1)
// 						& ~(runtime->tls.tls_info.tls_align - 1));
// 	/* Initialize the TLS block.  */
// 	tls_start_ptr = ((char *) tls_block + tcb_offset
// 						- roundup (runtime->tls.tls_info.tls_memsz, runtime->tls.tls_info.tls_align ?: 1));

// 	//DEBUG("Init TLS: Copying %d bytes from 0x%llx to 0x%llx\n", filesz, (uint64_t) initimage, (uint64_t) tls_start_ptr);
// 	crt_memcpy (tls_start_ptr, runtime->tls.tls_info.tls_initimage, runtime->tls.tls_info.tls_filesz);

// 	//Rest of tls vars are already cleared (mmap returns zeroed memory)
		

// 	return ERROR_CODE_OK;
// }


static void* __reflective_tls_get_addr(tls_index_t *ti)
{
	// this is a hack for TLS handling
	// most notably, due to the restriction of the reflective loader 
	// that is expected to be executed as a shellcode
	// it cannot support multi threaded payloads that use TLS
	// however it can be fixed by allocating a new TLS memory block for every new thread that call this function
	// and keep track of them using their thread id

	//!\\ Warning: due to the TLS handler being implemented in the loader
	// we can't encrypt/obfuscate this function when the payload is running
	// (we may encrypt the loader function, but must leave this code in plaintext)
	// sleep obfuscation on this function is possible
	// but this function must be encrypted after the payload and decrypted before the payload
	debug("__reflective_tls_get_addr function called\n");
	if(crt_gettid() == g_runtime->tls.tls_info.tid)
	{
		if((size_t)ti->ti_module == g_runtime->tls.tls_info.module_id) 
		{
			return (char*)g_runtime->tls.th_block_addr + ti->ti_offset;
		}
		else 
		{
			return g_runtime->elf.__tls_get_addr(ti);
		}
	}
	else 
	{
		// multi-thread module detected -> panic here because we don't support multi-threaded code
		__asm__ volatile ("ud2");
    	__builtin_unreachable(); 
	}
	
}


static int __reflective_cxa_thread_atexit_impl(dtor_func func, void *obj, void *dso_symbol)
{
	void* libc = resolve_libc();
	if(libc)
	{
		libc_calloc_t libc_calloc = (libc_calloc_t)resolve_function(libc, "calloc");
		if(libc_calloc)
		{
			dtor_list_t *node = (dtor_list_t*)calloc (1, sizeof (dtor_list_t));
			if(node)
			{
				node->func = func;
				node->obj = obj;
				node->next = g_runtime->tls.list;
				g_runtime->tls.list = node;
			}
			else 
			{
				debug("__reflective_cxa_thread_atexit_impl: allocation failed\n");
				debug("__reflective_cxa_thread_atexit_impl: Loader won't keep track of local thread objects\n");
			}
			
		}
		else 
		{
			debug("__reflective_cxa_thread_atexit_impl: failed to resolve 'calloc' function\n");
			debug("__reflective_cxa_thread_atexit_impl: Loader won't keep track of local thread objects\n");
		}
	}
	else 
	{
		debug("__reflective_cxa_thread_atexit_impl: failed to resolve the libc\n");
		debug("__reflective_cxa_thread_atexit_impl: Loader won't keep track of local thread objects\n");
	}
	
	return 0;
}
static inline void __call_tls_dtors ()
{
	void* libc = resolve_libc();
	if(!libc)
	{
		debug("__call_tls_dtors: failed to resolve the libc\n");
		debug("__call_tls_dtors: Expect memory leaks\n");
	}

    libc_free_t libc_free = NULL;
	if(libc)
	{

		libc_free = (libc_free_t)resolve_function(libc, "free");
		if(!libc_free)
		{
			debug("__call_tls_dtors: failed to resolve 'free' function\n");
			debug("__call_tls_dtors: Expect memory leaks\n");
		}
	}
	
	while (g_runtime->tls.list)
	{
		dtor_list_t *cur = g_runtime->tls.list;
		g_runtime->tls.list = g_runtime->tls.list->next;

		cur->func (cur->obj);
		if(libc_free)
		{
			libc_free (cur);
		}
	}
}


static inline error_code relocate(loader_runtime_t* runtime)
{
    /* 
	* Functions we need from libc for ELF loading, we resolve these on 
	* the fly by locating LIBC and finding these functions ourselves 
	*/
    void* libc = resolve_libc();
	if(!libc)
	{
		return ERROR_CODE_NO_LIBRARY;
	}

	libc_calloc_t libc_calloc = (libc_calloc_t)resolve_function(libc, "calloc");
    libc_free_t libc_free = (libc_free_t)resolve_function(libc, "free");
	libc_dlsym_t libc_dlsym = (libc_dlsym_t)resolve_function(libc, "dlsym");
	libc_dlopen_t libc_dlopen = (libc_dlopen_t)resolve_function(libc, "dlopen");
	libc_dlclose_t libc_dlclose = (libc_dlclose_t)resolve_function(libc, "dlclose");

    if(!(libc_calloc && libc_free && libc_dlsym && libc_dlopen && libc_dlclose))
    {
        return ERROR_CODE_NO_FUNCTION;
    }

	const char __tls_get_addr_name[] = {'_', '_', 
		't', 'l', 's', 
		'_', 'g', 'e', 't', 
		'_', 'a', 'd', 'd', 'r', '\0'
	};
	const char __cxa_thread_atexit_impl_name[] = { '_', '_', 
		'c', 'x', 'a', '_', 
		't', 'h', 'r', 'e', 'a', 'd', '_', 
		'a', 't', 'e', 'x', 'i', 't', '_', 
		'i', 'm', 'p', 'l', '\0'
	};


    unsigned int nb_needed = 0;
    //Count number of DT_NEEDED entries
	for(int i = 0; runtime->elf.dynamic[i].d_tag != DT_NULL; i++)
	{
		if(runtime->elf.dynamic[i].d_tag == DT_NEEDED)
		{
			nb_needed++;
		}
	}

    void** handles = (void**)libc_calloc(nb_needed, sizeof(void*));

    if(handles == NULL)
	{
		debug("[-] Memory allocation failed..");
		return ERROR_CODE_NO_MEMORY;
	}

    int idx = 0;
    //Open all libraries required by the shared object in order to execute
	for(int i = 0; runtime->elf.dynamic[i].d_tag != DT_NULL && idx < nb_needed; ++i)
	{
		if(runtime->elf.dynamic[i].d_tag == DT_NEEDED)
		{
			debug("[i] Opening DT_NEEEDED library [%s]\n", runtime->elf.dynamic[i].d_un.d_ptr + runtime->elf.dynstr);
			handles[idx] = (*libc_dlopen)(runtime->elf.dynamic[i].d_un.d_ptr + runtime->elf.dynstr, RTLD_LAZY);
			if(!handles[idx])
			{
                libc_free(handles);
                handles = NULL;
				return ERROR_CODE_UNKNOWN;
			}
			idx++;
		}
	}

	if(runtime->elf.relaplt)
	{
		int idx_symtab = 0;
		//Resolve PLT references
		size_t n = runtime->elf.secrelaplt->sh_size / sizeof(Elf64_Rela);
		for(int i = 0; i < n; i++)
		{
			if(ELF64_R_TYPE(runtime->elf.relaplt[i].r_info) == R_X86_64_JUMP_SLOT)
			{
				void *func_addr;
				char *func_name;

				//Get Index into symbol table for relocation
				idx_symtab = ELF64_R_SYM(runtime->elf.relaplt[i].r_info);

				func_name = runtime->elf.dynsym[idx_symtab].st_name + runtime->elf.dynstr;
				

				//If symbol is a local symbol write the address of it into the .got.plt
				if(
					ELF64_ST_TYPE(runtime->elf.dynsym[idx_symtab].st_info) == STT_FUNC && 
					runtime->elf.dynsym[idx_symtab].st_shndx != SHN_UNDEF
				)
				{
					debug("[i] Symbol type is STT_FUNC AND st_shndx IS NOT STD_UNDEF for %s\n", func_name);
					*((uintptr_t*)(runtime->elf.relaplt[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (
						(runtime->elf.dynsym[idx_symtab].st_value + (uintptr_t)runtime->elf.baseaddr)
					);
				}
				else 
				{
					//We need to lookup the symbol searching through DT_NEEDED libraries
					for(int x = 0; x < nb_needed; x++)
					{
						func_addr = (*libc_dlsym)(handles[x], func_name);
						if(func_addr != NULL)
						{
							
							if(crt_strcmp(func_name, __tls_get_addr_name, 1) == 0)
							{
								debug("[i] Patching symbol for %s function address is %p\n", func_name, (void*)((uintptr_t)__reflective_tls_get_addr));
								runtime->elf.__tls_get_addr = (tls_get_addr_t)func_addr;
								*((uintptr_t*)(runtime->elf.relaplt[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)__reflective_tls_get_addr;
							}
							else if(crt_strcmp(func_name, "__cxa_thread_atexit_impl", 1) == 0)
							{
								debug("[i] Patching symbol for %s function address is %p\n", func_name, (void*)((uintptr_t)__reflective_cxa_thread_atexit_impl));
								runtime->elf.__cxa_thread_atexit_impl = (cxa_thread_atexit_impl_t)func_addr;
								*((uintptr_t*)(runtime->elf.relaplt[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)__reflective_cxa_thread_atexit_impl;
							}
							else
							{
								debug("[i] Looking up symbol for %s function address is %p\n", func_name, func_addr);
								*((uintptr_t*)(runtime->elf.relaplt[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)func_addr;
							}
							break;
						}									
					}
				}	
			}
		}
	}
    

	if(runtime->elf.reladyn)
	{
		unsigned int index = 0;
		//Perform relocations (.rela.dyn)
		for(int i = 0; i < runtime->elf.secreladyn->sh_size / sizeof(Elf64_Rela); i++)
		{
			if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_64)
			{
				// debug("[i] Processing Relocation of type R_86_64_64\n");			
				index = ELF64_R_SYM(runtime->elf.reladyn[i].r_info);
				*((uintptr_t *) (runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (
					runtime->elf.dynsym[index].st_value + runtime->elf.reladyn[i].r_addend
				);
			}	
			else if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_GLOB_DAT)
			{
				/*
				* Lookup address of symbol and store it in GOT entry
				*/

				//Check symbol both locally and globally (searching through DT_NEEDED entries) 
				for(int x = 0; ;x++)
				{
					// debug("symbol: %s\n", runtime->elf.dynsym[x].st_name + runtime->elf.dynstr);
					if(
						crt_strcmp(
								runtime->elf.dynsym[x].st_name + runtime->elf.dynstr, 
								runtime->elf.dynsym[ELF64_R_SYM(runtime->elf.reladyn[i].r_info)].st_name + runtime->elf.dynstr, 
								1
						) == 0
					)						
					{
						const char* name = runtime->elf.dynsym[x].st_name + runtime->elf.dynstr;
						// debug("symbol: %s\n", name);
						//If symbol is a local symbol write the address of it into the .got.plt
						if(runtime->elf.dynsym[x].st_shndx == SHN_UNDEF)
						{
							for(int y = 0; y < nb_needed; y++)
							{
								void *faddr = libc_dlsym(handles[y], runtime->elf.dynsym[x].st_name + runtime->elf.dynstr);
								debug(
									"[i] Looking up symbol for relocation %d target %s function address is %p virtual address %ld\n", 
									i,
									runtime->elf.dynsym[x].st_name + runtime->elf.dynstr, 
									faddr,
									ELF64_R_SYM(runtime->elf.reladyn[i].r_info)
								);
								if(faddr != NULL)
								{
									if(crt_strcmp(name, __tls_get_addr_name, 1) == 0)
									{
										debug("[i] Patching symbol for %s function address is %p\n", name, (void*)((uintptr_t)__tls_get_addr_name));
										runtime->elf.__cxa_thread_atexit_impl = (cxa_thread_atexit_impl_t)faddr;
										*((uintptr_t*) (runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)__tls_get_addr_name;
									}
									else if(crt_strcmp(name, __cxa_thread_atexit_impl_name, 1) == 0)
									{
										// TODO: direct function symbol reference need to be replace the reflective loader context
										// with the manually resolved address of the function
										// can be done using egg hunting:
										// shellcode will look like this:
										// |reflective_load|reflective_load_size|__tls_get_addr_name|__tls_get_addr_name size|__reflective_cxa_thread_atexit_impl|__reflective_cxa_thread_atexit_impl size|elf|
										// so we first get an address on reflective_load (e.g rip) then move up until we find the elf payload using the header signature
										// the we work it backward using the functions size
										debug("[i] Patching symbol for %s function address is %p\n", name, (void*)((uintptr_t)__reflective_cxa_thread_atexit_impl));
										runtime->elf.__cxa_thread_atexit_impl = (cxa_thread_atexit_impl_t)faddr;
										*((uintptr_t*) (runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)__reflective_cxa_thread_atexit_impl;
									}
									else 
									{
										*((uintptr_t*) (runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr))  = (unsigned long )((unsigned long)faddr);
									}
									
									break;
								}
							}
							
							break;
						}
					
						//write value into got entry
						*((uintptr_t *)(runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)) = (uintptr_t)(
							runtime->elf.dynsym[x].st_value + (uintptr_t)runtime->elf.baseaddr
						);
						debug("address: %p\n", (void*)(*(uintptr_t*)(runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)));
						break;
					}
				}
			}
			else if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_RELATIVE)
			{
				// debug(
				// 	"[i] Processing Relocation of type R_x86_64_RELATIVE %s\n", 
				// 	runtime->elf.dynsym[ELF64_R_SYM(runtime->elf.reladyn[i].r_info)].st_name + runtime->elf.dynstr
				// );
				index = ELF64_R_SYM(runtime->elf.reladyn[i].r_info);
				*((uintptr_t *)((unsigned long)runtime->elf.reladyn[i].r_offset + (unsigned long)runtime->elf.baseaddr)) = (
				    runtime->elf.reladyn[i].r_addend + (uintptr_t)runtime->elf.baseaddr
				);
			}
			else if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_TPOFF64)
			{
				// Thread-local variable -> store offset from TLS base
				size_t sym = ELF64_R_SYM(runtime->elf.reladyn[i].r_info);

				uintptr_t tls_addr = (uintptr_t)runtime->elf.baseaddr + runtime->elf.reladyn[i].r_addend;
		
				*(uintptr_t *)(runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)
					= (uintptr_t)tls_addr;
		
			}
			else if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_DTPOFF64)
			{
				// module-relative TLS offset (dynamic TLS model)
				size_t sym = ELF64_R_SYM(runtime->elf.reladyn[i].r_info);

				uintptr_t off = runtime->elf.dynsym[sym].st_value;
		
				*(uintptr_t *)(runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)
					= off;
			}
			else if(ELF64_R_TYPE(runtime->elf.reladyn[i].r_info) == R_X86_64_DTPMOD64)
			{
				// TLS module ID (simplified loader model)
				*(uintptr_t *)(runtime->elf.reladyn[i].r_offset + (uintptr_t)runtime->elf.baseaddr)
					= (size_t)runtime->tls.tls_info.module_id; // fake module ID (you'd track real modules in full loader)
	
			}
			
		}
	}
    

	//Close Opened Libraries
	for(int i = 0; i < nb_needed; ++i)
	{
		libc_dlclose(handles[i]);
	}

    libc_free(handles);

    return ERROR_CODE_OK;
}

typedef void*(*libc_mprotect_t)(void*, size_t, int);
static inline error_code protect_module(loader_runtime_t* runtime) 
{
	void* handle = runtime->elf.baseaddr;
	if(!handle) return ERROR_CODE_INVALID_PARAMS;
	
	void* libc = resolve_libc();
	if(!libc)
	{
		return ERROR_CODE_NO_LIBRARY;
	}
	libc_mprotect_t libc_mprotect = (libc_mprotect_t)resolve_function(libc, "mprotect");
	if(!libc_mprotect)
	{
		return ERROR_CODE_NO_FUNCTION;
	}

    Elf_Ehdr* header = (Elf_Ehdr*)handle;  // ELF header at remapped base address
    Elf_Phdr* segments = (Elf_Phdr*)((char*)handle + header->e_phoff);  // Program header table in the remapped ELF

    // Iterate over all program headers to find PT_LOAD segments
    for (int i = 0; i < header->e_phnum; i++) {
        if (segments[i].p_type == PT_LOAD) {
            int prot = PROT_READ;  // Default to read-only

            // Determine protection flags based on segment flags (p_flags)
            if (segments[i].p_flags & PF_X) {
                prot |= PROT_EXEC;  // Executable (e.g., .text)
            }
            if (segments[i].p_flags & PF_W) {
                prot |= PROT_WRITE;  // Writable (e.g., .data)
            }

            // Calculate the segment's address in the remapped memory using baseaddr
            void* segment_addr = (void*)((uintptr_t)((char*)handle + segments[i].p_vaddr) & ~(0xFFF));  // Align to page boundary
            size_t segment_size = (
				segments[i].p_memsz + ((uintptr_t)((char*)handle + segments[i].p_vaddr) & 0xFFF) + 0xFFF
			) & ~(0xFFF);  // Page-aligned size

            // Apply memory protection using mprotect
            if (libc_mprotect(segment_addr, segment_size, prot) != 0) {
                debug("[!] Failed to set memory protection at %p\n", segment_addr);
                return ERROR_CODE_MPROTECT_FAIL;
            }

            debug("[+] Memory protection set at %p, prot: %d\n", segment_addr, prot);
        }
    }

    return ERROR_CODE_OK;
}

typedef void (*constructor_t)();
static inline void call_constructors(loader_runtime_t* runtime) 
{
	int init_arraysz = 0;
	void** init_array;

	//find DT_INIT_ARRAYSZ
	for(int i = 0; runtime->elf.dynamic[i].d_tag != DT_NULL; i++)
	{
		if(runtime->elf.dynamic[i].d_tag == DT_INIT_ARRAYSZ)
		{
			init_arraysz = runtime->elf.dynamic[i].d_un.d_ptr / sizeof(void*);	
			break;		
		}
	}
	
	//find DT_INIT_ARRAY
	for(int i = 0; runtime->elf.dynamic[i].d_tag != DT_NULL; i++)
	{
		if(runtime->elf.dynamic[i].d_tag == DT_INIT_ARRAY)
		{
			init_array = (void**)(runtime->elf.dynamic[i].d_un.d_ptr + (char*)runtime->elf.baseaddr);
			break;			
		}
	}
	debug("[i] init_array at: %p\n", init_array);

	//Call constructors in shared object
	for(int i = 0; i < init_arraysz; ++i)
	{
		constructor_t constructor = (constructor_t)((uintptr_t)init_array[i]);
		
		if(init_array[i] == 0)
			break;

		debug("[i] Calling constructor %p\n", constructor);
		constructor();
	}
}


static inline void* resolve_function_r(loader_runtime_t* runtime, const char* function_name)
{
    /* 
	* specific function resolver for reflectively loaded modules
	*/

	void* func = NULL;

    size_t dynsym_count = count_dynsym((void*)runtime->elf.header);
    for(size_t i = 0; i < dynsym_count; ++i)
    {
        char* name = runtime->elf.dynsym[i].st_name + runtime->elf.dynstr;
        if(crt_strcmp(name, function_name, 1) ==  0)
        {
            func = (void*)(runtime->elf.dynsym[i].st_value + (char*)runtime->elf.baseaddr);
            break;
        }
    }
    return func;
}

/**********************************************************/
typedef error_code(*func_t)(void* instance, size_t len, void* ptr);
#ifndef NDEBUG
static inline error_code reflective_load(unsigned char* payload, char* name, void* ptr, unsigned char sync)
#else
static inline error_code reflective_load(char* name, void* ptr, unsigned char sync)
#endif
{
    error_code error = ERROR_CODE_OK;
#ifndef NDEBUG
	void* self = payload;
#else
    void* self = resolve_self();
#endif
    if(self)
    {
        loader_runtime_t runtime = {0};
		crt_memset(&runtime, 0, sizeof(runtime));
		g_runtime = &runtime;

        error = rebase(self, &runtime);
        if(OK(error))
        {
			error = relocate(&runtime);
			if (OK(error))
			{
				error = protect_module(&runtime);
				if(OK(error))
				{
					if(runtime.elf.has_tls_section)
					{
						error = build_thread_tls_block(&runtime);
						if (OK(error))
						{
							error = setup_thread_tls(&runtime);
						}
					}
					if(OK(error))
					{
						call_constructors(&runtime);
						func_t func = (func_t)resolve_function_r(
							&runtime, name
						);
						if(func)
						{
							//!\\ this loader is synchronous, it expect the loaded module to be synchronous
							// The loader this clean the module from memory 
							// right after the called module function returns
							// If the module is still executing in another thread
							// then it will crash
							// TODO: wrap it in a reflective_loader function that will spawn the thread if needed
							error = func(runtime.elf.baseaddr, elf_memory_size(runtime.elf.baseaddr), ptr);
							__call_tls_dtors();
						}
						else 
						{
							error = ERROR_CODE_NO_FUNCTION;    
						}
					}
					if(runtime.elf.has_tls_section && runtime.tls.th_block_addr != NULL)
					{
						release_thread_tls_block(&runtime);
					}
				}
				
            }
			crt_munmap(runtime.elf.baseaddr, elf_memory_size(runtime.elf.baseaddr));
			//// Note: do not release memory: the agent is asynchronous
			// if(sync)
			// {
			// 	crt_munmap(runtime.elf.baseaddr, elf_memory_size(runtime.elf.baseaddr));
			// }
        }
    }
    else 
    {
        error = ERROR_CODE_NO_LIBRARY;
    }
    
    return error;
}

#endif // _REFLECTIVELOADER_H
