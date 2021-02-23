# GWLoader initialization routine
# Copies all of the required data to ITCM and DTCM and the jumps to ITCM.
# Written by Michal Prochazka, 2021.

.section .text
.global _start

_start:
.code 16

	# ITCM data
	
	ldr r0, itcm_addr
	ldr r1, itcm_data_ptr
	ldr r2, itcm_data_length
	
itcm_loop:
	ldr r3, [r1]
	str r3, [r0]
	
	add r0, #4
	add r1, #4
	
	sub r2, #4
	
	cmp r2, #0
	bne itcm_loop
	
	# DTCM data
	
	ldr r0, dtcm_addr
#	ldr r1, dtcm_data_ptr	-- we're already at this address
	ldr r2, dtcm_data_length
	
dtcm_loop:
	ldr r3, [r1]
	str r3, [r0]
	
	add r0, #4
	add r1, #4
	
	sub r2, #1
	
	cmp r2, #0
	bne dtcm_loop
	
	# Jump to the program
	
	ldr r0, init_pc
	bx r0

.align 2
	
itcm_addr: .long 0x00000000
dtcm_addr: .long 0x20000000

itcm_data_ptr: .long itcm_data
dtcm_data_ptr: .long dtcm_data

itcm_data_length: .long itcm_data_end - itcm_data
dtcm_data_length: .long dtcm_data_end - dtcm_data

.include "build/data.s"
