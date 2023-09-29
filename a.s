
a.out:     file format elf64-x86-64


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
    1078:	48 8d 3d 6c 01 00 00 	lea    0x16c(%rip),%rdi        # 11eb <main>
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

00000000000011eb <main>:
    11eb:	f3 0f 1e fa          	endbr64 
    11ef:	55                   	push   %rbp
    11f0:	48 89 e5             	mov    %rsp,%rbp
    11f3:	48 83 ec 20          	sub    $0x20,%rsp
    11f7:	89 7d ec             	mov    %edi,-0x14(%rbp)
    11fa:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
    11fe:	c7 45 f8 01 00 00 00 	movl   $0x1,-0x8(%rbp)
    1205:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
    120c:	8b 55 fc             	mov    -0x4(%rbp),%edx
    120f:	8b 45 f8             	mov    -0x8(%rbp),%eax
    1212:	89 d6                	mov    %edx,%esi
    1214:	89 c7                	mov    %eax,%edi
    1216:	e8 2e ff ff ff       	call   1149 <_Z12functioncallii>
    121b:	89 45 fc             	mov    %eax,-0x4(%rbp)
    121e:	8b 55 fc             	mov    -0x4(%rbp),%edx
    1221:	8b 45 f8             	mov    -0x8(%rbp),%eax
    1224:	89 d6                	mov    %edx,%esi
    1226:	89 c7                	mov    %eax,%edi
    1228:	e8 4d ff ff ff       	call   117a <_Z7foocallii>
    122d:	89 45 fc             	mov    %eax,-0x4(%rbp)
    1230:	b8 00 00 00 00       	mov    $0x0,%eax
    1235:	c9                   	leave  
    1236:	c3                   	ret    

Disassembly of section .fini:

0000000000001238 <_fini>:
    1238:	f3 0f 1e fa          	endbr64 
    123c:	48 83 ec 08          	sub    $0x8,%rsp
    1240:	48 83 c4 08          	add    $0x8,%rsp
    1244:	c3                   	ret    
