//=====================================================================
//
// iblit386.c - bitmap blitters implemention in i386
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
// -  Oct.03 2010  skywind  amd64 supported
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iblit386.h"
#include "ibitmap.h"

#ifdef __x86__


//---------------------------------------------------------------------
// GLOBAL DEFINITION
//---------------------------------------------------------------------
unsigned int _cpu_feature[4] = { 0, 0, 0, 0 };
unsigned int _cpu_cachesize;
int _cpu_level = -1;
int _cpu_device = -1;
int _cpu_vendor = X86_VENDOR_UNKNOWN;
char _cpu_vendor_name[14] = "unknow";

int _cpu_cache_l1i = 0;
int _cpu_cache_l1d = 0;
int _cpu_cache_l2 = 0;


struct _x86_cpu_types 
{
	int vendor;
	char ident[14];
	char name[14];
};


static struct _x86_cpu_types _x86_cpu_ident[] = {
	{ X86_VENDOR_INTEL,     "GenuineIntel", "Intel" }, 
	{ X86_VENDOR_CYRIX,     "CyrixInstead", "Cyrix" }, 
	{ X86_VENDOR_AMD,       "AuthenticAMD", "AMD" },        
	{ X86_VENDOR_UMC,       "UMC UMC UMC ", "UMC" }, 
	{ X86_VENDOR_NEXGEN,    "NexGenDriven", "NexGen" }, 
	{ X86_VENDOR_CENTAUR,   "CentaurHauls", "Centaur" }, 
	{ X86_VENDOR_RISE,      "RiseRiseRise", "Rise" }, 
	{ X86_VENDOR_TRANSMETA, "GenuineTMx86", "Transmeta" }, 
	{ X86_VENDOR_TRANSMETA, "TransmetaCPU", "Transmeta" }, 
	{ X86_VENDOR_NSC,       "Geode by NSC", "NSC" }, 
	{ X86_VENDOR_SIS,       "SiS SiS SiS ", "SiS" },
	{ -1, "", "Unknow" }
};


//---------------------------------------------------------------------
// _x86_cpuid
//---------------------------------------------------------------------
void _x86_cpuid(int op, int *eax, int *ebx, int *ecx, int *edx)
{
	int a = op, b = 0, c = 0, d = 0;
	
	#define mm_cpuid ASMCODE2(0x0F, 0xA2)	
	#ifdef __INLINEGNU__
	{
		#ifdef __i386__
		__asm__ __volatile__(
		"	pushal\n"
		"	movl %0, %%eax\n"
		mm_cpuid
		"	movl %%eax, %0\n"
		"	movl %%ebx, %1\n"
		"	movl %%ecx, %2\n"
		"	movl %%edx, %3\n"
		"	popal\n"
		: "=m" (a), "=m" (b), "=m" (c), "=m" (d) 
		: "a" (op): "memory");
		#else
		iulong x1, x2, x3, x4;
		__asm__ __volatile__(
		"	movq %%rax, %4\n"
		"	movq %%rbx, %5\n"
		"	movq %%rcx, %6\n"
		"	movq %%rdx, %7\n"
		"	movl %0, %%eax\n"
		mm_cpuid
		"	movl %%eax, %0\n"
		"	movl %%ebx, %1\n"
		"	movl %%ecx, %2\n"
		"	movl %%edx, %3\n"
		"	movq %4, %%rax\n"
		"	movq %5, %%rbx\n"
		"	movq %6, %%rcx\n"
		"	movq %7, %%rdx\n"
		: "=m" (a), "=m" (b), "=m" (c), "=m" (d),
		  "=m" (x1), "=m" (x2), "=m" (x3), "=m" (x4)
		: "a" (op): "memory");
		#endif
	}
	#elif defined(__INLINEMSC__)
	_asm {
		#ifdef __i386__
		pushad
		#else
		push rax
		push rbx
		push rcx
		push rdx
		#endif
		mov eax, op
		mm_cpuid
		mov a, eax
		mov b, ebx
		mov c, ecx
		mov d, edx
		#ifdef __i386__
		popad
		#else
		pop rdx
		pop rcx
		pop rbx
		pop rax
		#endif
	}
	#endif
	#undef mm_cpuid
	if (eax) *eax = a;
	if (ebx) *ebx = b;
	if (ecx) *ecx = c;
	if (edx) *edx = d;
}

//---------------------------------------------------------------------
// _flag_changeable
//---------------------------------------------------------------------
static int _flag_changeable(iulong flag) 
{ 
	iulong f1 = 0, f2 = 0;
	#ifdef __INLINEGNU__
	__asm__ __volatile__ ( 
		#ifndef __amd64__
		"pushfl\n" 
		"pushfl\n" 
		"popl %0\n" 
		"movl %0,%1\n" 
		"xorl %2,%0\n" 
		"pushl %0\n" 
		"popfl\n" 
		"pushfl\n" 
		"popl %0\n" 
		"popfl\n" 
		#else
		"pushfq\n"
		"pushfq\n"
		"popq %0\n"
		"movq %0, %1\n"
		"xorq %2, %0\n"
		"pushq %0\n"
		"popfq\n"
		"pushfq\n"
		"popq %0\n"
		"popfq\n"
		#endif
		: "=&r" (f1), "=&r" (f2) 
		: "ir" (flag) : "memory"); 
	#elif defined(__INLINEMSC__)
	_asm {
		#ifndef __amd64__
		pushfd
		pushfd
		pop eax
		mov f1, eax
		mov f2, eax
		xor eax, flag
		push eax
		popfd
		pushfd
		pop eax
		mov f1, eax
		popfd
		#else
		pushfq
		pushfq
		pop rax
		mov f1, rax
		mov f2, rax
		xor rax, flag
		push rax
		popfq
		pushfq
		pop rax
		mov f1, rax
		popfq
		#endif
	}
	#endif
	return ((f1^f2) & flag) != 0; 
} 

//---------------------------------------------------------------------
// _x86_detect_cpu
//---------------------------------------------------------------------
static void _x86_detect_cpu(void)
{
	iulong test = 0;
	int eax, edx, i;

	// setup default value
	_cpu_vendor = X86_VENDOR_UNKNOWN;
	_cpu_vendor_name[0] = 0;
	_cpu_device = -1;
	_cpu_level = -1;

	// check weather have cpuid instruction
	if (!_flag_changeable(0x00200000)) {	// have not cpuid	
		if (_flag_changeable(0x00040000)) 
			_cpu_device = 0x00000400;	// 486
		else 
			_cpu_device = 0x00000300;	// 386
		#ifndef __amd64__
		if (_cpu_device == 0x00000400) {
			#ifdef __INLINEGNU__
			__asm__ __volatile__ (
				"pushl %%ebx\n"
				"movl $5, %%eax\n"
				"movl $2, %%ebx\n"
				"sahf\n"
				"div %%ebx\n"
				"lahf\n"
				"movl %%eax, %0\n"
				"popl %%ebx\n"
				:"=m"(test): :"memory");
			#elif defined(__INLINEMSC__)
			iulong flag = 0;
			_asm {
				mov eax, 5
				mov ebx, 2
				sahf
				div ebx
				lahf
				mov flag, eax
			}
			test = flag;
			#endif
			if ((test & 0xFF00) == 0x0200) 
				memcpy(_cpu_vendor_name, "CyrixInstead", 13);
		}
		if (_cpu_vendor_name[0] == 0) {
			#ifdef __INLINEGNU__
			__asm__ __volatile__ ( 
				"movl $0x5555, %%eax\n" 
				"xorw %%dx,%%dx\n" 
				"movw $2, %%cx\n" 
				"divw %%cx\n" 
				"xorl %%eax, %%eax\n" 
				"jnz 1f\n" 
				"movl $1, %%eax\n" 
				"1:\n" 
				"mov %%eax, %0\n"
				: "=a" (test) : : "cx", "dx" ); 
			#elif defined(__INLINEMSC__)
			iulong flag = 0;
			_asm {
				mov eax, 0x5555
				xor dx, dx
				mov cx, 2
				div cx
				xor eax, eax
				jnz L1
				mov eax, 1
			L1:
				mov flag, eax
			}
			test = flag;
			#endif
			if (test) memcpy(_cpu_vendor_name, "NexGenDriven", 13);
		}
		#else
		test = test;
		#endif
	}	else {		// have cpuid
		int regs[4];
		char *p = (char*)regs;
		_x86_cpuid(0, regs, regs + 1, regs + 2, regs + 3);
		_cpu_level = regs[0];
		#ifdef __amd64__
		if (_cpu_level < 1) _cpu_level = 1;
		#endif
		for (i = 0; i < 12; i++) _cpu_vendor_name[i] = p[i + 4];
		_cpu_vendor_name[i] = 0;

		if (_cpu_level >= 1) {
			*(int*)(&_cpu_device) = 0;
			_x86_cpuid(1, &_cpu_device, 0, 0, 0);
		}	else {
			_cpu_device = 0x00000400;
		}
	}
	for (i = 0; _x86_cpu_ident[i].vendor >= 0; i++) {
		if (memcmp(_cpu_vendor_name, _x86_cpu_ident[i].ident, 12) == 0) {
			_cpu_vendor = _x86_cpu_ident[i].vendor;
			break;
		}
	}
	for (i = 0; i < 4; i++) 
		_cpu_feature[i] = 0;

	if (_cpu_level >= 1) {
		_x86_cpuid(1, 0, 0, 0, &edx);
		_cpu_feature[0] = (unsigned int)edx;
		_x86_cpuid(0x80000000uL, &eax, 0, 0, 0);
		if ((unsigned int)eax != 0x80000000ul) {
			_x86_cpuid(0x80000001uL, 0, 0, 0, &edx);
			_cpu_feature[1] = (unsigned int)edx;
		}
		#ifdef __amd64__
		_cpu_feature[0] |= (1 << X86_FEATURE_MMX);
		_cpu_feature[0] |= (1 << X86_FEATURE_XMM);
		#endif
	}
}


//---------------------------------------------------------------------
// _x86_detect_cpu
//---------------------------------------------------------------------
void _x86_detect(void)
{
	static int flag = 0;
	if (flag) return;
	_x86_detect_cpu();
	flag = 1;
}


