00001423 <pop>:
    1423:	55                   	push   %ebp
    1424:	89 e5                	mov    %esp,%ebp
    1426:	57                   	push   %edi
    1427:	56                   	push   %esi
    1428:	53                   	push   %ebx
    1429:	83 ec 0c             	sub    $0xc,%esp
    142c:	8b 75 08             	mov    0x8(%ebp),%esi
    142f:	0f b7 06             	movzwl (%esi),%eax
    1432:	66 85 c0             	test   %ax,%ax
    1435:	7e 41                	jle    1478 <pop+0x55>
    1437:	8d 50 ff             	lea    0xffffffff(%eax),%edx
    143a:	66 89 16             	mov    %dx,(%esi)
    143d:	98                   	cwtl   
    143e:	c1 e0 04             	shl    $0x4,%eax
    1441:	8d 5c 30 50          	lea    0x50(%eax,%esi,1),%ebx
    1445:	85 db                	test   %ebx,%ebx
    1447:	74 2f                	je     1478 <pop+0x55>
    1449:	31 c0                	xor    %eax,%eax
    144b:	66 85 d2             	test   %dx,%dx
    144e:	78 0a                	js     145a <pop+0x37>
    1450:	0f bf c2             	movswl %dx,%eax
    1453:	c1 e0 04             	shl    $0x4,%eax
    1456:	8d 44 30 50          	lea    0x50(%eax,%esi,1),%eax
    145a:	8b 00                	mov    (%eax),%eax
    145c:	8b 40 08             	mov    0x8(%eax),%eax
    145f:	0f 22 d8             	mov    %eax,%cr3
    1462:	e8 fc ff ff ff       	call   1463 <pop+0x40>
			1463: R_386_PC32	switch_host_pg_tbls
    1467:	8b 03                	mov    (%ebx),%eax
    1469:	e8 fc ff ff ff       	call   146a <pop+0x47>
			146a: R_386_PC32	spd_mpd_release
    146e:	89 d8                	mov    %ebx,%eax
    1470:	83 c4 0c             	add    $0xc,%esp
    1473:	5b                   	pop    %ebx
    1474:	5e                   	pop    %esi
    1475:	5f                   	pop    %edi
    1476:	5d                   	pop    %ebp
    1477:	c3                   	ret    
