
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
 377:	00 9d 3c 91 4a 24    	add    %bl,0x244a913c(%rbp)
 37d:	bd 5e 47 91 13       	mov    $0x1391475e,%ebp
 382:	f3 a7                	repz cmpsl %es:(%rdi),%ds:(%rsi)
 384:	40                   	rex
 385:	f2 c9                	repnz leave 
 387:	8b d3                	mov    %ebx,%edx
 389:	6f                   	outsl  %ds:(%rsi),(%dx)
 38a:	e1 9a                	loope  326 <__abi_tag-0x66>

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
 3b4:	06                   	(bad)  
 3b5:	00 00                	add    %al,(%rax)
 3b7:	00 01                	add    %al,(%rcx)
 3b9:	00 00                	add    %al,(%rax)
 3bb:	00 06                	add    %al,(%rsi)
 3bd:	00 00                	add    %al,(%rax)
 3bf:	00 00                	add    %al,(%rax)
 3c1:	00 81 00 00 00 00    	add    %al,0x0(%rcx)
 3c7:	00 06                	add    %al,(%rsi)
 3c9:	00 00                	add    %al,(%rax)
 3cb:	00 00                	add    %al,(%rax)
 3cd:	00 00                	add    %al,(%rax)
 3cf:	00 d1                	add    %dl,%cl
 3d1:	65 ce                	gs (bad) 
 3d3:	6d                   	insl   (%dx),%es:(%rdi)

Disassembly of section .dynsym:

00000000000003d8 <.dynsym>:
	...
 3f0:	46 00 00             	rex.RX add %r8b,(%rax)
 3f3:	00 12                	add    %dl,(%rdx)
	...
 405:	00 00                	add    %al,(%rax)
 407:	00 5b 00             	add    %bl,0x0(%rbx)
 40a:	00 00                	add    %al,(%rax)
 40c:	12 00                	adc    (%rax),%al
	...
 41e:	00 00                	add    %al,(%rax)
 420:	10 00                	adc    %al,(%rax)
 422:	00 00                	add    %al,(%rax)
 424:	20 00                	and    %al,(%rax)
	...
 436:	00 00                	add    %al,(%rax)
 438:	01 00                	add    %eax,(%rax)
 43a:	00 00                	add    %al,(%rax)
 43c:	20 00                	and    %al,(%rax)
	...
 44e:	00 00                	add    %al,(%rax)
 450:	2c 00                	sub    $0x0,%al
 452:	00 00                	add    %al,(%rax)
 454:	20 00                	and    %al,(%rax)
	...
 466:	00 00                	add    %al,(%rax)
 468:	4c 00 00             	rex.WR add %r8b,(%rax)
 46b:	00 22                	add    %ah,(%rdx)
	...

Disassembly of section .dynstr:

0000000000000480 <.dynstr>:
 480:	00 5f 5f             	add    %bl,0x5f(%rdi)
 483:	67 6d                	insl   (%dx),%es:(%edi)
 485:	6f                   	outsl  %ds:(%rsi),(%dx)
 486:	6e                   	outsb  %ds:(%rsi),(%dx)
 487:	5f                   	pop    %rdi
 488:	73 74                	jae    4fe <__abi_tag+0x172>
 48a:	61                   	(bad)  
 48b:	72 74                	jb     501 <__abi_tag+0x175>
 48d:	5f                   	pop    %rdi
 48e:	5f                   	pop    %rdi
 48f:	00 5f 49             	add    %bl,0x49(%rdi)
 492:	54                   	push   %rsp
 493:	4d 5f                	rex.WRB pop %r15
 495:	64 65 72 65          	fs gs jb 4fe <__abi_tag+0x172>
 499:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 4a0:	4d 
 4a1:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 4a3:	6f                   	outsl  %ds:(%rsi),(%dx)
 4a4:	6e                   	outsb  %ds:(%rsi),(%dx)
 4a5:	65 54                	gs push %rsp
 4a7:	61                   	(bad)  
 4a8:	62                   	(bad)  
 4a9:	6c                   	insb   (%dx),%es:(%rdi)
 4aa:	65 00 5f 49          	add    %bl,%gs:0x49(%rdi)
 4ae:	54                   	push   %rsp
 4af:	4d 5f                	rex.WRB pop %r15
 4b1:	72 65                	jb     518 <__abi_tag+0x18c>
 4b3:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 4ba:	4d 
 4bb:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 4bd:	6f                   	outsl  %ds:(%rsi),(%dx)
 4be:	6e                   	outsb  %ds:(%rsi),(%dx)
 4bf:	65 54                	gs push %rsp
 4c1:	61                   	(bad)  
 4c2:	62                   	(bad)  
 4c3:	6c                   	insb   (%dx),%es:(%rdi)
 4c4:	65 00 5f 5a          	add    %bl,%gs:0x5a(%rdi)
 4c8:	6e                   	outsb  %ds:(%rsi),(%dx)
 4c9:	61                   	(bad)  
 4ca:	6d                   	insl   (%dx),%es:(%rdi)
 4cb:	00 5f 5f             	add    %bl,0x5f(%rdi)
 4ce:	63 78 61             	movsxd 0x61(%rax),%edi
 4d1:	5f                   	pop    %rdi
 4d2:	66 69 6e 61 6c 69    	imul   $0x696c,0x61(%rsi),%bp
 4d8:	7a 65                	jp     53f <__abi_tag+0x1b3>
 4da:	00 5f 5f             	add    %bl,0x5f(%rdi)
 4dd:	6c                   	insb   (%dx),%es:(%rdi)
 4de:	69 62 63 5f 73 74 61 	imul   $0x6174735f,0x63(%rdx),%esp
 4e5:	72 74                	jb     55b <__abi_tag+0x1cf>
 4e7:	5f                   	pop    %rdi
 4e8:	6d                   	insl   (%dx),%es:(%rdi)
 4e9:	61                   	(bad)  
 4ea:	69 6e 00 6c 69 62 73 	imul   $0x7362696c,0x0(%rsi),%ebp
 4f1:	74 64                	je     557 <__abi_tag+0x1cb>
 4f3:	63 2b                	movsxd (%rbx),%ebp
 4f5:	2b 2e                	sub    (%rsi),%ebp
 4f7:	73 6f                	jae    568 <__abi_tag+0x1dc>
 4f9:	2e 36 00 6c 69 62    	cs ss add %ch,0x62(%rcx,%rbp,2)
 4ff:	63 2e                	movsxd (%rsi),%ebp
 501:	73 6f                	jae    572 <__abi_tag+0x1e6>
 503:	2e 36 00 47 4c       	cs ss add %al,0x4c(%rdi)
 508:	49                   	rex.WB
 509:	42                   	rex.X
 50a:	43 5f                	rex.XB pop %r15
 50c:	32 2e                	xor    (%rsi),%ch
 50e:	33 34 00             	xor    (%rax,%rax,1),%esi
 511:	47                   	rex.RXB
 512:	4c                   	rex.WR
 513:	49                   	rex.WB
 514:	42                   	rex.X
 515:	43 5f                	rex.XB pop %r15
 517:	32 2e                	xor    (%rsi),%ch
 519:	32 2e                	xor    (%rsi),%ch
 51b:	35 00 47 4c 49       	xor    $0x494c4700,%eax
 520:	42                   	rex.X
 521:	43 58                	rex.XB pop %r8
 523:	58                   	pop    %rax
 524:	5f                   	pop    %rdi
 525:	33 2e                	xor    (%rsi),%ebp
 527:	34 00                	xor    $0x0,%al

Disassembly of section .gnu.version:

000000000000052a <.gnu.version>:
 52a:	00 00                	add    %al,(%rax)
 52c:	02 00                	add    (%rax),%al
 52e:	04 00                	add    $0x0,%al
 530:	01 00                	add    %eax,(%rax)
 532:	01 00                	add    %eax,(%rax)
 534:	01 00                	add    %eax,(%rax)
 536:	03 00                	add    (%rax),%eax

Disassembly of section .gnu.version_r:

0000000000000538 <.gnu.version_r>:
 538:	01 00                	add    %eax,(%rax)
 53a:	02 00                	add    (%rax),%al
 53c:	7c 00                	jl     53e <__abi_tag+0x1b2>
 53e:	00 00                	add    %al,(%rax)
 540:	10 00                	adc    %al,(%rax)
 542:	00 00                	add    %al,(%rax)
 544:	30 00                	xor    %al,(%rax)
 546:	00 00                	add    %al,(%rax)
 548:	b4 91                	mov    $0x91,%ah
 54a:	96                   	xchg   %eax,%esi
 54b:	06                   	(bad)  
 54c:	00 00                	add    %al,(%rax)
 54e:	04 00                	add    $0x0,%al
 550:	86 00                	xchg   %al,(%rax)
 552:	00 00                	add    %al,(%rax)
 554:	10 00                	adc    %al,(%rax)
 556:	00 00                	add    %al,(%rax)
 558:	75 1a                	jne    574 <__abi_tag+0x1e8>
 55a:	69 09 00 00 03 00    	imul   $0x30000,(%rcx),%ecx
 560:	91                   	xchg   %eax,%ecx
 561:	00 00                	add    %al,(%rax)
 563:	00 00                	add    %al,(%rax)
 565:	00 00                	add    %al,(%rax)
 567:	00 01                	add    %al,(%rcx)
 569:	00 01                	add    %al,(%rcx)
 56b:	00 6d 00             	add    %ch,0x0(%rbp)
 56e:	00 00                	add    %al,(%rax)
 570:	10 00                	adc    %al,(%rax)
 572:	00 00                	add    %al,(%rax)
 574:	00 00                	add    %al,(%rax)
 576:	00 00                	add    %al,(%rax)
 578:	74 29                	je     5a3 <__abi_tag+0x217>
 57a:	92                   	xchg   %eax,%edx
 57b:	08 00                	or     %al,(%rax)
 57d:	00 02                	add    %al,(%rdx)
 57f:	00 9d 00 00 00 00    	add    %bl,0x0(%rbp)
 585:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rela.dyn:

0000000000000588 <.rela.dyn>:
 588:	a8 3d                	test   $0x3d,%al
 58a:	00 00                	add    %al,(%rax)
 58c:	00 00                	add    %al,(%rax)
 58e:	00 00                	add    %al,(%rax)
 590:	08 00                	or     %al,(%rax)
 592:	00 00                	add    %al,(%rax)
 594:	00 00                	add    %al,(%rax)
 596:	00 00                	add    %al,(%rax)
 598:	40 11 00             	rex adc %eax,(%rax)
 59b:	00 00                	add    %al,(%rax)
 59d:	00 00                	add    %al,(%rax)
 59f:	00 b0 3d 00 00 00    	add    %dh,0x3d(%rax)
 5a5:	00 00                	add    %al,(%rax)
 5a7:	00 08                	add    %cl,(%rax)
	...
 5b1:	11 00                	adc    %eax,(%rax)
 5b3:	00 00                	add    %al,(%rax)
 5b5:	00 00                	add    %al,(%rax)
 5b7:	00 08                	add    %cl,(%rax)
 5b9:	40 00 00             	rex add %al,(%rax)
 5bc:	00 00                	add    %al,(%rax)
 5be:	00 00                	add    %al,(%rax)
 5c0:	08 00                	or     %al,(%rax)
 5c2:	00 00                	add    %al,(%rax)
 5c4:	00 00                	add    %al,(%rax)
 5c6:	00 00                	add    %al,(%rax)
 5c8:	08 40 00             	or     %al,0x0(%rax)
 5cb:	00 00                	add    %al,(%rax)
 5cd:	00 00                	add    %al,(%rax)
 5cf:	00 d8                	add    %bl,%al
 5d1:	3f                   	(bad)  
 5d2:	00 00                	add    %al,(%rax)
 5d4:	00 00                	add    %al,(%rax)
 5d6:	00 00                	add    %al,(%rax)
 5d8:	06                   	(bad)  
 5d9:	00 00                	add    %al,(%rax)
 5db:	00 06                	add    %al,(%rsi)
	...
 5e5:	00 00                	add    %al,(%rax)
 5e7:	00 e0                	add    %ah,%al
 5e9:	3f                   	(bad)  
 5ea:	00 00                	add    %al,(%rax)
 5ec:	00 00                	add    %al,(%rax)
 5ee:	00 00                	add    %al,(%rax)
 5f0:	06                   	(bad)  
 5f1:	00 00                	add    %al,(%rax)
 5f3:	00 02                	add    %al,(%rdx)
	...
 5fd:	00 00                	add    %al,(%rax)
 5ff:	00 e8                	add    %ch,%al
 601:	3f                   	(bad)  
 602:	00 00                	add    %al,(%rax)
 604:	00 00                	add    %al,(%rax)
 606:	00 00                	add    %al,(%rax)
 608:	06                   	(bad)  
 609:	00 00                	add    %al,(%rax)
 60b:	00 03                	add    %al,(%rbx)
	...
 615:	00 00                	add    %al,(%rax)
 617:	00 f0                	add    %dh,%al
 619:	3f                   	(bad)  
 61a:	00 00                	add    %al,(%rax)
 61c:	00 00                	add    %al,(%rax)
 61e:	00 00                	add    %al,(%rax)
 620:	06                   	(bad)  
 621:	00 00                	add    %al,(%rax)
 623:	00 04 00             	add    %al,(%rax,%rax,1)
	...
 62e:	00 00                	add    %al,(%rax)
 630:	f8                   	clc    
 631:	3f                   	(bad)  
 632:	00 00                	add    %al,(%rax)
 634:	00 00                	add    %al,(%rax)
 636:	00 00                	add    %al,(%rax)
 638:	06                   	(bad)  
 639:	00 00                	add    %al,(%rax)
 63b:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 641 <__abi_tag+0x2b5>
 641:	00 00                	add    %al,(%rax)
 643:	00 00                	add    %al,(%rax)
 645:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rela.plt:

0000000000000648 <.rela.plt>:
 648:	d0 3f                	sarb   (%rdi)
 64a:	00 00                	add    %al,(%rax)
 64c:	00 00                	add    %al,(%rax)
 64e:	00 00                	add    %al,(%rax)
 650:	07                   	(bad)  
 651:	00 00                	add    %al,(%rax)
 653:	00 01                	add    %al,(%rcx)
	...

Disassembly of section .init:

0000000000001000 <_init>:
    1000:	f3 0f 1e fa          	endbr64 
    1004:	48 83 ec 08          	sub    $0x8,%rsp
    1008:	48 8b 05 e1 2f 00 00 	mov    0x2fe1(%rip),%rax        # 3ff0 <__gmon_start__@Base>
    100f:	48 85 c0             	test   %rax,%rax
    1012:	74 02                	je     1016 <_init+0x16>
    1014:	ff d0                	call   *%rax
    1016:	48 83 c4 08          	add    $0x8,%rsp
    101a:	c3                   	ret    

Disassembly of section .plt:

0000000000001020 <.plt>:
    1020:	ff 35 9a 2f 00 00    	push   0x2f9a(%rip)        # 3fc0 <_GLOBAL_OFFSET_TABLE_+0x8>
    1026:	f2 ff 25 9b 2f 00 00 	bnd jmp *0x2f9b(%rip)        # 3fc8 <_GLOBAL_OFFSET_TABLE_+0x10>
    102d:	0f 1f 00             	nopl   (%rax)
    1030:	f3 0f 1e fa          	endbr64 
    1034:	68 00 00 00 00       	push   $0x0
    1039:	f2 e9 e1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    103f:	90                   	nop

Disassembly of section .plt.got:

0000000000001040 <__cxa_finalize@plt>:
    1040:	f3 0f 1e fa          	endbr64 
    1044:	f2 ff 25 8d 2f 00 00 	bnd jmp *0x2f8d(%rip)        # 3fd8 <__cxa_finalize@GLIBC_2.2.5>
    104b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .plt.sec:

0000000000001050 <_Znam@plt>:
    1050:	f3 0f 1e fa          	endbr64 
    1054:	f2 ff 25 75 2f 00 00 	bnd jmp *0x2f75(%rip)        # 3fd0 <_Znam@GLIBCXX_3.4>
    105b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .text:

0000000000001060 <_start>:
    1060:	f3 0f 1e fa          	endbr64 
    1064:	31 ed                	xor    %ebp,%ebp
    1066:	49 89 d1             	mov    %rdx,%r9
    1069:	5e                   	pop    %rsi
    106a:	48 89 e2             	mov    %rsp,%rdx
    106d:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
    1071:	50                   	push   %rax
    1072:	54                   	push   %rsp
    1073:	45 31 c0             	xor    %r8d,%r8d
    1076:	31 c9                	xor    %ecx,%ecx
    1078:	48 8d 3d ba 01 00 00 	lea    0x1ba(%rip),%rdi        # 1239 <main>
    107f:	ff 15 5b 2f 00 00    	call   *0x2f5b(%rip)        # 3fe0 <__libc_start_main@GLIBC_2.34>
    1085:	f4                   	hlt    
    1086:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    108d:	00 00 00 

0000000000001090 <deregister_tm_clones>:
    1090:	48 8d 3d 79 2f 00 00 	lea    0x2f79(%rip),%rdi        # 4010 <__TMC_END__>
    1097:	48 8d 05 72 2f 00 00 	lea    0x2f72(%rip),%rax        # 4010 <__TMC_END__>
    109e:	48 39 f8             	cmp    %rdi,%rax
    10a1:	74 15                	je     10b8 <deregister_tm_clones+0x28>
    10a3:	48 8b 05 3e 2f 00 00 	mov    0x2f3e(%rip),%rax        # 3fe8 <_ITM_deregisterTMCloneTable@Base>
    10aa:	48 85 c0             	test   %rax,%rax
    10ad:	74 09                	je     10b8 <deregister_tm_clones+0x28>
    10af:	ff e0                	jmp    *%rax
    10b1:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    10b8:	c3                   	ret    
    10b9:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

00000000000010c0 <register_tm_clones>:
    10c0:	48 8d 3d 49 2f 00 00 	lea    0x2f49(%rip),%rdi        # 4010 <__TMC_END__>
    10c7:	48 8d 35 42 2f 00 00 	lea    0x2f42(%rip),%rsi        # 4010 <__TMC_END__>
    10ce:	48 29 fe             	sub    %rdi,%rsi
    10d1:	48 89 f0             	mov    %rsi,%rax
    10d4:	48 c1 ee 3f          	shr    $0x3f,%rsi
    10d8:	48 c1 f8 03          	sar    $0x3,%rax
    10dc:	48 01 c6             	add    %rax,%rsi
    10df:	48 d1 fe             	sar    %rsi
    10e2:	74 14                	je     10f8 <register_tm_clones+0x38>
    10e4:	48 8b 05 0d 2f 00 00 	mov    0x2f0d(%rip),%rax        # 3ff8 <_ITM_registerTMCloneTable@Base>
    10eb:	48 85 c0             	test   %rax,%rax
    10ee:	74 08                	je     10f8 <register_tm_clones+0x38>
    10f0:	ff e0                	jmp    *%rax
    10f2:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    10f8:	c3                   	ret    
    10f9:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001100 <__do_global_dtors_aux>:
    1100:	f3 0f 1e fa          	endbr64 
    1104:	80 3d 05 2f 00 00 00 	cmpb   $0x0,0x2f05(%rip)        # 4010 <__TMC_END__>
    110b:	75 2b                	jne    1138 <__do_global_dtors_aux+0x38>
    110d:	55                   	push   %rbp
    110e:	48 83 3d c2 2e 00 00 	cmpq   $0x0,0x2ec2(%rip)        # 3fd8 <__cxa_finalize@GLIBC_2.2.5>
    1115:	00 
    1116:	48 89 e5             	mov    %rsp,%rbp
    1119:	74 0c                	je     1127 <__do_global_dtors_aux+0x27>
    111b:	48 8b 3d e6 2e 00 00 	mov    0x2ee6(%rip),%rdi        # 4008 <__dso_handle>
    1122:	e8 19 ff ff ff       	call   1040 <__cxa_finalize@plt>
    1127:	e8 64 ff ff ff       	call   1090 <deregister_tm_clones>
    112c:	c6 05 dd 2e 00 00 01 	movb   $0x1,0x2edd(%rip)        # 4010 <__TMC_END__>
    1133:	5d                   	pop    %rbp
    1134:	c3                   	ret    
    1135:	0f 1f 00             	nopl   (%rax)
    1138:	c3                   	ret    
    1139:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001140 <frame_dummy>:
    1140:	f3 0f 1e fa          	endbr64 
    1144:	e9 77 ff ff ff       	jmp    10c0 <register_tm_clones>