//---------------------------------------------------------------------
// _x86_choose_blitter
//---------------------------------------------------------------------
void _x86_choose_blitter(void)
{
	_x86_detect();
	ibitmap_funcset(0, (void*)iblit_386);
	ibitmap_funcset(1, NULL);
	if (X86_FEATURE(X86_FEATURE_MMX)) {
		if (X86_FEATURE(X86_FEATURE_XMM)) 
			ibitmap_funcset(0, (void*)iblit_mix);
		else
			ibitmap_funcset(0, (void*)iblit_mix);
	}
	if (X86_FEATURE(X86_FEATURE_MMX)) {
		#ifndef __amd64__
		if (X86_FEATURE(X86_FEATURE_XMM)) {
			ibitmap_funcset(1, (void*)iblit_mask_mix);
		}	else {
			ibitmap_funcset(1, (void*)iblit_mask_mmx);
		}
		#else
		ibitmap_funcset(1, (void*)iblit_mask_mmx);
		#endif
	}
}


//---------------------------------------------------------------------
// iblit_386 - 386 normal blitter
// this routine is design to support the platform elder than 80486
//---------------------------------------------------------------------
int iblit_386(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize)
{
	volatile ilong linebytes = linesize;
	volatile ilong hh = h;
	volatile ilong c1, c2;

	if (pixelbyte == 1) linebytes = w;
	c1 = pitch1 - linesize;
	c2 = pitch2 - linesize;

	#ifdef __INLINEGNU__
	{
		#ifndef __amd64__
		__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movl %3, %%eax\n"
		"	movl %4, %%ebx\n"
		"	movl %5, %%edx\n"
		"	cld\n"
		"1:\n"
		"	movl %%eax, %%ecx\n"
		"	shrl $2, %%ecx\n"
		"	rep movsl\n"
		"	movl %%eax, %%ecx\n"
		"	andl $3, %%ecx\n"
		"	rep movsb\n"
		"	addl %%ebx, %%edi\n"
		"	addl %%edx, %%esi\n"
		"	decl %0\n"
		"	jnz 1b\n"
			ASM_ENDUP
		:"=m"(hh)
		:"m"(src), "m"(dst), "m"(linebytes), "m"(c1), "m"(c2)
		:"memory", ASM_REGS);
		#else
		volatile iulong x1, x2, x3, x4, x5, x6;
		volatile const char *ss = src;
		volatile char *dd = dst;
		__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movq %%rax, %0\n"
		"	movq %%rbx, %1\n"
		"	movq %%rcx, %2\n"
		"	movq %%rdx, %3\n"
		"	movq %%rsi, %4\n"
		"	movq %%rdi, %5\n"
		"	movq %7, %%rsi\n"
		"	movq %8, %%rdi\n"
		"	movq %9, %%rax\n"
		"	movq %10, %%rbx\n"
		"	movq %11, %%rdx\n"
		"	cld\n"
		"1:\n"
		"	movq %%rax, %%rcx\n"
		"	shrq $3, %%rcx\n"
		"	rep movsq\n"
		"	movq %%rax, %%rcx\n"
		"	andq $7, %%rcx\n"
		"	rep movsb\n"
		"	addq %%rbx, %%rdi\n"
		"	addq %%rdx, %%rsi\n"
		"	decq %6\n"
		"	jnz 1b\n"
		"	movq %0, %%rax\n"
		"	movq %1, %%rbx\n"
		"	movq %2, %%rcx\n"
		"	movq %3, %%rdx\n"
		"	movq %4, %%rsi\n"
		"	movq %5, %%rdi\n"
			ASM_ENDUP
		:"=m"(x1), "=m"(x2), "=m"(x3), "=m"(x4), "=m"(x5), "=m"(x6), "=m"(hh)
		:"m"(ss), "m"(dd), "m"(linebytes), "m"(c1), "m"(c2)
		:"memory", ASM_REGS);
		#endif
	}
	#elif defined(__INLINEMSC__)
	_asm {
	#ifndef __amd64__
		mov esi, src
		mov edi, dst
		mov eax, linebytes
		mov ebx, c1
		mov edx, c2
		cld
	blit_386n_loopline:
		mov ecx, eax
		shr ecx, 2
		rep movsd
		mov ecx, eax
		and ecx, 3
		rep movsb
		add edi, ebx
		add esi, edx
	#else
		mov rsi, src
		mov rdi, dst
		mov rax, linebytes
		mov rbx, c1
		mov rdx, c2
		cld
	blit_386n_loopline:
		mov rcx, rax
		shr rcx, 3
		rep movsq
		mov rcx, rax
		and rcx, 7
		rep movsb
		add rdi, rbx
		add rsi, rdx
	#endif
		dec hh
		jnz blit_386n_loopline
	}
	#else
	for (; h; h--) {
		memcpy(dst, src, linebytes);
		dst += linebytes + c1;
		src += linebytes + c2;
	}
	#endif

	return 0;
}


//---------------------------------------------------------------------
// iblit_mmx - mmx normal blitter
// this routine is design to support the platform with MMX feature
//---------------------------------------------------------------------
int iblit_mmx(char *dst, long pitch1, const char *src, long pitch2, 
	int w, int h, int pixelbyte, long linesize)
{
	volatile ilong c1, c2, m, n1, n2;
	volatile ilong hh = h;

	if (pixelbyte == 1) linesize = w;

	c1 = pitch1 - linesize;
	c2 = pitch2 - linesize;
	m = linesize >> 6;
	n1 = (linesize & 63) >> 2;
	n2 = (linesize & 63) & 3;

#if defined(__INLINEGNU__)
	{
	#ifndef __amd64__
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movl %3, %%eax\n"
		"	movl %4, %%ebx\n"
		"	movl %5, %%edx\n"
		"	cld\n"
		"1:\n"
		"	movl %%eax, %%ecx\n"
		"	testl %%eax, %%eax\n"
		"	jz 3f\n"
		"2:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm1\n"
		"	movq 16(%%esi), %%mm2\n"
		"	movq 24(%%esi), %%mm3\n"
		"	movq 32(%%esi), %%mm4\n"
		"	movq 40(%%esi), %%mm5\n"
		"	movq 48(%%esi), %%mm6\n"
		"	movq 56(%%esi), %%mm7\n"
		"	movq %%mm0, 0(%%edi)\n"
		"	movq %%mm1, 8(%%edi)\n"
		"	movq %%mm2, 16(%%edi)\n"
		"	movq %%mm3, 24(%%edi)\n"
		"	movq %%mm4, 32(%%edi)\n"
		"	movq %%mm5, 40(%%edi)\n"
		"	movq %%mm6, 48(%%edi)\n"
		"	movq %%mm7, 56(%%edi)\n"
		"	addl $64, %%esi\n"
		"	addl $64, %%edi\n"
		"	decl %%ecx\n"
		"	jnz 2b\n"
		"3:\n"
		"	movl %%ebx, %%ecx\n"
		"	rep movsl\n"
		"	movl %%edx, %%ecx\n"
		"	rep movsb\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz 1b\n"
		"	emms\n"
			ASM_ENDUP
		:"=m"(hh)
		:"m"(src),"m"(dst),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
	#else
		volatile iulong x1, x2, x3, x4, x5, x6;
		volatile const char *ss = src;
		volatile char *dd = dst;
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movq %%rax, %0\n"
		"	movq %%rbx, %1\n"
		"	movq %%rcx, %2\n"
		"	movq %%rdx, %3\n"
		"	movq %%rsi, %4\n"
		"	movq %%rdi, %5\n"
		"	mov %7, %%rsi\n"
		"	mov %8, %%rdi\n"
		"	mov %9, %%rax\n"
		"	mov %10, %%rbx\n"
		"	mov %11, %%rdx\n"
		"	cld\n"
		"1:\n"
		"	mov %%rax, %%rcx\n"
		"	test %%rax, %%rax\n"
		"	jz 3f\n"
		"2:\n"
		"	movq 0(%%rsi), %%mm0\n"
		"	movq 8(%%rsi), %%mm1\n"
		"	movq 16(%%rsi), %%mm2\n"
		"	movq 24(%%rsi), %%mm3\n"
		"	movq 32(%%rsi), %%mm4\n"
		"	movq 40(%%rsi), %%mm5\n"
		"	movq 48(%%rsi), %%mm6\n"
		"	movq 56(%%rsi), %%mm7\n"
		"	movq %%mm0, 0(%%rdi)\n"
		"	movq %%mm1, 8(%%rdi)\n"
		"	movq %%mm2, 16(%%rdi)\n"
		"	movq %%mm3, 24(%%rdi)\n"
		"	movq %%mm4, 32(%%rdi)\n"
		"	movq %%mm5, 40(%%rdi)\n"
		"	movq %%mm6, 48(%%rdi)\n"
		"	movq %%mm7, 56(%%rdi)\n"
		"	add $64, %%rsi\n"
		"	add $64, %%rdi\n"
		"	dec %%rcx\n"
		"	jnz 2b\n"
		"3:\n"
		"	mov %%rbx, %%rcx\n"
		"	rep movsl\n"
		"	mov %%rdx, %%rcx\n"
		"	rep movsb\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	decq %6\n"
		"	jnz 1b\n"
		"	emms\n"
		"	movq %0, %%rax\n"
		"	movq %1, %%rbx\n"
		"	movq %2, %%rcx\n"
		"	movq %3, %%rdx\n"
		"	movq %4, %%rsi\n"
		"	movq %5, %%rdi\n"
			ASM_ENDUP
		:"=m"(x1), "=m"(x2), "=m"(x3), "=m"(x4), "=m"(x5), "=m"(x6), "=m"(hh)
		:"m"(ss),"m"(dd),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
	#endif
	}

#elif defined(__INLINEMSC__)
	_asm {
	#ifndef __amd64__
		mov esi, src
		mov edi, dst
		mov eax, m
		mov ebx, n1
		mov edx, n2
		cld
	loop_line:
		mov ecx, eax
		test eax, eax
		jz jmp_next
	loop_pixel:
		movq mm0, [esi + 0]
		movq mm1, [esi + 8]
		movq mm2, [esi + 16]
		movq mm3, [esi + 24]
		movq mm4, [esi + 32]
		movq mm5, [esi + 40]
		movq mm6, [esi + 48]
		movq mm7, [esi + 56]
		movq [edi + 0], mm0
		movq [edi + 8], mm1
		movq [edi + 16], mm2
		movq [edi + 24], mm3
		movq [edi + 32], mm4
		movq [edi + 40], mm5
		movq [edi + 48], mm6
		movq [edi + 56], mm7
		add esi, 64
		add edi, 64
		dec ecx
		jnz loop_pixel
	jmp_next:
		mov ecx, ebx
		rep movsd
		mov ecx, edx
		rep movsb
		add edi, c1
		add esi, c2
	#else
		mov rsi, src
		mov rdi, dst
		mov rax, m
		mov rbx, n1
		mov rdx, n2
		cld
	loop_line:
		mov rcx, rax
		test rax, rax
		jz jmp_next
	loop_pixel:
		movq mm0, [rsi + 0]
		movq mm1, [rsi + 8]
		movq mm2, [rsi + 16]
		movq mm3, [rsi + 24]
		movq mm4, [rsi + 32]
		movq mm5, [rsi + 40]
		movq mm6, [rsi + 48]
		movq mm7, [rsi + 56]
		movq [rdi + 0], mm0
		movq [rdi + 8], mm1
		movq [rdi + 16], mm2
		movq [rdi + 24], mm3
		movq [rdi + 32], mm4
		movq [rdi + 40], mm5
		movq [rdi + 48], mm6
		movq [rdi + 56], mm7
		add rsi, 64
		add rdi, 64
		dec rcx
		jnz loop_pixel
	jmp_next:
		mov rcx, rbx
		rep movsd
		mov rcx, rdx
		rep movsb
		add rdi, c1
		add rsi, c2
	#endif
		dec hh
		jnz loop_line
		emms
	}
#else
	return -1;
#endif

	return 0;
}