00001599 <ipc_walk_static_cap>:
    1599:	55                   	push   %ebp
    159a:	89 e5                	mov    %esp,%ebp
    159c:	57                   	push   %edi
    159d:	56                   	push   %esi
    159e:	53                   	push   %ebx
    159f:	83 ec 18             	sub    $0x18,%esp
    15a2:	8b 75 0c             	mov    0xc(%ebp),%esi
    15a5:	c1 ee 14             	shr    $0x14,%esi
    15a8:	81 fe e7 03 00 00    	cmp    $0x3e7,%esi
    15ae:	0f 87 8b 01 00 00    	ja     173f <ipc_walk_static_cap+0x1a6>
    15b4:	89 f0                	mov    %esi,%eax
    15b6:	c1 e0 06             	shl    $0x6,%eax
    15b9:	8d 90 00 00 00 00    	lea    0x0(%eax),%edx
			15bb: R_386_32	invocation_capabilities
    15bf:	89 55 f0             	mov    %edx,0xfffffff0(%ebp)
    15c2:	8b 98 00 00 00 00    	mov    0x0(%eax),%ebx
			15c4: R_386_32	invocation_capabilities
    15c8:	85 db                	test   %ebx,%ebx
    15ca:	0f 84 aa 01 00 00    	je     177a <ipc_walk_static_cap+0x1e1>
    15d0:	8b 4d 08             	mov    0x8(%ebp),%ecx
    15d3:	0f b7 01             	movzwl (%ecx),%eax
    15d6:	8b 55 f0             	mov    0xfffffff0(%ebp),%edx
    15d9:	8b 52 04             	mov    0x4(%edx),%edx
    15dc:	89 55 ec             	mov    %edx,0xffffffec(%ebp)
    15df:	85 d2                	test   %edx,%edx
    15e1:	0f 84 42 01 00 00    	je     1729 <ipc_walk_static_cap+0x190>
    15e7:	83 fb 01             	cmp    $0x1,%ebx
    15ea:	0f 84 39 01 00 00    	je     1729 <ipc_walk_static_cap+0x190>
    15f0:	98                   	cwtl   
    15f1:	c1 e0 04             	shl    $0x4,%eax
    15f4:	83 c0 40             	add    $0x40,%eax
    15f7:	8b 7d 08             	mov    0x8(%ebp),%edi
    15fa:	01 c7                	add    %eax,%edi
    15fc:	8b 43 14             	mov    0x14(%ebx),%eax
    15ff:	3b 47 10             	cmp    0x10(%edi),%eax
    1602:	0f 84 85 00 00 00    	je     168d <ipc_walk_static_cap+0xf4>
    1608:	89 da                	mov    %ebx,%edx
    160a:	8b 45 08             	mov    0x8(%ebp),%eax
    160d:	e8 fc ff ff ff       	call   160e <ipc_walk_static_cap+0x75>
			160e: R_386_PC32	thd_spd_in_current_composite
    1612:	85 c0                	test   %eax,%eax
    1614:	75 77                	jne    168d <ipc_walk_static_cap+0xf4>
    ...
    168d:	8b 4d f0             	mov    0xfffffff0(%ebp),%ecx
    1690:	8b 51 08             	mov    0x8(%ecx),%edx
    1693:	83 c2 01             	add    $0x1,%edx
    1696:	81 e2 ff ff ff 3f    	and    $0x3fffffff,%edx
    169c:	8b 41 08             	mov    0x8(%ecx),%eax
    169f:	25 00 00 00 c0       	and    $0xc0000000,%eax
    16a4:	09 d0                	or     %edx,%eax
    16a6:	89 41 08             	mov    %eax,0x8(%ecx)
    16a9:	8b 55 ec             	mov    0xffffffec(%ebp),%edx
    16ac:	8b 42 14             	mov    0x14(%edx),%eax
    16af:	8b 50 08             	mov    0x8(%eax),%edx
    16b2:	8b 43 14             	mov    0x14(%ebx),%eax
    16b5:	39 50 08             	cmp    %edx,0x8(%eax)
    16b8:	74 0a                	je     16c4 <ipc_walk_static_cap+0x12b>
    16ba:	0f 22 da             	mov    %edx,%cr3
    16bd:	89 d0                	mov    %edx,%eax
    16bf:	e8 fc ff ff ff       	call   16c0 <ipc_walk_static_cap+0x127>
			16c0: R_386_PC32	switch_host_pg_tbls
    16c4:	8b 4d 08             	mov    0x8(%ebp),%ecx
    16c7:	0f b7 41 02          	movzwl 0x2(%ecx),%eax
    16cb:	8b 55 18             	mov    0x18(%ebp),%edx
    16ce:	89 02                	mov    %eax,(%edx)
    16d0:	81 eb 00 00 00 00    	sub    $0x0,%ebx
			16d2: R_386_32	spds
    16d6:	c1 eb 07             	shr    $0x7,%ebx
    16d9:	89 5a 04             	mov    %ebx,0x4(%edx)
    16dc:	8b 4d ec             	mov    0xffffffec(%ebp),%ecx
    16df:	8b 41 14             	mov    0x14(%ecx),%eax
    16e2:	83 c0 04             	add    $0x4,%eax
    16e5:	83 00 01             	addl   $0x1,(%eax)
    16e8:	8b 45 f0             	mov    0xfffffff0(%ebp),%eax
    16eb:	8b 58 04             	mov    0x4(%eax),%ebx
    16ee:	8b 55 08             	mov    0x8(%ebp),%edx
    16f1:	0f b7 02             	movzwl (%edx),%eax
    16f4:	83 c0 01             	add    $0x1,%eax
    16f7:	66 89 02             	mov    %ax,(%edx)
    16fa:	98                   	cwtl   
    16fb:	c1 e0 04             	shl    $0x4,%eax
    16fe:	83 c0 40             	add    $0x40,%eax
    1701:	01 d0                	add    %edx,%eax
    1703:	8d 48 10             	lea    0x10(%eax),%ecx
    1706:	8b 53 14             	mov    0x14(%ebx),%edx
    1709:	89 50 10             	mov    %edx,0x10(%eax)
    170c:	8b 45 10             	mov    0x10(%ebp),%eax
    170f:	89 41 08             	mov    %eax,0x8(%ecx)
    1712:	8b 45 14             	mov    0x14(%ebp),%eax
    1715:	89 41 0c             	mov    %eax,0xc(%ecx)
    1718:	89 59 04             	mov    %ebx,0x4(%ecx)
    171b:	8b 55 f0             	mov    0xfffffff0(%ebp),%edx
    171e:	8b 42 0c             	mov    0xc(%edx),%eax
    1721:	83 c4 18             	add    $0x18,%esp
    1724:	5b                   	pop    %ebx
    1725:	5e                   	pop    %esi
    1726:	5f                   	pop    %edi
    1727:	5d                   	pop    %ebp
    1728:	c3                   	ret    


Summary: Total 128 instructions, 
	 pop: 36 instructions, 8 memory reads
         ipc_walk_static_cap: 92, 34 memory reads (15 being spills)
