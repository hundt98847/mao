	.section .text
L1:	.byte	1	# Should be address 0x00
	.align	8
L2:	.byte	2	# Should be address 0x08
	.space	L2-L1
L3:	.byte	3	# Should be address 0x11
	