//---------------------------------------------------------------------
// iblit_sse - mmx normal blitter
// this routine is design to support the platform with sse feature,
// which has more than 256KB L2 cache
//---------------------------------------------------------------------
int iblit_sse(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize)
{
	volatile ilong c1, c2, m, n1, n2;
	volatile ilong hh = (ilong)h;

	if (pixelbyte == 1) linesize = w;
	c1 = pitch1 - linesize;
	c2 = pitch2 - linesize;
	m = linesize >> 6;
	n1 = (linesize & 63) >> 2;
	n2 = (linesize & 63) & 3;

#if defined(__INLINEGNU__)
	{
	#ifndef __amd64__
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movl %3, %%eax\n"
		"	movl %4, %%ebx\n"
		"	movl %5, %%edx\n"
			mm_prefetch_esi_n(0)
		"	shll $3, %%eax\n"
		"	cld\n"
		".ssen_loop_line:\n"
			mm_prefetch_esi_n(32)
		"	movl %%eax, %%ecx\n"
		"	negl %%ecx\n"
		"	leal 0(%%esi, %%eax, 8), %%esi\n"
		"	leal 0(%%edi, %%eax, 8), %%edi\n"
		"	testl %%eax, %%eax\n"
		"	jz .ssen_next\n"
			mm_prefetch_esi_8cx_n(0)
		".ssen_loop_pixel:\n"
			mm_prefetch_esi_8cx_n(256)
		"	movq 0(%%esi, %%ecx, 8), %%mm0\n"
		"	movq 8(%%esi, %%ecx, 8), %%mm1\n"
		"	movq 16(%%esi, %%ecx, 8), %%mm2\n"
		"	movq 24(%%esi, %%ecx, 8), %%mm3\n"
		"	movq 32(%%esi, %%ecx, 8), %%mm4\n"
		"	movq 40(%%esi, %%ecx, 8), %%mm5\n"
		"	movq 48(%%esi, %%ecx, 8), %%mm6\n"
		"	movq 56(%%esi, %%ecx, 8), %%mm7\n"
			mm_prefetch_esi_8cx_n(128)
			mm_movntq_edi_8cx_0_m0
			mm_movntq_edi_8cx_8_m1
			mm_movntq_edi_8cx_16_m2
			mm_movntq_edi_8cx_24_m3
			mm_movntq_edi_8cx_32_m4
			mm_movntq_edi_8cx_40_m5
			mm_movntq_edi_8cx_48_m6
			mm_movntq_edi_8cx_56_m7
		"	addl $8, %%ecx\n"
			mm_prefetch_esi_8cx_n(64)
		"	jnz .ssen_loop_pixel\n"
		".ssen_next:\n"
			mm_prefetch_esi_n(0)
		"	movl %%ebx, %%ecx\n"
		"	rep movsl\n"
		"	movl %%edx, %%ecx\n"
		"	rep movsb\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
			mm_prefetch_esi_n(0)
		"	decl %0\n"
		"	jnz .ssen_loop_line\n"
			mm_sfence
		"	emms\n"
			ASM_ENDUP
		:"=m"(hh)
		:"m"(src),"m"(dst),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
		#else
		volatile iulong x1, x2, x3, x4, x5, x6;
		volatile const char *ss = src;
		volatile char *dd = dst;
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movq %%rax, %0\n"
		"	movq %%rbx, %1\n"
		"	movq %%rcx, %2\n"
		"	movq %%rdx, %3\n"
		"	movq %%rsi, %4\n"
		"	movq %%rdi, %5\n"
		"	mov %7, %%rsi\n"
		"	mov %8, %%rdi\n"
		"	mov %9, %%rax\n"
		"	mov %10, %%rbx\n"
		"	mov %11, %%rdx\n"
		"	prefetchnta (%%rsi)\n"
		"	shl $3, %%rax\n"
		"	cld\n"
		".ssen_loop_line:\n"
		"	prefetchnta 32(%%esi)\n"
		"	mov %%rax, %%rcx\n"
		"	neg %%rcx\n"
		"	lea 0(%%rsi, %%rax, 8), %%rsi\n"
		"	lea 0(%%rdi, %%rax, 8), %%rdi\n"
		"	test %%rax, %%rax\n"
		"	jz .ssen_next\n"
		"	prefetchnta (%%rsi, %%rcx, 8)\n"
		".ssen_loop_pixel:\n"
		"	prefetchnta 256(%%rsi, %%rcx, 8)\n"
		"	movq 0(%%rsi, %%rcx, 8), %%mm0\n"
		"	movq 8(%%rsi, %%rcx, 8), %%mm1\n"
		"	movq 16(%%rsi, %%rcx, 8), %%mm2\n"
		"	movq 24(%%rsi, %%rcx, 8), %%mm3\n"
		"	movq 32(%%rsi, %%rcx, 8), %%mm4\n"
		"	movq 40(%%rsi, %%rcx, 8), %%mm5\n"
		"	movq 48(%%rsi, %%rcx, 8), %%mm6\n"
		"	movq 56(%%rsi, %%rcx, 8), %%mm7\n"
		"	prefetchnta 128(%%rsi, %%rcx, 8)\n"
		"	movntq %%mm0, 0(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm1, 8(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm2, 16(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm3, 24(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm4, 32(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm5, 40(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm6, 48(%%rdi, %%rcx, 8)\n"
		"	movntq %%mm7, 56(%%rdi, %%rcx, 8)\n"
		"	add $8, %%rcx\n"
		"	prefetchnta 64(%%rsi, %%rcx, 8)\n"
		"	jnz .ssen_loop_pixel\n"
		".ssen_next:\n"
		"	prefetchnta 0(%%rsi)\n"
		"	mov %%rbx, %%rcx\n"
		"	rep movsl\n"
		"	mov %%rdx, %%rcx\n"
		"	rep movsb\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	prefetchnta 0(%%rsi)\n"
		"	decq %6\n"
		"	jnz .ssen_loop_line\n"
		"	sfence\n"
		"	emms\n"
		"	movq %0, %%rax\n"
		"	movq %1, %%rbx\n"
		"	movq %2, %%rcx\n"
		"	movq %3, %%rdx\n"
		"	movq %4, %%rsi\n"
		"	movq %5, %%rdi\n"
			ASM_ENDUP
		:"=m"(x1), "=m"(x2), "=m"(x3), "=m"(x4), "=m"(x5), "=m"(x6), "=m"(hh)
		:"m"(ss),"m"(dd),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
	#endif
	}
#elif defined(__INLINEMSC__)
	_asm {
	#ifndef __amd64__
		mov esi, src
		mov edi, dst
		mm_prefetch_esi_n(64)
		mm_prefetch_edi_n(64)
		mm_prefetch_esi
		mm_prefetch_edi
		mov eax, m
		shl eax, 3
		mov ebx, n1
		mov edx, n2
		cld
	ssen_loop_line:
		mm_prefetch_esi
		mm_prefetch_esi_n(64)
		mm_prefetch_esi_n(128)
		mm_prefetch_esi_n(192)
		mov ecx, eax
		neg ecx
		lea esi, [esi + eax * 8]
		lea edi, [edi + eax * 8]
		test eax, eax
		jz ssen_jmp_next
	ssen_loop_pixel:
		mm_prefetch_esi_8cx_n(256)
		mm_prefetch_esi_8cx_n(128)
		movq mm0, [esi + ecx * 8 +  0]
		movq mm1, [esi + ecx * 8 +  8]
		movq mm2, [esi + ecx * 8 + 16]
		movq mm3, [esi + ecx * 8 + 24]
		movq mm4, [esi + ecx * 8 + 32]
		movq mm5, [esi + ecx * 8 + 40]
		movq mm6, [esi + ecx * 8 + 48]
		movq mm7, [esi + ecx * 8 + 56]
		mm_movntq_edi_8cx_0_m0  
		mm_movntq_edi_8cx_8_m1  
		mm_movntq_edi_8cx_16_m2 
		mm_movntq_edi_8cx_24_m3 
		mm_movntq_edi_8cx_32_m4 
		mm_movntq_edi_8cx_40_m5 
		mm_movntq_edi_8cx_48_m6 
		mm_movntq_edi_8cx_56_m7 
		add ecx, 8
		jnz ssen_loop_pixel
	ssen_jmp_next:
		mm_prefetch_esi
		mov ecx, ebx
		rep movsd
		mov ecx, edx
		rep movsb
		add edi, c1
		add esi, c2
		mm_prefetch_esi
	#else
		mov rsi, src
		mov rdi, dst
		prefetchnta [rsi + 64]
		prefetchnta [rdi + 64]
		prefetchnta [rsi]
		prefetchnta [rdi]
		mov rax, m
		shl rax, 3
		mov rbx, n1
		mov rdx, n2
		cld
	ssen_loop_line:
		prefetchnta [rsi]
		prefetchnta [rsi + 64]
		prefetchnta [rsi + 128]
		prefetchnta [rsi + 192]
		mov rcx, rax
		neg rcx
		lea rsi, [rsi + rax * 8]
		lea rdi, [rdi + rax * 8]
		test rax, rax
		jz ssen_jmp_next
	ssen_loop_pixel:
		prefetchnta [rsi + rcx * 8 + 256]
		prefetchnta [rsi + rcx * 8 + 128]
		movq mm0, [rsi + rcx * 8 +  0]
		movq mm1, [rsi + rcx * 8 +  8]
		movq mm2, [rsi + rcx * 8 + 16]
		movq mm3, [rsi + rcx * 8 + 24]
		movq mm4, [rsi + rcx * 8 + 32]
		movq mm5, [rsi + rcx * 8 + 40]
		movq mm6, [rsi + rcx * 8 + 48]
		movq mm7, [rsi + rcx * 8 + 56]
		movntq [rdi + rcx * 8 +  0], mm0
		movntq [rdi + rcx * 8 +  8], mm1
		movntq [rdi + rcx * 8 + 16], mm2
		movntq [rdi + rcx * 8 + 24], mm3
		movntq [rdi + rcx * 8 + 32], mm4
		movntq [rdi + rcx * 8 + 40], mm5
		movntq [rdi + rcx * 8 + 48], mm6
		movntq [rdi + rcx * 8 + 56], mm7
		add rcx, 8
		jnz ssen_loop_pixel
	ssen_jmp_next:
		prefetchnta [rsi]
		mov rcx, rbx
		rep movsd
		mov rcx, rdx
		rep movsb
		add rdi, c1
		add rsi, c2
		prefetchnta [rsi]
	#endif
		dec hh
		jnz ssen_loop_line
		mm_sfence
		emms
	}
	#else
	return -1;
	#endif

	return 0;
}


