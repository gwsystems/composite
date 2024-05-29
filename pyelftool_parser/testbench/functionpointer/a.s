
a.out:     file format elf64-x86-64


Disassembly of section .interp:

0000000000000318 <.interp>:
 318:	2f                   	(bad)  
 319:	6c                   	insb   (%dx),%es:(%rdi)
 31a:	69 62 36 34 2f 6c 64 	imul   $0x646c2f34,0x36(%rdx),%esp
 321:	2d 6c 69 6e 75       	sub    $0x756e696c,%eax
 326:	78 2d                	js     355 <__abi_tag-0x37>
 328:	78 38                	js     362 <__abi_tag-0x2a>
 32a:	36 2d 36 34 2e 73    	ss sub $0x732e3436,%eax
 330:	6f                   	outsl  %ds:(%rsi),(%dx)
 331:	2e 32 00             	cs xor (%rax),%al

Disassembly of section .note.gnu.property:

0000000000000338 <.note.gnu.property>:
 338:	04 00                	add    $0x0,%al
 33a:	00 00                	add    %al,(%rax)
 33c:	20 00                	and    %al,(%rax)
 33e:	00 00                	add    %al,(%rax)
 340:	05 00 00 00 47       	add    $0x47000000,%eax
 345:	4e 55                	rex.WRX push %rbp
 347:	00 02                	add    %al,(%rdx)
 349:	00 00                	add    %al,(%rax)
 34b:	c0 04 00 00          	rolb   $0x0,(%rax,%rax,1)
 34f:	00 03                	add    %al,(%rbx)
 351:	00 00                	add    %al,(%rax)
 353:	00 00                	add    %al,(%rax)
 355:	00 00                	add    %al,(%rax)
 357:	00 02                	add    %al,(%rdx)
 359:	80 00 c0             	addb   $0xc0,(%rax)
 35c:	04 00                	add    $0x0,%al
 35e:	00 00                	add    %al,(%rax)
 360:	01 00                	add    %eax,(%rax)
 362:	00 00                	add    %al,(%rax)
 364:	00 00                	add    %al,(%rax)
	...

Disassembly of section .note.gnu.build-id:

0000000000000368 <.note.gnu.build-id>:
 368:	04 00                	add    $0x0,%al
 36a:	00 00                	add    %al,(%rax)
 36c:	14 00                	adc    $0x0,%al
 36e:	00 00                	add    %al,(%rax)
 370:	03 00                	add    (%rax),%eax
 372:	00 00                	add    %al,(%rax)
 374:	47                   	rex.RXB
 375:	4e 55                	rex.WRX push %rbp
 377:	00 4e ab             	add    %cl,-0x55(%rsi)
 37a:	0a 0f                	or     (%rdi),%cl
 37c:	ab                   	stos   %eax,%es:(%rdi)
 37d:	a8 f3                	test   $0xf3,%al
 37f:	9d                   	popf   
 380:	70 6c                	jo     3ee <__abi_tag+0x62>
 382:	4b e7 69             	rex.WXB out %eax,$0x69
 385:	11 13                	adc    %edx,(%rbx)
 387:	64 eb b8             	fs jmp 342 <__abi_tag-0x4a>
 38a:	d7                   	xlat   %ds:(%rbx)
 38b:	f6                   	.byte 0xf6

Disassembly of section .note.ABI-tag:

000000000000038c <__abi_tag>:
 38c:	04 00                	add    $0x0,%al
 38e:	00 00                	add    %al,(%rax)
 390:	10 00                	adc    %al,(%rax)
 392:	00 00                	add    %al,(%rax)
 394:	01 00                	add    %eax,(%rax)
 396:	00 00                	add    %al,(%rax)
 398:	47                   	rex.RXB
 399:	4e 55                	rex.WRX push %rbp
 39b:	00 00                	add    %al,(%rax)
 39d:	00 00                	add    %al,(%rax)
 39f:	00 03                	add    %al,(%rbx)
 3a1:	00 00                	add    %al,(%rax)
 3a3:	00 02                	add    %al,(%rdx)
 3a5:	00 00                	add    %al,(%rax)
 3a7:	00 00                	add    %al,(%rax)
 3a9:	00 00                	add    %al,(%rax)
	...

Disassembly of section .gnu.hash:

00000000000003b0 <.gnu.hash>:
 3b0:	02 00                	add    (%rax),%al
 3b2:	00 00                	add    %al,(%rax)
 3b4:	09 00                	or     %eax,(%rax)
 3b6:	00 00                	add    %al,(%rax)
 3b8:	01 00                	add    %eax,(%rax)
 3ba:	00 00                	add    %al,(%rax)
 3bc:	06                   	(bad)  
 3bd:	00 00                	add    %al,(%rax)
 3bf:	00 00                	add    %al,(%rax)
 3c1:	00 81 00 00 00 00    	add    %al,0x0(%rcx)
 3c7:	00 09                	add    %cl,(%rcx)
 3c9:	00 00                	add    %al,(%rax)
 3cb:	00 00                	add    %al,(%rax)
 3cd:	00 00                	add    %al,(%rax)
 3cf:	00 d1                	add    %dl,%cl
 3d1:	65 ce                	gs (bad) 
 3d3:	6d                   	insl   (%dx),%es:(%rdi)

Disassembly of section .dynsym:

00000000000003d8 <.dynsym>:
	...
 3f0:	10 00                	adc    %al,(%rax)
 3f2:	00 00                	add    %al,(%rax)
 3f4:	12 00                	adc    (%rax),%al
	...
 406:	00 00                	add    %al,(%rax)
 408:	83 00 00             	addl   $0x0,(%rax)
 40b:	00 20                	add    %ah,(%rax)
	...
 41d:	00 00                	add    %al,(%rax)
 41f:	00 22                	add    %ah,(%rdx)
 421:	00 00                	add    %al,(%rax)
 423:	00 12                	add    %dl,(%rdx)
	...
 435:	00 00                	add    %al,(%rax)
 437:	00 36                	add    %dh,(%rsi)
 439:	00 00                	add    %al,(%rax)
 43b:	00 12                	add    %dl,(%rdx)
	...
 44d:	00 00                	add    %al,(%rax)
 44f:	00 47 00             	add    %al,0x0(%rdi)
 452:	00 00                	add    %al,(%rax)
 454:	12 00                	adc    (%rax),%al
	...
 466:	00 00                	add    %al,(%rax)
 468:	9f                   	lahf   
 469:	00 00                	add    %al,(%rax)
 46b:	00 20                	add    %ah,(%rax)
	...
 47d:	00 00                	add    %al,(%rax)
 47f:	00 27                	add    %ah,(%rdi)
 481:	00 00                	add    %al,(%rax)
 483:	00 12                	add    %dl,(%rdx)
	...
 495:	00 00                	add    %al,(%rax)
 497:	00 ae 00 00 00 20    	add    %ch,0x20000000(%rsi)
	...
 4ad:	00 00                	add    %al,(%rax)
 4af:	00 01                	add    %al,(%rcx)
 4b1:	00 00                	add    %al,(%rax)
 4b3:	00 22                	add    %ah,(%rdx)
	...

Disassembly of section .dynstr:

