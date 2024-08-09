
dhrystone:     file format elf64-x86-64


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
    1020:	ff 35 6a 2f 00 00    	push   0x2f6a(%rip)        # 3f90 <_GLOBAL_OFFSET_TABLE_+0x8>
    1026:	f2 ff 25 6b 2f 00 00 	bnd jmp *0x2f6b(%rip)        # 3f98 <_GLOBAL_OFFSET_TABLE_+0x10>
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
    1070:	f3 0f 1e fa          	endbr64 
    1074:	68 04 00 00 00       	push   $0x4
    1079:	f2 e9 a1 ff ff ff    	bnd jmp 1020 <_init+0x20>
    107f:	90                   	nop
    1080:	f3 0f 1e fa          	endbr64 
    1084:	68 05 00 00 00       	push   $0x5
    1089:	f2 e9 91 ff ff ff    	bnd jmp 1020 <_init+0x20>
    108f:	90                   	nop
    1090:	f3 0f 1e fa          	endbr64 
    1094:	68 06 00 00 00       	push   $0x6
    1099:	f2 e9 81 ff ff ff    	bnd jmp 1020 <_init+0x20>
    109f:	90                   	nop

Disassembly of section .plt.got:

00000000000010a0 <__cxa_finalize@plt>:
    10a0:	f3 0f 1e fa          	endbr64 
    10a4:	f2 ff 25 4d 2f 00 00 	bnd jmp *0x2f4d(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    10ab:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .plt.sec:

00000000000010b0 <putchar@plt>:
    10b0:	f3 0f 1e fa          	endbr64 
    10b4:	f2 ff 25 e5 2e 00 00 	bnd jmp *0x2ee5(%rip)        # 3fa0 <putchar@GLIBC_2.2.5>
    10bb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010c0 <puts@plt>:
    10c0:	f3 0f 1e fa          	endbr64 
    10c4:	f2 ff 25 dd 2e 00 00 	bnd jmp *0x2edd(%rip)        # 3fa8 <puts@GLIBC_2.2.5>
    10cb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010d0 <__stack_chk_fail@plt>:
    10d0:	f3 0f 1e fa          	endbr64 
    10d4:	f2 ff 25 d5 2e 00 00 	bnd jmp *0x2ed5(%rip)        # 3fb0 <__stack_chk_fail@GLIBC_2.4>
    10db:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010e0 <gettimeofday@plt>:
    10e0:	f3 0f 1e fa          	endbr64 
    10e4:	f2 ff 25 cd 2e 00 00 	bnd jmp *0x2ecd(%rip)        # 3fb8 <gettimeofday@GLIBC_2.2.5>
    10eb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000000010f0 <strcmp@plt>:
    10f0:	f3 0f 1e fa          	endbr64 
    10f4:	f2 ff 25 c5 2e 00 00 	bnd jmp *0x2ec5(%rip)        # 3fc0 <strcmp@GLIBC_2.2.5>
    10fb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000001100 <malloc@plt>:
    1100:	f3 0f 1e fa          	endbr64 
    1104:	f2 ff 25 bd 2e 00 00 	bnd jmp *0x2ebd(%rip)        # 3fc8 <malloc@GLIBC_2.2.5>
    110b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000001110 <__printf_chk@plt>:
    1110:	f3 0f 1e fa          	endbr64 
    1114:	f2 ff 25 b5 2e 00 00 	bnd jmp *0x2eb5(%rip)        # 3fd0 <__printf_chk@GLIBC_2.3.4>
    111b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .text:

0000000000001120 <main>:
    1120:	f3 0f 1e fa          	endbr64 
    1124:	41 57                	push   %r15
    1126:	bf 38 00 00 00       	mov    $0x38,%edi
    112b:	49 bf 45 20 50 52 4f 	movabs $0x4152474f52502045,%r15
    1132:	47 52 41 
    1135:	41 56                	push   %r14
    1137:	49 be 44 48 52 59 53 	movabs $0x4e4f545359524844,%r14
    113e:	54 4f 4e 
    1141:	41 55                	push   %r13
    1143:	41 54                	push   %r12
    1145:	55                   	push   %rbp
    1146:	53                   	push   %rbx
    1147:	48 81 ec 88 00 00 00 	sub    $0x88,%rsp
    114e:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    1155:	00 00 
    1157:	48 89 44 24 78       	mov    %rax,0x78(%rsp)
    115c:	31 c0                	xor    %eax,%eax
    115e:	e8 9d ff ff ff       	call   1100 <malloc@plt>
    1163:	bf 38 00 00 00       	mov    $0x38,%edi
    1168:	48 89 c3             	mov    %rax,%rbx
    116b:	48 89 05 06 57 00 00 	mov    %rax,0x5706(%rip)        # 6878 <Next_Ptr_Glob>
    1172:	e8 89 ff ff ff       	call   1100 <malloc@plt>
    1177:	b9 4e 47 00 00       	mov    $0x474e,%ecx
    117c:	be 4e 47 00 00       	mov    $0x474e,%esi
    1181:	48 8b 15 58 13 00 00 	mov    0x1358(%rip),%rdx        # 24e0 <_IO_stdin_used+0x4e0>
    1188:	66 0f 6f 05 70 13 00 	movdqa 0x1370(%rip),%xmm0        # 2500 <_IO_stdin_used+0x500>
    118f:	00 
    1190:	48 89 18             	mov    %rbx,(%rax)
    1193:	bf 0a 00 00 00       	mov    $0xa,%edi
    1198:	bb 01 00 00 00       	mov    $0x1,%ebx
    119d:	48 89 50 08          	mov    %rdx,0x8(%rax)
    11a1:	48 8d 50 14          	lea    0x14(%rax),%rdx
    11a5:	0f 11 40 14          	movups %xmm0,0x14(%rax)
    11a9:	c7 40 10 28 00 00 00 	movl   $0x28,0x10(%rax)
    11b0:	48 89 05 c9 56 00 00 	mov    %rax,0x56c9(%rip)        # 6880 <Ptr_Glob>
    11b7:	48 b8 4d 2c 20 53 4f 	movabs $0x20454d4f53202c4d,%rax
    11be:	4d 45 20 
    11c1:	66 89 4a 1c          	mov    %cx,0x1c(%rdx)
    11c5:	48 89 42 10          	mov    %rax,0x10(%rdx)
    11c9:	48 b8 4d 2c 20 31 27 	movabs $0x2054532731202c4d,%rax
    11d0:	53 54 20 
    11d3:	c7 42 18 53 54 52 49 	movl   $0x49525453,0x18(%rdx)
    11da:	c6 42 1e 00          	movb   $0x0,0x1e(%rdx)
    11de:	0f 29 44 24 30       	movaps %xmm0,0x30(%rsp)
    11e3:	66 89 74 24 4c       	mov    %si,0x4c(%rsp)
    11e8:	48 89 44 24 40       	mov    %rax,0x40(%rsp)
    11ed:	c7 44 24 48 53 54 52 	movl   $0x49525453,0x48(%rsp)
    11f4:	49 
    11f5:	c6 44 24 4e 00       	movb   $0x0,0x4e(%rsp)
    11fa:	c7 05 d8 34 00 00 0a 	movl   $0xa,0x34d8(%rip)        # 46dc <Arr_2_Glob+0x65c>
    1201:	00 00 00 
    1204:	e8 a7 fe ff ff       	call   10b0 <putchar@plt>
    1209:	48 8d 3d f8 0d 00 00 	lea    0xdf8(%rip),%rdi        # 2008 <_IO_stdin_used+0x8>
    1210:	e8 ab fe ff ff       	call   10c0 <puts@plt>
    1215:	bf 0a 00 00 00       	mov    $0xa,%edi
    121a:	e8 91 fe ff ff       	call   10b0 <putchar@plt>
    121f:	ba 00 e1 f5 05       	mov    $0x5f5e100,%edx
    1224:	48 8d 35 0d 0e 00 00 	lea    0xe0d(%rip),%rsi        # 2038 <_IO_stdin_used+0x38>
    122b:	31 c0                	xor    %eax,%eax
    122d:	bf 01 00 00 00       	mov    $0x1,%edi
    1232:	e8 d9 fe ff ff       	call   1110 <__printf_chk@plt>
    1237:	31 f6                	xor    %esi,%esi
    1239:	48 8d 3d 20 2e 00 00 	lea    0x2e20(%rip),%rdi        # 4060 <time_info>
    1240:	e8 9b fe ff ff       	call   10e0 <gettimeofday@plt>
    1245:	48 69 05 10 2e 00 00 	imul   $0xf4240,0x2e10(%rip),%rax        # 4060 <time_info>
    124c:	40 42 0f 00 
    1250:	48 03 05 11 2e 00 00 	add    0x2e11(%rip),%rax        # 4068 <time_info+0x8>
    1257:	48 89 05 fa 2d 00 00 	mov    %rax,0x2dfa(%rip)        # 4058 <Begin_Time>
    125e:	48 8d 44 24 50       	lea    0x50(%rsp),%rax
    1263:	48 89 44 24 08       	mov    %rax,0x8(%rsp)
    1268:	48 8d 44 24 30       	lea    0x30(%rsp),%rax
    126d:	48 89 04 24          	mov    %rax,(%rsp)
    1271:	48 8d 44 24 28       	lea    0x28(%rsp),%rax
    1276:	48 89 44 24 10       	mov    %rax,0x10(%rsp)
    127b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
    1280:	48 8b 74 24 08       	mov    0x8(%rsp),%rsi
    1285:	48 8b 3c 24          	mov    (%rsp),%rdi
    1289:	ba 4e 47 00 00       	mov    $0x474e,%edx
    128e:	48 b8 4d 2c 20 32 27 	movabs $0x20444e2732202c4d,%rax
    1295:	4e 44 20 
    1298:	66 89 54 24 6c       	mov    %dx,0x6c(%rsp)
    129d:	c6 05 c5 55 00 00 41 	movb   $0x41,0x55c5(%rip)        # 6869 <Ch_1_Glob>
    12a4:	c7 05 be 55 00 00 01 	movl   $0x1,0x55be(%rip)        # 686c <Bool_Glob>
    12ab:	00 00 00 
    12ae:	c6 05 b3 55 00 00 42 	movb   $0x42,0x55b3(%rip)        # 6868 <Ch_2_Glob>
    12b5:	4c 89 74 24 50       	mov    %r14,0x50(%rsp)
    12ba:	4c 89 7c 24 58       	mov    %r15,0x58(%rsp)
    12bf:	48 89 44 24 60       	mov    %rax,0x60(%rsp)
    12c4:	c7 44 24 68 53 54 52 	movl   $0x49525453,0x68(%rsp)
    12cb:	49 
    12cc:	c6 44 24 6e 00       	movb   $0x0,0x6e(%rsp)
    12d1:	c7 44 24 2c 01 00 00 	movl   $0x1,0x2c(%rsp)
    12d8:	00 
    12d9:	e8 32 0a 00 00       	call   1d10 <Func_2>
    12de:	48 8b 54 24 10       	mov    0x10(%rsp),%rdx
    12e3:	be 03 00 00 00       	mov    $0x3,%esi
    12e8:	bf 02 00 00 00       	mov    $0x2,%edi
    12ed:	85 c0                	test   %eax,%eax
    12ef:	c7 44 24 28 07 00 00 	movl   $0x7,0x28(%rsp)
    12f6:	00 
    12f7:	0f 94 c0             	sete   %al
    12fa:	0f b6 c0             	movzbl %al,%eax
    12fd:	89 05 69 55 00 00    	mov    %eax,0x5569(%rip)        # 686c <Bool_Glob>
    1303:	e8 68 09 00 00       	call   1c70 <Proc_7>
    1308:	8b 4c 24 28          	mov    0x28(%rsp),%ecx
    130c:	ba 03 00 00 00       	mov    $0x3,%edx
    1311:	48 8d 35 68 2d 00 00 	lea    0x2d68(%rip),%rsi        # 4080 <Arr_2_Glob>
    1318:	48 8d 3d 81 54 00 00 	lea    0x5481(%rip),%rdi        # 67a0 <Arr_1_Glob>
    131f:	e8 5c 09 00 00       	call   1c80 <Proc_8>
    1324:	48 8b 3d 55 55 00 00 	mov    0x5555(%rip),%rdi        # 6880 <Ptr_Glob>
    132b:	e8 70 07 00 00       	call   1aa0 <Proc_1>
    1330:	80 3d 31 55 00 00 40 	cmpb   $0x40,0x5531(%rip)        # 6868 <Ch_2_Glob>
    1337:	0f 8e a3 05 00 00    	jle    18e0 <main+0x7c0>
    133d:	41 bd 41 00 00 00    	mov    $0x41,%r13d
    1343:	bd 03 00 00 00       	mov    $0x3,%ebp
    1348:	4c 8d 64 24 2c       	lea    0x2c(%rsp),%r12
    134d:	0f 1f 00             	nopl   (%rax)
    1350:	41 0f be fd          	movsbl %r13b,%edi
    1354:	be 43 00 00 00       	mov    $0x43,%esi
    1359:	e8 92 09 00 00       	call   1cf0 <Func_1>
    135e:	3b 44 24 2c          	cmp    0x2c(%rsp),%eax
    1362:	0f 84 18 05 00 00    	je     1880 <main+0x760>
    1368:	41 83 c5 01          	add    $0x1,%r13d
    136c:	44 38 2d f5 54 00 00 	cmp    %r13b,0x54f5(%rip)        # 6868 <Ch_2_Glob>
    1373:	7d db                	jge    1350 <main+0x230>
    1375:	8d 6c 6d 00          	lea    0x0(%rbp,%rbp,2),%ebp
    1379:	89 e8                	mov    %ebp,%eax
    137b:	44 8b 64 24 28       	mov    0x28(%rsp),%r12d
    1380:	99                   	cltd   
    1381:	41 f7 fc             	idiv   %r12d
    1384:	80 3d de 54 00 00 41 	cmpb   $0x41,0x54de(%rip)        # 6869 <Ch_1_Glob>
    138b:	41 89 c0             	mov    %eax,%r8d
    138e:	75 0b                	jne    139b <main+0x27b>
    1390:	44 8d 40 09          	lea    0x9(%rax),%r8d
    1394:	44 2b 05 d5 54 00 00 	sub    0x54d5(%rip),%r8d        # 6870 <Int_Glob>
    139b:	83 c3 01             	add    $0x1,%ebx
    139e:	81 fb 01 e1 f5 05    	cmp    $0x5f5e101,%ebx
    13a4:	0f 85 d6 fe ff ff    	jne    1280 <main+0x160>
    13aa:	31 f6                	xor    %esi,%esi
    13ac:	48 8d 3d ad 2c 00 00 	lea    0x2cad(%rip),%rdi        # 4060 <time_info>
    13b3:	44 89 44 24 1c       	mov    %r8d,0x1c(%rsp)
    13b8:	4c 8d 2d 4d 0f 00 00 	lea    0xf4d(%rip),%r13        # 230c <_IO_stdin_used+0x30c>
    13bf:	89 44 24 10          	mov    %eax,0x10(%rsp)
    13c3:	4c 8d 35 8d 0f 00 00 	lea    0xf8d(%rip),%r14        # 2357 <_IO_stdin_used+0x357>
    13ca:	48 8d 1d 03 10 00 00 	lea    0x1003(%rip),%rbx        # 23d4 <_IO_stdin_used+0x3d4>
    13d1:	e8 0a fd ff ff       	call   10e0 <gettimeofday@plt>
    13d6:	48 8d 3d 07 0f 00 00 	lea    0xf07(%rip),%rdi        # 22e4 <_IO_stdin_used+0x2e4>
    13dd:	4c 8d 3d 09 10 00 00 	lea    0x1009(%rip),%r15        # 23ed <_IO_stdin_used+0x3ed>
    13e4:	48 69 05 71 2c 00 00 	imul   $0xf4240,0x2c71(%rip),%rax        # 4060 <time_info>
    13eb:	40 42 0f 00 
    13ef:	48 03 05 72 2c 00 00 	add    0x2c72(%rip),%rax        # 4068 <time_info+0x8>
    13f6:	48 89 05 53 2c 00 00 	mov    %rax,0x2c53(%rip)        # 4050 <End_Time>
    13fd:	e8 be fc ff ff       	call   10c0 <puts@plt>
    1402:	bf 0a 00 00 00       	mov    $0xa,%edi
    1407:	e8 a4 fc ff ff       	call   10b0 <putchar@plt>
    140c:	48 8d 3d 55 0c 00 00 	lea    0xc55(%rip),%rdi        # 2068 <_IO_stdin_used+0x68>
    1413:	e8 a8 fc ff ff       	call   10c0 <puts@plt>
    1418:	bf 0a 00 00 00       	mov    $0xa,%edi
    141d:	e8 8e fc ff ff       	call   10b0 <putchar@plt>
    1422:	8b 15 48 54 00 00    	mov    0x5448(%rip),%edx        # 6870 <Int_Glob>
    1428:	48 8d 35 c4 0e 00 00 	lea    0xec4(%rip),%rsi        # 22f3 <_IO_stdin_used+0x2f3>
    142f:	31 c0                	xor    %eax,%eax
    1431:	bf 01 00 00 00       	mov    $0x1,%edi
    1436:	e8 d5 fc ff ff       	call   1110 <__printf_chk@plt>
    143b:	ba 05 00 00 00       	mov    $0x5,%edx
    1440:	4c 89 ee             	mov    %r13,%rsi
    1443:	31 c0                	xor    %eax,%eax
    1445:	bf 01 00 00 00       	mov    $0x1,%edi
    144a:	e8 c1 fc ff ff       	call   1110 <__printf_chk@plt>
    144f:	8b 15 17 54 00 00    	mov    0x5417(%rip),%edx        # 686c <Bool_Glob>
    1455:	48 8d 35 c9 0e 00 00 	lea    0xec9(%rip),%rsi        # 2325 <_IO_stdin_used+0x325>
    145c:	31 c0                	xor    %eax,%eax
    145e:	bf 01 00 00 00       	mov    $0x1,%edi
    1463:	e8 a8 fc ff ff       	call   1110 <__printf_chk@plt>
    1468:	ba 01 00 00 00       	mov    $0x1,%edx
    146d:	4c 89 ee             	mov    %r13,%rsi
    1470:	31 c0                	xor    %eax,%eax
    1472:	bf 01 00 00 00       	mov    $0x1,%edi
    1477:	e8 94 fc ff ff       	call   1110 <__printf_chk@plt>
    147c:	0f be 15 e6 53 00 00 	movsbl 0x53e6(%rip),%edx        # 6869 <Ch_1_Glob>
    1483:	48 8d 35 b4 0e 00 00 	lea    0xeb4(%rip),%rsi        # 233e <_IO_stdin_used+0x33e>
    148a:	31 c0                	xor    %eax,%eax
    148c:	bf 01 00 00 00       	mov    $0x1,%edi
    1491:	e8 7a fc ff ff       	call   1110 <__printf_chk@plt>
    1496:	4c 89 f6             	mov    %r14,%rsi
    1499:	ba 41 00 00 00       	mov    $0x41,%edx
    149e:	31 c0                	xor    %eax,%eax
    14a0:	bf 01 00 00 00       	mov    $0x1,%edi
    14a5:	e8 66 fc ff ff       	call   1110 <__printf_chk@plt>
    14aa:	0f be 15 b7 53 00 00 	movsbl 0x53b7(%rip),%edx        # 6868 <Ch_2_Glob>
    14b1:	48 8d 35 b8 0e 00 00 	lea    0xeb8(%rip),%rsi        # 2370 <_IO_stdin_used+0x370>
    14b8:	31 c0                	xor    %eax,%eax
    14ba:	bf 01 00 00 00       	mov    $0x1,%edi
    14bf:	e8 4c fc ff ff       	call   1110 <__printf_chk@plt>
    14c4:	4c 89 f6             	mov    %r14,%rsi
    14c7:	ba 42 00 00 00       	mov    $0x42,%edx
    14cc:	31 c0                	xor    %eax,%eax
    14ce:	bf 01 00 00 00       	mov    $0x1,%edi
    14d3:	4c 8d 35 26 0c 00 00 	lea    0xc26(%rip),%r14        # 2100 <_IO_stdin_used+0x100>
    14da:	e8 31 fc ff ff       	call   1110 <__printf_chk@plt>
    14df:	8b 15 db 52 00 00    	mov    0x52db(%rip),%edx        # 67c0 <Arr_1_Glob+0x20>
    14e5:	48 8d 35 9d 0e 00 00 	lea    0xe9d(%rip),%rsi        # 2389 <_IO_stdin_used+0x389>
    14ec:	31 c0                	xor    %eax,%eax
    14ee:	bf 01 00 00 00       	mov    $0x1,%edi
    14f3:	e8 18 fc ff ff       	call   1110 <__printf_chk@plt>
    14f8:	ba 07 00 00 00       	mov    $0x7,%edx
    14fd:	4c 89 ee             	mov    %r13,%rsi
    1500:	31 c0                	xor    %eax,%eax
    1502:	bf 01 00 00 00       	mov    $0x1,%edi
    1507:	e8 04 fc ff ff       	call   1110 <__printf_chk@plt>
    150c:	8b 15 ca 31 00 00    	mov    0x31ca(%rip),%edx        # 46dc <Arr_2_Glob+0x65c>
    1512:	48 8d 35 89 0e 00 00 	lea    0xe89(%rip),%rsi        # 23a2 <_IO_stdin_used+0x3a2>
    1519:	31 c0                	xor    %eax,%eax
    151b:	bf 01 00 00 00       	mov    $0x1,%edi
    1520:	e8 eb fb ff ff       	call   1110 <__printf_chk@plt>
    1525:	48 8d 3d 74 0b 00 00 	lea    0xb74(%rip),%rdi        # 20a0 <_IO_stdin_used+0xa0>
    152c:	e8 8f fb ff ff       	call   10c0 <puts@plt>
    1531:	48 8d 3d 05 0f 00 00 	lea    0xf05(%rip),%rdi        # 243d <_IO_stdin_used+0x43d>
    1538:	e8 83 fb ff ff       	call   10c0 <puts@plt>
    153d:	48 8b 05 3c 53 00 00 	mov    0x533c(%rip),%rax        # 6880 <Ptr_Glob>
    1544:	bf 01 00 00 00       	mov    $0x1,%edi
    1549:	4c 8d 0d 6b 0e 00 00 	lea    0xe6b(%rip),%r9        # 23bb <_IO_stdin_used+0x3bb>
    1550:	4c 89 ce             	mov    %r9,%rsi
    1553:	48 8b 10             	mov    (%rax),%rdx
    1556:	31 c0                	xor    %eax,%eax
    1558:	e8 b3 fb ff ff       	call   1110 <__printf_chk@plt>
    155d:	48 8d 3d 6c 0b 00 00 	lea    0xb6c(%rip),%rdi        # 20d0 <_IO_stdin_used+0xd0>
    1564:	e8 57 fb ff ff       	call   10c0 <puts@plt>
    1569:	48 8b 05 10 53 00 00 	mov    0x5310(%rip),%rax        # 6880 <Ptr_Glob>
    1570:	48 89 de             	mov    %rbx,%rsi
    1573:	bf 01 00 00 00       	mov    $0x1,%edi
    1578:	8b 50 08             	mov    0x8(%rax),%edx
    157b:	31 c0                	xor    %eax,%eax
    157d:	e8 8e fb ff ff       	call   1110 <__printf_chk@plt>
    1582:	31 d2                	xor    %edx,%edx
    1584:	4c 89 ee             	mov    %r13,%rsi
    1587:	bf 01 00 00 00       	mov    $0x1,%edi
    158c:	31 c0                	xor    %eax,%eax
    158e:	e8 7d fb ff ff       	call   1110 <__printf_chk@plt>
    1593:	48 8b 05 e6 52 00 00 	mov    0x52e6(%rip),%rax        # 6880 <Ptr_Glob>
    159a:	4c 89 fe             	mov    %r15,%rsi
    159d:	bf 01 00 00 00       	mov    $0x1,%edi
    15a2:	8b 50 0c             	mov    0xc(%rax),%edx
    15a5:	31 c0                	xor    %eax,%eax
    15a7:	e8 64 fb ff ff       	call   1110 <__printf_chk@plt>
    15ac:	ba 02 00 00 00       	mov    $0x2,%edx
    15b1:	4c 89 ee             	mov    %r13,%rsi
    15b4:	31 c0                	xor    %eax,%eax
    15b6:	bf 01 00 00 00       	mov    $0x1,%edi
    15bb:	e8 50 fb ff ff       	call   1110 <__printf_chk@plt>
    15c0:	48 8b 05 b9 52 00 00 	mov    0x52b9(%rip),%rax        # 6880 <Ptr_Glob>
    15c7:	bf 01 00 00 00       	mov    $0x1,%edi
    15cc:	4c 8d 15 33 0e 00 00 	lea    0xe33(%rip),%r10        # 2406 <_IO_stdin_used+0x406>
    15d3:	4c 89 d6             	mov    %r10,%rsi
    15d6:	8b 50 10             	mov    0x10(%rax),%edx
    15d9:	31 c0                	xor    %eax,%eax
    15db:	e8 30 fb ff ff       	call   1110 <__printf_chk@plt>
    15e0:	ba 11 00 00 00       	mov    $0x11,%edx
    15e5:	4c 89 ee             	mov    %r13,%rsi
    15e8:	31 c0                	xor    %eax,%eax
    15ea:	bf 01 00 00 00       	mov    $0x1,%edi
    15ef:	e8 1c fb ff ff       	call   1110 <__printf_chk@plt>
    15f4:	48 8b 05 85 52 00 00 	mov    0x5285(%rip),%rax        # 6880 <Ptr_Glob>
    15fb:	bf 01 00 00 00       	mov    $0x1,%edi
    1600:	4c 8d 1d 18 0e 00 00 	lea    0xe18(%rip),%r11        # 241f <_IO_stdin_used+0x41f>
    1607:	4c 89 de             	mov    %r11,%rsi
    160a:	48 8d 50 14          	lea    0x14(%rax),%rdx
    160e:	31 c0                	xor    %eax,%eax
    1610:	e8 fb fa ff ff       	call   1110 <__printf_chk@plt>
    1615:	4c 89 f7             	mov    %r14,%rdi
    1618:	e8 a3 fa ff ff       	call   10c0 <puts@plt>
    161d:	48 8d 3d 14 0e 00 00 	lea    0xe14(%rip),%rdi        # 2438 <_IO_stdin_used+0x438>
    1624:	e8 97 fa ff ff       	call   10c0 <puts@plt>
    1629:	48 8b 05 48 52 00 00 	mov    0x5248(%rip),%rax        # 6878 <Next_Ptr_Glob>
    1630:	bf 01 00 00 00       	mov    $0x1,%edi
    1635:	4c 8d 0d 7f 0d 00 00 	lea    0xd7f(%rip),%r9        # 23bb <_IO_stdin_used+0x3bb>
    163c:	4c 89 ce             	mov    %r9,%rsi
    163f:	48 8b 10             	mov    (%rax),%rdx
    1642:	31 c0                	xor    %eax,%eax
    1644:	e8 c7 fa ff ff       	call   1110 <__printf_chk@plt>
    1649:	48 8d 3d e8 0a 00 00 	lea    0xae8(%rip),%rdi        # 2138 <_IO_stdin_used+0x138>
    1650:	e8 6b fa ff ff       	call   10c0 <puts@plt>
    1655:	48 8b 05 1c 52 00 00 	mov    0x521c(%rip),%rax        # 6878 <Next_Ptr_Glob>
    165c:	48 89 de             	mov    %rbx,%rsi
    165f:	bf 01 00 00 00       	mov    $0x1,%edi
    1664:	8b 50 08             	mov    0x8(%rax),%edx
    1667:	31 c0                	xor    %eax,%eax
    1669:	e8 a2 fa ff ff       	call   1110 <__printf_chk@plt>
    166e:	31 d2                	xor    %edx,%edx
    1670:	4c 89 ee             	mov    %r13,%rsi
    1673:	bf 01 00 00 00       	mov    $0x1,%edi
    1678:	31 c0                	xor    %eax,%eax
    167a:	e8 91 fa ff ff       	call   1110 <__printf_chk@plt>
    167f:	48 8b 05 f2 51 00 00 	mov    0x51f2(%rip),%rax        # 6878 <Next_Ptr_Glob>
    1686:	4c 89 fe             	mov    %r15,%rsi
    1689:	bf 01 00 00 00       	mov    $0x1,%edi
    168e:	8b 50 0c             	mov    0xc(%rax),%edx
    1691:	31 c0                	xor    %eax,%eax
    1693:	e8 78 fa ff ff       	call   1110 <__printf_chk@plt>
    1698:	ba 01 00 00 00       	mov    $0x1,%edx
    169d:	4c 89 ee             	mov    %r13,%rsi
    16a0:	31 c0                	xor    %eax,%eax
    16a2:	bf 01 00 00 00       	mov    $0x1,%edi
    16a7:	e8 64 fa ff ff       	call   1110 <__printf_chk@plt>
    16ac:	48 8b 05 c5 51 00 00 	mov    0x51c5(%rip),%rax        # 6878 <Next_Ptr_Glob>
    16b3:	bf 01 00 00 00       	mov    $0x1,%edi
    16b8:	4c 8d 15 47 0d 00 00 	lea    0xd47(%rip),%r10        # 2406 <_IO_stdin_used+0x406>
    16bf:	4c 89 d6             	mov    %r10,%rsi
    16c2:	8b 50 10             	mov    0x10(%rax),%edx
    16c5:	31 c0                	xor    %eax,%eax
    16c7:	44 29 e5             	sub    %r12d,%ebp
    16ca:	e8 41 fa ff ff       	call   1110 <__printf_chk@plt>
    16cf:	ba 12 00 00 00       	mov    $0x12,%edx
    16d4:	4c 89 ee             	mov    %r13,%rsi
    16d7:	31 c0                	xor    %eax,%eax
    16d9:	bf 01 00 00 00       	mov    $0x1,%edi
    16de:	e8 2d fa ff ff       	call   1110 <__printf_chk@plt>
    16e3:	48 8b 05 8e 51 00 00 	mov    0x518e(%rip),%rax        # 6878 <Next_Ptr_Glob>
    16ea:	bf 01 00 00 00       	mov    $0x1,%edi
    16ef:	4c 8d 1d 29 0d 00 00 	lea    0xd29(%rip),%r11        # 241f <_IO_stdin_used+0x41f>
    16f6:	4c 89 de             	mov    %r11,%rsi
    16f9:	48 8d 50 14          	lea    0x14(%rax),%rdx
    16fd:	31 c0                	xor    %eax,%eax
    16ff:	e8 0c fa ff ff       	call   1110 <__printf_chk@plt>
    1704:	4c 89 f7             	mov    %r14,%rdi
    1707:	e8 b4 f9 ff ff       	call   10c0 <puts@plt>
    170c:	8b 54 24 1c          	mov    0x1c(%rsp),%edx
    1710:	bf 01 00 00 00       	mov    $0x1,%edi
    1715:	31 c0                	xor    %eax,%eax
    1717:	48 8d 35 2a 0d 00 00 	lea    0xd2a(%rip),%rsi        # 2448 <_IO_stdin_used+0x448>
    171e:	e8 ed f9 ff ff       	call   1110 <__printf_chk@plt>
    1723:	ba 05 00 00 00       	mov    $0x5,%edx
    1728:	4c 89 ee             	mov    %r13,%rsi
    172b:	31 c0                	xor    %eax,%eax
    172d:	bf 01 00 00 00       	mov    $0x1,%edi
    1732:	e8 d9 f9 ff ff       	call   1110 <__printf_chk@plt>
    1737:	8b 4c 24 10          	mov    0x10(%rsp),%ecx
    173b:	bf 01 00 00 00       	mov    $0x1,%edi
    1740:	31 c0                	xor    %eax,%eax
    1742:	8d 14 ed 00 00 00 00 	lea    0x0(,%rbp,8),%edx
    1749:	48 8d 35 11 0d 00 00 	lea    0xd11(%rip),%rsi        # 2461 <_IO_stdin_used+0x461>
    1750:	29 ea                	sub    %ebp,%edx
    1752:	29 ca                	sub    %ecx,%edx
    1754:	e8 b7 f9 ff ff       	call   1110 <__printf_chk@plt>
    1759:	ba 0d 00 00 00       	mov    $0xd,%edx
    175e:	4c 89 ee             	mov    %r13,%rsi
    1761:	31 c0                	xor    %eax,%eax
    1763:	bf 01 00 00 00       	mov    $0x1,%edi
    1768:	e8 a3 f9 ff ff       	call   1110 <__printf_chk@plt>
    176d:	8b 54 24 28          	mov    0x28(%rsp),%edx
    1771:	bf 01 00 00 00       	mov    $0x1,%edi
    1776:	31 c0                	xor    %eax,%eax
    1778:	48 8d 35 fb 0c 00 00 	lea    0xcfb(%rip),%rsi        # 247a <_IO_stdin_used+0x47a>
    177f:	e8 8c f9 ff ff       	call   1110 <__printf_chk@plt>
    1784:	ba 07 00 00 00       	mov    $0x7,%edx
    1789:	4c 89 ee             	mov    %r13,%rsi
    178c:	31 c0                	xor    %eax,%eax
    178e:	bf 01 00 00 00       	mov    $0x1,%edi
    1793:	e8 78 f9 ff ff       	call   1110 <__printf_chk@plt>
    1798:	8b 54 24 2c          	mov    0x2c(%rsp),%edx
    179c:	bf 01 00 00 00       	mov    $0x1,%edi
    17a1:	31 c0                	xor    %eax,%eax
    17a3:	48 8d 35 e9 0c 00 00 	lea    0xce9(%rip),%rsi        # 2493 <_IO_stdin_used+0x493>
    17aa:	e8 61 f9 ff ff       	call   1110 <__printf_chk@plt>
    17af:	ba 01 00 00 00       	mov    $0x1,%edx
    17b4:	4c 89 ee             	mov    %r13,%rsi
    17b7:	31 c0                	xor    %eax,%eax
    17b9:	bf 01 00 00 00       	mov    $0x1,%edi
    17be:	e8 4d f9 ff ff       	call   1110 <__printf_chk@plt>
    17c3:	48 8b 14 24          	mov    (%rsp),%rdx
    17c7:	bf 01 00 00 00       	mov    $0x1,%edi
    17cc:	31 c0                	xor    %eax,%eax
    17ce:	48 8d 35 d7 0c 00 00 	lea    0xcd7(%rip),%rsi        # 24ac <_IO_stdin_used+0x4ac>
    17d5:	e8 36 f9 ff ff       	call   1110 <__printf_chk@plt>
    17da:	48 8d 3d 97 09 00 00 	lea    0x997(%rip),%rdi        # 2178 <_IO_stdin_used+0x178>
    17e1:	e8 da f8 ff ff       	call   10c0 <puts@plt>
    17e6:	48 8b 54 24 08       	mov    0x8(%rsp),%rdx
    17eb:	48 8d 35 d3 0c 00 00 	lea    0xcd3(%rip),%rsi        # 24c5 <_IO_stdin_used+0x4c5>
    17f2:	31 c0                	xor    %eax,%eax
    17f4:	bf 01 00 00 00       	mov    $0x1,%edi
    17f9:	e8 12 f9 ff ff       	call   1110 <__printf_chk@plt>
    17fe:	48 8d 3d ab 09 00 00 	lea    0x9ab(%rip),%rdi        # 21b0 <_IO_stdin_used+0x1b0>
    1805:	e8 b6 f8 ff ff       	call   10c0 <puts@plt>
    180a:	bf 0a 00 00 00       	mov    $0xa,%edi
    180f:	e8 9c f8 ff ff       	call   10b0 <putchar@plt>
    1814:	48 8b 05 35 28 00 00 	mov    0x2835(%rip),%rax        # 4050 <End_Time>
    181b:	48 2b 05 36 28 00 00 	sub    0x2836(%rip),%rax        # 4058 <Begin_Time>
    1822:	48 89 05 1f 28 00 00 	mov    %rax,0x281f(%rip)        # 4048 <User_Time>
    1829:	48 3d 7f 84 1e 00    	cmp    $0x1e847f,%rax
    182f:	0f 8f b5 00 00 00    	jg     18ea <main+0x7ca>
    1835:	48 8d 3d ac 09 00 00 	lea    0x9ac(%rip),%rdi        # 21e8 <_IO_stdin_used+0x1e8>
    183c:	e8 7f f8 ff ff       	call   10c0 <puts@plt>
    1841:	48 8d 3d d8 09 00 00 	lea    0x9d8(%rip),%rdi        # 2220 <_IO_stdin_used+0x220>
    1848:	e8 73 f8 ff ff       	call   10c0 <puts@plt>
    184d:	bf 0a 00 00 00       	mov    $0xa,%edi
    1852:	e8 59 f8 ff ff       	call   10b0 <putchar@plt>
    1857:	48 8b 44 24 78       	mov    0x78(%rsp),%rax
    185c:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    1863:	00 00 
    1865:	0f 85 34 01 00 00    	jne    199f <main+0x87f>
    186b:	48 81 c4 88 00 00 00 	add    $0x88,%rsp
    1872:	31 c0                	xor    %eax,%eax
    1874:	5b                   	pop    %rbx
    1875:	5d                   	pop    %rbp
    1876:	41 5c                	pop    %r12
    1878:	41 5d                	pop    %r13
    187a:	41 5e                	pop    %r14
    187c:	41 5f                	pop    %r15
    187e:	c3                   	ret    
    187f:	90                   	nop
    1880:	31 ff                	xor    %edi,%edi
    1882:	4c 89 e6             	mov    %r12,%rsi
    1885:	41 83 c5 01          	add    $0x1,%r13d
    1889:	89 dd                	mov    %ebx,%ebp
    188b:	e8 90 03 00 00       	call   1c20 <Proc_6>
    1890:	44 38 2d d1 4f 00 00 	cmp    %r13b,0x4fd1(%rip)        # 6868 <Ch_2_Glob>
    1897:	48 b8 4d 2c 20 33 27 	movabs $0x2044522733202c4d,%rax
    189e:	52 44 20 
    18a1:	4c 89 74 24 50       	mov    %r14,0x50(%rsp)
    18a6:	48 89 44 24 60       	mov    %rax,0x60(%rsp)
    18ab:	b8 4e 47 00 00       	mov    $0x474e,%eax
    18b0:	4c 89 7c 24 58       	mov    %r15,0x58(%rsp)
    18b5:	c7 44 24 68 53 54 52 	movl   $0x49525453,0x68(%rsp)
    18bc:	49 
    18bd:	66 89 44 24 6c       	mov    %ax,0x6c(%rsp)
    18c2:	c6 44 24 6e 00       	movb   $0x0,0x6e(%rsp)
    18c7:	89 1d a3 4f 00 00    	mov    %ebx,0x4fa3(%rip)        # 6870 <Int_Glob>
    18cd:	0f 8d 7d fa ff ff    	jge    1350 <main+0x230>
    18d3:	e9 9d fa ff ff       	jmp    1375 <main+0x255>
    18d8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
    18df:	00 
    18e0:	bd 09 00 00 00       	mov    $0x9,%ebp
    18e5:	e9 8f fa ff ff       	jmp    1379 <main+0x259>
    18ea:	66 0f ef d2          	pxor   %xmm2,%xmm2
    18ee:	f3 0f 10 0d f6 0b 00 	movss  0xbf6(%rip),%xmm1        # 24ec <_IO_stdin_used+0x4ec>
    18f5:	00 
    18f6:	bf 01 00 00 00       	mov    $0x1,%edi
    18fb:	48 8d 35 3e 09 00 00 	lea    0x93e(%rip),%rsi        # 2240 <_IO_stdin_used+0x240>
    1902:	f3 48 0f 2a d0       	cvtsi2ss %rax,%xmm2
    1907:	b8 01 00 00 00       	mov    $0x1,%eax
    190c:	f3 0f 5e ca          	divss  %xmm2,%xmm1
    1910:	0f 28 c2             	movaps %xmm2,%xmm0
    1913:	f3 0f 5e 05 cd 0b 00 	divss  0xbcd(%rip),%xmm0        # 24e8 <_IO_stdin_used+0x4e8>
    191a:	00 
    191b:	f3 0f 11 05 21 27 00 	movss  %xmm0,0x2721(%rip)        # 4044 <Microseconds>
    1922:	00 
    1923:	f3 0f 59 05 c5 0b 00 	mulss  0xbc5(%rip),%xmm0        # 24f0 <_IO_stdin_used+0x4f0>
    192a:	00 
    192b:	f3 0f 5a c0          	cvtss2sd %xmm0,%xmm0
    192f:	f3 0f 11 0d 09 27 00 	movss  %xmm1,0x2709(%rip)        # 4040 <Dhrystones_Per_Second>
    1936:	00 
    1937:	e8 d4 f7 ff ff       	call   1110 <__printf_chk@plt>
    193c:	bf 01 00 00 00       	mov    $0x1,%edi
    1941:	48 8d 35 30 09 00 00 	lea    0x930(%rip),%rsi        # 2278 <_IO_stdin_used+0x278>
    1948:	b8 01 00 00 00       	mov    $0x1,%eax
    194d:	f3 0f 10 05 eb 26 00 	movss  0x26eb(%rip),%xmm0        # 4040 <Dhrystones_Per_Second>
    1954:	00 
    1955:	f3 0f 5e 05 97 0b 00 	divss  0xb97(%rip),%xmm0        # 24f4 <_IO_stdin_used+0x4f4>
    195c:	00 
    195d:	f3 0f 5a c0          	cvtss2sd %xmm0,%xmm0
    1961:	e8 aa f7 ff ff       	call   1110 <__printf_chk@plt>
    1966:	bf 01 00 00 00       	mov    $0x1,%edi
    196b:	48 8d 35 3e 09 00 00 	lea    0x93e(%rip),%rsi        # 22b0 <_IO_stdin_used+0x2b0>
    1972:	b8 01 00 00 00       	mov    $0x1,%eax
    1977:	f3 0f 10 05 c1 26 00 	movss  0x26c1(%rip),%xmm0        # 4040 <Dhrystones_Per_Second>
    197e:	00 
    197f:	f3 0f 5e 05 71 0b 00 	divss  0xb71(%rip),%xmm0        # 24f8 <_IO_stdin_used+0x4f8>
    1986:	00 
    1987:	f3 0f 5a c0          	cvtss2sd %xmm0,%xmm0
    198b:	e8 80 f7 ff ff       	call   1110 <__printf_chk@plt>
    1990:	bf 0a 00 00 00       	mov    $0xa,%edi
    1995:	e8 16 f7 ff ff       	call   10b0 <putchar@plt>
    199a:	e9 b8 fe ff ff       	jmp    1857 <main+0x737>
    199f:	e8 2c f7 ff ff       	call   10d0 <__stack_chk_fail@plt>
    19a4:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    19ab:	00 00 00 
    19ae:	66 90                	xchg   %ax,%ax

00000000000019b0 <_start>:
    19b0:	f3 0f 1e fa          	endbr64 
    19b4:	31 ed                	xor    %ebp,%ebp
    19b6:	49 89 d1             	mov    %rdx,%r9
    19b9:	5e                   	pop    %rsi
    19ba:	48 89 e2             	mov    %rsp,%rdx
    19bd:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
    19c1:	50                   	push   %rax
    19c2:	54                   	push   %rsp
    19c3:	45 31 c0             	xor    %r8d,%r8d
    19c6:	31 c9                	xor    %ecx,%ecx
    19c8:	48 8d 3d 51 f7 ff ff 	lea    -0x8af(%rip),%rdi        # 1120 <main>
    19cf:	ff 15 03 26 00 00    	call   *0x2603(%rip)        # 3fd8 <__libc_start_main@GLIBC_2.34>
    19d5:	f4                   	hlt    
    19d6:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    19dd:	00 00 00 

00000000000019e0 <deregister_tm_clones>:
    19e0:	48 8d 3d 29 26 00 00 	lea    0x2629(%rip),%rdi        # 4010 <__TMC_END__>
    19e7:	48 8d 05 22 26 00 00 	lea    0x2622(%rip),%rax        # 4010 <__TMC_END__>
    19ee:	48 39 f8             	cmp    %rdi,%rax
    19f1:	74 15                	je     1a08 <deregister_tm_clones+0x28>
    19f3:	48 8b 05 e6 25 00 00 	mov    0x25e6(%rip),%rax        # 3fe0 <_ITM_deregisterTMCloneTable@Base>
    19fa:	48 85 c0             	test   %rax,%rax
    19fd:	74 09                	je     1a08 <deregister_tm_clones+0x28>
    19ff:	ff e0                	jmp    *%rax
    1a01:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    1a08:	c3                   	ret    
    1a09:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001a10 <register_tm_clones>:
    1a10:	48 8d 3d f9 25 00 00 	lea    0x25f9(%rip),%rdi        # 4010 <__TMC_END__>
    1a17:	48 8d 35 f2 25 00 00 	lea    0x25f2(%rip),%rsi        # 4010 <__TMC_END__>
    1a1e:	48 29 fe             	sub    %rdi,%rsi
    1a21:	48 89 f0             	mov    %rsi,%rax
    1a24:	48 c1 ee 3f          	shr    $0x3f,%rsi
    1a28:	48 c1 f8 03          	sar    $0x3,%rax
    1a2c:	48 01 c6             	add    %rax,%rsi
    1a2f:	48 d1 fe             	sar    %rsi
    1a32:	74 14                	je     1a48 <register_tm_clones+0x38>
    1a34:	48 8b 05 b5 25 00 00 	mov    0x25b5(%rip),%rax        # 3ff0 <_ITM_registerTMCloneTable@Base>
    1a3b:	48 85 c0             	test   %rax,%rax
    1a3e:	74 08                	je     1a48 <register_tm_clones+0x38>
    1a40:	ff e0                	jmp    *%rax
    1a42:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    1a48:	c3                   	ret    
    1a49:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001a50 <__do_global_dtors_aux>:
    1a50:	f3 0f 1e fa          	endbr64 
    1a54:	80 3d c5 25 00 00 00 	cmpb   $0x0,0x25c5(%rip)        # 4020 <completed.0>
    1a5b:	75 2b                	jne    1a88 <__do_global_dtors_aux+0x38>
    1a5d:	55                   	push   %rbp
    1a5e:	48 83 3d 92 25 00 00 	cmpq   $0x0,0x2592(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    1a65:	00 
    1a66:	48 89 e5             	mov    %rsp,%rbp
    1a69:	74 0c                	je     1a77 <__do_global_dtors_aux+0x27>
    1a6b:	48 8b 3d 96 25 00 00 	mov    0x2596(%rip),%rdi        # 4008 <__dso_handle>
    1a72:	e8 29 f6 ff ff       	call   10a0 <__cxa_finalize@plt>
    1a77:	e8 64 ff ff ff       	call   19e0 <deregister_tm_clones>
    1a7c:	c6 05 9d 25 00 00 01 	movb   $0x1,0x259d(%rip)        # 4020 <completed.0>
    1a83:	5d                   	pop    %rbp
    1a84:	c3                   	ret    
    1a85:	0f 1f 00             	nopl   (%rax)
    1a88:	c3                   	ret    
    1a89:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001a90 <frame_dummy>:
    1a90:	f3 0f 1e fa          	endbr64 
    1a94:	e9 77 ff ff ff       	jmp    1a10 <register_tm_clones>
    1a99:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001aa0 <Proc_1>:
    1aa0:	f3 0f 1e fa          	endbr64 
    1aa4:	55                   	push   %rbp
    1aa5:	48 89 fd             	mov    %rdi,%rbp
    1aa8:	53                   	push   %rbx
    1aa9:	48 83 ec 08          	sub    $0x8,%rsp
    1aad:	48 8b 05 cc 4d 00 00 	mov    0x4dcc(%rip),%rax        # 6880 <Ptr_Glob>
    1ab4:	48 8b 1f             	mov    (%rdi),%rbx
    1ab7:	8b 35 b3 4d 00 00    	mov    0x4db3(%rip),%esi        # 6870 <Int_Glob>
    1abd:	f3 0f 6f 00          	movdqu (%rax),%xmm0
    1ac1:	0f 11 03             	movups %xmm0,(%rbx)
    1ac4:	f3 0f 6f 48 10       	movdqu 0x10(%rax),%xmm1
    1ac9:	0f 11 4b 10          	movups %xmm1,0x10(%rbx)
    1acd:	f3 0f 6f 50 20       	movdqu 0x20(%rax),%xmm2
    1ad2:	0f 11 53 20          	movups %xmm2,0x20(%rbx)
    1ad6:	48 8b 50 30          	mov    0x30(%rax),%rdx
    1ada:	48 89 53 30          	mov    %rdx,0x30(%rbx)
    1ade:	48 8b 17             	mov    (%rdi),%rdx
    1ae1:	c7 47 10 05 00 00 00 	movl   $0x5,0x10(%rdi)
    1ae8:	bf 0a 00 00 00       	mov    $0xa,%edi
    1aed:	48 89 13             	mov    %rdx,(%rbx)
    1af0:	48 8b 00             	mov    (%rax),%rax
    1af3:	c7 43 10 05 00 00 00 	movl   $0x5,0x10(%rbx)
    1afa:	48 89 03             	mov    %rax,(%rbx)
    1afd:	48 8b 05 7c 4d 00 00 	mov    0x4d7c(%rip),%rax        # 6880 <Ptr_Glob>
    1b04:	48 8d 50 10          	lea    0x10(%rax),%rdx
    1b08:	e8 63 01 00 00       	call   1c70 <Proc_7>
    1b0d:	8b 43 08             	mov    0x8(%rbx),%eax
    1b10:	85 c0                	test   %eax,%eax
    1b12:	74 34                	je     1b48 <Proc_1+0xa8>
    1b14:	48 8b 45 00          	mov    0x0(%rbp),%rax
    1b18:	f3 0f 6f 18          	movdqu (%rax),%xmm3
    1b1c:	0f 11 5d 00          	movups %xmm3,0x0(%rbp)
    1b20:	f3 0f 6f 60 10       	movdqu 0x10(%rax),%xmm4
    1b25:	0f 11 65 10          	movups %xmm4,0x10(%rbp)
    1b29:	f3 0f 6f 68 20       	movdqu 0x20(%rax),%xmm5
    1b2e:	0f 11 6d 20          	movups %xmm5,0x20(%rbp)
    1b32:	48 8b 40 30          	mov    0x30(%rax),%rax
    1b36:	48 89 45 30          	mov    %rax,0x30(%rbp)
    1b3a:	48 83 c4 08          	add    $0x8,%rsp
    1b3e:	5b                   	pop    %rbx
    1b3f:	5d                   	pop    %rbp
    1b40:	c3                   	ret    
    1b41:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    1b48:	c7 43 10 06 00 00 00 	movl   $0x6,0x10(%rbx)
    1b4f:	8b 7d 0c             	mov    0xc(%rbp),%edi
    1b52:	48 8d 73 0c          	lea    0xc(%rbx),%rsi
    1b56:	e8 c5 00 00 00       	call   1c20 <Proc_6>
    1b5b:	48 8b 05 1e 4d 00 00 	mov    0x4d1e(%rip),%rax        # 6880 <Ptr_Glob>
    1b62:	8b 7b 10             	mov    0x10(%rbx),%edi
    1b65:	48 8d 53 10          	lea    0x10(%rbx),%rdx
    1b69:	be 0a 00 00 00       	mov    $0xa,%esi
    1b6e:	48 8b 00             	mov    (%rax),%rax
    1b71:	48 89 03             	mov    %rax,(%rbx)
    1b74:	48 83 c4 08          	add    $0x8,%rsp
    1b78:	5b                   	pop    %rbx
    1b79:	5d                   	pop    %rbp
    1b7a:	e9 f1 00 00 00       	jmp    1c70 <Proc_7>
    1b7f:	90                   	nop

0000000000001b80 <Proc_2>:
    1b80:	f3 0f 1e fa          	endbr64 
    1b84:	80 3d de 4c 00 00 41 	cmpb   $0x41,0x4cde(%rip)        # 6869 <Ch_1_Glob>
    1b8b:	74 03                	je     1b90 <Proc_2+0x10>
    1b8d:	c3                   	ret    
    1b8e:	66 90                	xchg   %ax,%ax
    1b90:	8b 07                	mov    (%rdi),%eax
    1b92:	83 c0 09             	add    $0x9,%eax
    1b95:	2b 05 d5 4c 00 00    	sub    0x4cd5(%rip),%eax        # 6870 <Int_Glob>
    1b9b:	89 07                	mov    %eax,(%rdi)
    1b9d:	c3                   	ret    
    1b9e:	66 90                	xchg   %ax,%ax

0000000000001ba0 <Proc_3>:
    1ba0:	f3 0f 1e fa          	endbr64 
    1ba4:	48 8b 15 d5 4c 00 00 	mov    0x4cd5(%rip),%rdx        # 6880 <Ptr_Glob>
    1bab:	48 85 d2             	test   %rdx,%rdx
    1bae:	74 0d                	je     1bbd <Proc_3+0x1d>
    1bb0:	48 8b 02             	mov    (%rdx),%rax
    1bb3:	48 89 07             	mov    %rax,(%rdi)
    1bb6:	48 8b 15 c3 4c 00 00 	mov    0x4cc3(%rip),%rdx        # 6880 <Ptr_Glob>
    1bbd:	8b 35 ad 4c 00 00    	mov    0x4cad(%rip),%esi        # 6870 <Int_Glob>
    1bc3:	48 83 c2 10          	add    $0x10,%rdx
    1bc7:	bf 0a 00 00 00       	mov    $0xa,%edi
    1bcc:	e9 9f 00 00 00       	jmp    1c70 <Proc_7>
    1bd1:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
    1bd8:	00 00 00 00 
    1bdc:	0f 1f 40 00          	nopl   0x0(%rax)

0000000000001be0 <Proc_4>:
    1be0:	f3 0f 1e fa          	endbr64 
    1be4:	31 c0                	xor    %eax,%eax
    1be6:	80 3d 7c 4c 00 00 41 	cmpb   $0x41,0x4c7c(%rip)        # 6869 <Ch_1_Glob>
    1bed:	c6 05 74 4c 00 00 42 	movb   $0x42,0x4c74(%rip)        # 6868 <Ch_2_Glob>
    1bf4:	0f 94 c0             	sete   %al
    1bf7:	09 05 6f 4c 00 00    	or     %eax,0x4c6f(%rip)        # 686c <Bool_Glob>
    1bfd:	c3                   	ret    
    1bfe:	66 90                	xchg   %ax,%ax

0000000000001c00 <Proc_5>:
    1c00:	f3 0f 1e fa          	endbr64 
    1c04:	c6 05 5e 4c 00 00 41 	movb   $0x41,0x4c5e(%rip)        # 6869 <Ch_1_Glob>
    1c0b:	c7 05 57 4c 00 00 00 	movl   $0x0,0x4c57(%rip)        # 686c <Bool_Glob>
    1c12:	00 00 00 
    1c15:	c3                   	ret    
    1c16:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    1c1d:	00 00 00 

0000000000001c20 <Proc_6>:
    1c20:	f3 0f 1e fa          	endbr64 
    1c24:	83 ff 02             	cmp    $0x2,%edi
    1c27:	74 3f                	je     1c68 <Proc_6+0x48>
    1c29:	c7 06 03 00 00 00    	movl   $0x3,(%rsi)
    1c2f:	83 ff 01             	cmp    $0x1,%edi
    1c32:	74 14                	je     1c48 <Proc_6+0x28>
    1c34:	76 1b                	jbe    1c51 <Proc_6+0x31>
    1c36:	83 ff 04             	cmp    $0x4,%edi
    1c39:	75 25                	jne    1c60 <Proc_6+0x40>
    1c3b:	c7 06 02 00 00 00    	movl   $0x2,(%rsi)
    1c41:	c3                   	ret    
    1c42:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    1c48:	83 3d 21 4c 00 00 64 	cmpl   $0x64,0x4c21(%rip)        # 6870 <Int_Glob>
    1c4f:	7e f0                	jle    1c41 <Proc_6+0x21>
    1c51:	c7 06 00 00 00 00    	movl   $0x0,(%rsi)
    1c57:	c3                   	ret    
    1c58:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
    1c5f:	00 
    1c60:	c3                   	ret    
    1c61:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    1c68:	c7 06 01 00 00 00    	movl   $0x1,(%rsi)
    1c6e:	c3                   	ret    
    1c6f:	90                   	nop

0000000000001c70 <Proc_7>:
    1c70:	f3 0f 1e fa          	endbr64 
    1c74:	8d 44 37 02          	lea    0x2(%rdi,%rsi,1),%eax
    1c78:	89 02                	mov    %eax,(%rdx)
    1c7a:	c3                   	ret    
    1c7b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000001c80 <Proc_8>:
    1c80:	f3 0f 1e fa          	endbr64 
    1c84:	44 8d 42 05          	lea    0x5(%rdx),%r8d
    1c88:	48 89 f0             	mov    %rsi,%rax
    1c8b:	66 0f 6e c9          	movd   %ecx,%xmm1
    1c8f:	48 63 d2             	movslq %edx,%rdx
    1c92:	4d 63 d0             	movslq %r8d,%r10
    1c95:	66 0f 70 c1 e0       	pshufd $0xe0,%xmm1,%xmm0
    1c9a:	48 c1 e2 02          	shl    $0x2,%rdx
    1c9e:	4a 8d 34 95 00 00 00 	lea    0x0(,%r10,4),%rsi
    1ca5:	00 
    1ca6:	4c 8d 0c 37          	lea    (%rdi,%rsi,1),%r9
    1caa:	66 41 0f d6 01       	movq   %xmm0,(%r9)
    1caf:	44 89 44 37 78       	mov    %r8d,0x78(%rdi,%rsi,1)
    1cb4:	4c 01 d6             	add    %r10,%rsi
    1cb7:	48 8d 34 b6          	lea    (%rsi,%rsi,4),%rsi
    1cbb:	48 c1 e6 03          	shl    $0x3,%rsi
    1cbf:	48 8d 0c 16          	lea    (%rsi,%rdx,1),%rcx
    1cc3:	48 01 c1             	add    %rax,%rcx
    1cc6:	48 01 f0             	add    %rsi,%rax
    1cc9:	83 41 10 01          	addl   $0x1,0x10(%rcx)
    1ccd:	44 89 41 14          	mov    %r8d,0x14(%rcx)
    1cd1:	44 89 41 18          	mov    %r8d,0x18(%rcx)
    1cd5:	41 8b 09             	mov    (%r9),%ecx
    1cd8:	c7 05 8e 4b 00 00 05 	movl   $0x5,0x4b8e(%rip)        # 6870 <Int_Glob>
    1cdf:	00 00 00 
    1ce2:	89 8c 02 b4 0f 00 00 	mov    %ecx,0xfb4(%rdx,%rax,1)
    1ce9:	c3                   	ret    
    1cea:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)

0000000000001cf0 <Func_1>:
    1cf0:	f3 0f 1e fa          	endbr64 
    1cf4:	31 c0                	xor    %eax,%eax
    1cf6:	40 38 f7             	cmp    %sil,%dil
    1cf9:	74 05                	je     1d00 <Func_1+0x10>
    1cfb:	c3                   	ret    
    1cfc:	0f 1f 40 00          	nopl   0x0(%rax)
    1d00:	40 88 3d 62 4b 00 00 	mov    %dil,0x4b62(%rip)        # 6869 <Ch_1_Glob>
    1d07:	b8 01 00 00 00       	mov    $0x1,%eax
    1d0c:	c3                   	ret    
    1d0d:	0f 1f 00             	nopl   (%rax)

0000000000001d10 <Func_2>:
    1d10:	f3 0f 1e fa          	endbr64 
    1d14:	0f b6 47 02          	movzbl 0x2(%rdi),%eax
    1d18:	38 46 03             	cmp    %al,0x3(%rsi)
    1d1b:	74 28                	je     1d45 <Func_2+0x35>
    1d1d:	48 83 ec 08          	sub    $0x8,%rsp
    1d21:	e8 ca f3 ff ff       	call   10f0 <strcmp@plt>
    1d26:	45 31 c0             	xor    %r8d,%r8d
    1d29:	85 c0                	test   %eax,%eax
    1d2b:	7e 10                	jle    1d3d <Func_2+0x2d>
    1d2d:	c7 05 39 4b 00 00 0a 	movl   $0xa,0x4b39(%rip)        # 6870 <Int_Glob>
    1d34:	00 00 00 
    1d37:	41 b8 01 00 00 00    	mov    $0x1,%r8d
    1d3d:	44 89 c0             	mov    %r8d,%eax
    1d40:	48 83 c4 08          	add    $0x8,%rsp
    1d44:	c3                   	ret    
    1d45:	eb fe                	jmp    1d45 <Func_2+0x35>
    1d47:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
    1d4e:	00 00 

0000000000001d50 <Func_3>:
    1d50:	f3 0f 1e fa          	endbr64 
    1d54:	31 c0                	xor    %eax,%eax
    1d56:	83 ff 02             	cmp    $0x2,%edi
    1d59:	0f 94 c0             	sete   %al
    1d5c:	c3                   	ret    

Disassembly of section .fini:

0000000000001d60 <_fini>:
    1d60:	f3 0f 1e fa          	endbr64 
    1d64:	48 83 ec 08          	sub    $0x8,%rsp
    1d68:	48 83 c4 08          	add    $0x8,%rsp
    1d6c:	c3                   	ret    