//---------------------------------------------------------------------
// iblit_mix - mix normal blitter
// this routine is design to support the platform with MMX feature
//---------------------------------------------------------------------
int iblit_mix(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize)
{
	volatile ilong c1, c2, m, n1, n2;
	volatile ilong hh = (ilong)h;

	if (pixelbyte == 1) linesize = w;

	c1 = pitch1 - linesize;
	c2 = pitch2 - linesize;
	m = linesize >> 6;
	n1 = (linesize & 63) >> 2;
	n2 = (linesize & 63) & 3;

#if defined(__INLINEGNU__)
	{
	#ifndef __amd64__
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movl %3, %%eax\n"
		"	movl %4, %%ebx\n"
		"	movl %5, %%edx\n"
		"	cld\n"
		".mixn_loopline:\n"
		"	movl %%eax, %%ecx\n"
		"	testl %%eax, %%eax\n"
		"	jz .mixn_last\n"
		".mixn_looppixel:\n"
			mm_prefetch_esi_n(512)
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm1\n"
		"	movq 16(%%esi), %%mm2\n"
		"	movq 24(%%esi), %%mm3\n"
		"	movq 32(%%esi), %%mm4\n"
		"	movq 40(%%esi), %%mm5\n"
		"	movq 48(%%esi), %%mm6\n"
		"	movq 56(%%esi), %%mm7\n"
		"	movq %%mm0, 0(%%edi)\n"
		"	movq %%mm1, 8(%%edi)\n"
		"	movq %%mm2, 16(%%edi)\n"
		"	movq %%mm3, 24(%%edi)\n"
		"	movq %%mm4, 32(%%edi)\n"
		"	movq %%mm5, 40(%%edi)\n"
		"	movq %%mm6, 48(%%edi)\n"
		"	movq %%mm7, 56(%%edi)\n"
		"	addl $64, %%esi\n"
		"	addl $64, %%edi\n"
		"	decl %%ecx\n"
		"	jnz .mixn_looppixel\n"
		".mixn_last:\n"
		"	movl %%ebx, %%ecx\n"
		"	rep movsl\n"
		"	movl %%edx, %%ecx\n"
		"	rep movsb\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
			mm_prefetch_esi_n(64)
		"	decl %0\n"
		"	jnz .mixn_loopline\n"
		"	emms\n"
			ASM_ENDUP
		:"=m"(hh)
		:"m"(src),"m"(dst),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
		#else
		volatile iulong x1, x2, x3, x4, x5, x6;
		volatile const char *ss = src;
		volatile char *dd = dst;
		__asm__ __volatile__ ( "\n"
			ASM_BEGIN
		"	movq %%rax, %0\n"
		"	movq %%rbx, %1\n"
		"	movq %%rcx, %2\n"
		"	movq %%rdx, %3\n"
		"	movq %%rsi, %4\n"
		"	movq %%rdi, %5\n"
		"	mov %7, %%rsi\n"
		"	mov %8, %%rdi\n"
		"	mov %9, %%rax\n"
		"	mov %10, %%rbx\n"
		"	mov %11, %%rdx\n"
		"	cld\n"
		".mixn_loopline:\n"
		"	mov %%rax, %%rcx\n"
		"	test %%rax, %%rax\n"
		"	jz .mixn_last\n"
		".mixn_looppixel:\n"
		"	prefetchnta 512(%%rsi)\n"
		"	movq 0(%%rsi), %%mm0\n"
		"	movq 8(%%rsi), %%mm1\n"
		"	movq 16(%%rsi), %%mm2\n"
		"	movq 24(%%rsi), %%mm3\n"
		"	movq 32(%%rsi), %%mm4\n"
		"	movq 40(%%rsi), %%mm5\n"
		"	movq 48(%%rsi), %%mm6\n"
		"	movq 56(%%rsi), %%mm7\n"
		"	movq %%mm0, 0(%%rdi)\n"
		"	movq %%mm1, 8(%%rdi)\n"
		"	movq %%mm2, 16(%%rdi)\n"
		"	movq %%mm3, 24(%%rdi)\n"
		"	movq %%mm4, 32(%%rdi)\n"
		"	movq %%mm5, 40(%%rdi)\n"
		"	movq %%mm6, 48(%%rdi)\n"
		"	movq %%mm7, 56(%%rdi)\n"
		"	add $64, %%rsi\n"
		"	add $64, %%rdi\n"
		"	dec %%rcx\n"
		"	jnz .mixn_looppixel\n"
		".mixn_last:\n"
		"	mov %%rbx, %%rcx\n"
		"	rep movsl\n"
		"	mov %%rdx, %%rcx\n"
		"	rep movsb\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	prefetchnta 64(%%rsi)\n"
		"	decq %6\n"
		"	jnz .mixn_loopline\n"
		"	emms\n"
		"	movq %0, %%rax\n"
		"	movq %1, %%rbx\n"
		"	movq %2, %%rcx\n"
		"	movq %3, %%rdx\n"
		"	movq %4, %%rsi\n"
		"	movq %5, %%rdi\n"
			ASM_ENDUP
		:"=m"(x1), "=m"(x2), "=m"(x3), "=m"(x4), "=m"(x5), "=m"(x6), "=m"(hh)
		:"m"(ss),"m"(dd),"m"(m),"m"(n1),"m"(n2),"m"(c1),"m"(c2)
		:"memory", ASM_REGS);
		#endif
	}
#elif defined(__INLINEMSC__)
	_asm {
	#ifndef __amd64__
		mov esi, src
		mov edi, dst
		mov eax, m
		mov ebx, n1
		mov edx, n2
		cld
	mixn_loop_line:
		mov ecx, eax
		test eax, eax
		jz mixn_jmp_next
	mixn_loop_pixel:
		mm_prefetch_esi_n(512)
		movq mm0, [esi + 0]
		movq mm1, [esi + 8]
		movq mm2, [esi + 16]
		movq mm3, [esi + 24]
		movq mm4, [esi + 32]
		movq mm5, [esi + 40]
		movq mm6, [esi + 48]
		movq mm7, [esi + 56]
		movq [edi + 0], mm0
		movq [edi + 8], mm1
		movq [edi + 16], mm2
		movq [edi + 24], mm3
		movq [edi + 32], mm4
		movq [edi + 40], mm5
		movq [edi + 48], mm6
		movq [edi + 56], mm7
		add esi, 64
		add edi, 64
		dec ecx
		jnz mixn_loop_pixel
	mixn_jmp_next:
		mov ecx, ebx
		rep movsd
		mov ecx, edx
		rep movsb
		add edi, c1
		add esi, c2
		mm_prefetch_esi_n(64)
	#else
		mov rsi, src
		mov rdi, dst
		mov rax, m
		mov rbx, n1
		mov rdx, n2
		cld
	mixn_loop_line:
		mov rcx, rax
		test rax, rax
		jz mixn_jmp_next
	mixn_loop_pixel:
		prefetchnta [rsi + 512]
		movq mm0, [rsi + 0]
		movq mm1, [rsi + 8]
		movq mm2, [rsi + 16]
		movq mm3, [rsi + 24]
		movq mm4, [rsi + 32]
		movq mm5, [rsi + 40]
		movq mm6, [rsi + 48]
		movq mm7, [rsi + 56]
		movq [rdi + 0], mm0
		movq [rdi + 8], mm1
		movq [rdi + 16], mm2
		movq [rdi + 24], mm3
		movq [rdi + 32], mm4
		movq [rdi + 40], mm5
		movq [rdi + 48], mm6
		movq [rdi + 56], mm7
		add rsi, 64
		add rdi, 64
		dec rcx
		jnz mixn_loop_pixel
	mixn_jmp_next:
		mov rcx, rbx
		rep movsd
		mov rcx, rdx
		rep movsb
		add rdi, c1
		add rsi, c2
		prefetchnta [rsi + 64]
	#endif
		dec hh
		jnz mixn_loop_line
		emms
	}
#else
	return -1;
#endif

	return 0;
}



