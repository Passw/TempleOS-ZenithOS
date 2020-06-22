//By mint
asm {
_REALMODEINT::
		PUSH RBP
		MOV RBP, RSP

@@04:	// Acquire function lock
		LOCK BTS [@@100], 0
		JC @@04

		PUSH RAX
		PUSH RBX
		PUSH RCX
		PUSH RDX
		PUSH RSI
		PUSH RDI
		PUSH R8
		PUSH R9
		PUSH R10
		PUSH R11
		PUSH R12
		PUSH R13
		PUSH R14
		PUSH R15

		MOV U64 [@@50], RSP	// Save stack pointer and base ptr
		MOV U64 [@@55], RBP

		MOV AL, U8 SF_ARG1[RBP] // Interrupt number
		MOV U8 [@@05], AL		// Self modify interrupt call

		MOV RSI, U64 SF_ARG3[RBP]  // Copy in_regs to @@70
		MOV RDI, @@70
		MOV ECX, 10
		REP_MOVSD

		CLI

		MOV_RAX_CR3
		MOV U64 [@@56], RAX

		IN AL, 0x21
		MOV U8 [@@80], AL

		IN AL, 0xA1
		MOV U8 [@@90], AL

		MOV AL, 0x11
		OUT 0x20, AL
		OUT 0xA0, AL

		MOV AL, 0x08
		OUT 0x21, AL
		MOV AL, 0x70
		OUT 0xA1, AL

		MOV AL, 0x04
		OUT 0x21, AL
		MOV AL, 0x02
		OUT 0xA1, AL

		MOV AL, 0x01
		OUT 0x21, AL
		OUT 0xA1, AL

		MOV AL, 0xb8
		OUT 0x21, AL
		MOV AL, 0x8e
		OUT 0xA1, AL

		MOV RCX, 0xC0000100 // Store FS base
		RDMSR
		MOV U32 [@@60], EAX
		MOV U32 [@@61], EDX

		MOV RCX, 0xC0000101 // Store GS base
		RDMSR
		MOV U32 [@@65], EAX
		MOV U32 [@@66], EDX

		SGDT U64 [@@10]		// Save GDT and IDT for later
		SIDT U64 [@@20]

		LIDT U64 [@@30]		// Switch to Real Mode IVT

		DU8  0xFF,0x2C,0x25;	// jmp dword far m32
		DU32 @@40;

		USE16
@@01:
		MOV AX, U16 0x90
		MOV DS, AX
		MOV ES, AX
		MOV FS, AX
		MOV GS, AX
		MOV SS, AX

		MOV_EAX_CR0
		BTR EAX, 31
		BTR EAX, 0
		MOV_CR0_EAX

		MOV ECX, 0xC0000080
		RDMSR
		BTR ECX, 8
		WRMSR

		DU8  0xEA;		// jmp far 16:16
		DU16 @@02;
		DU16 0;

@@02:
		XOR AX, AX
		MOV SS, AX

		// Read up register state from in_regs
		MOV ESP, U32 @@70
		POP GS
		POP FS
		POP ES
		POP DS
		POPFD
		POP EBP
		POP EDI
		POP ESI
		POP EDX
		POP ECX
		POP EBX
		POP EAX

		MOV ESP, 0x7C00

		STI

		DU8 0xCD;	// 0xCD == int
@@05:	DU8 0;

		PUSH AX
		MOV AL, 0xFF
		OUT 0xA1, AL
		OUT 0x21, AL
		POP AX

		CLI

		// Save out register state to out_regs
		MOV ESP, U32 @@70
		ADD ESP, 10 * 4
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH ESI
		PUSH EDI
		PUSH EBP
		PUSHFD
		PUSH DS
		PUSH ES
		PUSH FS
		PUSH GS

		XOR AX, AX
		MOV DS, AX

		LGDT U32 [@@10]

		MOV EAX, U32 [@@56]
		MOV_CR3_EAX

		MOV ECX, 0xC0000080
		RDMSR
		BTS EAX, 8
		WRMSR

		MOV_EAX_CR0
		BTS EAX, 31
		BTS EAX, 0
		MOV_CR0_EAX

		DU8  0xEA;		// jmp far 16:16
		DU16 @@03;
		DU16 0x40;

		USE64
@@03:
		MOV AX, 0x60
		MOV DS, AX
		MOV ES, AX
		MOV FS, AX
		MOV GS, AX
		MOV SS, AX

		MOV RSP, U64 [@@50]
		MOV RBP, U64 [@@55]

		LIDT U64 [@@20]

		MOV RCX, 0xC0000100 // Restore FS base
		MOV EAX, U32 [@@60]
		MOV EDX, U32 [@@61]
		WRMSR

		MOV RCX, 0xC0000101 // Restore GS base
		MOV EAX, U32 [@@65]
		MOV EDX, U32 [@@66]
		WRMSR

		MOV AL, 0x11
		OUT 0x20, AL
		OUT 0xA0, AL

		MOV AL, 0x20
		OUT 0x21, AL
		MOV AL, 0x28
		OUT 0xA1, AL

		MOV AL, 0x04
		OUT 0x21, AL
		MOV AL, 0x02
		OUT 0xA1, AL

		MOV AL, 0x01
		OUT 0x21, AL
		OUT 0xA1, AL

		MOV AL, U8 [@@80]
		OUT 0x21, AL
		MOV AL, U8 [@@90]
		OUT 0xA1, AL

		STI

		// Save register state to out_regs from @@70
		MOV RSI, @@70
		MOV RDI, U64 SF_ARG2[RBP]
		MOV RCX, 10
		REP_MOVSD

		POP R15
		POP R14
		POP R13
		POP R12
		POP R11
		POP R10
		POP R9
		POP R8
		POP RDI
		POP RSI
		POP RDX
		POP RCX
		POP RBX
		POP RAX

		LOCK BTR [@@100], 0

		POP RBP

		RET1 24


ALIGN 8,OC_NOP

@@10:	DU64 0,0;

@@20:	DU64 0,0;

@@30:	DU16 0x3FF,0,0,0;
		DU64 0;

@@40:	DU32 @@01;
		DU16 0x80,0;

@@50:	DU64 0;
@@55:	DU64 0;

@@56:	DU64 0;

@@60:	DU32 0;
@@61:	DU32 0;

@@65:	DU32 0;
@@66:	DU32 0;

@@70:	DU16 0,0,0,0;
		DU32 0,0,0,0,0,0,0,0;

@@100:	DU64 0;

@@80:	DU8  0;
@@90:	DU8  0;

};

_extern _REALMODEINT U0 RealModeInt(U8 int, CRMRegs *regs_in, CRMRegs *regs_out); //Drop to real mode to perform a BIOS call.
