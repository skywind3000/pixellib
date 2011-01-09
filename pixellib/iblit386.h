//=====================================================================
//
// iblit386.h - bitmap blitters implemention in i386
// 
// -  bypasse on-chip, write directly into combining buffer
// -  software prefetch instruction
// -  improvement: speed up 200% - 250% vs tradition blitters
// -  compile it with gcc, msvc, borlandc, watcomc, ...
//
// for more information about cache-control instructions please see:
//    The IA-32 Intel Architecture Software Developer's Manual
//    Volume 3: System Programming Guide
// with the URL below:
//    http://developer.intel.com/design/pentium4/manuals/253668.htm
// Using Block Prefetch for Optimized Memory Performance, Mike Wall
//
// history of this file:
// -  Feb.15 2004  skywind  create this file with iblit_386, iblit_mmx
// -  Feb.20 2004  skywind  add most i386 assembler globals
// -  Jun.17 2004  skywind  implement normal blitter with sse
// -  May.05 2005  skywind  implement mask blitter with mmx & sse
// -  Aug.02 2005  skywind  improve blitters with cache-control tech.
// -  Dec.11 2006  skywind  rewrite cpu detection method
// -  Dec.27 2006  skywind  rewrite mask blitters 
// -  Jan.28 2007  skywind  reform this file with new comment
// -  Sep.09 2010  skywind  add 64bits & mmx macros
// -  Oct.03 2010  skywind  amd64 supported
//
//=====================================================================

#ifndef __IBLIT386_H__
#define __IBLIT386_H__

#include <stddef.h>


//---------------------------------------------------------------------
// platform detection
//---------------------------------------------------------------------
#if (defined(_WIN32) && !defined(WIN32))
	#define WIN32 _WIN32
#elif (defined(WIN32) && !defined(_WIN32))
	#define _WIN32 WIN32
#endif

#if (defined(_WIN32) && !defined(_MSC_VER) && !defined(_WIN64))
	#ifndef __i386__
	#define __i386__
	#endif
#elif defined(_MSC_VER)
	#if (defined(_M_IX86) && !defined(__i386__))
	#define __i386__
	#endif
#endif

#ifndef __i386__
	#if (defined(__386__) || defined(__I386__) || _M_IX86)
	#define __i386__
	#endif
#endif

#if (defined(__i386__) && !defined(__I386__))
	#define __I386__
#endif

#if (defined(__x86_64__) && !defined(__x86_64))
	#define __x86_64
#endif

#if (defined(__x86_64) && !defined(__x86_64__))
	#define __x86_64__
#endif

#if (defined(_M_AMD64)) && (!defined(__amd64__))
	#define __amd64__
#endif

#if (defined(__amd64) && !defined(__amd64__))
	#define __amd64__
#endif

#if (defined(__amd64__) && !defined(__amd64))
	#define __amd64
#endif

#if (defined(__i386__) || defined(__amd64__)) && (!defined(__x86__))
	#if !(defined(_MSC_VER) && defined(__amd64__))	
		#define __x86__  // MSVC doesn't support inline assembly in x64
	#endif
#endif

//=====================================================================
// i386 archtech support
//=====================================================================
#ifdef __x86__


#ifndef ASMCODE
#define ASMCODE
//---------------------------------------------------------------------
// basic inline asm operation code generation                         
//---------------------------------------------------------------------
#ifdef __GNUC__
#define ASMCODE1(a)             ".byte "#a"\n"
#define ASMCODE2(a, b)          ".byte "#a", "#b"\n"
#define ASMCODE3(a, b, c)       ".byte "#a", "#b", "#c"\n"
#define ASMCODE4(a, b, c, d)    ".byte "#a", "#b", "#c", "#d"\n"
#define ASMCODE5(a, b, c, d, e) ".byte "#a", "#b", "#c", "#d", "#e"\n"
#define ASMCODEW(n)             ".short "#n"\n"
#define ASMCODED(n)             ".long "#n"\n"
#define ASMALIGN(n)             ".align "#n", 0x90\n"
#define __ALIGNC__              ASMALIGN(8)
#define __INLINEGNU__
#ifndef __MACH__
#define ASM_BEGIN
#define ASM_ENDUP
#ifdef __i386__
#define ASM_REGS                "esi", "edi", "eax", "ebx", "ecx", "edx"
#else
#define ASM_REGS                "rsi", "rdi", "rax", "rbx", "rcx", "rdx"
#endif
#else
#ifdef __i386__
#define ASM_BEGIN               "    pushl %%ebx\n"
#define ASM_ENDUP               "    popl %%ebx\n"
#define ASM_REGS                "esi", "edi", "eax", "ecx", "edx"
#else
#define ASM_BEGIN               "    push %%rbx\n"
#define ASM_ENDUP               "    pop %%rbx\n"
#define ASM_REGS				"rsi", "rdi", "rax", "rcx", "rdx"
#endif
#endif

#elif defined(_MSC_VER)
#define ASMCODE1(a)             _asm _emit (a)
#define ASMCODE2(a, b)          ASMCODE1(a) ASMCODE1(b)
#define ASMCODE3(a, b, c)       ASMCODE2(a, b) ASMCODE1(c)
#define ASMCODE4(a, b, c, d)    ASMCODE3(a, b, c) ASMCODE1(d)
#define ASMCODE5(a, b, c, d, e) ASMCODE4(a, b, c, d) ASMCODE1(e)
#define ASMCODEW(n)             ASMCODE2(((n) AND 0xff), ((n) / 0x100))
#define ASMCODED(n)             ASMCODEW(n) ASMCODEW((n) / 0x10000)
#define ASMALIGN(n)             ALIGN n
#define __ALIGNC__              ASMALIGN(8)
#define __INLINEMSC__
#define ASM_BEGIN
#define ASM_ENDUP