//---------------------------------------------------------------------
// iblit_mask_mmx - mmx mask blitter 
// this routine is designed to support mmx mask blit
//---------------------------------------------------------------------
int iblit_mask_mmx(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize, unsigned long ckey)
{
	volatile ilong linebytes = linesize;
	volatile ilong c1, c2, m, n, pb;
	volatile ilong hh = h;
	volatile iulong mask;

	switch (pixelbyte) {
	case 1:
		m = w >> 4;
		n = w & 15;
		mask = (ckey | (ckey << 8)) & 0xffff;
		mask = mask | (mask << 16);
		break;
	case 2: 
		linebytes = (((long)w) << 1); 
		m = w >> 3;
		n = w & 7;
		mask = ckey | (ckey << 16);
		break;
	case 3: 
		linebytes = (((long)w) << 1) + w; 
		m = 0;
		n = w;
		mask = ckey;
		break;
	case 4: 
		linebytes = (((long)w) << 2); 
		m = w >> 2;
		n = w & 3;
		mask = ckey;
		break;
	default: 
		linebytes = -1; 
		break;
	}

	if (linebytes < 0) return -1;

	c1 = pitch1 - linebytes;
	c2 = pitch2 - linebytes;
	pb = pixelbyte;

#if defined(__INLINEGNU__)
	{
	#ifndef __amd64__
		__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movd %3, %%mm7\n"
		"	movd %3, %%mm6\n"
		"	movl %3, %%ebx\n"
		"	psllq $32, %%mm7\n"
		"	por %%mm6, %%mm7\n"
		"	cld\n"
		"	movl %8, %%eax\n"
		"	cmpl $2, %%eax\n"
		"	jz bmaskmmx16_loopline\n"
		"	cmpl $4, %%eax\n"
		"	jz bmaskmmx32_loopline\n"
		"	cmpl $1, %%eax\n"
		"	jz bmaskmmx8_loopline\n"
		"	jmp bmaskmmx24_loopline\n"
		__ALIGNC__		/* bmaskmmx8_loopline */
		"bmaskmmx8_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx8_last\n"
		"bmaskmmx8_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqb %%mm7, %%mm0\n"
		"	pcmpeqb %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx8_looppixel\n"
		"bmaskmmx8_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx8_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx8_l2:\n"
		"	lodsb\n"
		"	cmpb %%al, %%bl\n"
		"	jz bmaskmmx8_l3\n"
		"	movb %%al, (%%edi)\n"
		"bmaskmmx8_l3:\n"
		"	incl %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx8_l2\n"
		"bmaskmmx8_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmmx8_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx16_loopline */
		"bmaskmmx16_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx16_last\n"
		"bmaskmmx16_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqw %%mm7, %%mm0\n"
		"	pcmpeqw %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx16_looppixel\n"
		"bmaskmmx16_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx16_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx16_l2:\n"
		"	lodsw\n"
		"	cmpw %%ax, %%bx\n"
		"	jz bmaskmmx16_l3\n"
		"	movw %%ax, (%%edi)\n"
		"bmaskmmx16_l3:\n"
		"	addl $2, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx16_l2\n"
		"bmaskmmx16_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmmx16_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx32_loopline */
		"bmaskmmx32_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx32_last\n"
		"bmaskmmx32_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqd %%mm7, %%mm0\n"
		"	pcmpeqd %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx32_looppixel\n"
		"bmaskmmx32_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx32_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx32_l2:\n"
		"	lodsl\n"
		"	cmpl %%eax, %%ebx\n"
		"	jz bmaskmmx32_l3\n"
		"	movl %%eax, (%%edi)\n"
		"bmaskmmx32_l3:\n"
		"	addl $4, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx32_l2\n"
		"bmaskmmx32_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmmx32_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx24_loopline */
		"bmaskmmx24_loopline:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmmx24_linend\n"
		"	andl $0xffffff, %%ebx\n"
		"bmaskmmx24_looppixel:\n"
		"	movb 2(%%esi), %%al\n"
		"	movw 0(%%esi), %%dx\n"
		"	shll $16, %%eax\n"
		"	addl $3, %%esi\n"
		"	movw %%dx, %%ax\n"
		"	andl $0xffffff, %%eax\n"
		"	cmpl %%ebx, %%eax\n"
		"	jz bmaskmmx24_skip\n"
		"	movw %%ax, (%%edi)\n"
		"	shrl $16, %%eax\n"
		"	movb %%al, 2(%%edi)\n"
		"bmaskmmx24_skip:\n"
		"	addl $3, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmmx24_looppixel\n"
		"bmaskmmx24_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmmx24_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx_endup */
		"bmaskmmx_endup:\n"
		"	emms\n"
			ASM_ENDUP
		:"=m"(hh)
		:"m"(src),"m"(dst),"m"(mask),"m"(m),"m"(n),"m"(c1),"m"(c2),"m"(pb)
		:"memory", ASM_REGS);
	#else
		volatile iulong x1, x2, x3, x4, x5, x6;
		volatile const char *ss = src;
		volatile char *dd = dst;
		__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movq %%rax, %0\n"
		"	movq %%rbx, %1\n"
		"	movq %%rcx, %2\n"
		"	movq %%rdx, %3\n"
		"	movq %%rsi, %4\n"
		"	movq %%rdi, %5\n"
		"	mov %7, %%rsi\n"
		"	mov %8, %%rdi\n"
		"	movq %9, %%mm7\n"
		"	movq %9, %%mm6\n"
		"	mov %9, %%rbx\n"
		"	psllq $32, %%mm7\n"
		"	por %%mm6, %%mm7\n"
		"	cld\n"
		"	mov %14, %%rax\n"
		"	cmp $2, %%eax\n"
		"	jz bmaskmmx16_loopline\n"
		"	cmp $4, %%eax\n"
		"	jz bmaskmmx32_loopline\n"
		"	cmp $1, %%eax\n"
		"	jz bmaskmmx8_loopline\n"
		"	jmp bmaskmmx24_loopline\n"
		__ALIGNC__		/* bmaskmmx8_loopline */
		"bmaskmmx8_loopline:\n"
		"	mov %10, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx8_last\n"
		"bmaskmmx8_looppixel:\n"
		"	movq 0(%%rsi), %%mm0\n"
		"	movq 8(%%rsi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqb %%mm7, %%mm0\n"
		"	pcmpeqb %%mm7, %%mm4\n"
		"	add $16, %%rsi\n"
		"	add $16, %%rdi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%rdi), %%mm2\n"
		"	pand -8(%%rdi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%rdi)\n"
		"	movq %%mm6, -8(%%rdi)\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx8_looppixel\n"
		"bmaskmmx8_last:\n"
		"	mov %11, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx8_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx8_l2:\n"
		"	lodsb\n"
		"	cmpb %%al, %%bl\n"
		"	jz bmaskmmx8_l3\n"
		"	movb %%al, (%%rdi)\n"
		"bmaskmmx8_l3:\n"
		"	inc %%rdi\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx8_l2\n"
		"bmaskmmx8_linend:\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	decq %6\n"
		"	jnz bmaskmmx8_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx16_loopline */
		"bmaskmmx16_loopline:\n"
		"	mov %10, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx16_last\n"
		"bmaskmmx16_looppixel:\n"
		"	movq 0(%%rsi), %%mm0\n"
		"	movq 8(%%rsi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqw %%mm7, %%mm0\n"
		"	pcmpeqw %%mm7, %%mm4\n"
		"	add $16, %%rsi\n"
		"	add $16, %%rdi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%rdi), %%mm2\n"
		"	pand -8(%%rdi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%rdi)\n"
		"	movq %%mm6, -8(%%rdi)\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx16_looppixel\n"
		"bmaskmmx16_last:\n"
		"	mov %11, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx16_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx16_l2:\n"
		"	lodsw\n"
		"	cmpw %%ax, %%bx\n"
		"	jz bmaskmmx16_l3\n"
		"	movw %%ax, (%%rdi)\n"
		"bmaskmmx16_l3:\n"
		"	add $2, %%rdi\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx16_l2\n"
		"bmaskmmx16_linend:\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	decq %6\n"
		"	jnz bmaskmmx16_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx32_loopline */
		"bmaskmmx32_loopline:\n"
		"	mov %10, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx32_last\n"
		"bmaskmmx32_looppixel:\n"
		"	movq 0(%%rsi), %%mm0\n"
		"	movq 8(%%rsi), %%mm4\n"
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqd %%mm7, %%mm0\n"
		"	pcmpeqd %%mm7, %%mm4\n"
		"	add $16, %%rsi\n"
		"	add $16, %%rdi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%rdi), %%mm2\n"
		"	pand -8(%%rdi), %%mm6\n"
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%rdi)\n"
		"	movq %%mm6, -8(%%rdi)\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx32_looppixel\n"
		"bmaskmmx32_last:\n"
		"	mov %11, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx32_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmmx32_l2:\n"
		"	lodsl\n"
		"	cmpl %%eax, %%ebx\n"
		"	jz bmaskmmx32_l3\n"
		"	movl %%eax, (%%rdi)\n"
		"bmaskmmx32_l3:\n"
		"	add $4, %%rdi\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx32_l2\n"
		"bmaskmmx32_linend:\n"
		"	addq %12, %%rdi\n"
		"	addq %13, %%rsi\n"
		"	decq %6\n"
		"	jnz bmaskmmx32_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx24_loopline */
		"bmaskmmx24_loopline:\n"
		"	mov %11, %%rcx\n"
		"	test %%rcx, %%rcx\n"
		"	jz bmaskmmx24_linend\n"
		"	and $0xffffff, %%rbx\n"
		"bmaskmmx24_looppixel:\n"
		"	movb 2(%%rsi), %%al\n"
		"	movw 0(%%rsi), %%dx\n"
		"	shll $16, %%eax\n"
		"	add $3, %%rsi\n"
		"	movw %%dx, %%ax\n"
		"	and $0xffffff, %%eax\n"
		"	cmpl %%ebx, %%eax\n"
		"	jz bmaskmmx24_skip\n"
		"	movw %%ax, (%%rdi)\n"
		"	shrl $16, %%eax\n"
		"	movb %%al, 2(%%rdi)\n"
		"bmaskmmx24_skip:\n"
		"	add $3, %%rdi\n"
		"	dec %%rcx\n"
		"	jnz bmaskmmx24_looppixel\n"
		"bmaskmmx24_linend:\n"
		"	add %12, %%rdi\n"
		"	add %13, %%rsi\n"
		"	decq %6\n"
		"	jnz bmaskmmx24_loopline\n"
		"	jmp bmaskmmx_endup\n"
		__ALIGNC__		/* bmaskmmx_endup */
		"bmaskmmx_endup:\n"
		"	emms\n"
		"	movq %0, %%rax\n"
		"	movq %1, %%rbx\n"
		"	movq %2, %%rcx\n"
		"	movq %3, %%rdx\n"
		"	movq %4, %%rsi\n"
		"	movq %5, %%rdi\n"
			ASM_ENDUP
		:"=m"(x1), "=m"(x2), "=m"(x3), "=m"(x4), "=m"(x5), "=m"(x6), "=m"(hh)
		:"m"(ss),"m"(dd),"m"(mask),"m"(m),"m"(n),"m"(c1),"m"(c2),"m"(pb)
		:"memory", ASM_REGS);
	#endif
	}
