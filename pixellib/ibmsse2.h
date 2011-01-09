//=====================================================================
//
// ibmsse2.h - sse2 & mmx optimize
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#ifndef __IBMSSE2_H__
#define __IBMSSE2_H__

#include "ibmbits.h"



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

#endif

//---------------------------------------------------------------------
// PLATFORM DETECT
//---------------------------------------------------------------------
#if defined(__GNUC__)
	#if defined(__MMX__)
		#define __ARCH_MMX__
	#endif
	#if defined(__SSE__) 
		#define __ARCH_SSE__
	#endif
	#if defined(__SSE2__)
		#define __ARCH_SSE2__
	#endif
#elif defined(_MSC_VER) && defined(__x86__)
	#if !(defined(WIN64) || defined(_WIN64) || defined(__x86_64))
		#if _MSC_VER >= 1200
			#define __ARCH_MMX__
		#elif _MSC_VER >= 1300
			#define __ARCH_SSE__
		#elif _MSC_VER >= 1400
			#define __ARCH_SSE2__
		#endif
	#endif
#endif

#ifndef __IULONG_DEFINED
#define __IULONG_DEFINED
typedef ptrdiff_t ilong;
typedef size_t iulong;
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IUINT64;
#else
typedef unsigned long long IUINT64;
#endif
#endif

//---------------------------------------------------------------------
// pixel load and store
//---------------------------------------------------------------------



//---------------------------------------------------------------------
// INTERFACES
//---------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

int pixellib_mmx_init(void);

int pixellib_xmm_init(void);

#ifdef __cplusplus
}
#endif



#endif