#elif (defined(__BORLANDC__) || defined(__WATCOMC__))
#define ASMCODE1(a)             DB a; 
#define ASMCODE2(a, b)          DB a, b; 
#define ASMCODE3(a, b, c)       DB a, b, c; 
#define ASMCODE4(a, b, c, d)    DB a, b, c, d;
#define ASMCODE5(a, b, c, d, e) DB a, b, c, d, e;
#define ASMCODEW(n)             DW n;
#define ASMCODED(n)             DD n;
#define ASMALIGN(n)             ALIGN n
#pragma warn -8002  
#pragma warn -8004  
#pragma warn -8008  
#pragma warn -8012
#pragma warn -8027
#pragma warn -8057  
#pragma warn -8066  
#define __ALIGNC__              ASMALIGN(4)
#define __INLINEMSC__
//#ifdef __WATCOMC__
#undef __ALIGNC__
#define __ALIGNC__
//#endif
#define ASM_BEGIN
#define ASM_ENDUP
#endif
#endif


#ifndef ASM_MMPREFETCH
#define ASM_MMPREFETCH
//---------------------------------------------------------------------
// cache instructions operation code generation - SSE only          
//---------------------------------------------------------------------
#define mm_prefetch_esi      ASMCODE3(0x0f, 0x18, 0x06)
#define mm_prefetch_edi      ASMCODE3(0x0f, 0x18, 0x07)
#define mm_sfence            ASMCODE3(0x0f, 0xae, 0xf8)
#endif


//---------------------------------------------------------------------
// transform instructions operation code generation - SSE only      
//---------------------------------------------------------------------
#define mm_maskmovq(MMA, MMB) ASMCODE3(0x0f, 0xf7, 0xc0 | ((MMA)<<3)|(MMB) )
#define mm_maskmovq_0_2 ASMCODE3(0x0f, 0xf7, 0xC2)
#define mm_maskmovq_1_3 ASMCODE3(0x0f, 0xf7, 0xCB)


#ifndef ASM_MMPREFETCHX
#define ASM_MMPREFETCHX
//---------------------------------------------------------------------
// cache instructions extended operation code generation - SSE only 
//---------------------------------------------------------------------
#define mm_prefetch_esi_n(n) ASMCODE3(0x0f, 0x18, 0x86) ASMCODED(n)
#define mm_prefetch_edi_n(n) ASMCODE3(0x0f, 0x18, 0x87) ASMCODED(n)
#define mm_prefetch_esi_8cx_n(n) ASMCODE4(0x0f,0x18,0x84,0xce) ASMCODED(n)
#define mm_prefetch_esi_8dx ASMCODE4(0x0f, 0x18, 0x04, 0xd6)
#define mm_prefetch_edi_8dx ASMCODE4(0x0f, 0x18, 0x04, 0xd7)
#ifdef __WATCOMC__
#undef mm_prefetch_esi_n
#undef mm_prefetch_edi_n
#undef mm_prefetch_esi_8cx_n
#define mm_prefetch_esi_n(n) DD 0x86180f90, n
#define mm_prefetch_edi_n(n) DD 0x86180f90, n
#define mm_prefetch_esi_8cx_n(n) DD 0xCE84180f, n
#endif
#endif

//---------------------------------------------------------------------
// data instructions operation code generation - SSE only           
//---------------------------------------------------------------------
#define mm_movntq_edi_0_m0  ASMCODE3(0x0f, 0xe7, 0x07)
#define mm_movntq_edi_8_m1  ASMCODE4(0x0f, 0xe7, 0x4f, 0x08)
#define mm_movntq_edi_16_m2 ASMCODE4(0x0f, 0xe7, 0x57, 0x10)
#define mm_movntq_edi_24_m3 ASMCODE4(0x0f, 0xe7, 0x5f, 0x18)
#define mm_movntq_edi_32_m4 ASMCODE4(0x0f, 0xe7, 0x67, 0x20)
#define mm_movntq_edi_40_m5 ASMCODE4(0x0f, 0xe7, 0x6f, 0x28)
#define mm_movntq_edi_48_m6 ASMCODE4(0x0f, 0xe7, 0x77, 0x30)
#define mm_movntq_edi_56_m7 ASMCODE4(0x0f, 0xe7, 0x7f, 0x38)


//---------------------------------------------------------------------
// data instructions operation code generation - SSE only           
//---------------------------------------------------------------------
#define mm_movntq_edi_8cx_0_m0  ASMCODE4(0x0f, 0xe7, 0x04, 0xcf)
#define mm_movntq_edi_8cx_8_m1  ASMCODE5(0x0f, 0xe7, 0x4c, 0xcf, 0x08)
#define mm_movntq_edi_8cx_16_m2 ASMCODE5(0x0f, 0xe7, 0x54, 0xcf, 0x10)
#define mm_movntq_edi_8cx_24_m3 ASMCODE5(0x0f, 0xe7, 0x5c, 0xcf, 0x18)
#define mm_movntq_edi_8cx_32_m4 ASMCODE5(0x0f, 0xe7, 0x64, 0xcf, 0x20)
#define mm_movntq_edi_8cx_40_m5 ASMCODE5(0x0f, 0xe7, 0x6c, 0xcf, 0x28)
#define mm_movntq_edi_8cx_48_m6 ASMCODE5(0x0f, 0xe7, 0x74, 0xcf, 0x30)
#define mm_movntq_edi_8cx_56_m7 ASMCODE5(0x0f, 0xe7, 0x7c, 0xcf, 0x38)