#elif defined(__INLINEMSC__) && (!defined(__amd64__))
	_asm {
		mov esi, src
		mov edi, dst
		movd mm7, mask
		movd mm6, mask
		mov ebx, mask
		psllq mm7, 32
		por mm7, mm6
		cld
		mov eax, pb
		cmp eax, 1
		jnz bmaskmmx16_check
	__ALIGNC__		/* bmaskmmx8_loopline */
	bmaskmmx8_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmmx8_last
	bmaskmmx8_looppixel:
		movq mm0, [esi + 0]
		movq mm4, [esi + 8]
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqb mm0, mm7
		pcmpeqb mm4, mm7
		add esi, 16
		add edi, 16
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmmx8_looppixel
	bmaskmmx8_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmmx8_linend
	bmaskmmx8_l2:
		lodsb
		cmp al, bl
		jz bmaskmmx8_l3
		mov [edi], al
	bmaskmmx8_l3:
		inc edi
		dec ecx
		jnz bmaskmmx8_l2
	bmaskmmx8_linend:
		add edi, c1
		add esi, c2
		dec hh
		jnz bmaskmmx8_loopline
	bmaskmmx16_check:
		mov eax, pb
		cmp eax, 2
		jnz bmaskmmx32_check
	__ALIGNC__		/* bmaskmmx16_loopline */
	bmaskmmx16_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmmx16_last
	bmaskmmx16_looppixel:
		movq mm0, [esi + 0]
		movq mm4, [esi + 8]
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqw mm0, mm7
		pcmpeqw mm4, mm7
		add esi, 16
		add edi, 16
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmmx16_looppixel
	bmaskmmx16_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmmx16_linend
	bmaskmmx16_l2:
		lodsw
		cmp ax, bx
		jz bmaskmmx16_l3
		mov [edi], ax
	bmaskmmx16_l3:
		add edi, 2
		dec ecx
		jnz bmaskmmx16_l2
	bmaskmmx16_linend:
		add edi, c1
		add esi, c2
		dec hh
		jnz bmaskmmx16_loopline
	bmaskmmx32_check:
		mov eax, pb
		cmp eax, 4
		jnz bmaskmmx24_check
	__ALIGNC__		/* bmaskmmx32_loopline */
	bmaskmmx32_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmmx32_last
	bmaskmmx32_looppixel:
		movq mm0, [esi + 0]
		movq mm4, [esi + 8]
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqd mm0, mm7
		pcmpeqd mm4, mm7
		add esi, 16
		add edi, 16
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmmx32_looppixel
	bmaskmmx32_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmmx32_linend
	bmaskmmx32_l2:
		lodsd
		cmp eax, ebx
		jz bmaskmmx32_l3
		mov [edi], eax
	bmaskmmx32_l3:
		add edi, 4
		dec ecx
		jnz bmaskmmx32_l2
	bmaskmmx32_linend:
		add edi, c1
		add esi, c2
		dec hh
		jnz bmaskmmx32_loopline
		jmp bmaskmmx_endup
	bmaskmmx24_check:
		mov eax, pb
		cmp eax, 3
		jnz bmaskmmx_endup
	__ALIGNC__		/* maskmmx24_loopline */
	bmaskmmx24_loopline:
		mov ecx, n
		test ecx, ecx
		jz bmaskmmx24_linend
		and ebx, 0xffffff
	bmaskmmx24_looppixel:
		lodsd
		dec esi
		and eax, 0xffffff
		cmp eax, ebx
		jz bmaskmmx24_skip
		mov [edi], ax
		shr eax, 16
		mov [edi + 2], al
	bmaskmmx24_skip:
		add edi, 3
		dec ecx
		jnz bmaskmmx24_looppixel
	bmaskmmx24_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmaskmmx24_loopline
	__ALIGNC__
	bmaskmmx_endup:
		emms
	}
#else
	return -1;
#endif
	return 0;
}


//---------------------------------------------------------------------
// iblit_mask_sse - sse mask blitter 
// this routine is designed to support mmx mask blit
//---------------------------------------------------------------------
int iblit_mask_sse(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize, unsigned long ckey)
{
	long linebytes = linesize;
	long c1, c2, m, n, pb;
	unsigned long mask;

	switch (pixelbyte) {
	case 1:
		m = w >> 4;
		n = w & 15;
		mask = (ckey | (ckey << 8)) & 0xffff;
		mask = mask | (mask << 16);
		break;
	case 2: 
		linebytes = (((long)w) << 1); 
		m = w >> 3;
		n = w & 7;
		mask = ckey | (ckey << 16);
		break;
	case 3: 
		linebytes = (((long)w) << 1) + w; 
		m = 0;
		n = w;
		mask = ckey;
		break;
	case 4: 
		linebytes = (((long)w) << 2); 
		m = w >> 2;
		n = w & 3;
		mask = ckey;
		break;
	default: 
		linebytes = -1; 
		break;
	}

	if (linebytes < 0) return -1;

	c1 = pitch1 - linebytes;
	c2 = pitch2 - linebytes;
	pb = pixelbyte;

#if defined(__INLINEGNU__) && (!defined(__amd64__))
	__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movd %3, %%mm7\n"
		"	movd %3, %%mm6\n"
		"	movl %3, %%ebx\n"
		"	psllq $32, %%mm7\n"
		"	por %%mm6, %%mm7\n"
		"	pcmpeqw %%mm6, %%mm6\n"
		"	movl %8, %%eax\n"
		"	cmpl $2, %%eax\n"
		"	jz bmasksse16_loopline\n"
		"	cmpl $4, %%eax\n"
		"	jz bmasksse32_loopline\n"
		"	cmpl $1, %%eax\n"
		"	jz bmasksse8_loopline\n"
		"	jmp bmasksse24_loopline\n"
		"	cld\n"
		__ALIGNC__		/* bmasksse8_loopline */
		"bmasksse8_loopline:\n"
		"	movl %4, %%ecx\n"
			mm_prefetch_esi_n(32)
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse8_last\n"
		__ALIGNC__
		"bmasksse8_looppixel:\n"
			mm_prefetch_esi_n(512)
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm1\n"
		"	movq %%mm7, %%mm2\n"
		"	movq %%mm7, %%mm3\n"
			mm_prefetch_esi_n(128)
		"	pcmpeqb %%mm0, %%mm2\n"
		"	pcmpeqb %%mm1, %%mm3\n"
		"	addl $16, %%esi\n"
		"	pxor %%mm6, %%mm2\n"
		"	pxor %%mm6, %%mm3\n"
			mm_prefetch_esi_n(96)
			mm_maskmovq(0, 2)
		"	addl $8, %%edi\n"
			mm_prefetch_esi_n(64)
			mm_maskmovq(1, 3)
		"	addl $8, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse8_looppixel\n"
		"bmasksse8_last:\n"
			mm_prefetch_esi_n(0)
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse8_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmasksse8_l1:\n"
		"	lodsb\n"
		"	cmpb %%bl, %%al\n"
		"	jz bmasksse8_l2\n"
		"	movb %%al, (%%edi)\n"
		"bmasksse8_l2:\n"
		"	incl %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse8_l1\n"
		"bmasksse8_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
			mm_prefetch_esi_n(0)
		"	decl %0\n"
		"	jnz bmasksse8_loopline\n"
		"	jmp bmasksse_endup\n"
		__ALIGNC__		/* bmasksse16_loopline */
		"bmasksse16_loopline:\n"
		"	movl %4, %%ecx\n"
			mm_prefetch_esi_n(32)
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse16_last\n"
		__ALIGNC__
		"bmasksse16_looppixel:\n"
			mm_prefetch_esi_n(160)
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm1\n"
		"	movq %%mm7, %%mm2\n"
		"	movq %%mm7, %%mm3\n"
			mm_prefetch_esi_n(128)
		"	pcmpeqw %%mm0, %%mm2\n"
		"	pcmpeqw %%mm1, %%mm3\n"
		"	addl $16, %%esi\n"
		"	pxor %%mm6, %%mm2\n"
		"	pxor %%mm6, %%mm3\n"
			mm_prefetch_esi_n(96)
			mm_maskmovq(0, 2)
		"	addl $8, %%edi\n"
			mm_prefetch_esi_n(64)
			mm_maskmovq(1, 3)
		"	addl $8, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse16_looppixel\n"
		"bmasksse16_last:\n"
			mm_prefetch_esi_n(0)
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse16_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmasksse16_l1:\n"
		"	lodsw\n"
		"	cmpw %%bx, %%ax\n"
		"	jz bmasksse16_l2\n"
		"	movw %%ax, (%%edi)\n"
		"bmasksse16_l2:\n"
		"	addl $2, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse16_l1\n"
		"bmasksse16_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
			mm_prefetch_esi_n(0)
		"	decl %0\n"
		"	jnz bmasksse16_loopline\n"
		"	jmp bmasksse_endup\n"
		__ALIGNC__		/* bmasksse32_loopline */
		"bmasksse32_loopline:\n"
		"	movl %4, %%ecx\n"
			mm_prefetch_esi_n(32)
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse32_last\n"
		__ALIGNC__
		"bmasksse32_looppixel:\n"
			mm_prefetch_esi_n(160)
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm1\n"
		"	movq %%mm7, %%mm2\n"
		"	movq %%mm7, %%mm3\n"
			mm_prefetch_esi_n(128)
		"	pcmpeqd %%mm0, %%mm2\n"
		"	pcmpeqd %%mm1, %%mm3\n"
		"	addl $16, %%esi\n"
		"	pxor %%mm6, %%mm2\n"
		"	pxor %%mm6, %%mm3\n"
			mm_prefetch_esi_n(96)
			mm_maskmovq(0, 2)
		"	addl $8, %%edi\n"
			mm_prefetch_esi_n(64)
			mm_maskmovq(1, 3)
		"	addl $8, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse32_looppixel\n"
		__ALIGNC__
		"bmasksse32_last:\n"
			mm_prefetch_esi_n(0)
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse32_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmasksse32_l1:\n"
		"	lodsl\n"
		"	cmpl %%ebx, %%eax\n"
		"	jz bmasksse32_l2\n"
		"	movl %%eax, (%%edi)\n"
		"bmasksse32_l2:\n"
		"	addl $4, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse32_l1\n"
		"bmasksse32_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
			mm_prefetch_esi_n(0)
		"	decl %0\n"
		"	jnz bmasksse32_loopline\n"
		"	jmp bmasksse_endup\n"
		__ALIGNC__		/* bmasksse24_loopline */
		"bmasksse24_loopline:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmasksse24_linend\n"
		"	andl $0xffffff, %%ebx\n"
		"bmasksse24_looppixel:\n"
		"	movb 2(%%esi), %%al\n"
		"	movw 0(%%esi), %%dx\n"
		"	shll $16, %%eax\n"
		"	addl $3, %%esi\n"
		"	movw %%dx, %%ax\n"
			mm_prefetch_esi_n(64)
		"	andl $0xffffff, %%eax\n"
		"	cmpl %%ebx, %%eax\n"
		"	jz bmasksse24_skip\n"
		"	movw %%ax, (%%edi)\n"
		"	shrl $16, %%eax\n"
		"	movb %%al, 2(%%edi)\n"
		"bmasksse24_skip:\n"
		"	addl $3, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmasksse24_looppixel\n"
		"bmasksse24_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmasksse24_loopline\n"
		"	jmp bmasksse_endup\n"
		"bmasksse_endup:\n"
			mm_sfence
		"	emms\n"
			ASM_ENDUP
		:"=m"(h)
		:"m"(src),"m"(dst),"m"(mask),"m"(m),"m"(n),"m"(c1),"m"(c2),"m"(pb)
		:"memory", ASM_REGS);