0000000000001149 <_Z12functioncallii>:
    1149:	f3 0f 1e fa          	endbr64 
    114d:	55                   	push   %rbp
    114e:	48 89 e5             	mov    %rsp,%rbp
    1151:	89 7d fc             	mov    %edi,-0x4(%rbp)
    1154:	89 75 f8             	mov    %esi,-0x8(%rbp)
    1157:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
    115e:	eb 0b                	jmp    116b <_Z12functioncallii+0x22>
    1160:	81 45 f8 c8 01 00 00 	addl   $0x1c8,-0x8(%rbp)
    1167:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
    116b:	83 7d fc 0a          	cmpl   $0xa,-0x4(%rbp)
    116f:	7e ef                	jle    1160 <_Z12functioncallii+0x17>
    1171:	83 6d f8 37          	subl   $0x37,-0x8(%rbp)
    1175:	8b 45 f8             	mov    -0x8(%rbp),%eax
    1178:	5d                   	pop    %rbp
    1179:	c3                   	ret    

000000000000117a <_Z7foocallii>:
    117a:	f3 0f 1e fa          	endbr64 
    117e:	55                   	push   %rbp
    117f:	48 89 e5             	mov    %rsp,%rbp
    1182:	48 83 ec 20          	sub    $0x20,%rsp
    1186:	89 7d ec             	mov    %edi,-0x14(%rbp)
    1189:	89 75 e8             	mov    %esi,-0x18(%rbp)
    118c:	bf 28 00 00 00       	mov    $0x28,%edi
    1191:	e8 ba fe ff ff       	call   1050 <_Znam@plt>
    1196:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    119a:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%rbp)
    11a1:	eb 24                	jmp    11c7 <_Z7foocallii+0x4d>
    11a3:	81 45 e8 c8 01 00 00 	addl   $0x1c8,-0x18(%rbp)
    11aa:	8b 45 f4             	mov    -0xc(%rbp),%eax
    11ad:	48 98                	cltq   
    11af:	48 8d 14 85 00 00 00 	lea    0x0(,%rax,4),%rdx
    11b6:	00 
    11b7:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    11bb:	48 01 c2             	add    %rax,%rdx
    11be:	8b 45 e8             	mov    -0x18(%rbp),%eax
    11c1:	89 02                	mov    %eax,(%rdx)
    11c3:	83 45 f4 01          	addl   $0x1,-0xc(%rbp)
    11c7:	83 7d f4 0a          	cmpl   $0xa,-0xc(%rbp)
    11cb:	7e d6                	jle    11a3 <_Z7foocallii+0x29>
    11cd:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    11d1:	48 83 c0 14          	add    $0x14,%rax
    11d5:	8b 00                	mov    (%rax),%eax
    11d7:	48 8b 55 f8          	mov    -0x8(%rbp),%rdx
    11db:	48 83 c2 1c          	add    $0x1c,%rdx
    11df:	8b 12                	mov    (%rdx),%edx
    11e1:	29 d0                	sub    %edx,%eax
    11e3:	89 45 e8             	mov    %eax,-0x18(%rbp)
    11e6:	8b 45 e8             	mov    -0x18(%rbp),%eax
    11e9:	c9                   	leave  
    11ea:	c3                   	ret    

00000000000011eb <_Z3fibi>:
    11eb:	f3 0f 1e fa          	endbr64 
    11ef:	55                   	push   %rbp
    11f0:	48 89 e5             	mov    %rsp,%rbp
    11f3:	53                   	push   %rbx
    11f4:	48 83 ec 18          	sub    $0x18,%rsp
    11f8:	89 7d ec             	mov    %edi,-0x14(%rbp)
    11fb:	83 7d ec 01          	cmpl   $0x1,-0x14(%rbp)
    11ff:	75 07                	jne    1208 <_Z3fibi+0x1d>
    1201:	b8 01 00 00 00       	mov    $0x1,%eax
    1206:	eb 2b                	jmp    1233 <_Z3fibi+0x48>
    1208:	83 7d ec 02          	cmpl   $0x2,-0x14(%rbp)
    120c:	75 07                	jne    1215 <_Z3fibi+0x2a>
    120e:	b8 01 00 00 00       	mov    $0x1,%eax
    1213:	eb 1e                	jmp    1233 <_Z3fibi+0x48>
    1215:	8b 45 ec             	mov    -0x14(%rbp),%eax
    1218:	83 e8 01             	sub    $0x1,%eax
    121b:	89 c7                	mov    %eax,%edi
    121d:	e8 c9 ff ff ff       	call   11eb <_Z3fibi>
    1222:	89 c3                	mov    %eax,%ebx
    1224:	8b 45 ec             	mov    -0x14(%rbp),%eax
    1227:	83 e8 02             	sub    $0x2,%eax
    122a:	89 c7                	mov    %eax,%edi
    122c:	e8 ba ff ff ff       	call   11eb <_Z3fibi>
    1231:	01 d8                	add    %ebx,%eax
    1233:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
    1237:	c9                   	leave  
    1238:	c3                   	ret    

0000000000001239 <main>:
    1239:	f3 0f 1e fa          	endbr64 
    123d:	55                   	push   %rbp
    123e:	48 89 e5             	mov    %rsp,%rbp
    1241:	48 83 ec 20          	sub    $0x20,%rsp
    1245:	89 7d ec             	mov    %edi,-0x14(%rbp)
    1248:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
    124c:	c7 45 f8 01 00 00 00 	movl   $0x1,-0x8(%rbp)
    1253:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
    125a:	8b 55 fc             	mov    -0x4(%rbp),%edx
    125d:	8b 45 f8             	mov    -0x8(%rbp),%eax
    1260:	89 d6                	mov    %edx,%esi
    1262:	89 c7                	mov    %eax,%edi
    1264:	e8 e0 fe ff ff       	call   1149 <_Z12functioncallii>
    1269:	89 45 fc             	mov    %eax,-0x4(%rbp)
    126c:	8b 55 fc             	mov    -0x4(%rbp),%edx
    126f:	8b 45 f8             	mov    -0x8(%rbp),%eax
    1272:	89 d6                	mov    %edx,%esi
    1274:	89 c7                	mov    %eax,%edi
    1276:	e8 ff fe ff ff       	call   117a <_Z7foocallii>
    127b:	89 45 fc             	mov    %eax,-0x4(%rbp)
    127e:	bf 14 00 00 00       	mov    $0x14,%edi
    1283:	e8 63 ff ff ff       	call   11eb <_Z3fibi>
    1288:	90                   	nop
    1289:	c9                   	leave  
    128a:	c3                   	ret    

Disassembly of section .fini:

000000000000128c <_fini>:
    128c:	f3 0f 1e fa          	endbr64 
    1290:	48 83 ec 08          	sub    $0x8,%rsp
    1294:	48 83 c4 08          	add    $0x8,%rsp
    1298:	c3                   	ret    

Disassembly of section .rodata:

0000000000002000 <_IO_stdin_used>:
    2000:	01 00                	add    %eax,(%rax)
    2002:	02 00                	add    (%rax),%al

Disassembly of section .eh_frame_hdr:

0000000000002004 <__GNU_EH_FRAME_HDR>:
    2004:	01 1b                	add    %ebx,(%rbx)
    2006:	03 3b                	add    (%rbx),%edi
    2008:	48 00 00             	rex.W add %al,(%rax)
    200b:	00 08                	add    %cl,(%rax)
    200d:	00 00                	add    %al,(%rax)
    200f:	00 1c f0             	add    %bl,(%rax,%rsi,8)
    2012:	ff                   	(bad)  
    2013:	ff                   	(bad)  
    2014:	7c 00                	jl     2016 <__GNU_EH_FRAME_HDR+0x12>
    2016:	00 00                	add    %al,(%rax)
    2018:	3c f0                	cmp    $0xf0,%al
    201a:	ff                   	(bad)  
    201b:	ff a4 00 00 00 4c f0 	jmp    *-0xfb40000(%rax,%rax,1)
    2022:	ff                   	(bad)  
    2023:	ff                   	(bad)  
    2024:	bc 00 00 00 5c       	mov    $0x5c000000,%esp
    2029:	f0 ff                	lock (bad) 
    202b:	ff 64 00 00          	jmp    *0x0(%rax,%rax,1)
    202f:	00 45 f1             	add    %al,-0xf(%rbp)
    2032:	ff                   	(bad)  
    2033:	ff d4                	call   *%rsp
    2035:	00 00                	add    %al,(%rax)
    2037:	00 76 f1             	add    %dh,-0xf(%rsi)
    203a:	ff                   	(bad)  
    203b:	ff f4                	push   %rsp
    203d:	00 00                	add    %al,(%rax)
    203f:	00 e7                	add    %ah,%bh
    2041:	f1                   	int1   
    2042:	ff                   	(bad)  
    2043:	ff 14 01             	call   *(%rcx,%rax,1)
    2046:	00 00                	add    %al,(%rax)
    2048:	35 f2 ff ff 38       	xor    $0x38fffff2,%eax
    204d:	01 00                	add    %eax,(%rax)
	...

Disassembly of section .eh_frame:

0000000000002050 <__FRAME_END__-0x10c>:
    2050:	14 00                	adc    $0x0,%al
    2052:	00 00                	add    %al,(%rax)
    2054:	00 00                	add    %al,(%rax)
    2056:	00 00                	add    %al,(%rax)
    2058:	01 7a 52             	add    %edi,0x52(%rdx)
    205b:	00 01                	add    %al,(%rcx)
    205d:	78 10                	js     206f <__GNU_EH_FRAME_HDR+0x6b>
    205f:	01 1b                	add    %ebx,(%rbx)
    2061:	0c 07                	or     $0x7,%al
    2063:	08 90 01 00 00 14    	or     %dl,0x14000001(%rax)
    2069:	00 00                	add    %al,(%rax)
    206b:	00 1c 00             	add    %bl,(%rax,%rax,1)
    206e:	00 00                	add    %al,(%rax)
    2070:	f0 ef                	lock out %eax,(%dx)
    2072:	ff                   	(bad)  
    2073:	ff 26                	jmp    *(%rsi)
    2075:	00 00                	add    %al,(%rax)
    2077:	00 00                	add    %al,(%rax)
    2079:	44 07                	rex.R (bad) 
    207b:	10 00                	adc    %al,(%rax)
    207d:	00 00                	add    %al,(%rax)
    207f:	00 24 00             	add    %ah,(%rax,%rax,1)
    2082:	00 00                	add    %al,(%rax)
    2084:	34 00                	xor    $0x0,%al
    2086:	00 00                	add    %al,(%rax)
    2088:	98                   	cwtl   
    2089:	ef                   	out    %eax,(%dx)
    208a:	ff                   	(bad)  
    208b:	ff 20                	jmp    *(%rax)
    208d:	00 00                	add    %al,(%rax)
    208f:	00 00                	add    %al,(%rax)
    2091:	0e                   	(bad)  
    2092:	10 46 0e             	adc    %al,0xe(%rsi)
    2095:	18 4a 0f             	sbb    %cl,0xf(%rdx)
    2098:	0b 77 08             	or     0x8(%rdi),%esi
    209b:	80 00 3f             	addb   $0x3f,(%rax)
    209e:	1a 3a                	sbb    (%rdx),%bh
    20a0:	2a 33                	sub    (%rbx),%dh
    20a2:	24 22                	and    $0x22,%al
    20a4:	00 00                	add    %al,(%rax)
    20a6:	00 00                	add    %al,(%rax)
    20a8:	14 00                	adc    $0x0,%al
    20aa:	00 00                	add    %al,(%rax)
    20ac:	5c                   	pop    %rsp
    20ad:	00 00                	add    %al,(%rax)
    20af:	00 90 ef ff ff 10    	add    %dl,0x10ffffef(%rax)
	...
    20bd:	00 00                	add    %al,(%rax)
    20bf:	00 14 00             	add    %dl,(%rax,%rax,1)
    20c2:	00 00                	add    %al,(%rax)
    20c4:	74 00                	je     20c6 <__GNU_EH_FRAME_HDR+0xc2>
    20c6:	00 00                	add    %al,(%rax)
    20c8:	88 ef                	mov    %ch,%bh
    20ca:	ff                   	(bad)  
    20cb:	ff 10                	call   *(%rax)
	...
    20d5:	00 00                	add    %al,(%rax)
    20d7:	00 1c 00             	add    %bl,(%rax,%rax,1)
    20da:	00 00                	add    %al,(%rax)
    20dc:	8c 00                	mov    %es,(%rax)
    20de:	00 00                	add    %al,(%rax)
    20e0:	69 f0 ff ff 31 00    	imul   $0x31ffff,%eax,%esi
    20e6:	00 00                	add    %al,(%rax)
    20e8:	00 45 0e             	add    %al,0xe(%rbp)
    20eb:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    20f1:	68 0c 07 08 00       	push   $0x8070c
    20f6:	00 00                	add    %al,(%rax)
    20f8:	1c 00                	sbb    $0x0,%al
    20fa:	00 00                	add    %al,(%rax)
    20fc:	ac                   	lods   %ds:(%rsi),%al
    20fd:	00 00                	add    %al,(%rax)
    20ff:	00 7a f0             	add    %bh,-0x10(%rdx)
    2102:	ff                   	(bad)  
    2103:	ff 71 00             	push   0x0(%rcx)
    2106:	00 00                	add    %al,(%rax)
    2108:	00 45 0e             	add    %al,0xe(%rbp)
    210b:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2111:	02 68 0c             	add    0xc(%rax),%ch
    2114:	07                   	(bad)  
    2115:	08 00                	or     %al,(%rax)
    2117:	00 20                	add    %ah,(%rax)
    2119:	00 00                	add    %al,(%rax)
    211b:	00 cc                	add    %cl,%ah
    211d:	00 00                	add    %al,(%rax)
    211f:	00 cb                	add    %cl,%bl
    2121:	f0 ff                	lock (bad) 
    2123:	ff 4e 00             	decl   0x0(%rsi)
    2126:	00 00                	add    %al,(%rax)
    2128:	00 45 0e             	add    %al,0xe(%rbp)
    212b:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2131:	45 83 03 02          	rex.RB addl $0x2,(%r11)
    2135:	40 0c 07             	rex or $0x7,%al
    2138:	08 00                	or     %al,(%rax)
    213a:	00 00                	add    %al,(%rax)
    213c:	1c 00                	sbb    $0x0,%al
    213e:	00 00                	add    %al,(%rax)
    2140:	f0 00 00             	lock add %al,(%rax)
    2143:	00 f5                	add    %dh,%ch
    2145:	f0 ff                	lock (bad) 
    2147:	ff 52 00             	call   *0x0(%rdx)
    214a:	00 00                	add    %al,(%rax)
    214c:	00 45 0e             	add    %al,0xe(%rbp)
    214f:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2155:	02 49 0c             	add    0xc(%rcx),%cl
    2158:	07                   	(bad)  
    2159:	08 00                	or     %al,(%rax)
	...

000000000000215c <__FRAME_END__>:
    215c:	00 00                	add    %al,(%rax)
	...

Disassembly of section .init_array:

0000000000003da8 <__frame_dummy_init_array_entry>:
    3da8:	40 11 00             	rex adc %eax,(%rax)
    3dab:	00 00                	add    %al,(%rax)
    3dad:	00 00                	add    %al,(%rax)
	...

Disassembly of section .fini_array:

0000000000003db0 <__do_global_dtors_aux_fini_array_entry>:
    3db0:	00 11                	add    %dl,(%rcx)
    3db2:	00 00                	add    %al,(%rax)
    3db4:	00 00                	add    %al,(%rax)
	...

Disassembly of section .dynamic:

0000000000003db8 <_DYNAMIC>:
    3db8:	01 00                	add    %eax,(%rax)
    3dba:	00 00                	add    %al,(%rax)
    3dbc:	00 00                	add    %al,(%rax)
    3dbe:	00 00                	add    %al,(%rax)
    3dc0:	6d                   	insl   (%dx),%es:(%rdi)
    3dc1:	00 00                	add    %al,(%rax)
    3dc3:	00 00                	add    %al,(%rax)
    3dc5:	00 00                	add    %al,(%rax)
    3dc7:	00 01                	add    %al,(%rcx)
    3dc9:	00 00                	add    %al,(%rax)
    3dcb:	00 00                	add    %al,(%rax)
    3dcd:	00 00                	add    %al,(%rax)
    3dcf:	00 7c 00 00          	add    %bh,0x0(%rax,%rax,1)
    3dd3:	00 00                	add    %al,(%rax)
    3dd5:	00 00                	add    %al,(%rax)
    3dd7:	00 0c 00             	add    %cl,(%rax,%rax,1)
    3dda:	00 00                	add    %al,(%rax)
    3ddc:	00 00                	add    %al,(%rax)
    3dde:	00 00                	add    %al,(%rax)
    3de0:	00 10                	add    %dl,(%rax)
    3de2:	00 00                	add    %al,(%rax)
    3de4:	00 00                	add    %al,(%rax)
    3de6:	00 00                	add    %al,(%rax)
    3de8:	0d 00 00 00 00       	or     $0x0,%eax
    3ded:	00 00                	add    %al,(%rax)
    3def:	00 8c 12 00 00 00 00 	add    %cl,0x0(%rdx,%rdx,1)
    3df6:	00 00                	add    %al,(%rax)
    3df8:	19 00                	sbb    %eax,(%rax)
    3dfa:	00 00                	add    %al,(%rax)
    3dfc:	00 00                	add    %al,(%rax)
    3dfe:	00 00                	add    %al,(%rax)
    3e00:	a8 3d                	test   $0x3d,%al
    3e02:	00 00                	add    %al,(%rax)
    3e04:	00 00                	add    %al,(%rax)
    3e06:	00 00                	add    %al,(%rax)
    3e08:	1b 00                	sbb    (%rax),%eax
    3e0a:	00 00                	add    %al,(%rax)
    3e0c:	00 00                	add    %al,(%rax)
    3e0e:	00 00                	add    %al,(%rax)
    3e10:	08 00                	or     %al,(%rax)
    3e12:	00 00                	add    %al,(%rax)
    3e14:	00 00                	add    %al,(%rax)
    3e16:	00 00                	add    %al,(%rax)
    3e18:	1a 00                	sbb    (%rax),%al
    3e1a:	00 00                	add    %al,(%rax)
    3e1c:	00 00                	add    %al,(%rax)
    3e1e:	00 00                	add    %al,(%rax)
    3e20:	b0 3d                	mov    $0x3d,%al
    3e22:	00 00                	add    %al,(%rax)
    3e24:	00 00                	add    %al,(%rax)
    3e26:	00 00                	add    %al,(%rax)
    3e28:	1c 00                	sbb    $0x0,%al
    3e2a:	00 00                	add    %al,(%rax)
    3e2c:	00 00                	add    %al,(%rax)
    3e2e:	00 00                	add    %al,(%rax)
    3e30:	08 00                	or     %al,(%rax)
    3e32:	00 00                	add    %al,(%rax)
    3e34:	00 00                	add    %al,(%rax)
    3e36:	00 00                	add    %al,(%rax)
    3e38:	f5                   	cmc    
    3e39:	fe                   	(bad)  
    3e3a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3e3d:	00 00                	add    %al,(%rax)
    3e3f:	00 b0 03 00 00 00    	add    %dh,0x3(%rax)
    3e45:	00 00                	add    %al,(%rax)
    3e47:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 3e4d <_DYNAMIC+0x95>
    3e4d:	00 00                	add    %al,(%rax)
    3e4f:	00 80 04 00 00 00    	add    %al,0x4(%rax)
    3e55:	00 00                	add    %al,(%rax)
    3e57:	00 06                	add    %al,(%rsi)
    3e59:	00 00                	add    %al,(%rax)
    3e5b:	00 00                	add    %al,(%rax)
    3e5d:	00 00                	add    %al,(%rax)
    3e5f:	00 d8                	add    %bl,%al
    3e61:	03 00                	add    (%rax),%eax
    3e63:	00 00                	add    %al,(%rax)
    3e65:	00 00                	add    %al,(%rax)
    3e67:	00 0a                	add    %cl,(%rdx)
    3e69:	00 00                	add    %al,(%rax)
    3e6b:	00 00                	add    %al,(%rax)
    3e6d:	00 00                	add    %al,(%rax)
    3e6f:	00 a9 00 00 00 00    	add    %ch,0x0(%rcx)
    3e75:	00 00                	add    %al,(%rax)
    3e77:	00 0b                	add    %cl,(%rbx)
    3e79:	00 00                	add    %al,(%rax)
    3e7b:	00 00                	add    %al,(%rax)
    3e7d:	00 00                	add    %al,(%rax)
    3e7f:	00 18                	add    %bl,(%rax)
    3e81:	00 00                	add    %al,(%rax)
    3e83:	00 00                	add    %al,(%rax)
    3e85:	00 00                	add    %al,(%rax)
    3e87:	00 15 00 00 00 00    	add    %dl,0x0(%rip)        # 3e8d <_DYNAMIC+0xd5>
	...
    3e95:	00 00                	add    %al,(%rax)
    3e97:	00 03                	add    %al,(%rbx)
    3e99:	00 00                	add    %al,(%rax)
    3e9b:	00 00                	add    %al,(%rax)
    3e9d:	00 00                	add    %al,(%rax)
    3e9f:	00 b8 3f 00 00 00    	add    %bh,0x3f(%rax)
    3ea5:	00 00                	add    %al,(%rax)
    3ea7:	00 02                	add    %al,(%rdx)
    3ea9:	00 00                	add    %al,(%rax)
    3eab:	00 00                	add    %al,(%rax)
    3ead:	00 00                	add    %al,(%rax)
    3eaf:	00 18                	add    %bl,(%rax)
    3eb1:	00 00                	add    %al,(%rax)
    3eb3:	00 00                	add    %al,(%rax)
    3eb5:	00 00                	add    %al,(%rax)
    3eb7:	00 14 00             	add    %dl,(%rax,%rax,1)
    3eba:	00 00                	add    %al,(%rax)
    3ebc:	00 00                	add    %al,(%rax)
    3ebe:	00 00                	add    %al,(%rax)
    3ec0:	07                   	(bad)  
    3ec1:	00 00                	add    %al,(%rax)
    3ec3:	00 00                	add    %al,(%rax)
    3ec5:	00 00                	add    %al,(%rax)
    3ec7:	00 17                	add    %dl,(%rdi)
    3ec9:	00 00                	add    %al,(%rax)
    3ecb:	00 00                	add    %al,(%rax)
    3ecd:	00 00                	add    %al,(%rax)
    3ecf:	00 48 06             	add    %cl,0x6(%rax)
    3ed2:	00 00                	add    %al,(%rax)
    3ed4:	00 00                	add    %al,(%rax)
    3ed6:	00 00                	add    %al,(%rax)
    3ed8:	07                   	(bad)  
    3ed9:	00 00                	add    %al,(%rax)
    3edb:	00 00                	add    %al,(%rax)
    3edd:	00 00                	add    %al,(%rax)
    3edf:	00 88 05 00 00 00    	add    %cl,0x5(%rax)
    3ee5:	00 00                	add    %al,(%rax)
    3ee7:	00 08                	add    %cl,(%rax)
    3ee9:	00 00                	add    %al,(%rax)
    3eeb:	00 00                	add    %al,(%rax)
    3eed:	00 00                	add    %al,(%rax)
    3eef:	00 c0                	add    %al,%al
    3ef1:	00 00                	add    %al,(%rax)
    3ef3:	00 00                	add    %al,(%rax)
    3ef5:	00 00                	add    %al,(%rax)
    3ef7:	00 09                	add    %cl,(%rcx)
    3ef9:	00 00                	add    %al,(%rax)
    3efb:	00 00                	add    %al,(%rax)
    3efd:	00 00                	add    %al,(%rax)
    3eff:	00 18                	add    %bl,(%rax)
    3f01:	00 00                	add    %al,(%rax)
    3f03:	00 00                	add    %al,(%rax)
    3f05:	00 00                	add    %al,(%rax)
    3f07:	00 1e                	add    %bl,(%rsi)
    3f09:	00 00                	add    %al,(%rax)
    3f0b:	00 00                	add    %al,(%rax)
    3f0d:	00 00                	add    %al,(%rax)
    3f0f:	00 08                	add    %cl,(%rax)
    3f11:	00 00                	add    %al,(%rax)
    3f13:	00 00                	add    %al,(%rax)
    3f15:	00 00                	add    %al,(%rax)
    3f17:	00 fb                	add    %bh,%bl
    3f19:	ff                   	(bad)  
    3f1a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f1d:	00 00                	add    %al,(%rax)
    3f1f:	00 01                	add    %al,(%rcx)
    3f21:	00 00                	add    %al,(%rax)
    3f23:	08 00                	or     %al,(%rax)
    3f25:	00 00                	add    %al,(%rax)
    3f27:	00 fe                	add    %bh,%dh
    3f29:	ff                   	(bad)  
    3f2a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f2d:	00 00                	add    %al,(%rax)
    3f2f:	00 38                	add    %bh,(%rax)
    3f31:	05 00 00 00 00       	add    $0x0,%eax
    3f36:	00 00                	add    %al,(%rax)
    3f38:	ff                   	(bad)  
    3f39:	ff                   	(bad)  
    3f3a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f3d:	00 00                	add    %al,(%rax)
    3f3f:	00 02                	add    %al,(%rdx)
    3f41:	00 00                	add    %al,(%rax)
    3f43:	00 00                	add    %al,(%rax)
    3f45:	00 00                	add    %al,(%rax)
    3f47:	00 f0                	add    %dh,%al
    3f49:	ff                   	(bad)  
    3f4a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f4d:	00 00                	add    %al,(%rax)
    3f4f:	00 2a                	add    %ch,(%rdx)
    3f51:	05 00 00 00 00       	add    $0x0,%eax
    3f56:	00 00                	add    %al,(%rax)
    3f58:	f9                   	stc    
    3f59:	ff                   	(bad)  
    3f5a:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f5d:	00 00                	add    %al,(%rax)
    3f5f:	00 03                	add    %al,(%rbx)
	...

Disassembly of section .got:

0000000000003fb8 <_GLOBAL_OFFSET_TABLE_>:
    3fb8:	b8 3d 00 00 00       	mov    $0x3d,%eax
	...
    3fcd:	00 00                	add    %al,(%rax)
    3fcf:	00 30                	add    %dh,(%rax)
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

Disassembly of section .debug_aranges:

0000000000000000 <.debug_aranges>:
   0:	2c 00                	sub    $0x0,%al
   2:	00 00                	add    %al,(%rax)
   4:	02 00                	add    (%rax),%al
   6:	00 00                	add    %al,(%rax)
   8:	00 00                	add    %al,(%rax)
   a:	08 00                	or     %al,(%rax)
   c:	00 00                	add    %al,(%rax)
   e:	00 00                	add    %al,(%rax)
  10:	49 11 00             	adc    %rax,(%r8)
  13:	00 00                	add    %al,(%rax)
  15:	00 00                	add    %al,(%rax)
  17:	00 42 01             	add    %al,0x1(%rdx)
	...

Disassembly of section .debug_info:

0000000000000000 <.debug_info>:
   0:	d1 01                	roll   (%rcx)
   2:	00 00                	add    %al,(%rax)
   4:	05 00 01 08 00       	add    $0x80100,%eax
   9:	00 00                	add    %al,(%rax)
   b:	00 06                	add    %al,(%rsi)
   d:	00 00                	add    %al,(%rax)
   f:	00 00                	add    %al,(%rax)
  11:	21 20                	and    %esp,(%rax)
  13:	00 00                	add    %al,(%rax)
  15:	00 00                	add    %al,(%rax)
  17:	00 00                	add    %al,(%rax)
  19:	00 49 11             	add    %cl,0x11(%rcx)
  1c:	00 00                	add    %al,(%rax)
  1e:	00 00                	add    %al,(%rax)
  20:	00 00                	add    %al,(%rax)
  22:	42 01 00             	rex.X add %eax,(%rax)
	...
  2d:	00 01                	add    %al,(%rcx)
  2f:	08 07                	or     %al,(%rdi)
  31:	d9 00                	flds   (%rax)
  33:	00 00                	add    %al,(%rax)
  35:	01 04 07             	add    %eax,(%rdi,%rax,1)
  38:	de 00                	fiadds (%rax)
  3a:	00 00                	add    %al,(%rax)
  3c:	01 01                	add    %eax,(%rcx)
  3e:	08 bd 00 00 00 01    	or     %bh,0x1000000(%rbp)
  44:	02 07                	add    (%rdi),%al
  46:	0a 01                	or     (%rcx),%al
  48:	00 00                	add    %al,(%rax)
  4a:	01 01                	add    %eax,(%rcx)
  4c:	06                   	(bad)  
  4d:	bf 00 00 00 01       	mov    $0x1000000,%edi
  52:	02 05 2f 01 00 00    	add    0x12f(%rip),%al        # 187 <__abi_tag-0x205>
  58:	07                   	(bad)  
  59:	04 05                	add    $0x5,%al
  5b:	69 6e 74 00 01 08 05 	imul   $0x5080100,0x74(%rsi),%ebp
  62:	d0 00                	rolb   (%rax)
  64:	00 00                	add    %al,(%rax)
  66:	03 6b 00             	add    0x0(%rbx),%ebp
  69:	00 00                	add    %al,(%rax)
  6b:	01 01                	add    %eax,(%rcx)
  6d:	06                   	(bad)  
  6e:	c6 00 00             	movb   $0x0,(%rax)
  71:	00 01                	add    %al,(%rcx)
  73:	20 03                	and    %al,(%rbx)
  75:	f8                   	clc    
  76:	00 00                	add    %al,(%rax)
  78:	00 01                	add    %al,(%rcx)
  7a:	10 04 90             	adc    %al,(%rax,%rdx,4)
  7d:	00 00                	add    %al,(%rax)
  7f:	00 01                	add    %al,(%rcx)
  81:	04 04                	add    $0x4,%al
  83:	29 01                	sub    %eax,(%rcx)
  85:	00 00                	add    %al,(%rax)
  87:	01 08                	add    %ecx,(%rax)
  89:	04 22                	add    $0x22,%al
  8b:	01 00                	add    %eax,(%rax)
  8d:	00 01                	add    %al,(%rcx)
  8f:	10 04 1d 01 00 00 08 	adc    %al,0x8000001(,%rbx,1)
  96:	cb                   	lret   
  97:	00 00                	add    %al,(%rax)
  99:	00 01                	add    %al,(%rcx)
  9b:	20 05 58 00 00 00    	and    %al,0x58(%rip)        # f9 <__abi_tag-0x293>
  a1:	39 12                	cmp    %edx,(%rdx)
  a3:	00 00                	add    %al,(%rax)
  a5:	00 00                	add    %al,(%rax)
  a7:	00 00                	add    %al,(%rax)
  a9:	52                   	push   %rdx
  aa:	00 00                	add    %al,(%rax)
  ac:	00 00                	add    %al,(%rax)
  ae:	00 00                	add    %al,(%rax)
  b0:	00 01                	add    %al,(%rcx)
  b2:	9c                   	pushf  
  b3:	ec                   	in     (%dx),%al
  b4:	00 00                	add    %al,(%rax)
  b6:	00 05 9b 00 00 00    	add    %al,0x9b(%rip)        # 157 <__abi_tag-0x235>
  bc:	0e                   	(bad)  
  bd:	58                   	pop    %rax
  be:	00 00                	add    %al,(%rax)
  c0:	00 02                	add    %al,(%rdx)
  c2:	91                   	xchg   %eax,%ecx
  c3:	5c                   	pop    %rsp
  c4:	05 39 01 00 00       	add    $0x139,%eax
  c9:	1a ec                	sbb    %ah,%ch
  cb:	00 00                	add    %al,(%rax)
  cd:	00 02                	add    %al,(%rdx)
  cf:	91                   	xchg   %eax,%ecx
  d0:	50                   	push   %rax
  d1:	04 69                	add    $0x69,%al
  d3:	00 21                	add    %ah,(%rcx)
  d5:	09 58 00             	or     %ebx,0x0(%rax)
  d8:	00 00                	add    %al,(%rax)
  da:	02 91 68 04 73 75    	add    0x75730468(%rcx),%dl
  e0:	6d                   	insl   (%dx),%es:(%rdi)
  e1:	00 22                	add    %ah,(%rdx)
  e3:	09 58 00             	or     %ebx,0x0(%rax)
  e6:	00 00                	add    %al,(%rax)
  e8:	02 91 6c 00 03 66    	add    0x6603006c(%rcx),%dl
  ee:	00 00                	add    %al,(%rax)
  f0:	00 09                	add    %cl,(%rcx)
  f2:	66 69 62 00 01 16    	imul   $0x1601,0x0(%rdx),%sp
  f8:	05 b5 00 00 00       	add    $0xb5,%eax
  fd:	58                   	pop    %rax
  fe:	00 00                	add    %al,(%rax)
 100:	00 eb                	add    %ch,%bl
 102:	11 00                	adc    %eax,(%rax)
 104:	00 00                	add    %al,(%rax)
 106:	00 00                	add    %al,(%rax)
 108:	00 4e 00             	add    %cl,0x0(%rsi)
 10b:	00 00                	add    %al,(%rax)
 10d:	00 00                	add    %al,(%rax)
 10f:	00 00                	add    %al,(%rax)
 111:	01 9c 24 01 00 00 02 	add    %ebx,0x2000001(%rsp)
 118:	6e                   	outsb  %ds:(%rsi),(%dx)
 119:	00 16                	add    %dl,(%rsi)
 11b:	0d 58 00 00 00       	or     $0x58,%eax
 120:	02 91 5c 00 0a ad    	add    -0x52f5ffa4(%rcx),%dl
 126:	00 00                	add    %al,(%rax)
 128:	00 01                	add    %al,(%rcx)
 12a:	0b 05 a0 00 00 00    	or     0xa0(%rip),%eax        # 1d0 <__abi_tag-0x1bc>
 130:	58                   	pop    %rax
 131:	00 00                	add    %al,(%rax)
 133:	00 7a 11             	add    %bh,0x11(%rdx)
 136:	00 00                	add    %al,(%rax)
 138:	00 00                	add    %al,(%rax)
 13a:	00 00                	add    %al,(%rax)
 13c:	71 00                	jno    13e <__abi_tag-0x24e>
 13e:	00 00                	add    %al,(%rax)
 140:	00 00                	add    %al,(%rax)
 142:	00 00                	add    %al,(%rax)
 144:	01 9c 92 01 00 00 02 	add    %ebx,0x2000001(%rdx,%rdx,4)
 14b:	69 00 0b 11 58 00    	imul   $0x58110b,(%rax),%eax
 151:	00 00                	add    %al,(%rax)
 153:	02 91 5c 02 73 75    	add    0x7573025c(%rcx),%dl
 159:	6d                   	insl   (%dx),%es:(%rdi)
 15a:	00 0b                	add    %cl,(%rbx)
 15c:	18 58 00             	sbb    %bl,0x0(%rax)
 15f:	00 00                	add    %al,(%rax)
 161:	02 91 58 0b 04 01    	add    0x1040b58(%rcx),%dl
 167:	00 00                	add    %al,(%rax)
 169:	01 0c 0a             	add    %ecx,(%rdx,%rcx,1)
 16c:	92                   	xchg   %eax,%edx
 16d:	01 00                	add    %eax,(%rax)
 16f:	00 02                	add    %al,(%rdx)
 171:	91                   	xchg   %eax,%ecx
 172:	68 0c 9a 11 00       	push   $0x119a0c
 177:	00 00                	add    %al,(%rax)
 179:	00 00                	add    %al,(%rax)
 17b:	00 33                	add    %dh,(%rbx)
 17d:	00 00                	add    %al,(%rax)
 17f:	00 00                	add    %al,(%rax)
 181:	00 00                	add    %al,(%rax)
 183:	00 04 69             	add    %al,(%rcx,%rbp,2)
 186:	00 0e                	add    %cl,(%rsi)
 188:	0d 58 00 00 00       	or     $0x58,%eax
 18d:	02 91 64 00 00 03    	add    0x3000064(%rcx),%dl
 193:	58                   	pop    %rax
 194:	00 00                	add    %al,(%rax)
 196:	00 0d eb 00 00 00    	add    %cl,0xeb(%rip)        # 287 <__abi_tag-0x105>
 19c:	01 03                	add    %eax,(%rbx)
 19e:	05 3e 01 00 00       	add    $0x13e,%eax
 1a3:	58                   	pop    %rax
 1a4:	00 00                	add    %al,(%rax)
 1a6:	00 49 11             	add    %cl,0x11(%rcx)
 1a9:	00 00                	add    %al,(%rax)
 1ab:	00 00                	add    %al,(%rax)
 1ad:	00 00                	add    %al,(%rax)
 1af:	31 00                	xor    %eax,(%rax)
 1b1:	00 00                	add    %al,(%rax)
 1b3:	00 00                	add    %al,(%rax)
 1b5:	00 00                	add    %al,(%rax)
 1b7:	01 9c 02 69 00 03 16 	add    %ebx,0x16030069(%rdx,%rax,1)
 1be:	58                   	pop    %rax
 1bf:	00 00                	add    %al,(%rax)
 1c1:	00 02                	add    %al,(%rdx)
 1c3:	91                   	xchg   %eax,%ecx
 1c4:	6c                   	insb   (%dx),%es:(%rdi)
 1c5:	02 73 75             	add    0x75(%rbx),%dh
 1c8:	6d                   	insl   (%dx),%es:(%rdi)
 1c9:	00 03                	add    %al,(%rbx)
 1cb:	1d 58 00 00 00       	sbb    $0x58,%eax
 1d0:	02                   	.byte 0x2
 1d1:	91                   	xchg   %eax,%ecx
 1d2:	68                   	.byte 0x68
	...

Disassembly of section .debug_abbrev:

0000000000000000 <.debug_abbrev>:
   0:	01 24 00             	add    %esp,(%rax,%rax,1)
   3:	0b 0b                	or     (%rbx),%ecx
   5:	3e 0b 03             	ds or  (%rbx),%eax
   8:	0e                   	(bad)  
   9:	00 00                	add    %al,(%rax)
   b:	02 05 00 03 08 3a    	add    0x3a080300(%rip),%al        # 3a080311 <_end+0x3a07c2f9>
  11:	21 01                	and    %eax,(%rcx)
  13:	3b 0b                	cmp    (%rbx),%ecx
  15:	39 0b                	cmp    %ecx,(%rbx)
  17:	49 13 02             	adc    (%r10),%rax
  1a:	18 00                	sbb    %al,(%rax)
  1c:	00 03                	add    %al,(%rbx)
  1e:	0f 00 0b             	str    (%rbx)
  21:	21 08                	and    %ecx,(%rax)
  23:	49 13 00             	adc    (%r8),%rax
  26:	00 04 34             	add    %al,(%rsp,%rsi,1)
  29:	00 03                	add    %al,(%rbx)
  2b:	08 3a                	or     %bh,(%rdx)
  2d:	21 01                	and    %eax,(%rcx)
  2f:	3b 0b                	cmp    (%rbx),%ecx
  31:	39 0b                	cmp    %ecx,(%rbx)
  33:	49 13 02             	adc    (%r10),%rax
  36:	18 00                	sbb    %al,(%rax)
  38:	00 05 05 00 03 0e    	add    %al,0xe030005(%rip)        # e030043 <_end+0xe02c02b>
  3e:	3a 21                	cmp    (%rcx),%ah
  40:	01 3b                	add    %edi,(%rbx)
  42:	21 20                	and    %esp,(%rax)
  44:	39 0b                	cmp    %ecx,(%rbx)
  46:	49 13 02             	adc    (%r10),%rax
  49:	18 00                	sbb    %al,(%rax)
  4b:	00 06                	add    %al,(%rsi)
  4d:	11 01                	adc    %eax,(%rcx)
  4f:	25 0e 13 0b 03       	and    $0x30b130e,%eax
  54:	1f                   	(bad)  
  55:	1b 1f                	sbb    (%rdi),%ebx
  57:	11 01                	adc    %eax,(%rcx)
  59:	12 07                	adc    (%rdi),%al
  5b:	10 17                	adc    %dl,(%rdi)
  5d:	00 00                	add    %al,(%rax)
  5f:	07                   	(bad)  
  60:	24 00                	and    $0x0,%al
  62:	0b 0b                	or     (%rbx),%ecx
  64:	3e 0b 03             	ds or  (%rbx),%eax
  67:	08 00                	or     %al,(%rax)
  69:	00 08                	add    %cl,(%rax)
  6b:	2e 01 3f             	cs add %edi,(%rdi)
  6e:	19 03                	sbb    %eax,(%rbx)
  70:	0e                   	(bad)  
  71:	3a 0b                	cmp    (%rbx),%cl
  73:	3b 0b                	cmp    (%rbx),%ecx
  75:	39 0b                	cmp    %ecx,(%rbx)
  77:	49 13 11             	adc    (%r9),%rdx
  7a:	01 12                	add    %edx,(%rdx)
  7c:	07                   	(bad)  
  7d:	40 18 7c 19 01       	sbb    %dil,0x1(%rcx,%rbx,1)
  82:	13 00                	adc    (%rax),%eax
  84:	00 09                	add    %cl,(%rcx)
  86:	2e 01 3f             	cs add %edi,(%rdi)
  89:	19 03                	sbb    %eax,(%rbx)
  8b:	08 3a                	or     %bh,(%rdx)
  8d:	0b 3b                	or     (%rbx),%edi
  8f:	0b 39                	or     (%rcx),%edi
  91:	0b 6e 0e             	or     0xe(%rsi),%ebp
  94:	49 13 11             	adc    (%r9),%rdx
  97:	01 12                	add    %edx,(%rdx)
  99:	07                   	(bad)  
  9a:	40 18 7c 19 01       	sbb    %dil,0x1(%rcx,%rbx,1)
  9f:	13 00                	adc    (%rax),%eax
  a1:	00 0a                	add    %cl,(%rdx)
  a3:	2e 01 3f             	cs add %edi,(%rdi)
  a6:	19 03                	sbb    %eax,(%rbx)
  a8:	0e                   	(bad)  
  a9:	3a 0b                	cmp    (%rbx),%cl
  ab:	3b 0b                	cmp    (%rbx),%ecx
  ad:	39 0b                	cmp    %ecx,(%rbx)
  af:	6e                   	outsb  %ds:(%rsi),(%dx)
  b0:	0e                   	(bad)  
  b1:	49 13 11             	adc    (%r9),%rdx
  b4:	01 12                	add    %edx,(%rdx)
  b6:	07                   	(bad)  
  b7:	40 18 7c 19 01       	sbb    %dil,0x1(%rcx,%rbx,1)
  bc:	13 00                	adc    (%rax),%eax
  be:	00 0b                	add    %cl,(%rbx)
  c0:	34 00                	xor    $0x0,%al
  c2:	03 0e                	add    (%rsi),%ecx
  c4:	3a 0b                	cmp    (%rbx),%cl
  c6:	3b 0b                	cmp    (%rbx),%ecx
  c8:	39 0b                	cmp    %ecx,(%rbx)
  ca:	49 13 02             	adc    (%r10),%rax
  cd:	18 00                	sbb    %al,(%rax)
  cf:	00 0c 0b             	add    %cl,(%rbx,%rcx,1)
  d2:	01 11                	add    %edx,(%rcx)
  d4:	01 12                	add    %edx,(%rdx)
  d6:	07                   	(bad)  
  d7:	00 00                	add    %al,(%rax)
  d9:	0d 2e 01 3f 19       	or     $0x193f012e,%eax
  de:	03 0e                	add    (%rsi),%ecx
  e0:	3a 0b                	cmp    (%rbx),%cl
  e2:	3b 0b                	cmp    (%rbx),%ecx
  e4:	39 0b                	cmp    %ecx,(%rbx)
  e6:	6e                   	outsb  %ds:(%rsi),(%dx)
  e7:	0e                   	(bad)  
  e8:	49 13 11             	adc    (%r9),%rdx
  eb:	01 12                	add    %edx,(%rdx)
  ed:	07                   	(bad)  
  ee:	40 18 7a 19          	sbb    %dil,0x19(%rdx)
  f2:	00 00                	add    %al,(%rax)
	...

Disassembly of section .debug_line:

0000000000000000 <.debug_line>:
   0:	e3 00                	jrcxz  2 <__abi_tag-0x38a>
   2:	00 00                	add    %al,(%rax)
   4:	05 00 08 00 2a       	add    $0x2a000800,%eax
   9:	00 00                	add    %al,(%rax)
   b:	00 01                	add    %al,(%rcx)
   d:	01 01                	add    %eax,(%rcx)
   f:	fb                   	sti    
  10:	0e                   	(bad)  
  11:	0d 00 01 01 01       	or     $0x1010100,%eax
  16:	01 00                	add    %eax,(%rax)
  18:	00 00                	add    %al,(%rax)
  1a:	01 00                	add    %eax,(%rax)
  1c:	00 01                	add    %al,(%rcx)
  1e:	01 01                	add    %eax,(%rcx)
  20:	1f                   	(bad)  
  21:	01 00                	add    %eax,(%rax)
  23:	00 00                	add    %al,(%rax)
  25:	00 02                	add    %al,(%rdx)
  27:	01 1f                	add    %ebx,(%rdi)
  29:	02 0f                	add    (%rdi),%cl
  2b:	02 20                	add    (%rax),%ah
  2d:	00 00                	add    %al,(%rax)
  2f:	00 00                	add    %al,(%rax)
  31:	20 00                	and    %al,(%rax)
  33:	00 00                	add    %al,(%rax)
  35:	00 05 22 00 09 02    	add    %al,0x2090022(%rip)        # 209005d <_end+0x208c045>
  3b:	49 11 00             	adc    %rax,(%r8)
  3e:	00 00                	add    %al,(%rax)
  40:	00 00                	add    %al,(%rax)
  42:	00 14 05 0b d7 05 05 	add    %dl,0x505d70b(,%rax,1)
  49:	74 05                	je     50 <__abi_tag-0x33c>
  4b:	0d 00 02 04 03       	or     $0x3040200,%eax
  50:	2f                   	(bad)  
  51:	05 05 00 02 04       	add    $0x4020005,%eax
  56:	03 73 05             	add    0x5(%rbx),%esi
  59:	13 00                	adc    (%rax),%eax
  5b:	02 04 01             	add    (%rcx,%rax,1),%al
  5e:	4a 05 09 69 05 0c    	rex.WX add $0xc056909,%rax
  64:	4b 05 01 3d 05 1d    	rex.WXB add $0x1d053d01,%rax
  6a:	30 05 17 08 22 05    	xor    %al,0x5220817(%rip)        # 5220887 <_end+0x521c86f>
  70:	0d d7 05 05 74       	or     $0x740505d7,%eax
  75:	05 0d 00 02 04       	add    $0x402000d,%eax
  7a:	03 2f                	add    (%rdi),%ebp
  7c:	05 0f 00 02 04       	add    $0x402000f,%eax
  81:	03 75 05             	add    0x5(%rbp),%esi
  84:	10 00                	adc    %al,(%rax)
  86:	02 04 03             	add    (%rbx,%rax,1),%al
  89:	58                   	pop    %rax
  8a:	05 12 00 02 04       	add    $0x4020012,%eax
  8f:	03 e4                	add    %esp,%esp
  91:	05 05 00 02 04       	add    $0x4020005,%eax
  96:	03 56 05             	add    0x5(%rsi),%edx
  99:	17                   	(bad)  
  9a:	00 02                	add    %al,(%rdx)
  9c:	04 01                	add    $0x1,%al
  9e:	4a 05 12 6a 05 1c    	rex.WX add $0x1c056a12,%rax
  a4:	9e                   	sahf   
  a5:	05 09 9e 05 0c       	add    $0xc059e09,%eax
  aa:	59                   	pop    %rcx
  ab:	05 01 3d 05 10       	add    $0x10053d01,%eax
  b0:	30 05 05 f3 05 10    	xor    %al,0x1005f305(%rip)        # 1005f3bb <_end+0x1005b3a3>
  b6:	67 05 0c 75 05 10    	addr32 add $0x1005750c,%eax
  bc:	67 05 0f 76 05 1a    	addr32 add $0x1a05760f,%eax
  c2:	e4 05                	in     $0x5,%al
  c4:	1e                   	(bad)  
  c5:	c8 05 01 2f          	enter  $0x105,$0x2f
  c9:	05 22 69 05 09       	add    $0x9056922,%eax
  ce:	08 2f                	or     %ch,(%rdi)
  d0:	75 05                	jne    d7 <__abi_tag-0x2b5>
  d2:	17                   	(bad)  
  d3:	75 05                	jne    da <__abi_tag-0x2b2>
  d5:	12 08                	adc    (%rax),%cl
  d7:	21 05 0f 08 21 05    	and    %eax,0x521080f(%rip)        # 52108ec <_end+0x520c8d4>
  dd:	12 9e 05 01 21 02    	adc    0x2210105(%rsi),%bl
  e3:	02 00                	add    (%rax),%al
  e5:	01 01                	add    %eax,(%rcx)

Disassembly of section .debug_str:

0000000000000000 <.debug_str>:
   0:	47                   	rex.RXB
   1:	4e 55                	rex.WRX push %rbp
   3:	20 43 2b             	and    %al,0x2b(%rbx)
   6:	2b 31                	sub    (%rcx),%esi
   8:	37                   	(bad)  
   9:	20 31                	and    %dh,(%rcx)
   b:	31 2e                	xor    %ebp,(%rsi)
   d:	34 2e                	xor    $0x2e,%al
   f:	30 20                	xor    %ah,(%rax)
  11:	2d 6d 74 75 6e       	sub    $0x6e75746d,%eax
  16:	65 3d 67 65 6e 65    	gs cmp $0x656e6567,%eax
  1c:	72 69                	jb     87 <__abi_tag-0x305>
  1e:	63 20                	movsxd (%rax),%esp
  20:	2d 6d 61 72 63       	sub    $0x6372616d,%eax
  25:	68 3d 78 38 36       	push   $0x3638783d
  2a:	2d 36 34 20 2d       	sub    $0x2d203436,%eax
  2f:	67 20 2d 66 61 73 79 	and    %ch,0x79736166(%eip)        # 7973619c <_end+0x79732184>
  36:	6e                   	outsb  %ds:(%rsi),(%dx)
  37:	63 68 72             	movsxd 0x72(%rax),%ebp
  3a:	6f                   	outsl  %ds:(%rsi),(%dx)
  3b:	6e                   	outsb  %ds:(%rsi),(%dx)
  3c:	6f                   	outsl  %ds:(%rsi),(%dx)
  3d:	75 73                	jne    b2 <__abi_tag-0x2da>
  3f:	2d 75 6e 77 69       	sub    $0x69776e75,%eax
  44:	6e                   	outsb  %ds:(%rsi),(%dx)
  45:	64 2d 74 61 62 6c    	fs sub $0x6c626174,%eax
  4b:	65 73 20             	gs jae 6e <__abi_tag-0x31e>
  4e:	2d 66 73 74 61       	sub    $0x61747366,%eax
  53:	63 6b 2d             	movsxd 0x2d(%rbx),%ebp
  56:	70 72                	jo     ca <__abi_tag-0x2c2>
  58:	6f                   	outsl  %ds:(%rsi),(%dx)
  59:	74 65                	je     c0 <__abi_tag-0x2cc>
  5b:	63 74 6f 72          	movsxd 0x72(%rdi,%rbp,2),%esi
  5f:	2d 73 74 72 6f       	sub    $0x6f727473,%eax
  64:	6e                   	outsb  %ds:(%rsi),(%dx)
  65:	67 20 2d 66 73 74 61 	and    %ch,0x61747366(%eip)        # 617473d2 <_end+0x617433ba>
  6c:	63 6b 2d             	movsxd 0x2d(%rbx),%ebp
  6f:	63 6c 61 73          	movsxd 0x73(%rcx,%riz,2),%ebp
  73:	68 2d 70 72 6f       	push   $0x6f72702d
  78:	74 65                	je     df <__abi_tag-0x2ad>
  7a:	63 74 69 6f          	movsxd 0x6f(%rcx,%rbp,2),%esi
  7e:	6e                   	outsb  %ds:(%rsi),(%dx)
  7f:	20 2d 66 63 66 2d    	and    %ch,0x2d666366(%rip)        # 2d6663eb <_end+0x2d6623d3>
  85:	70 72                	jo     f9 <__abi_tag-0x293>
  87:	6f                   	outsl  %ds:(%rsi),(%dx)
  88:	74 65                	je     ef <__abi_tag-0x29d>
  8a:	63 74 69 6f          	movsxd 0x6f(%rcx,%rbp,2),%esi
  8e:	6e                   	outsb  %ds:(%rsi),(%dx)
  8f:	00 5f 5f             	add    %bl,0x5f(%rdi)
  92:	66 6c                	data16 insb (%dx),%es:(%rdi)
  94:	6f                   	outsl  %ds:(%rsi),(%dx)
  95:	61                   	(bad)  
  96:	74 31                	je     c9 <__abi_tag-0x2c3>
  98:	32 38                	xor    (%rax),%bh
  9a:	00 61 72             	add    %ah,0x72(%rcx)
  9d:	67 63 00             	movsxd (%eax),%eax
  a0:	5f                   	pop    %rdi
  a1:	5a                   	pop    %rdx
  a2:	37                   	(bad)  
  a3:	66 6f                	outsw  %ds:(%rsi),(%dx)
  a5:	6f                   	outsl  %ds:(%rsi),(%dx)
  a6:	63 61 6c             	movsxd 0x6c(%rcx),%esp
  a9:	6c                   	insb   (%dx),%es:(%rdi)
  aa:	69 69 00 66 6f 6f 63 	imul   $0x636f6f66,0x0(%rcx),%ebp
  b1:	61                   	(bad)  
  b2:	6c                   	insb   (%dx),%es:(%rdi)
  b3:	6c                   	insb   (%dx),%es:(%rdi)
  b4:	00 5f 5a             	add    %bl,0x5a(%rdi)
  b7:	33 66 69             	xor    0x69(%rsi),%esp
  ba:	62                   	(bad)  
  bb:	69 00 75 6e 73 69    	imul   $0x69736e75,(%rax),%eax
  c1:	67 6e                	outsb  %ds:(%esi),(%dx)
  c3:	65 64 20 63 68       	gs and %ah,%fs:0x68(%rbx)
  c8:	61                   	(bad)  
  c9:	72 00                	jb     cb <__abi_tag-0x2c1>
  cb:	6d                   	insl   (%dx),%es:(%rdi)
  cc:	61                   	(bad)  
  cd:	69 6e 00 6c 6f 6e 67 	imul   $0x676e6f6c,0x0(%rsi),%ebp
  d4:	20 69 6e             	and    %ch,0x6e(%rcx)
  d7:	74 00                	je     d9 <__abi_tag-0x2b3>
  d9:	6c                   	insb   (%dx),%es:(%rdi)
  da:	6f                   	outsl  %ds:(%rsi),(%dx)
  db:	6e                   	outsb  %ds:(%rsi),(%dx)
  dc:	67 20 75 6e          	and    %dh,0x6e(%ebp)
  e0:	73 69                	jae    14b <__abi_tag-0x241>
  e2:	67 6e                	outsb  %ds:(%esi),(%dx)
  e4:	65 64 20 69 6e       	gs and %ch,%fs:0x6e(%rcx)
  e9:	74 00                	je     eb <__abi_tag-0x2a1>
  eb:	66 75 6e             	data16 jne 15c <__abi_tag-0x230>
  ee:	63 74 69 6f          	movsxd 0x6f(%rcx,%rbp,2),%esi
  f2:	6e                   	outsb  %ds:(%rsi),(%dx)
  f3:	63 61 6c             	movsxd 0x6c(%rcx),%esp
  f6:	6c                   	insb   (%dx),%es:(%rdi)
  f7:	00 5f 5f             	add    %bl,0x5f(%rdi)
  fa:	75 6e                	jne    16a <__abi_tag-0x222>
  fc:	6b 6e 6f 77          	imul   $0x77,0x6f(%rsi),%ebp
 100:	6e                   	outsb  %ds:(%rsi),(%dx)
 101:	5f                   	pop    %rdi
 102:	5f                   	pop    %rdi
 103:	00 61 72             	add    %ah,0x72(%rcx)
 106:	72 61                	jb     169 <__abi_tag-0x223>
 108:	79 00                	jns    10a <__abi_tag-0x282>
 10a:	73 68                	jae    174 <__abi_tag-0x218>
 10c:	6f                   	outsl  %ds:(%rsi),(%dx)
 10d:	72 74                	jb     183 <__abi_tag-0x209>
 10f:	20 75 6e             	and    %dh,0x6e(%rbp)
 112:	73 69                	jae    17d <__abi_tag-0x20f>
 114:	67 6e                	outsb  %ds:(%esi),(%dx)
 116:	65 64 20 69 6e       	gs and %ch,%fs:0x6e(%rcx)
 11b:	74 00                	je     11d <__abi_tag-0x26f>
 11d:	6c                   	insb   (%dx),%es:(%rdi)
 11e:	6f                   	outsl  %ds:(%rsi),(%dx)
 11f:	6e                   	outsb  %ds:(%rsi),(%dx)
 120:	67 20 64 6f 75       	and    %ah,0x75(%edi,%ebp,2)
 125:	62                   	(bad)  
 126:	6c                   	insb   (%dx),%es:(%rdi)
 127:	65 00 66 6c          	add    %ah,%gs:0x6c(%rsi)
 12b:	6f                   	outsl  %ds:(%rsi),(%dx)
 12c:	61                   	(bad)  
 12d:	74 00                	je     12f <__abi_tag-0x25d>
 12f:	73 68                	jae    199 <__abi_tag-0x1f3>
 131:	6f                   	outsl  %ds:(%rsi),(%dx)
 132:	72 74                	jb     1a8 <__abi_tag-0x1e4>
 134:	20 69 6e             	and    %ch,0x6e(%rcx)
 137:	74 00                	je     139 <__abi_tag-0x253>
 139:	61                   	(bad)  
 13a:	72 67                	jb     1a3 <__abi_tag-0x1e9>
 13c:	76 00                	jbe    13e <__abi_tag-0x24e>
 13e:	5f                   	pop    %rdi
 13f:	5a                   	pop    %rdx
 140:	31 32                	xor    %esi,(%rdx)
 142:	66 75 6e             	data16 jne 1b3 <__abi_tag-0x1d9>
 145:	63 74 69 6f          	movsxd 0x6f(%rcx,%rbp,2),%esi
 149:	6e                   	outsb  %ds:(%rsi),(%dx)
 14a:	63 61 6c             	movsxd 0x6c(%rcx),%esp
 14d:	6c                   	insb   (%dx),%es:(%rdi)
 14e:	69                   	.byte 0x69
 14f:	69                   	.byte 0x69
	...

Disassembly of section .debug_line_str:

0000000000000000 <.debug_line_str>:
   0:	2f                   	(bad)  
   1:	68 6f 6d 65 2f       	push   $0x2f656d6f
   6:	73 70                	jae    78 <__abi_tag-0x314>
   8:	61                   	(bad)  
   9:	64 65 6b 36 37       	fs imul $0x37,%gs:(%rsi),%esi
   e:	34 32                	xor    $0x32,%al
  10:	34 2f                	xor    $0x2f,%al
  12:	77 6f                	ja     83 <__abi_tag-0x309>
  14:	72 6b                	jb     81 <__abi_tag-0x30b>
  16:	2f                   	(bad)  
  17:	72 65                	jb     7e <__abi_tag-0x30e>
  19:	73 65                	jae    80 <__abi_tag-0x30c>
  1b:	61                   	(bad)  
  1c:	72 63                	jb     81 <__abi_tag-0x30b>
  1e:	68 00 6d 61 69       	push   $0x69616d00
  23:	6e                   	outsb  %ds:(%rsi),(%dx)
  24:	2e 63 70 70          	cs movsxd 0x70(%rax),%esi
	...