#define _MMX_FEATURE_BIT        0x00800000
#define _SSE_FEATURE_BIT        0x02000000
#define _SSE2_FEATURE_BIT       0x04000000
#define _3DNOW_FEATURE_BIT      0x80000000

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// X86 FEATURE DEFINITION
//---------------------------------------------------------------------
#define X86_FEATURE_FPU		(0*32+ 0) /* Onboard FPU */
#define X86_FEATURE_VME		(0*32+ 1) /* Virtual Mode Extensions */
#define X86_FEATURE_DE		(0*32+ 2) /* Debugging Extensions */
#define X86_FEATURE_TSC		(0*32+ 4) /* Time Stamp Counter */
#define X86_FEATURE_MSR		(0*32+ 5) /* Model-Specific Registers */
#define X86_FEATURE_PAE		(0*32+ 6) /* Physical Address Extensions */
#define X86_FEATURE_MCE		(0*32+ 7) /* Machine Check Architecture */
#define X86_FEATURE_CX8		(0*32+ 8) /* CMPXCHG8 instruction */
#define X86_FEATURE_MCA		(0*32+14) /* Machine Check Architecture */
#define X86_FEATURE_CMOV	(0*32+15) /* CMOV (FCMOVCC/FCOMI too if FPU) */
#define X86_FEATURE_PAT		(0*32+16) /* Page Attribute Table */
#define X86_FEATURE_PSE36	(0*32+17) /* 36-bit PSEs */
#define X86_FEATURE_CLFLSH	(0*32+19) /* Supports the CLFLUSH instruction */
#define X86_FEATURE_MMX		(0*32+23) /* Multimedia Extensions */
#define X86_FEATURE_XMM		(0*32+25) /* Streaming SIMD Extensions */
#define X86_FEATURE_XMM2	(0*32+26) /* Streaming SIMD Extensions-2 */
#define X86_FEATURE_SELFSNOOP	(0*32+27) /* CPU self snoop */
#define X86_FEATURE_HT		(0*32+28) /* Hyper-Threading */
#define X86_FEATURE_ACC		(0*32+29) /* Automatic clock control */
#define X86_FEATURE_IA64	(0*32+30) /* IA-64 processor */

#define X86_FEATURE_MP		(1*32+19) /* MP Capable. */
#define X86_FEATURE_NX		(1*32+20) /* Execute Disable */
#define X86_FEATURE_MMXEXT	(1*32+22) /* AMD MMX extensions */
#define X86_FEATURE_LM		(1*32+29) /* Long Mode (x86-64) */
#define X86_FEATURE_3DNOWEXT	(1*32+30) /* AMD 3DNow! extensions */
#define X86_FEATURE_3DNOW	(1*32+31) /* 3DNow! */

#define X86_FEATURE_CXMMX	(3*32+ 0) /* Cyrix MMX extensions */
#define X86_FEATURE_K6_MTRR	(3*32+ 1) /* AMD K6 nonstandard MTRRs */
#define X86_FEATURE_CYRIX_ARR	(3*32+ 2) /* Cyrix ARRs (= MTRRs) */
#define X86_FEATURE_CENTAUR_MCR	(3*32+ 3) /* Centaur MCRs (= MTRRs) */
#define X86_FEATURE_K8		(3*32+ 4) /* Opteron, Athlon64 */
#define X86_FEATURE_K7		(3*32+ 5) /* Athlon */
#define X86_FEATURE_P3		(3*32+ 6) /* P3 */
#define X86_FEATURE_P4		(3*32+ 7) /* P4 */
#define X86_FEATURE_EST		(4*32+ 7) /* Enhanced SpeedStep */
#define X86_FEATURE_MWAIT	(4*32+ 3) /* Monitor/Mwait support */

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NSC 8
#define X86_VENDOR_NUM 9
#define X86_VENDOR_SIS 10
#define X86_VENDOR_UNKNOWN 0xff


//---------------------------------------------------------------------
// cpu global information
//---------------------------------------------------------------------
extern unsigned int _cpu_feature[];
extern unsigned int _cpu_cachesize;
extern int _cpu_level;
extern int _cpu_device;
extern int _cpu_vendor;
extern char _cpu_vendor_name[];
extern int _cpu_cache_l1i;
extern int _cpu_cache_l1d;
extern int _cpu_cache_l2;

#define _TEST_BIT(p, x) (((unsigned int*)(p))[(x) >> 5] & (1 << ((x) & 31)))
#define X86_FEATURE(x) _TEST_BIT(_cpu_feature, x)

#ifndef IMASK32
#define IMASK32 unsigned long
#endif

#ifndef ISRCPTR
#define ISRCPTR const char *
#endif

#ifdef __amd64__
typedef unsigned int IDWORD;
typedef unsigned long IQWORD;
#else
typedef unsigned long IDWORD;
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IQWORD;
#else
typedef unsigned long long IQWORD;
#endif
#endif

#ifndef __IULONG_DEFINED
#define __IULONG_DEFINED
typedef ptrdiff_t ilong;
typedef size_t iulong;
#endif