#elif defined(__INLINEMSC__) && (!defined(__amd64__))
	_asm {
		mov esi, src
		mov edi, dst
		mov ebx, mask
		movd mm7, mask
		movd mm6, mask
		psllq mm7, 32
		por mm7, mm6
		movq mm6, mm7
		pcmpeqw mm6, mm6
		mov edx, 8
		cld
		mov eax, pb
		cmp eax, 1
		jnz bmasksse16_check
	__ALIGNC__			/* bmasksse16_loopline */
	bmasksse8_loopline:	
		mov ecx, m
		test ecx, ecx
		jz bmasksse8_last
	__ALIGNC__
	bmasksse8_looppixel:
		mm_prefetch_esi_n(190)
		movq mm0, [esi + 0]
		movq mm1, [esi + 8]
		movq mm2, mm7
		movq mm3, mm7
		mm_prefetch_esi_n(128)
		pcmpeqb mm2, mm0
		pcmpeqb mm3, mm1
		add esi, 16
		pxor mm2, mm6
		pxor mm3, mm6
		mm_maskmovq_0_2
		add edi, edx
		mm_prefetch_esi_n(64)
		mm_maskmovq_1_3
		add edi, edx
		dec ecx
		jnz bmasksse8_looppixel
	bmasksse8_last:
		mov ecx, n
		test ecx, ecx
		jz bmasksse8_linend
		movd ebx, mm7
	bmasksse8_l1:
		lodsb
		cmp al, bl
		jz bmasksse8_l2
		mov [edi], al
	bmasksse8_l2:
		inc edi
		dec ecx
		jnz bmasksse8_l1
	bmasksse8_linend:
		add edi, c1
		add esi, c2
		mm_prefetch_esi_n(0)
		dec h
		jnz bmasksse8_loopline
	bmasksse16_check:
		mov eax, pb
		cmp al, 2
		jnz bmasksse32_check
	__ALIGNC__			/* bmasksse16_loopline */
	bmasksse16_loopline:	
		mov ecx, m
		test ecx, ecx
		jz bmasksse16_last
	__ALIGNC__
	bmasksse16_looppixel:
		mm_prefetch_esi_n(190)
		movq mm0, [esi + 0]
		movq mm1, [esi + 8]
		movq mm2, mm7
		movq mm3, mm7
		pcmpeqw mm2, mm0
		pcmpeqw mm3, mm1
		add esi, 16
		pxor mm2, mm6
		pxor mm3, mm6
		mm_maskmovq_0_2
		add edi, edx
		mm_prefetch_esi_n(64)
		mm_maskmovq_1_3
		add edi, edx
		dec ecx
		jnz bmasksse16_looppixel
	bmasksse16_last:
		mov ecx, n
		test ecx, ecx
		jz bmasksse16_linend
		movd ebx, mm7
	bmasksse16_l1:
		lodsw
		cmp ax, bx
		jz bmasksse16_l2
		mov [edi], ax
	bmasksse16_l2:
		add edi, 2
		dec ecx
		jnz bmasksse16_l1
	bmasksse16_linend:
		add edi, c1
		add esi, c2
		mm_prefetch_esi_n(0)
		dec h
		jnz bmasksse16_loopline
	bmasksse32_check:
		mov eax, pb
		cmp eax, 4
		jnz bmasksse24_check
	__ALIGNC__			/* bmasksse32_loopline */
	bmasksse32_loopline:	
		mov ecx, m
		test ecx, ecx
		jz bmasksse32_last
	__ALIGNC__
	bmasksse32_looppixel:
		mm_prefetch_esi_n(190)
		movq mm0, [esi + 0]
		movq mm1, [esi + 8]
		movq mm2, mm7
		movq mm3, mm7
		pcmpeqd mm2, mm0
		pcmpeqd mm3, mm1
		add esi, 16
		pxor mm2, mm6
		pxor mm3, mm6
		mm_maskmovq_0_2
		add edi, edx
		mm_prefetch_esi_n(64)
		mm_maskmovq_1_3
		add edi, edx
		dec ecx
		jnz bmasksse32_looppixel
	bmasksse32_last:
		mov ecx, n
		test ecx, ecx
		jz bmasksse32_linend
		movd ebx, mm7
	bmasksse32_l1:
		lodsd
		cmp eax, ebx
		jz bmasksse32_l2
		mov [edi], eax
	bmasksse32_l2:
		add edi, 4
		dec ecx
		jnz bmasksse32_l1
	bmasksse32_linend:
		add edi, c1
		add esi, c2
		mm_prefetch_esi_n(0)
		dec h
		jnz bmasksse32_loopline
	bmasksse24_check:
		mov eax, pb
		cmp eax, 3
		jnz bmasksse_endup
	__ALIGNC__		/* masksse24_loopline */
	bmasksse24_loopline:
		mov ecx, n
		test ecx, ecx
		jz bmasksse24_linend
		and ebx, 0xffffff
	__ALIGNC__
	bmasksse24_looppixel:
		lodsd
		dec esi
		and eax, 0xffffff
		mm_prefetch_esi_n(0)
		cmp eax, ebx
		jz bmasksse24_skip
		mov [edi], ax
		shr eax, 16
		mov [edi + 2], al
	bmasksse24_skip:
		add edi, 3
		dec ecx
		jnz bmasksse24_looppixel
	bmasksse24_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmasksse24_loopline
	bmasksse_endup:
		mm_sfence
		emms
	}
#else
	return -1;
#endif
	return 0;
}