00000000000004c8 <.dynstr>:
 4c8:	00 5f 5f             	add    %bl,0x5f(%rdi)
 4cb:	63 78 61             	movsxd 0x61(%rax),%edi
 4ce:	5f                   	pop    %rdi
 4cf:	66 69 6e 61 6c 69    	imul   $0x696c,0x61(%rsi),%bp
 4d5:	7a 65                	jp     53c <__abi_tag+0x1b0>
 4d7:	00 5f 5f             	add    %bl,0x5f(%rdi)
 4da:	6c                   	insb   (%dx),%es:(%rdi)
 4db:	69 62 63 5f 73 74 61 	imul   $0x6174735f,0x63(%rdx),%esp
 4e2:	72 74                	jb     558 <__abi_tag+0x1cc>
 4e4:	5f                   	pop    %rdi
 4e5:	6d                   	insl   (%dx),%es:(%rdi)
 4e6:	61                   	(bad)  
 4e7:	69 6e 00 70 75 74 73 	imul   $0x73747570,0x0(%rsi),%ebp
 4ee:	00 5f 5f             	add    %bl,0x5f(%rdi)
 4f1:	69 73 6f 63 39 39 5f 	imul   $0x5f393963,0x6f(%rbx),%esi
 4f8:	73 63                	jae    55d <__abi_tag+0x1d1>
 4fa:	61                   	(bad)  
 4fb:	6e                   	outsb  %ds:(%rsi),(%dx)
 4fc:	66 00 5f 5f          	data16 add %bl,0x5f(%rdi)
 500:	73 74                	jae    576 <__abi_tag+0x1ea>
 502:	61                   	(bad)  
 503:	63 6b 5f             	movsxd 0x5f(%rbx),%ebp
 506:	63 68 6b             	movsxd 0x6b(%rax),%ebp
 509:	5f                   	pop    %rdi
 50a:	66 61                	data16 (bad) 
 50c:	69 6c 00 70 72 69 6e 	imul   $0x746e6972,0x70(%rax,%rax,1),%ebp
 513:	74 
 514:	66 00 6c 69 62       	data16 add %ch,0x62(%rcx,%rbp,2)
 519:	63 2e                	movsxd (%rsi),%ebp
 51b:	73 6f                	jae    58c <__abi_tag+0x200>
 51d:	2e 36 00 47 4c       	cs ss add %al,0x4c(%rdi)
 522:	49                   	rex.WB
 523:	42                   	rex.X
 524:	43 5f                	rex.XB pop %r15
 526:	32 2e                	xor    (%rsi),%ch
 528:	37                   	(bad)  
 529:	00 47 4c             	add    %al,0x4c(%rdi)
 52c:	49                   	rex.WB
 52d:	42                   	rex.X
 52e:	43 5f                	rex.XB pop %r15
 530:	32 2e                	xor    (%rsi),%ch
 532:	34 00                	xor    $0x0,%al
 534:	47                   	rex.RXB
 535:	4c                   	rex.WR
 536:	49                   	rex.WB
 537:	42                   	rex.X
 538:	43 5f                	rex.XB pop %r15
 53a:	32 2e                	xor    (%rsi),%ch
 53c:	32 2e                	xor    (%rsi),%ch
 53e:	35 00 47 4c 49       	xor    $0x494c4700,%eax
 543:	42                   	rex.X
 544:	43 5f                	rex.XB pop %r15
 546:	32 2e                	xor    (%rsi),%ch
 548:	33 34 00             	xor    (%rax,%rax,1),%esi
 54b:	5f                   	pop    %rdi
 54c:	49 54                	rex.WB push %r12
 54e:	4d 5f                	rex.WRB pop %r15
 550:	64 65 72 65          	fs gs jb 5b9 <__abi_tag+0x22d>
 554:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 55b:	4d 
 55c:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 55e:	6f                   	outsl  %ds:(%rsi),(%dx)
 55f:	6e                   	outsb  %ds:(%rsi),(%dx)
 560:	65 54                	gs push %rsp
 562:	61                   	(bad)  
 563:	62                   	(bad)  
 564:	6c                   	insb   (%dx),%es:(%rdi)
 565:	65 00 5f 5f          	add    %bl,%gs:0x5f(%rdi)
 569:	67 6d                	insl   (%dx),%es:(%edi)
 56b:	6f                   	outsl  %ds:(%rsi),(%dx)
 56c:	6e                   	outsb  %ds:(%rsi),(%dx)
 56d:	5f                   	pop    %rdi
 56e:	73 74                	jae    5e4 <__abi_tag+0x258>
 570:	61                   	(bad)  
 571:	72 74                	jb     5e7 <__abi_tag+0x25b>
 573:	5f                   	pop    %rdi
 574:	5f                   	pop    %rdi
 575:	00 5f 49             	add    %bl,0x49(%rdi)
 578:	54                   	push   %rsp
 579:	4d 5f                	rex.WRB pop %r15
 57b:	72 65                	jb     5e2 <__abi_tag+0x256>
 57d:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 584:	4d 
 585:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 587:	6f                   	outsl  %ds:(%rsi),(%dx)
 588:	6e                   	outsb  %ds:(%rsi),(%dx)
 589:	65 54                	gs push %rsp
 58b:	61                   	(bad)  
 58c:	62                   	.byte 0x62
 58d:	6c                   	insb   (%dx),%es:(%rdi)
 58e:	65                   	gs
	...

Disassembly of section .gnu.version:

0000000000000590 <.gnu.version>:
 590:	00 00                	add    %al,(%rax)
 592:	02 00                	add    (%rax),%al
 594:	01 00                	add    %eax,(%rax)
 596:	03 00                	add    (%rax),%eax
 598:	04 00                	add    $0x0,%al
 59a:	03 00                	add    (%rax),%eax
 59c:	01 00                	add    %eax,(%rax)
 59e:	05 00 01 00 03       	add    $0x3000100,%eax
	...

Disassembly of section .gnu.version_r:

00000000000005a8 <.gnu.version_r>:
 5a8:	01 00                	add    %eax,(%rax)
 5aa:	04 00                	add    $0x0,%al
 5ac:	4e 00 00             	rex.WRX add %r8b,(%rax)
 5af:	00 10                	add    %dl,(%rax)
 5b1:	00 00                	add    %al,(%rax)
 5b3:	00 00                	add    %al,(%rax)
 5b5:	00 00                	add    %al,(%rax)
 5b7:	00 17                	add    %dl,(%rdi)
 5b9:	69 69 0d 00 00 05 00 	imul   $0x50000,0xd(%rcx),%ebp
 5c0:	58                   	pop    %rax
 5c1:	00 00                	add    %al,(%rax)
 5c3:	00 10                	add    %dl,(%rax)
 5c5:	00 00                	add    %al,(%rax)
 5c7:	00 14 69             	add    %dl,(%rcx,%rbp,2)
 5ca:	69 0d 00 00 04 00 62 	imul   $0x62,0x40000(%rip),%ecx        # 405d4 <_end+0x3c5bc>
 5d1:	00 00 00 
 5d4:	10 00                	adc    %al,(%rax)
 5d6:	00 00                	add    %al,(%rax)
 5d8:	75 1a                	jne    5f4 <__abi_tag+0x268>
 5da:	69 09 00 00 03 00    	imul   $0x30000,(%rcx),%ecx
 5e0:	6c                   	insb   (%dx),%es:(%rdi)
 5e1:	00 00                	add    %al,(%rax)
 5e3:	00 10                	add    %dl,(%rax)
 5e5:	00 00                	add    %al,(%rax)
 5e7:	00 b4 91 96 06 00 00 	add    %dh,0x696(%rcx,%rdx,4)
 5ee:	02 00                	add    (%rax),%al
 5f0:	78 00                	js     5f2 <__abi_tag+0x266>
 5f2:	00 00                	add    %al,(%rax)
 5f4:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rela.dyn:

00000000000005f8 <.rela.dyn>:
 5f8:	a0 3d 00 00 00 00 00 	movabs 0x80000000000003d,%al
 5ff:	00 08 
 601:	00 00                	add    %al,(%rax)
 603:	00 00                	add    %al,(%rax)
 605:	00 00                	add    %al,(%rax)
 607:	00 a0 11 00 00 00    	add    %ah,0x11(%rax)
 60d:	00 00                	add    %al,(%rax)
 60f:	00 a8 3d 00 00 00    	add    %ch,0x3d(%rax)
 615:	00 00                	add    %al,(%rax)
 617:	00 08                	add    %cl,(%rax)
 619:	00 00                	add    %al,(%rax)
 61b:	00 00                	add    %al,(%rax)
 61d:	00 00                	add    %al,(%rax)
 61f:	00 60 11             	add    %ah,0x11(%rax)
 622:	00 00                	add    %al,(%rax)
 624:	00 00                	add    %al,(%rax)
 626:	00 00                	add    %al,(%rax)
 628:	08 40 00             	or     %al,0x0(%rax)
 62b:	00 00                	add    %al,(%rax)
 62d:	00 00                	add    %al,(%rax)
 62f:	00 08                	add    %cl,(%rax)
 631:	00 00                	add    %al,(%rax)
 633:	00 00                	add    %al,(%rax)
 635:	00 00                	add    %al,(%rax)
 637:	00 08                	add    %cl,(%rax)
 639:	40 00 00             	rex add %al,(%rax)
 63c:	00 00                	add    %al,(%rax)
 63e:	00 00                	add    %al,(%rax)
 640:	d8 3f                	fdivrs (%rdi)
 642:	00 00                	add    %al,(%rax)
 644:	00 00                	add    %al,(%rax)
 646:	00 00                	add    %al,(%rax)
 648:	06                   	(bad)  
 649:	00 00                	add    %al,(%rax)
 64b:	00 01                	add    %al,(%rcx)
	...
 655:	00 00                	add    %al,(%rax)
 657:	00 e0                	add    %ah,%al
 659:	3f                   	(bad)  
 65a:	00 00                	add    %al,(%rax)
 65c:	00 00                	add    %al,(%rax)
 65e:	00 00                	add    %al,(%rax)
 660:	06                   	(bad)  
 661:	00 00                	add    %al,(%rax)
 663:	00 02                	add    %al,(%rdx)
	...
 66d:	00 00                	add    %al,(%rax)
 66f:	00 e8                	add    %ch,%al
 671:	3f                   	(bad)  
 672:	00 00                	add    %al,(%rax)
 674:	00 00                	add    %al,(%rax)
 676:	00 00                	add    %al,(%rax)
 678:	06                   	(bad)  
 679:	00 00                	add    %al,(%rax)
 67b:	00 06                	add    %al,(%rsi)
	...
 685:	00 00                	add    %al,(%rax)
 687:	00 f0                	add    %dh,%al
 689:	3f                   	(bad)  
 68a:	00 00                	add    %al,(%rax)
 68c:	00 00                	add    %al,(%rax)
 68e:	00 00                	add    %al,(%rax)
 690:	06                   	(bad)  
 691:	00 00                	add    %al,(%rax)
 693:	00 08                	add    %cl,(%rax)
	...
 69d:	00 00                	add    %al,(%rax)
 69f:	00 f8                	add    %bh,%al
 6a1:	3f                   	(bad)  
 6a2:	00 00                	add    %al,(%rax)
 6a4:	00 00                	add    %al,(%rax)
 6a6:	00 00                	add    %al,(%rax)
 6a8:	06                   	(bad)  
 6a9:	00 00                	add    %al,(%rax)
 6ab:	00 09                	add    %cl,(%rcx)
	...

Disassembly of section .rela.plt:

00000000000006b8 <.rela.plt>:
 6b8:	b8 3f 00 00 00       	mov    $0x3f,%eax
 6bd:	00 00                	add    %al,(%rax)
 6bf:	00 07                	add    %al,(%rdi)
 6c1:	00 00                	add    %al,(%rax)
 6c3:	00 03                	add    %al,(%rbx)
	...
 6cd:	00 00                	add    %al,(%rax)
 6cf:	00 c0                	add    %al,%al
 6d1:	3f                   	(bad)  
 6d2:	00 00                	add    %al,(%rax)
 6d4:	00 00                	add    %al,(%rax)
 6d6:	00 00                	add    %al,(%rax)
 6d8:	07                   	(bad)  
 6d9:	00 00                	add    %al,(%rax)
 6db:	00 04 00             	add    %al,(%rax,%rax,1)
	...
 6e6:	00 00                	add    %al,(%rax)
 6e8:	c8 3f 00 00          	enter  $0x3f,$0x0
 6ec:	00 00                	add    %al,(%rax)
 6ee:	00 00                	add    %al,(%rax)
 6f0:	07                   	(bad)  
 6f1:	00 00                	add    %al,(%rax)
 6f3:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 6f9 <__abi_tag+0x36d>
 6f9:	00 00                	add    %al,(%rax)
 6fb:	00 00                	add    %al,(%rax)
 6fd:	00 00                	add    %al,(%rax)
 6ff:	00 d0                	add    %dl,%al
 701:	3f                   	(bad)  
 702:	00 00                	add    %al,(%rax)
 704:	00 00                	add    %al,(%rax)
 706:	00 00                	add    %al,(%rax)
 708:	07                   	(bad)  
 709:	00 00                	add    %al,(%rax)
 70b:	00 07                	add    %al,(%rdi)
	...

Disassembly of section .init:

0000000000001000 <_init>:
    1000:	f3 0f 1e fa          	endbr64 
    1004:	48 83 ec 08          	sub    $0x8,%rsp
    1008:	48 8b 05 d9 2f 00 00 	mov    0x2fd9(%rip),%rax        # 3fe8 <__gmon_start__@Base>
    100f:	48 85 c0             	test   %rax,%rax
    1012:	74 02                	je     1016 <_init+0x16>
    1014:	ff d0                	call   *%rax
    1016:	48 83 c4 08          	add    $0x8,%rsp
    101a:	c3                   	ret    

Disassembly of section .plt:

0000000000001020 <.plt>:
    1020:	ff 35 82 2f 00 00    	push   0x2f82(%rip)        # 3fa8 <_GLOBAL_OFFSET_TABLE_+0x8>
    1026:	f2 ff 25 83 2f 00 00 	bnd jmp *0x2f83(%rip)        # 3fb0 <_GLOBAL_OFFSET_TABLE_+0x10>
    102d:	0f 1f 00             	nopl   (%rax)
    1030:	f3 0f 1e fa          	endbr64 
    1034:	68 00 00 00 00       	push   $0x0
    1039:	f2 e9 e1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    103f:	90                   	nop
    1040:	f3 0f 1e fa          	endbr64 
    1044:	68 01 00 00 00       	push   $0x1
    1049:	f2 e9 d1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    104f:	90                   	nop
    1050:	f3 0f 1e fa          	endbr64 
    1054:	68 02 00 00 00       	push   $0x2
    1059:	f2 e9 c1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    105f:	90                   	nop
    1060:	f3 0f 1e fa          	endbr64 
    1064:	68 03 00 00 00       	push   $0x3
    1069:	f2 e9 b1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    106f:	90                   	nop

Disassembly of section .plt.got:

0000000000001070 <__cxa_finalize@plt>:
    1070:	f3 0f 1e fa          	endbr64 
    1074:	f2 ff 25 7d 2f 00 00 	bnd jmp *0x2f7d(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    107b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .plt.sec:

0000000000001080 <puts@plt>:
    1080:	f3 0f 1e fa          	endbr64 
    1084:	f2 ff 25 2d 2f 00 00 	bnd jmp *0x2f2d(%rip)        # 3fb8 <puts@GLIBC_2.2.5>
    108b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000001090 <__stack_chk_fail@plt>:
    1090:	f3 0f 1e fa          	endbr64 
    1094:	f2 ff 25 25 2f 00 00 	bnd jmp *0x2f25(%rip)        # 3fc0 <__stack_chk_fail@GLIBC_2.4>
    109b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010a0 <printf@plt>:
    10a0:	f3 0f 1e fa          	endbr64 
    10a4:	f2 ff 25 1d 2f 00 00 	bnd jmp *0x2f1d(%rip)        # 3fc8 <printf@GLIBC_2.2.5>
    10ab:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010b0 <__isoc99_scanf@plt>:
    10b0:	f3 0f 1e fa          	endbr64 
    10b4:	f2 ff 25 15 2f 00 00 	bnd jmp *0x2f15(%rip)        # 3fd0 <__isoc99_scanf@GLIBC_2.7>
    10bb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .text:

00000000000010c0 <_start>:
    10c0:	f3 0f 1e fa          	endbr64 
    10c4:	31 ed                	xor    %ebp,%ebp
    10c6:	49 89 d1             	mov    %rdx,%r9
    10c9:	5e                   	pop    %rsi
    10ca:	48 89 e2             	mov    %rsp,%rdx
    10cd:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
    10d1:	50                   	push   %rax
    10d2:	54                   	push   %rsp
    10d3:	45 31 c0             	xor    %r8d,%r8d
    10d6:	31 c9                	xor    %ecx,%ecx
    10d8:	48 8d 3d 60 01 00 00 	lea    0x160(%rip),%rdi        # 123f <main>
    10df:	ff 15 f3 2e 00 00    	call   *0x2ef3(%rip)        # 3fd8 <__libc_start_main@GLIBC_2.34>
    10e5:	f4                   	hlt    
    10e6:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    10ed:	00 00 00 

00000000000010f0 <deregister_tm_clones>:
    10f0:	48 8d 3d 19 2f 00 00 	lea    0x2f19(%rip),%rdi        # 4010 <__TMC_END__>
    10f7:	48 8d 05 12 2f 00 00 	lea    0x2f12(%rip),%rax        # 4010 <__TMC_END__>
    10fe:	48 39 f8             	cmp    %rdi,%rax
    1101:	74 15                	je     1118 <deregister_tm_clones+0x28>
    1103:	48 8b 05 d6 2e 00 00 	mov    0x2ed6(%rip),%rax        # 3fe0 <_ITM_deregisterTMCloneTable@Base>
    110a:	48 85 c0             	test   %rax,%rax
    110d:	74 09                	je     1118 <deregister_tm_clones+0x28>
    110f:	ff e0                	jmp    *%rax
    1111:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    1118:	c3                   	ret    
    1119:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001120 <register_tm_clones>:
    1120:	48 8d 3d e9 2e 00 00 	lea    0x2ee9(%rip),%rdi        # 4010 <__TMC_END__>
    1127:	48 8d 35 e2 2e 00 00 	lea    0x2ee2(%rip),%rsi        # 4010 <__TMC_END__>
    112e:	48 29 fe             	sub    %rdi,%rsi
    1131:	48 89 f0             	mov    %rsi,%rax
    1134:	48 c1 ee 3f          	shr    $0x3f,%rsi
    1138:	48 c1 f8 03          	sar    $0x3,%rax
    113c:	48 01 c6             	add    %rax,%rsi
    113f:	48 d1 fe             	sar    %rsi
    1142:	74 14                	je     1158 <register_tm_clones+0x38>
    1144:	48 8b 05 a5 2e 00 00 	mov    0x2ea5(%rip),%rax        # 3ff0 <_ITM_registerTMCloneTable@Base>
    114b:	48 85 c0             	test   %rax,%rax
    114e:	74 08                	je     1158 <register_tm_clones+0x38>
    1150:	ff e0                	jmp    *%rax
    1152:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    1158:	c3                   	ret    
    1159:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001160 <__do_global_dtors_aux>:
    1160:	f3 0f 1e fa          	endbr64 
    1164:	80 3d a5 2e 00 00 00 	cmpb   $0x0,0x2ea5(%rip)        # 4010 <__TMC_END__>
    116b:	75 2b                	jne    1198 <__do_global_dtors_aux+0x38>
    116d:	55                   	push   %rbp
    116e:	48 83 3d 82 2e 00 00 	cmpq   $0x0,0x2e82(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    1175:	00 
    1176:	48 89 e5             	mov    %rsp,%rbp
    1179:	74 0c                	je     1187 <__do_global_dtors_aux+0x27>
    117b:	48 8b 3d 86 2e 00 00 	mov    0x2e86(%rip),%rdi        # 4008 <__dso_handle>
    1182:	e8 e9 fe ff ff       	call   1070 <__cxa_finalize@plt>
    1187:	e8 64 ff ff ff       	call   10f0 <deregister_tm_clones>
    118c:	c6 05 7d 2e 00 00 01 	movb   $0x1,0x2e7d(%rip)        # 4010 <__TMC_END__>
    1193:	5d                   	pop    %rbp
    1194:	c3                   	ret    
    1195:	0f 1f 00             	nopl   (%rax)
    1198:	c3                   	ret    
    1199:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

00000000000011a0 <frame_dummy>:
    11a0:	f3 0f 1e fa          	endbr64 
    11a4:	e9 77 ff ff ff       	jmp    1120 <register_tm_clones>

00000000000011a9 <add>:
    11a9:	f3 0f 1e fa          	endbr64 
    11ad:	55                   	push   %rbp
    11ae:	48 89 e5             	mov    %rsp,%rbp
    11b1:	48 83 ec 10          	sub    $0x10,%rsp
    11b5:	89 7d fc             	mov    %edi,-0x4(%rbp)
    11b8:	89 75 f8             	mov    %esi,-0x8(%rbp)
    11bb:	8b 55 fc             	mov    -0x4(%rbp),%edx
    11be:	8b 45 f8             	mov    -0x8(%rbp),%eax
    11c1:	01 d0                	add    %edx,%eax
    11c3:	89 c6                	mov    %eax,%esi
    11c5:	48 8d 05 3c 0e 00 00 	lea    0xe3c(%rip),%rax        # 2008 <_IO_stdin_used+0x8>
    11cc:	48 89 c7             	mov    %rax,%rdi
    11cf:	b8 00 00 00 00       	mov    $0x0,%eax
    11d4:	e8 c7 fe ff ff       	call   10a0 <printf@plt>
    11d9:	90                   	nop
    11da:	c9                   	leave  
    11db:	c3                   	ret    

00000000000011dc <subtract>:
    11dc:	f3 0f 1e fa          	endbr64 
    11e0:	55                   	push   %rbp
    11e1:	48 89 e5             	mov    %rsp,%rbp
    11e4:	48 83 ec 10          	sub    $0x10,%rsp
    11e8:	89 7d fc             	mov    %edi,-0x4(%rbp)
    11eb:	89 75 f8             	mov    %esi,-0x8(%rbp)
    11ee:	8b 45 fc             	mov    -0x4(%rbp),%eax
    11f1:	2b 45 f8             	sub    -0x8(%rbp),%eax
    11f4:	89 c6                	mov    %eax,%esi
    11f6:	48 8d 05 1b 0e 00 00 	lea    0xe1b(%rip),%rax        # 2018 <_IO_stdin_used+0x18>
    11fd:	48 89 c7             	mov    %rax,%rdi
    1200:	b8 00 00 00 00       	mov    $0x0,%eax
    1205:	e8 96 fe ff ff       	call   10a0 <printf@plt>
    120a:	90                   	nop
    120b:	c9                   	leave  
    120c:	c3                   	ret    

000000000000120d <multiply>:
    120d:	f3 0f 1e fa          	endbr64 
    1211:	55                   	push   %rbp
    1212:	48 89 e5             	mov    %rsp,%rbp
    1215:	48 83 ec 10          	sub    $0x10,%rsp
    1219:	89 7d fc             	mov    %edi,-0x4(%rbp)
    121c:	89 75 f8             	mov    %esi,-0x8(%rbp)
    121f:	8b 45 fc             	mov    -0x4(%rbp),%eax
    1222:	0f af 45 f8          	imul   -0x8(%rbp),%eax
    1226:	89 c6                	mov    %eax,%esi
    1228:	48 8d 05 fc 0d 00 00 	lea    0xdfc(%rip),%rax        # 202b <_IO_stdin_used+0x2b>
    122f:	48 89 c7             	mov    %rax,%rdi
    1232:	b8 00 00 00 00       	mov    $0x0,%eax
    1237:	e8 64 fe ff ff       	call   10a0 <printf@plt>
    123c:	90                   	nop
    123d:	c9                   	leave  
    123e:	c3                   	ret    

000000000000123f <main>:
    123f:	f3 0f 1e fa          	endbr64 
    1243:	55                   	push   %rbp
    1244:	48 89 e5             	mov    %rsp,%rbp
    1247:	48 83 ec 30          	sub    $0x30,%rsp
    124b:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    1252:	00 00 
    1254:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    1258:	31 c0                	xor    %eax,%eax
    125a:	48 8d 05 48 ff ff ff 	lea    -0xb8(%rip),%rax        # 11a9 <add>
    1261:	48 89 45 e0          	mov    %rax,-0x20(%rbp)
    1265:	48 8d 05 70 ff ff ff 	lea    -0x90(%rip),%rax        # 11dc <subtract>
    126c:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    1270:	48 8d 05 96 ff ff ff 	lea    -0x6a(%rip),%rax        # 120d <multiply>
    1277:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    127b:	c7 45 d8 0f 00 00 00 	movl   $0xf,-0x28(%rbp)
    1282:	c7 45 dc 0a 00 00 00 	movl   $0xa,-0x24(%rbp)
    1289:	48 8d 05 b8 0d 00 00 	lea    0xdb8(%rip),%rax        # 2048 <_IO_stdin_used+0x48>
    1290:	48 89 c7             	mov    %rax,%rdi
    1293:	e8 e8 fd ff ff       	call   1080 <puts@plt>
    1298:	48 8d 45 d4          	lea    -0x2c(%rbp),%rax
    129c:	48 89 c6             	mov    %rax,%rsi
    129f:	48 8d 05 dd 0d 00 00 	lea    0xddd(%rip),%rax        # 2083 <_IO_stdin_used+0x83>
    12a6:	48 89 c7             	mov    %rax,%rdi
    12a9:	b8 00 00 00 00       	mov    $0x0,%eax
    12ae:	e8 fd fd ff ff       	call   10b0 <__isoc99_scanf@plt>
    12b3:	8b 45 d4             	mov    -0x2c(%rbp),%eax
    12b6:	83 f8 02             	cmp    $0x2,%eax
    12b9:	76 07                	jbe    12c2 <main+0x83>
    12bb:	b8 00 00 00 00       	mov    $0x0,%eax
    12c0:	eb 1b                	jmp    12dd <main+0x9e>
    12c2:	8b 45 d4             	mov    -0x2c(%rbp),%eax
    12c5:	89 c0                	mov    %eax,%eax
    12c7:	48 8b 4c c5 e0       	mov    -0x20(%rbp,%rax,8),%rcx
    12cc:	8b 55 dc             	mov    -0x24(%rbp),%edx
    12cf:	8b 45 d8             	mov    -0x28(%rbp),%eax
    12d2:	89 d6                	mov    %edx,%esi
    12d4:	89 c7                	mov    %eax,%edi
    12d6:	ff d1                	call   *%rcx
    12d8:	b8 00 00 00 00       	mov    $0x0,%eax
    12dd:	48 8b 55 f8          	mov    -0x8(%rbp),%rdx
    12e1:	64 48 2b 14 25 28 00 	sub    %fs:0x28,%rdx
    12e8:	00 00 
    12ea:	74 05                	je     12f1 <main+0xb2>
    12ec:	e8 9f fd ff ff       	call   1090 <__stack_chk_fail@plt>
    12f1:	c9                   	leave  
    12f2:	c3                   	ret    

Disassembly of section .fini:

00000000000012f4 <_fini>:
    12f4:	f3 0f 1e fa          	endbr64 
    12f8:	48 83 ec 08          	sub    $0x8,%rsp
    12fc:	48 83 c4 08          	add    $0x8,%rsp
    1300:	c3                   	ret    

Disassembly of section .rodata:

0000000000002000 <_IO_stdin_used>:
    2000:	01 00                	add    %eax,(%rax)
    2002:	02 00                	add    (%rax),%al
    2004:	00 00                	add    %al,(%rax)
    2006:	00 00                	add    %al,(%rax)
    2008:	41                   	rex.B
    2009:	64 64 69 74 69 6f 6e 	fs imul $0x7369206e,%fs:0x6f(%rcx,%rbp,2),%esi
    2010:	20 69 73 
    2013:	20 25 64 0a 00 53    	and    %ah,0x53000a64(%rip)        # 53002a7d <_end+0x52ffea65>
    2019:	75 62                	jne    207d <_IO_stdin_used+0x7d>
    201b:	74 72                	je     208f <__GNU_EH_FRAME_HDR+0x7>
    201d:	61                   	(bad)  
    201e:	63 74 69 6f          	movsxd 0x6f(%rcx,%rbp,2),%esi
    2022:	6e                   	outsb  %ds:(%rsi),(%dx)
    2023:	20 69 73             	and    %ch,0x73(%rcx)
    2026:	20 25 64 0a 00 4d    	and    %ah,0x4d000a64(%rip)        # 4d002a90 <_end+0x4cffea78>
    202c:	75 6c                	jne    209a <__GNU_EH_FRAME_HDR+0x12>
    202e:	74 69                	je     2099 <__GNU_EH_FRAME_HDR+0x11>
    2030:	70 6c                	jo     209e <__GNU_EH_FRAME_HDR+0x16>
    2032:	69 63 61 74 69 6f 6e 	imul   $0x6e6f6974,0x61(%rbx),%esp
    2039:	20 69 73             	and    %ch,0x73(%rcx)
    203c:	20 25 64 0a 00 00    	and    %ah,0xa64(%rip)        # 2aa6 <__FRAME_END__+0x8c6>
    2042:	00 00                	add    %al,(%rax)
    2044:	00 00                	add    %al,(%rax)
    2046:	00 00                	add    %al,(%rax)
    2048:	45 6e                	rex.RB outsb %ds:(%rsi),(%dx)
    204a:	74 65                	je     20b1 <__GNU_EH_FRAME_HDR+0x29>
    204c:	72 20                	jb     206e <_IO_stdin_used+0x6e>
    204e:	43 68 6f 69 63 65    	rex.XB push $0x6563696f
    2054:	3a 20                	cmp    (%rax),%ah
    2056:	30 20                	xor    %ah,(%rax)
    2058:	66 6f                	outsw  %ds:(%rsi),(%dx)
    205a:	72 20                	jb     207c <_IO_stdin_used+0x7c>
    205c:	61                   	(bad)  
    205d:	64 64 2c 20          	fs fs sub $0x20,%al
    2061:	31 20                	xor    %esp,(%rax)
    2063:	66 6f                	outsw  %ds:(%rsi),(%dx)
    2065:	72 20                	jb     2087 <_IO_stdin_used+0x87>
    2067:	73 75                	jae    20de <__GNU_EH_FRAME_HDR+0x56>
    2069:	62                   	(bad)  
    206a:	74 72                	je     20de <__GNU_EH_FRAME_HDR+0x56>
    206c:	61                   	(bad)  
    206d:	63 74 20 61          	movsxd 0x61(%rax,%riz,1),%esi
    2071:	6e                   	outsb  %ds:(%rsi),(%dx)
    2072:	64 20 32             	and    %dh,%fs:(%rdx)
    2075:	20 66 6f             	and    %ah,0x6f(%rsi)
    2078:	72 20                	jb     209a <__GNU_EH_FRAME_HDR+0x12>
    207a:	6d                   	insl   (%dx),%es:(%rdi)
    207b:	75 6c                	jne    20e9 <__GNU_EH_FRAME_HDR+0x61>
    207d:	74 69                	je     20e8 <__GNU_EH_FRAME_HDR+0x60>
    207f:	70 6c                	jo     20ed <__GNU_EH_FRAME_HDR+0x65>
    2081:	79 00                	jns    2083 <_IO_stdin_used+0x83>
    2083:	25                   	.byte 0x25
    2084:	64                   	fs
	...

Disassembly of section .eh_frame_hdr:

0000000000002088 <__GNU_EH_FRAME_HDR>:
    2088:	01 1b                	add    %ebx,(%rbx)
    208a:	03 3b                	add    (%rbx),%edi
    208c:	4c 00 00             	rex.WR add %r8b,(%rax)
    208f:	00 08                	add    %cl,(%rax)
    2091:	00 00                	add    %al,(%rax)
    2093:	00 98 ef ff ff 80    	add    %bl,-0x7f000011(%rax)
    2099:	00 00                	add    %al,(%rax)
    209b:	00 e8                	add    %ch,%al
    209d:	ef                   	out    %eax,(%dx)
    209e:	ff                   	(bad)  
    209f:	ff a8 00 00 00 f8    	ljmp   *-0x8000000(%rax)
    20a5:	ef                   	out    %eax,(%dx)
    20a6:	ff                   	(bad)  
    20a7:	ff c0                	inc    %eax
    20a9:	00 00                	add    %al,(%rax)
    20ab:	00 38                	add    %bh,(%rax)
    20ad:	f0 ff                	lock (bad) 
    20af:	ff 68 00             	ljmp   *0x0(%rax)
    20b2:	00 00                	add    %al,(%rax)
    20b4:	21 f1                	and    %esi,%ecx
    20b6:	ff                   	(bad)  
    20b7:	ff                   	(bad)  
    20b8:	d8 00                	fadds  (%rax)
    20ba:	00 00                	add    %al,(%rax)
    20bc:	54                   	push   %rsp
    20bd:	f1                   	int1   
    20be:	ff                   	(bad)  
    20bf:	ff                   	(bad)  
    20c0:	f8                   	clc    
    20c1:	00 00                	add    %al,(%rax)
    20c3:	00 85 f1 ff ff 18    	add    %al,0x18fffff1(%rbp)
    20c9:	01 00                	add    %eax,(%rax)
    20cb:	00 b7 f1 ff ff 38    	add    %dh,0x38fffff1(%rdi)
    20d1:	01 00                	add    %eax,(%rax)
	...

Disassembly of section .eh_frame:

00000000000020d8 <__FRAME_END__-0x108>:
    20d8:	14 00                	adc    $0x0,%al
    20da:	00 00                	add    %al,(%rax)
    20dc:	00 00                	add    %al,(%rax)
    20de:	00 00                	add    %al,(%rax)
    20e0:	01 7a 52             	add    %edi,0x52(%rdx)
    20e3:	00 01                	add    %al,(%rcx)
    20e5:	78 10                	js     20f7 <__GNU_EH_FRAME_HDR+0x6f>
    20e7:	01 1b                	add    %ebx,(%rbx)
    20e9:	0c 07                	or     $0x7,%al
    20eb:	08 90 01 00 00 14    	or     %dl,0x14000001(%rax)
    20f1:	00 00                	add    %al,(%rax)
    20f3:	00 1c 00             	add    %bl,(%rax,%rax,1)
    20f6:	00 00                	add    %al,(%rax)
    20f8:	c8 ef ff ff          	enter  $0xffef,$0xff
    20fc:	26 00 00             	es add %al,(%rax)
    20ff:	00 00                	add    %al,(%rax)
    2101:	44 07                	rex.R (bad) 
    2103:	10 00                	adc    %al,(%rax)
    2105:	00 00                	add    %al,(%rax)
    2107:	00 24 00             	add    %ah,(%rax,%rax,1)
    210a:	00 00                	add    %al,(%rax)
    210c:	34 00                	xor    $0x0,%al
    210e:	00 00                	add    %al,(%rax)
    2110:	10 ef                	adc    %ch,%bh
    2112:	ff                   	(bad)  
    2113:	ff 50 00             	call   *0x0(%rax)
    2116:	00 00                	add    %al,(%rax)
    2118:	00 0e                	add    %cl,(%rsi)
    211a:	10 46 0e             	adc    %al,0xe(%rsi)
    211d:	18 4a 0f             	sbb    %cl,0xf(%rdx)
    2120:	0b 77 08             	or     0x8(%rdi),%esi
    2123:	80 00 3f             	addb   $0x3f,(%rax)
    2126:	1a 3a                	sbb    (%rdx),%bh
    2128:	2a 33                	sub    (%rbx),%dh
    212a:	24 22                	and    $0x22,%al
    212c:	00 00                	add    %al,(%rax)
    212e:	00 00                	add    %al,(%rax)
    2130:	14 00                	adc    $0x0,%al
    2132:	00 00                	add    %al,(%rax)
    2134:	5c                   	pop    %rsp
    2135:	00 00                	add    %al,(%rax)
    2137:	00 38                	add    %bh,(%rax)
    2139:	ef                   	out    %eax,(%dx)
    213a:	ff                   	(bad)  
    213b:	ff 10                	call   *(%rax)
	...
    2145:	00 00                	add    %al,(%rax)
    2147:	00 14 00             	add    %dl,(%rax,%rax,1)
    214a:	00 00                	add    %al,(%rax)
    214c:	74 00                	je     214e <__GNU_EH_FRAME_HDR+0xc6>
    214e:	00 00                	add    %al,(%rax)
    2150:	30 ef                	xor    %ch,%bh
    2152:	ff                   	(bad)  
    2153:	ff 40 00             	incl   0x0(%rax)
	...
    215e:	00 00                	add    %al,(%rax)
    2160:	1c 00                	sbb    $0x0,%al
    2162:	00 00                	add    %al,(%rax)
    2164:	8c 00                	mov    %es,(%rax)
    2166:	00 00                	add    %al,(%rax)
    2168:	41                   	rex.B
    2169:	f0 ff                	lock (bad) 
    216b:	ff 33                	push   (%rbx)
    216d:	00 00                	add    %al,(%rax)
    216f:	00 00                	add    %al,(%rax)
    2171:	45 0e                	rex.RB (bad) 
    2173:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2179:	6a 0c                	push   $0xc
    217b:	07                   	(bad)  
    217c:	08 00                	or     %al,(%rax)
    217e:	00 00                	add    %al,(%rax)
    2180:	1c 00                	sbb    $0x0,%al
    2182:	00 00                	add    %al,(%rax)
    2184:	ac                   	lods   %ds:(%rsi),%al
    2185:	00 00                	add    %al,(%rax)
    2187:	00 54 f0 ff          	add    %dl,-0x1(%rax,%rsi,8)
    218b:	ff 31                	push   (%rcx)
    218d:	00 00                	add    %al,(%rax)
    218f:	00 00                	add    %al,(%rax)
    2191:	45 0e                	rex.RB (bad) 
    2193:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2199:	68 0c 07 08 00       	push   $0x8070c
    219e:	00 00                	add    %al,(%rax)
    21a0:	1c 00                	sbb    $0x0,%al
    21a2:	00 00                	add    %al,(%rax)
    21a4:	cc                   	int3   
    21a5:	00 00                	add    %al,(%rax)
    21a7:	00 65 f0             	add    %ah,-0x10(%rbp)
    21aa:	ff                   	(bad)  
    21ab:	ff 32                	push   (%rdx)
    21ad:	00 00                	add    %al,(%rax)
    21af:	00 00                	add    %al,(%rax)
    21b1:	45 0e                	rex.RB (bad) 
    21b3:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    21b9:	69 0c 07 08 00 00 00 	imul   $0x8,(%rdi,%rax,1),%ecx
    21c0:	1c 00                	sbb    $0x0,%al
    21c2:	00 00                	add    %al,(%rax)
    21c4:	ec                   	in     (%dx),%al
    21c5:	00 00                	add    %al,(%rax)
    21c7:	00 77 f0             	add    %dh,-0x10(%rdi)
    21ca:	ff                   	(bad)  
    21cb:	ff b4 00 00 00 00 45 	push   0x45000000(%rax,%rax,1)
    21d2:	0e                   	(bad)  
    21d3:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    21d9:	02 ab 0c 07 08 00    	add    0x8070c(%rbx),%ch
	...

00000000000021e0 <__FRAME_END__>:
    21e0:	00 00                	add    %al,(%rax)
	...

Disassembly of section .init_array:

0000000000003da0 <__frame_dummy_init_array_entry>:
    3da0:	a0                   	.byte 0xa0
    3da1:	11 00                	adc    %eax,(%rax)
    3da3:	00 00                	add    %al,(%rax)
    3da5:	00 00                	add    %al,(%rax)
	...

Disassembly of section .fini_array:

0000000000003da8 <__do_global_dtors_aux_fini_array_entry>:
    3da8:	60                   	(bad)  
    3da9:	11 00                	adc    %eax,(%rax)
    3dab:	00 00                	add    %al,(%rax)
    3dad:	00 00                	add    %al,(%rax)
	...

Disassembly of section .dynamic:

0000000000003db0 <_DYNAMIC>:
    3db0:	01 00                	add    %eax,(%rax)
    3db2:	00 00                	add    %al,(%rax)
    3db4:	00 00                	add    %al,(%rax)
    3db6:	00 00                	add    %al,(%rax)
    3db8:	4e 00 00             	rex.WRX add %r8b,(%rax)
    3dbb:	00 00                	add    %al,(%rax)
    3dbd:	00 00                	add    %al,(%rax)
    3dbf:	00 0c 00             	add    %cl,(%rax,%rax,1)
    3dc2:	00 00                	add    %al,(%rax)
    3dc4:	00 00                	add    %al,(%rax)
    3dc6:	00 00                	add    %al,(%rax)
    3dc8:	00 10                	add    %dl,(%rax)
    3dca:	00 00                	add    %al,(%rax)
    3dcc:	00 00                	add    %al,(%rax)
    3dce:	00 00                	add    %al,(%rax)
    3dd0:	0d 00 00 00 00       	or     $0x0,%eax
    3dd5:	00 00                	add    %al,(%rax)
    3dd7:	00 f4                	add    %dh,%ah
    3dd9:	12 00                	adc    (%rax),%al
    3ddb:	00 00                	add    %al,(%rax)
    3ddd:	00 00                	add    %al,(%rax)
    3ddf:	00 19                	add    %bl,(%rcx)
    3de1:	00 00                	add    %al,(%rax)
    3de3:	00 00                	add    %al,(%rax)
    3de5:	00 00                	add    %al,(%rax)
    3de7:	00 a0 3d 00 00 00    	add    %ah,0x3d(%rax)
    3ded:	00 00                	add    %al,(%rax)
    3def:	00 1b                	add    %bl,(%rbx)
    3df1:	00 00                	add    %al,(%rax)
    3df3:	00 00                	add    %al,(%rax)
    3df5:	00 00                	add    %al,(%rax)
    3df7:	00 08                	add    %cl,(%rax)
    3df9:	00 00                	add    %al,(%rax)
    3dfb:	00 00                	add    %al,(%rax)
    3dfd:	00 00                	add    %al,(%rax)
    3dff:	00 1a                	add    %bl,(%rdx)
    3e01:	00 00                	add    %al,(%rax)
    3e03:	00 00                	add    %al,(%rax)
    3e05:	00 00                	add    %al,(%rax)
    3e07:	00 a8 3d 00 00 00    	add    %ch,0x3d(%rax)
    3e0d:	00 00                	add    %al,(%rax)
    3e0f:	00 1c 00             	add    %bl,(%rax,%rax,1)
    3e12:	00 00                	add    %al,(%rax)
    3e14:	00 00                	add    %al,(%rax)
    3e16:	00 00                	add    %al,(%rax)
    3e18:	08 00                	or     %al,(%rax)
    3e1a:	00 00                	add    %al,(%rax)
    3e1c:	00 00                	add    %al,(%rax)
    3e1e:	00 00                	add    %al,(%rax)
    3e20:	f5                   	cmc    
    3e21:	fe                   	(bad)  
    3e22:	ff 6f 00             	ljmp   *0x0(%rdi)
    3e25:	00 00                	add    %al,(%rax)
    3e27:	00 b0 03 00 00 00    	add    %dh,0x3(%rax)
    3e2d:	00 00                	add    %al,(%rax)
    3e2f:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 3e35 <_DYNAMIC+0x85>
    3e35:	00 00                	add    %al,(%rax)
    3e37:	00 c8                	add    %cl,%al
    3e39:	04 00                	add    $0x0,%al
    3e3b:	00 00                	add    %al,(%rax)
    3e3d:	00 00                	add    %al,(%rax)
    3e3f:	00 06                	add    %al,(%rsi)
    3e41:	00 00                	add    %al,(%rax)
    3e43:	00 00                	add    %al,(%rax)
    3e45:	00 00                	add    %al,(%rax)
    3e47:	00 d8                	add    %bl,%al
    3e49:	03 00                	add    (%rax),%eax
    3e4b:	00 00                	add    %al,(%rax)
    3e4d:	00 00                	add    %al,(%rax)
    3e4f:	00 0a                	add    %cl,(%rdx)
    3e51:	00 00                	add    %al,(%rax)
    3e53:	00 00                	add    %al,(%rax)
    3e55:	00 00                	add    %al,(%rax)
    3e57:	00 c8                	add    %cl,%al
    3e59:	00 00                	add    %al,(%rax)
    3e5b:	00 00                	add    %al,(%rax)
    3e5d:	00 00                	add    %al,(%rax)
    3e5f:	00 0b                	add    %cl,(%rbx)
    3e61:	00 00                	add    %al,(%rax)
    3e63:	00 00                	add    %al,(%rax)
    3e65:	00 00                	add    %al,(%rax)
    3e67:	00 18                	add    %bl,(%rax)
    3e69:	00 00                	add    %al,(%rax)
    3e6b:	00 00                	add    %al,(%rax)
    3e6d:	00 00                	add    %al,(%rax)
    3e6f:	00 15 00 00 00 00    	add    %dl,0x0(%rip)        # 3e75 <_DYNAMIC+0xc5>
	...
    3e7d:	00 00                	add    %al,(%rax)
    3e7f:	00 03                	add    %al,(%rbx)
    3e81:	00 00                	add    %al,(%rax)
    3e83:	00 00                	add    %al,(%rax)
    3e85:	00 00                	add    %al,(%rax)
    3e87:	00 a0 3f 00 00 00    	add    %ah,0x3f(%rax)
    3e8d:	00 00                	add    %al,(%rax)
    3e8f:	00 02                	add    %al,(%rdx)
    3e91:	00 00                	add    %al,(%rax)
    3e93:	00 00                	add    %al,(%rax)
    3e95:	00 00                	add    %al,(%rax)
    3e97:	00 60 00             	add    %ah,0x0(%rax)
    3e9a:	00 00                	add    %al,(%rax)
    3e9c:	00 00                	add    %al,(%rax)
    3e9e:	00 00                	add    %al,(%rax)
    3ea0:	14 00                	adc    $0x0,%al
    3ea2:	00 00                	add    %al,(%rax)
    3ea4:	00 00                	add    %al,(%rax)
    3ea6:	00 00                	add    %al,(%rax)
    3ea8:	07                   	(bad)  
    3ea9:	00 00                	add    %al,(%rax)
    3eab:	00 00                	add    %al,(%rax)
    3ead:	00 00                	add    %al,(%rax)
    3eaf:	00 17                	add    %dl,(%rdi)
    3eb1:	00 00                	add    %al,(%rax)
    3eb3:	00 00                	add    %al,(%rax)
    3eb5:	00 00                	add    %al,(%rax)
    3eb7:	00 b8 06 00 00 00    	add    %bh,0x6(%rax)
    3ebd:	00 00                	add    %al,(%rax)
    3ebf:	00 07                	add    %al,(%rdi)
    3ec1:	00 00                	add    %al,(%rax)
    3ec3:	00 00                	add    %al,(%rax)
    3ec5:	00 00                	add    %al,(%rax)
    3ec7:	00 f8                	add    %bh,%al
    3ec9:	05 00 00 00 00       	add    $0x0,%eax
    3ece:	00 00                	add    %al,(%rax)
    3ed0:	08 00                	or     %al,(%rax)
    3ed2:	00 00                	add    %al,(%rax)
    3ed4:	00 00                	add    %al,(%rax)
    3ed6:	00 00                	add    %al,(%rax)
    3ed8:	c0 00 00             	rolb   $0x0,(%rax)
    3edb:	00 00                	add    %al,(%rax)
    3edd:	00 00                	add    %al,(%rax)
    3edf:	00 09                	add    %cl,(%rcx)
    3ee1:	00 00                	add    %al,(%rax)
    3ee3:	00 00                	add    %al,(%rax)
    3ee5:	00 00                	add    %al,(%rax)
    3ee7:	00 18                	add    %bl,(%rax)
    3ee9:	00 00                	add    %al,(%rax)
    3eeb:	00 00                	add    %al,(%rax)
    3eed:	00 00                	add    %al,(%rax)
    3eef:	00 1e                	add    %bl,(%rsi)
    3ef1:	00 00                	add    %al,(%rax)
    3ef3:	00 00                	add    %al,(%rax)
    3ef5:	00 00                	add    %al,(%rax)
    3ef7:	00 08                	add    %cl,(%rax)
    3ef9:	00 00                	add    %al,(%rax)
    3efb:	00 00                	add    %al,(%rax)
    3efd:	00 00                	add    %al,(%rax)
    3eff:	00 fb                	add    %bh,%bl
    3f01:	ff                   	(bad)  
    3f02:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f05:	00 00                	add    %al,(%rax)
    3f07:	00 01                	add    %al,(%rcx)
    3f09:	00 00                	add    %al,(%rax)
    3f0b:	08 00                	or     %al,(%rax)
    3f0d:	00 00                	add    %al,(%rax)
    3f0f:	00 fe                	add    %bh,%dh
    3f11:	ff                   	(bad)  
    3f12:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f15:	00 00                	add    %al,(%rax)
    3f17:	00 a8 05 00 00 00    	add    %ch,0x5(%rax)
    3f1d:	00 00                	add    %al,(%rax)
    3f1f:	00 ff                	add    %bh,%bh
    3f21:	ff                   	(bad)  
    3f22:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f25:	00 00                	add    %al,(%rax)
    3f27:	00 01                	add    %al,(%rcx)
    3f29:	00 00                	add    %al,(%rax)
    3f2b:	00 00                	add    %al,(%rax)
    3f2d:	00 00                	add    %al,(%rax)
    3f2f:	00 f0                	add    %dh,%al
    3f31:	ff                   	(bad)  
    3f32:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f35:	00 00                	add    %al,(%rax)
    3f37:	00 90 05 00 00 00    	add    %dl,0x5(%rax)
    3f3d:	00 00                	add    %al,(%rax)
    3f3f:	00 f9                	add    %bh,%cl
    3f41:	ff                   	(bad)  
    3f42:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f45:	00 00                	add    %al,(%rax)
    3f47:	00 03                	add    %al,(%rbx)
	...

Disassembly of section .got:

0000000000003fa0 <_GLOBAL_OFFSET_TABLE_>:
    3fa0:	b0 3d                	mov    $0x3d,%al
	...
    3fb6:	00 00                	add    %al,(%rax)
    3fb8:	30 10                	xor    %dl,(%rax)
    3fba:	00 00                	add    %al,(%rax)
    3fbc:	00 00                	add    %al,(%rax)
    3fbe:	00 00                	add    %al,(%rax)
    3fc0:	40 10 00             	rex adc %al,(%rax)
    3fc3:	00 00                	add    %al,(%rax)
    3fc5:	00 00                	add    %al,(%rax)
    3fc7:	00 50 10             	add    %dl,0x10(%rax)
    3fca:	00 00                	add    %al,(%rax)
    3fcc:	00 00                	add    %al,(%rax)
    3fce:	00 00                	add    %al,(%rax)
    3fd0:	60                   	(bad)  
    3fd1:	10 00                	adc    %al,(%rax)
	...

Disassembly of section .data:

0000000000004000 <__data_start>:
	...

0000000000004008 <__dso_handle>:
    4008:	08 40 00             	or     %al,0x0(%rax)
    400b:	00 00                	add    %al,(%rax)
    400d:	00 00                	add    %al,(%rax)
	...

Disassembly of section .bss:

0000000000004010 <completed.0>:
	...

Disassembly of section .comment:

0000000000000000 <.comment>:
   0:	47                   	rex.RXB
   1:	43                   	rex.XB
   2:	43 3a 20             	rex.XB cmp (%r8),%spl
   5:	28 55 62             	sub    %dl,0x62(%rbp)
   8:	75 6e                	jne    78 <__abi_tag-0x314>
   a:	74 75                	je     81 <__abi_tag-0x30b>
   c:	20 31                	and    %dh,(%rcx)
   e:	31 2e                	xor    %ebp,(%rsi)
  10:	34 2e                	xor    $0x2e,%al
  12:	30 2d 31 75 62 75    	xor    %ch,0x75627531(%rip)        # 75627549 <_end+0x75623531>
  18:	6e                   	outsb  %ds:(%rsi),(%dx)
  19:	74 75                	je     90 <__abi_tag-0x2fc>
  1b:	31 7e 32             	xor    %edi,0x32(%rsi)
  1e:	32 2e                	xor    (%rsi),%ch
  20:	30 34 29             	xor    %dh,(%rcx,%rbp,1)
  23:	20 31                	and    %dh,(%rcx)
  25:	31 2e                	xor    %ebp,(%rsi)
  27:	34 2e                	xor    $0x2e,%al
  29:	30 00                	xor    %al,(%rax)