void _x86_cpuid(int op, int *eax, int *ebx, int *ecx, int *edx);
void _x86_detect(void);

void _x86_choose_blitter(void);

int iblit_386(char*, long, const char*, long, int, int, int, long);
int iblit_mmx(char*, long, const char*, long, int, int, int, long);
int iblit_sse(char*, long, const char*, long, int, int, int, long);
int iblit_mix(char*, long, const char*, long, int, int, int, long);

int iblit_mask_mmx(char*, long, ISRCPTR, long, int, int, int, long, IMASK32);
int iblit_mask_sse(char*, long, ISRCPTR, long, int, int, int, long, IMASK32);
int iblit_mask_mix(char*, long, ISRCPTR, long, int, int, int, long, IMASK32);


#ifdef __cplusplus
}
#endif



//=====================================================================
// MMX INLINE INSTRUCTIONS
//=====================================================================
#if defined(__GNUC__)
#define immx_op(op)	\
	__asm__ __volatile__ (#op "" : : : "memory")
#define immx_op_r_r(op, regd, regs) \
	__asm__ __volatile__ (#op " %%" #regs ", %%" #regd "" : : : "memory")
#define immx_op_r_i(op, regd, imm) \
	__asm__ __volatile__ (#op " %0, %%" #regd : : "i"(imm) : "memory")
#define immx_op_r_m(op, regd, mem) \
	__asm__ __volatile__ (#op " %0, %%" #regd : : "m"(*(mem)) : "memory")
#define immx_op_m_r(op, mem, regs) \
	__asm__ __volatile__ (#op " %%" #regs ", %0" : "=X"(*(mem)) : : "memory")
#define immx_op_r_m32(op, regd, mem32) \
	immx_op_r_m(op, regd, mem32)
#define immx_op_m32_r(op, mem32, regs) \
	immx_op_m_r(op, mem32, regs)
#define immx_op_r_v(op, regd, vars) \
	immx_op_r_m(op, regd, (&(vars)))
#define immx_op_v_r(op, vard, regs) \
	immx_op_m_r(op, (&(vard)), regs)
typedef unsigned long long immx_uint64;
typedef unsigned int immx_uint32;
typedef long long immx_int64;
typedef int immx_int32;
#define immx_const_uint64(__imm__) (__imm__ ## ULL)
#define immx_const_int64(__imm__) (__imm__ ## LL)
#elif defined(_MSC_VER) || defined(__BORLANDC__)
#define immx_op(op)  \
	_asm { op }
#define immx_op_r_r(op, regd, regs) \
	_asm { op regd, regs }
#define immx_op_r_i(op, regd, imm) \
	_asm { op regd, imm }
#define immx_op_r_m(op, regd, mem) \
	{ register __int64 var = *(__int64*)(mem); _asm { op regd, var } }
#define immx_op_m_r(op, mem, regs) \
	{ register __int64 var; _asm { op var, regs }; *(__int64*)(mem) = var; }
#define immx_op_r_m32(op, regd, mem32) \
	{ register int var = *(int*)(mem32); _asm { op regd, var } }
#define immx_op_m32_r(op, mem32, regs) \
	{ register int var; _asm { op var, regs }; *(int*)(mem32) = var; }
#define immx_op_r_v(op, regd, vars) \
	_asm { op regd, vars }
#define immx_op_v_r(op, vard, regs) \
	_asm { op vard, regs }
typedef unsigned __int64 immx_uint64;
typedef unsigned int immx_uint32;
typedef __int64 immx_int64;
typedef int immx_int32;
#define immx_const_uint64(__imm__) (__imm__ ## UI64)
#define immx_const_int64(__imm__) (__imm__ ## I64)
#endif



//---------------------------------------------------------------------
// MMX Instruction Tracing
//---------------------------------------------------------------------
#ifdef immx_op

typedef union {
	immx_uint64 uq;
	immx_int64 q;
	immx_uint32 ud[2];
	immx_int32 d[2];
	unsigned short uw[4];
	short w[4];
	unsigned char ub[8];
	char b[8];
	float s[2];
}	immx_t;

#define immx_op_trace(op) { \
		printf(#op "\n"); \
		immx_op(op); \
	}

#define immx_op_m_m(op, memd, mems) { \
		immx_op_r_m(movq, mm0, memd); \
		immx_op_r_m(op, mm0, mems); \
		immx_op_m_r(movq, memd, mm0); \
	}

#define immx_op_m32_m32(op, memd, mems) { \
		immx_op_r_m32(movd, mm0, memd); \
		immx_op_r_m32(movd, mm1, mems); \
		immx_op_r_r(op, mm0, mm1); \
		immx_op_m_r(movd, memd, mm0); \
	}

#define immx_op_trace_r_r(op, regd, regs) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#op "_r_r(" #regd "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m_r(movq, &mmx_trace, regs); \
		printf(#regs "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_r_r(op, regd, regs); \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#regd "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_r_i(op, regd, imm) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#op "_r_i(" #regd "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		mmx_trace.uq = (imm); \
		printf(#imm "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_r_i(op, regd, imm); \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#regd "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_r_m(op, regd, mem) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#op "_r_m(" #regd "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		mmx_trace.uq = *(immx_int64*)(mem); \
		printf(#mem "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_r_m(op, regd, mem); \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#regd "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_m_r(op, mem, regs) { \
		immx_t mmx_trace; \
		mmx_trace.uq = *(immx_int64*)(mem); \
		printf(#op "_m_r(" #mem "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m_r(movq, &mmx_trace, regs); \
		printf(#regs "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m_r(op, mem, regs); \
		mmx_trace.uq = *(immx_int64*)(mem); \
		printf(#mem "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_r_m32(op, regd, mem) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#op "_r_m32(" #regd "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		mmx_trace.d[0] = *(immx_int32*)(mem); \
		mmx_trace.d[1] = 0; \
		printf(#mem "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_r_m32(op, regd, mem); \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#regd "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_m32_r(op, mem, regs) { \
		immx_t mmx_trace; \
		mmx_trace.d[0] = *(immx_int32*)(mem); \
		mmx_trace.d[1] = 0; \
		printf(#op "_m32_r(" #mem "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m_r(movq, &mmx_trace, regs); \
		printf(#regs "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m32_r(op, mem, regs); \
		mmx_trace.d[0] = *(immx_int32*)(mem); \
		mmx_trace.d[1] = 0; \
		printf(#mem "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_m_m(op, memd, mems) { \
		immx_t mmx_trace1, mmx_trace2; \
		immx_op_r_m(movq, mm0, memd); \
		immx_op_r_m(movq, mm1, mems); \
		immx_op_m_r(movq, &mmx_trace1, mm0); \
		immx_op_m_r(movq, &mmx_trace2, mm1); \
		printf(#op "_m_m(" #memd "=0x%08x%08x, ", \
			mmx_trace1.d[1], mmx_trace1.d[0]); \
		printf(#mems "=0x%08x%08x) => ", \
			mmx_trace2.d[1], mmx_trace2.d[0]); \
		immx_op_m_m(op, memd, mems); \
		immx_op_r_m(movq, mm0, memd); \
		immx_op_m_r(movq, &mmx_trace1, mm0); \
		printf(#memd "=0x%08x%08x\n", \
			mmx_trace1.d[1], mmx_trace1.d[0]); \
	}

#define immx_op_trace_m32_m32(op, memd, mems) { \
		immx_t mmx_trace1, mmx_trace2; \
		immx_op_r_m32(movd, mm0, memd); \
		immx_op_r_m32(movd, mm1, mems); \
		immx_op_m_r(movq, immx_addr(mmx_trace1), mm0); \
		immx_op_m_r(movq, immx_addr(mmx_trace2), mm1); \
		printf(#op "_m_m(" #memd "=0x%08x, ", mmx_trace1.d[0]); \
		printf(#mems "=0x%08x) => ", mmx_trace2.d[0]); \
		immx_op_m_m(op, memd, mems); \
		immx_op_r_m32(movd, mm0, memd); \
		immx_op_m_r(movq, immx_addr(mmx_trace1), mm0); \
		printf(#memd "=0x%08x\n", mmx_trace1.d[0]); \
	}

#define immx_op_trace_r_v(op, regd, vars) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#op "_r_v(" #regd "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		mmx_trace.uq = (immx_int64)(vars); \
		printf(#vars "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_r_v(op, regd, vars); \
		immx_op_m_r(movq, &mmx_trace, regd); \
		printf(#regd "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_op_trace_v_r(op, vard, regs) { \
		immx_t mmx_trace; \
		mmx_trace.uq = (immx_int64)(vard); \
		printf(#op "_v_r(" #vard "=0x%08x%08x, ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_m_r(movq, &mmx_trace, regs); \
		printf(#regs "=0x%08x%08x) => ", \
			mmx_trace.d[1], mmx_trace.d[0]); \
		immx_op_v_r(op, vard, regs); \
		mmx_trace.uq = (immx_int64)(vard); \
		printf(#vard "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}

#define immx_trace_print_reg(regs) { \
		immx_t mmx_trace; \
		immx_op_m_r(movq, immx_addr(mmx_trace), regs); \
		printf(#regs "=0x%08x%08x\n", \
			mmx_trace.d[1], mmx_trace.d[0]); \
	}
			
#endif


#ifndef IMMX_TRACE
#define immx_inst(op)					immx_op(op)
#define immx_inst_r_r(op, regd, regs)	immx_op_r_r(op, regd, regs)
#define immx_inst_r_i(op, regd, imm)	immx_op_r_i(op, regd, imm)
#define immx_inst_r_m(op, regd, mem)	immx_op_r_m(op, regd, mem)
#define immx_inst_m_r(op, mem, regs)	immx_op_m_r(op, mem, regs)
#define immx_inst_r_m32(op, regd, mem)	immx_op_r_m32(op, regd, mem)
#define immx_inst_m32_r(op, mem, regs)	immx_op_m32_r(op, mem, regs)
#define immx_inst_r_v(op, regd, vars)	immx_op_r_v(op, regd, vars)
#define immx_inst_v_r(op, vard, regs)	immx_op_v_r(op, vard, regs)
#define immx_inst_m_m(op, memd, mems)	immx_op_m_m(op, memd, mems)
#define immx_inst_m32_m32(op, m1, m2)	immx_op_m32_m32(op, m1, m2)
#define immx_trace_reg(reg)	
#else
#define immx_inst(op)					immx_op_trace(op)
#define immx_inst_r_r(op, regd, regs)	immx_op_trace_r_r(op, regd, regs)
#define immx_inst_r_i(op, regd, imm)	immx_op_trace_r_i(op, regd, imm)
#define immx_inst_r_m(op, regd, mem)	immx_op_trace_r_m(op, regd, mem)
#define immx_inst_m_r(op, mem, regs)	immx_op_trace_m_r(op, mem, regs)
#define immx_inst_r_m32(op, regd, mem)	immx_op_trace_r_m32(op, regd, mem)
#define immx_inst_m32_r(op, mem, regs)	immx_op_trace_m32_r(op, mem, regs)
#define immx_inst_r_v(op, regd, vars)	immx_op_trace_r_v(op, regd, vars)
#define immx_inst_v_r(op, vard, regs)	immx_op_trace_v_r(op, vard, regs)
#define immx_inst_m_m(op, memd, mems)	immx_op_trace_m_m(op, memd, mems)
#define immx_inst_m32_m32(op, m1, m2)	immx_op_trace_m32_m32(op, m1, m2)
#define immx_trace_reg(reg)				immx_trace_print_reg(reg)
#endif


//---------------------------------------------------------------------
// MMX instructions (generated by python)
//---------------------------------------------------------------------
#define movq_r_r(regd, regs)		immx_inst_r_r(movq, regd, regs)
#define movq_r_m(regd, mem)			immx_inst_r_m(movq, regd, mem)
#define movq_m_r(mem, regs)			immx_inst_m_r(movq, mem, regs)
#define movq_m_m(memd, mems)		immx_inst_m_m(movq, memd, mems)
#define movq_r_v(regd, vars)		immx_inst_r_v(movq, regd, vars)
#define movq_v_r(vard, regs)		immx_inst_v_r(movq, vard, regs)

#define movd_r_r(regd, regs)		immx_inst_r_r(movd, regd, regs)
#define movd_r_m(regd, mem)			immx_inst_r_m32(movd, regd, mem)
#define movd_m_r(mem, regs)			immx_inst_m32_r(movd, mem, regs)
#define movd_m_m(memd, mems)		immx_inst_m_m(movd, memd, mems)
#define movd_r_v(regd, vars)		immx_inst_r_v(movd, regd, vars)
#define movd_v_r(vard, regs)		immx_inst_v_r(movd, vard, regs)

#define paddd_r_m(regd, mem)       immx_inst_r_m(paddd, regd, mem)
#define paddd_r_r(regd, regs)      immx_inst_r_r(paddd, regd, regs)
#define paddd_r_v(regd, vars)      immx_inst_r_v(paddd, regd, vars)
#define paddw_r_m(regd, mem)       immx_inst_r_m(paddw, regd, mem)
#define paddw_r_r(regd, regs)      immx_inst_r_r(paddw, regd, regs)
#define paddw_r_v(regd, vars)      immx_inst_r_v(paddw, regd, vars)
#define paddb_r_m(regd, mem)       immx_inst_r_m(paddb, regd, mem)
#define paddb_r_r(regd, regs)      immx_inst_r_r(paddb, regd, regs)
#define paddb_r_v(regd, vars)      immx_inst_r_v(paddb, regd, vars)

#define paddsw_r_m(regd, mem)      immx_inst_r_m(paddsw, regd, mem)
#define paddsw_r_r(regd, regs)     immx_inst_r_r(paddsw, regd, regs)
#define paddsw_r_v(regd, vars)     immx_inst_r_v(paddsw, regd, vars)
#define paddsb_r_m(regd, mem)      immx_inst_r_m(paddsb, regd, mem)
#define paddsb_r_r(regd, regs)     immx_inst_r_r(paddsb, regd, regs)
#define paddsb_r_v(regd, vars)     immx_inst_r_v(paddsb, regd, vars)

#define paddusw_r_m(regd, mem)     immx_inst_r_m(paddusw, regd, mem)
#define paddusw_r_r(regd, regs)    immx_inst_r_r(paddusw, regd, regs)
#define paddusw_r_v(regd, vars)    immx_inst_r_v(paddusw, regd, vars)
#define paddusb_r_m(regd, mem)     immx_inst_r_m(paddusb, regd, mem)
#define paddusb_r_r(regd, regs)    immx_inst_r_r(paddusb, regd, regs)
#define paddusb_r_v(regd, vars)    immx_inst_r_v(paddusb, regd, vars)

#define psubd_r_m(regd, mem)       immx_inst_r_m(psubd, regd, mem)
#define psubd_r_r(regd, regs)      immx_inst_r_r(psubd, regd, regs)
#define psubd_r_v(regd, vars)      immx_inst_r_v(psubd, regd, vars)
#define psubw_r_m(regd, mem)       immx_inst_r_m(psubw, regd, mem)
#define psubw_r_r(regd, regs)      immx_inst_r_r(psubw, regd, regs)
#define psubw_r_v(regd, vars)      immx_inst_r_v(psubw, regd, vars)
#define psubb_r_m(regd, mem)       immx_inst_r_m(psubb, regd, mem)
#define psubb_r_r(regd, regs)      immx_inst_r_r(psubb, regd, regs)
#define psubb_r_v(regd, vars)      immx_inst_r_v(psubb, regd, vars)

#define psubsw_r_m(regd, mem)      immx_inst_r_m(psubsw, regd, mem)
#define psubsw_r_r(regd, regs)     immx_inst_r_r(psubsw, regd, regs)
#define psubsw_r_v(regd, vars)     immx_inst_r_v(psubsw, regd, vars)
#define psubsb_r_m(regd, mem)      immx_inst_r_m(psubsb, regd, mem)
#define psubsb_r_r(regd, regs)     immx_inst_r_r(psubsb, regd, regs)
#define psubsb_r_v(regd, vars)     immx_inst_r_v(psubsb, regd, vars)

#define psubusw_r_m(regd, mem)     immx_inst_r_m(psubusw, regd, mem)
#define psubusw_r_r(regd, regs)    immx_inst_r_r(psubusw, regd, regs)
#define psubusw_r_v(regd, vars)    immx_inst_r_v(psubusw, regd, vars)
#define psubusb_r_m(regd, mem)     immx_inst_r_m(psubusb, regd, mem)
#define psubusb_r_r(regd, regs)    immx_inst_r_r(psubusb, regd, regs)
#define psubusb_r_v(regd, vars)    immx_inst_r_v(psubusb, regd, vars)

#define pmullw_r_m(regd, mem)      immx_inst_r_m(pmullw, regd, mem)
#define pmullw_r_r(regd, regs)     immx_inst_r_r(pmullw, regd, regs)
#define pmullw_r_v(regd, vars)     immx_inst_r_v(pmullw, regd, vars)

#define pmulhw_r_m(regd, mem)      immx_inst_r_m(pmulhw, regd, mem)
#define pmulhw_r_r(regd, regs)     immx_inst_r_r(pmulhw, regd, regs)
#define pmulhw_r_v(regd, vars)     immx_inst_r_v(pmulhw, regd, vars)

#define pmaddwd_r_m(regd, mem)     immx_inst_r_m(pmaddwd, regd, mem)
#define pmaddwd_r_r(regd, regs)    immx_inst_r_r(pmaddwd, regd, regs)
#define pmaddwd_r_v(regd, vars)    immx_inst_r_v(pmaddwd, regd, vars)

#define pand_r_m(regd, mem)        immx_inst_r_m(pand, regd, mem)
#define pand_r_r(regd, regs)       immx_inst_r_r(pand, regd, regs)
#define pand_r_v(regd, vars)       immx_inst_r_v(pand, regd, vars)

#define pandn_r_m(regd, mem)       immx_inst_r_m(pandn, regd, mem)
#define pandn_r_r(regd, regs)      immx_inst_r_r(pandn, regd, regs)
#define pandn_r_v(regd, vars)      immx_inst_r_v(pandn, regd, vars)

#define por_r_m(regd, mem)         immx_inst_r_m(por, regd, mem)
#define por_r_r(regd, regs)        immx_inst_r_r(por, regd, regs)
#define por_r_v(regd, vars)        immx_inst_r_v(por, regd, vars)

#define pxor_r_m(regd, mem)        immx_inst_r_m(pxor, regd, mem)
#define pxor_r_r(regd, regs)       immx_inst_r_r(pxor, regd, regs)
#define pxor_r_v(regd, vars)       immx_inst_r_v(pxor, regd, vars)

#define pcmpeqd_r_m(regd, mem)     immx_inst_r_m(pcmpeqd, regd, mem)
#define pcmpeqd_r_r(regd, regs)    immx_inst_r_r(pcmpeqd, regd, regs)
#define pcmpeqd_r_v(regd, vars)    immx_inst_r_v(pcmpeqd, regd, vars)
#define pcmpeqw_r_m(regd, mem)     immx_inst_r_m(pcmpeqw, regd, mem)
#define pcmpeqw_r_r(regd, regs)    immx_inst_r_r(pcmpeqw, regd, regs)
#define pcmpeqw_r_v(regd, vars)    immx_inst_r_v(pcmpeqw, regd, vars)
#define pcmpeqb_r_m(regd, mem)     immx_inst_r_m(pcmpeqb, regd, mem)
#define pcmpeqb_r_r(regd, regs)    immx_inst_r_r(pcmpeqb, regd, regs)
#define pcmpeqb_r_v(regd, vars)    immx_inst_r_v(pcmpeqb, regd, vars)

#define pcmpgtd_r_m(regd, mem)     immx_inst_r_m(pcmpgtd, regd, mem)
#define pcmpgtd_r_r(regd, regs)    immx_inst_r_r(pcmpgtd, regd, regs)
#define pcmpgtd_r_v(regd, vars)    immx_inst_r_v(pcmpgtd, regd, vars)
#define pcmpgtw_r_m(regd, mem)     immx_inst_r_m(pcmpgtw, regd, mem)
#define pcmpgtw_r_r(regd, regs)    immx_inst_r_r(pcmpgtw, regd, regs)
#define pcmpgtw_r_v(regd, vars)    immx_inst_r_v(pcmpgtw, regd, vars)
#define pcmpgtb_r_m(regd, mem)     immx_inst_r_m(pcmpgtb, regd, mem)
#define pcmpgtb_r_r(regd, regs)    immx_inst_r_r(pcmpgtb, regd, regs)
#define pcmpgtb_r_v(regd, vars)    immx_inst_r_v(pcmpgtb, regd, vars)

#define psllq_r_i(regd, imm)       immx_inst_r_i(psllq, regd, imm)
#define psllq_r_m(regd, mem)       immx_inst_r_m(psllq, regd, mem)
#define psllq_r_r(regd, regs)      immx_inst_r_r(psllq, regd, regs)
#define psllq_r_v(regd, vars)      immx_inst_r_v(psllq, regd, vars)
#define pslld_r_i(regd, imm)       immx_inst_r_i(pslld, regd, imm)
#define pslld_r_m(regd, mem)       immx_inst_r_m(pslld, regd, mem)
#define pslld_r_r(regd, regs)      immx_inst_r_r(pslld, regd, regs)
#define pslld_r_v(regd, vars)      immx_inst_r_v(pslld, regd, vars)
#define psllw_r_i(regd, imm)       immx_inst_r_i(psllw, regd, imm)
#define psllw_r_m(regd, mem)       immx_inst_r_m(psllw, regd, mem)
#define psllw_r_r(regd, regs)      immx_inst_r_r(psllw, regd, regs)
#define psllw_r_v(regd, vars)      immx_inst_r_v(psllw, regd, vars)

#define psrlq_r_i(regd, imm)       immx_inst_r_i(psrlq, regd, imm)
#define psrlq_r_m(regd, mem)       immx_inst_r_m(psrlq, regd, mem)
#define psrlq_r_r(regd, regs)      immx_inst_r_r(psrlq, regd, regs)
#define psrlq_r_v(regd, vars)      immx_inst_r_v(psrlq, regd, vars)
#define psrld_r_i(regd, imm)       immx_inst_r_i(psrld, regd, imm)
#define psrld_r_m(regd, mem)       immx_inst_r_m(psrld, regd, mem)
#define psrld_r_r(regd, regs)      immx_inst_r_r(psrld, regd, regs)
#define psrld_r_v(regd, vars)      immx_inst_r_v(psrld, regd, vars)
#define psrlw_r_i(regd, imm)       immx_inst_r_i(psrlw, regd, imm)
#define psrlw_r_m(regd, mem)       immx_inst_r_m(psrlw, regd, mem)
#define psrlw_r_r(regd, regs)      immx_inst_r_r(psrlw, regd, regs)
#define psrlw_r_v(regd, vars)      immx_inst_r_v(psrlw, regd, vars)

#define psrad_r_i(regd, imm)       immx_inst_r_i(psrad, regd, imm)
#define psrad_r_m(regd, mem)       immx_inst_r_m(psrad, regd, mem)
#define psrad_r_r(regd, regs)      immx_inst_r_r(psrad, regd, regs)
#define psrad_r_v(regd, vars)      immx_inst_r_v(psrad, regd, vars)
#define psraw_r_i(regd, imm)       immx_inst_r_i(psraw, regd, imm)
#define psraw_r_m(regd, mem)       immx_inst_r_m(psraw, regd, mem)
#define psraw_r_r(regd, regs)      immx_inst_r_r(psraw, regd, regs)
#define psraw_r_v(regd, vars)      immx_inst_r_v(psraw, regd, vars)

#define packssdw_r_m(regd, mem)    immx_inst_r_m(packssdw, regd, mem)
#define packssdw_r_r(regd, regs)   immx_inst_r_r(packssdw, regd, regs)
#define packssdw_r_v(regd, vars)   immx_inst_r_v(packssdw, regd, vars)
#define packsswb_r_m(regd, mem)    immx_inst_r_m(packsswb, regd, mem)
#define packsswb_r_r(regd, regs)   immx_inst_r_r(packsswb, regd, regs)
#define packsswb_r_v(regd, vars)   immx_inst_r_v(packsswb, regd, vars)

#define packuswb_r_m(regd, mem)    immx_inst_r_m(packuswb, regd, mem)
#define packuswb_r_r(regd, regs)   immx_inst_r_r(packuswb, regd, regs)
#define packuswb_r_v(regd, vars)   immx_inst_r_v(packuswb, regd, vars)

#define punpckldq_r_m(regd, mem)   immx_inst_r_m(punpckldq, regd, mem)
#define punpckldq_r_r(regd, regs)  immx_inst_r_r(punpckldq, regd, regs)
#define punpckldq_r_v(regd, vars)  immx_inst_r_v(punpckldq, regd, vars)
#define punpcklwd_r_m(regd, mem)   immx_inst_r_m(punpcklwd, regd, mem)
#define punpcklwd_r_r(regd, regs)  immx_inst_r_r(punpcklwd, regd, regs)
#define punpcklwd_r_v(regd, vars)  immx_inst_r_v(punpcklwd, regd, vars)
#define punpcklbw_r_m(regd, mem)   immx_inst_r_m(punpcklbw, regd, mem)
#define punpcklbw_r_r(regd, regs)  immx_inst_r_r(punpcklbw, regd, regs)
#define punpcklbw_r_v(regd, vars)  immx_inst_r_v(punpcklbw, regd, vars)

#define punpckhdq_r_m(regd, mem)   immx_inst_r_m(punpckhdq, regd, mem)
#define punpckhdq_r_r(regd, regs)  immx_inst_r_r(punpckhdq, regd, regs)
#define punpckhdq_r_v(regd, vars)  immx_inst_r_v(punpckhdq, regd, vars)
#define punpckhwd_r_m(regd, mem)   immx_inst_r_m(punpckhwd, regd, mem)
#define punpckhwd_r_r(regd, regs)  immx_inst_r_r(punpckhwd, regd, regs)
#define punpckhwd_r_v(regd, vars)  immx_inst_r_v(punpckhwd, regd, vars)
#define punpckhbw_r_m(regd, mem)   immx_inst_r_m(punpckhbw, regd, mem)
#define punpckhbw_r_r(regd, regs)  immx_inst_r_r(punpckhbw, regd, regs)
#define punpckhbw_r_v(regd, vars)  immx_inst_r_v(punpckhbw, regd, vars)

#define immx_emms()		immx_inst(emms)


#if (defined(__i386__) || defined(__amd64__) || defined(_M_IA64))
	#if defined(immx_inst) && defined(__x86__)
		#define ASMMMX_ENABLE
		#define __asmmmx__
	#endif
#endif

#endif

#endif