//---------------------------------------------------------------------
// iblit_mask_mix - mmx & sse mixed mask blitter 
// this routine is designed to support mmx mask blit
//---------------------------------------------------------------------
int iblit_mask_mix(char *dst, long pitch1, const char *src, long pitch2,
	int w, int h, int pixelbyte, long linesize, unsigned long ckey)
{
	long linebytes = (long)linesize;
	long c1, c2, m, n, pb;
	unsigned long mask;

	switch (pixelbyte) {
	case 1:
		m = w >> 4;
		n = w & 15;
		mask = (ckey | (ckey << 8)) & 0xffff;
		mask = mask | (mask << 16);
		break;
	case 2: 
		linebytes = (((long)w) << 1); 
		m = w >> 3;
		n = w & 7;
		mask = ckey | (ckey << 16);
		break;
	case 3: 
		linebytes = (((long)w) << 1) + w; 
		m = 0;
		n = w;
		mask = ckey;
		break;
	case 4: 
		linebytes = (((long)w) << 2); 
		m = w >> 2;
		n = w & 3;
		mask = ckey;
		break;
	default: 
		linebytes = -1; 
		break;
	}

	if (linebytes < 0) return -1;

	c1 = pitch1 - linebytes;
	c2 = pitch2 - linebytes;
	pb = pixelbyte;

#if defined(__INLINEGNU__) && (!defined(__amd64__))
	__asm__ __volatile__ ("\n"
			ASM_BEGIN
		"	movl %1, %%esi\n"
		"	movl %2, %%edi\n"
		"	movd %3, %%mm7\n"
		"	movd %3, %%mm6\n"
		"	movl %3, %%ebx\n"
		"	psllq $32, %%mm7\n"
		"	por %%mm6, %%mm7\n"
		"	cld\n"
		"	movl %8, %%eax\n"
		"	cmpl $2, %%eax\n"
		"	jz bmaskmix16_loopline\n"
		"	cmpl $4, %%eax\n"
		"	jz bmaskmix32_loopline\n"
		"	cmpl $1, %%eax\n"
		"	jz bmaskmix8_loopline\n"
		"	jmp bmaskmix24_loopline\n"
		__ALIGNC__		/* bmaskmix8_loopline */
		"bmaskmix8_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix8_last\n"
		__ALIGNC__
		"bmaskmix8_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
			mm_prefetch_esi_n(256)
			mm_prefetch_edi_n(256)
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqb %%mm7, %%mm0\n"
		"	pcmpeqb %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
			mm_prefetch_esi_n(128)
			mm_prefetch_edi_n(128)
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix8_looppixel\n"
		"bmaskmix8_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix8_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmix8_l2:\n"
		"	lodsb\n"
		"	cmpb %%al, %%bl\n"
		"	jz bmaskmix8_l3\n"
		"	movb %%al, (%%edi)\n"
		"bmaskmix8_l3:\n"
		"	incl %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix8_l2\n"
		"bmaskmix8_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmix8_loopline\n"
		"	jmp bmaskmix_endup\n"
		__ALIGNC__		/* bmaskmix16_loopline */
		"bmaskmix16_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix16_last\n"
		__ALIGNC__
		"bmaskmix16_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
			mm_prefetch_esi_n(256)
			mm_prefetch_edi_n(256)
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqw %%mm7, %%mm0\n"
		"	pcmpeqw %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
			mm_prefetch_esi_n(128)
			mm_prefetch_edi_n(128)
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix16_looppixel\n"
		"bmaskmix16_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix16_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmix16_l2:\n"
		"	lodsw\n"
		"	cmpw %%ax, %%bx\n"
		"	jz bmaskmix16_l3\n"
		"	movw %%ax, (%%edi)\n"
		"bmaskmix16_l3:\n"
		"	addl $2, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix16_l2\n"
		"bmaskmix16_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmix16_loopline\n"
		"	jmp bmaskmix_endup\n"
		__ALIGNC__		/* bmaskmix32_loopline */
		"bmaskmix32_loopline:\n"
		"	movl %4, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix32_last\n"
		__ALIGNC__
		"bmaskmix32_looppixel:\n"
		"	movq 0(%%esi), %%mm0\n"
		"	movq 8(%%esi), %%mm4\n"
			mm_prefetch_esi_n(256)
			mm_prefetch_edi_n(256)
		"	movq %%mm0, %%mm1\n"
		"	movq %%mm4, %%mm5\n"
		"	pcmpeqd %%mm7, %%mm0\n"
		"	pcmpeqd %%mm7, %%mm4\n"
		"	addl $16, %%esi\n"
		"	addl $16, %%edi\n"
		"	movq %%mm0, %%mm2\n"
		"	movq %%mm4, %%mm6\n"
		"	pandn %%mm1, %%mm0\n"
		"	pandn %%mm5, %%mm4\n"
		"	pand -16(%%edi), %%mm2\n"
		"	pand -8(%%edi), %%mm6\n"
			mm_prefetch_esi_n(128)
			mm_prefetch_edi_n(128)
		"	por %%mm0, %%mm2\n"
		"	por %%mm4, %%mm6\n"
		"	movq %%mm2, -16(%%edi)\n"
		"	movq %%mm6, -8(%%edi)\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix32_looppixel\n"
		"bmaskmix32_last:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix32_linend\n"
		"	movd %%mm7, %%ebx\n"
		"bmaskmix32_l2:\n"
		"	lodsl\n"
		"	cmpl %%eax, %%ebx\n"
		"	jz bmaskmix32_l3\n"
		"	movl %%eax, (%%edi)\n"
		"bmaskmix32_l3:\n"
		"	addl $4, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix32_l2\n"
		"bmaskmix32_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmix32_loopline\n"
		"	jmp bmaskmix_endup\n"
		__ALIGNC__		/* bmaskmix24_loopline */
		"bmaskmix24_loopline:\n"
		"	movl %5, %%ecx\n"
		"	testl %%ecx, %%ecx\n"
		"	jz bmaskmix24_linend\n"
		"	andl $0xffffff, %%ebx\n"
		"bmaskmix24_looppixel:\n"
		"	movb 2(%%esi), %%al\n"
		"	movw 0(%%esi), %%dx\n"
		"	shll $16, %%eax\n"
		"	addl $3, %%esi\n"
		"	movw %%dx, %%ax\n"
			mm_prefetch_esi_n(64)
		"	andl $0xffffff, %%eax\n"
		"	cmpl %%ebx, %%eax\n"
		"	jz bmaskmix24_skip\n"
		"	movw %%ax, (%%edi)\n"
		"	shrl $16, %%eax\n"
		"	movb %%al, 2(%%edi)\n"
		"bmaskmix24_skip:\n"
		"	addl $3, %%edi\n"
		"	decl %%ecx\n"
		"	jnz bmaskmix24_looppixel\n"
		"bmaskmix24_linend:\n"
		"	addl %6, %%edi\n"
		"	addl %7, %%esi\n"
		"	decl %0\n"
		"	jnz bmaskmix24_loopline\n"
		"	jmp bmaskmix_endup\n"
		__ALIGNC__		/* bmaskmix_endup */
		"bmaskmix_endup:\n"
		"	emms\n"
			ASM_ENDUP
		:"=m"(h)
		:"m"(src),"m"(dst),"m"(mask),"m"(m),"m"(n),"m"(c1),"m"(c2),"m"(pb)
		:"memory", ASM_REGS);

#elif defined(__INLINEMSC__) && (!defined(__amd64__))
	_asm {
		mov esi, src
		mov edi, dst
		movd mm7, mask
		movd mm6, mask
		mov ebx, mask
		psllq mm7, 32
		por mm7, mm6
		cld
		mov edx, 16
		mov eax, pb
		cmp eax, 1
		jnz bmaskmix16_check
	__ALIGNC__		/* bmaskmix8_loopline */
	bmaskmix8_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmix8_last
	__ALIGNC__
	bmaskmix8_looppixel:
		movq mm0, [esi + 0]
		movq mm4, [esi + 8]
		mm_prefetch_esi_n(256)
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqb mm0, mm7
		pcmpeqb mm4, mm7
		add esi, edx
		add edi, edx
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		mm_prefetch_edi_n(256)
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmix8_looppixel
	bmaskmix8_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmix8_linend
	bmaskmix8_l2:
		lodsb
		cmp al, bl
		jz bmaskmix8_l3
		mov [edi], al
	bmaskmix8_l3:
		inc edi
		dec ecx
		jnz bmaskmix8_l2
	bmaskmix8_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmaskmix8_loopline
	bmaskmix16_check:
		mov eax, pb
		cmp eax, 2
		jnz bmaskmix32_check
	__ALIGNC__		/* bmaskmix16_loopline */
	bmaskmix16_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmix16_last
	__ALIGNC__
	bmaskmix16_looppixel:
		movq mm0, [esi]
		movq mm4, [esi + 8]
		mm_prefetch_esi_8dx
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqw mm0, mm7
		pcmpeqw mm4, mm7
		add esi, edx
		add edi, edx
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		mm_prefetch_edi_8dx
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmix16_looppixel
	bmaskmix16_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmix16_linend
	bmaskmix16_l2:
		lodsw
		cmp ax, bx
		jz bmaskmix16_l3
		mov [edi], ax
	bmaskmix16_l3:
		add edi, 2
		dec ecx
		jnz bmaskmix16_l2
	bmaskmix16_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmaskmix16_loopline
	bmaskmix32_check:
		mov eax, pb
		cmp eax, 4
		jnz bmaskmix24_check
	__ALIGNC__		/* bmaskmix32_loopline */
	bmaskmix32_loopline:
		mov ecx, m
		test ecx, ecx
		jz bmaskmix32_last
	__ALIGNC__
	bmaskmix32_looppixel:
		movq mm0, [esi + 0]
		movq mm4, [esi + 8]
		mm_prefetch_esi_8dx
		movq mm1, mm0
		movq mm5, mm4
		pcmpeqd mm0, mm7
		pcmpeqd mm4, mm7
		add esi, edx
		add edi, edx
		movq  mm2, mm0
		movq  mm6, mm4
		pandn mm0, mm1
		pandn mm4, mm5
		pand  mm2, [edi - 16]
		pand  mm6, [edi - 8]
		mm_prefetch_edi_8dx
		por  mm2, mm0
		por  mm6, mm4
		movq [edi - 16], mm2
		movq [edi - 8], mm6
		dec ecx
		jnz bmaskmix32_looppixel
	bmaskmix32_last:
		mov ecx, n
		test ecx, ecx
		jz bmaskmix32_linend
	bmaskmix32_l2:
		lodsd
		cmp eax, ebx
		jz bmaskmix32_l3
		mov [edi], eax
	bmaskmix32_l3:
		add edi, 4
		dec ecx
		jnz bmaskmix32_l2
	bmaskmix32_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmaskmix32_loopline
		jmp bmaskmix_endup
	bmaskmix24_check:
		mov eax, pb
		cmp eax, 3
		jnz bmaskmix_endup
	__ALIGNC__		/* bmaskmix24_loopline */
	bmaskmix24_loopline:
		mov ecx, n
		test ecx, ecx
		jz bmaskmix24_linend
		and ebx, 0xffffff
	__ALIGNC__
	bmaskmix24_looppixel:
		lodsd
		dec esi
		and eax, 0xffffff
		cmp eax, ebx
		jz bmaskmix24_skip
		mov [edi], ax
		shr eax, 16
		mov [edi + 2], al
	__ALIGNC__
	bmaskmix24_skip:
		add edi, 3
		dec ecx
		jnz bmaskmix24_looppixel
	__ALIGNC__
	bmaskmix24_linend:
		add edi, c1
		add esi, c2
		dec h
		jnz bmaskmix24_loopline
	__ALIGNC__
	bmaskmix_endup:
		emms
	}
#else
	return -1;
#endif
	return 0;
}


#endif

