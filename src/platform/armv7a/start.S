/* the assembly part for cortex-a7 */
    .section            .text
    .code               32
    .align              16

.macro cp15_get_init crn op1 crm op2
    mrc                 p15,\op1,r0,\crn,\crm,\op2
    isb
.endm

.macro cp15_get crn op1 crm op2
    mrc                 p15,\op1,r0,\crn,\crm,\op2
    isb
    bx                  lr
.endm

.macro cp15_get_double crm op
    mrrc                p15,\op,r2,r3,\crm
    isb
    str                 r2,[r0]
    str                 r3,[r1]
    bx                  lr
.endm

.macro cp15_set_init crn op1 crm op2
    mcr                 p15,\op1,r0,\crn,\crm,\op2
    isb
.endm

.macro cp15_set crn op1 crm op2
    mcr                 p15,\op1,r0,\crn,\crm,\op2
    isb
    bx                  lr
.endm

.macro cp15_set_double crm op
    mcrr                p15,\op,r0,r0,\crm
    isb
    bx                  lr
.endm

    /* disable all interrupts */
    .global             __cos_disable_int
    /* enable all interrupts */
    .global             __cos_enable_int
    /* read acquire/write release for cortex-a (armv7) */
    .global             __cos_cav7_read_acquire
    .global             __cos_cav7_write_release
    /* get the msb in a word */
    .global             __cos_cav7_msb_get
    /* kernel main function wrapper */
    .global             _cos_kmain
    /* entering of the user mode */
    .global             __cos_enter_user_mode

    /* cav7 specific stuff */
    /* halt processor to wait for interrupt */
    .global             __cos_cav7_halt
    /* load page table */
    .global             __cos_cav7_pgtbl_set
    /* get register values */
    .global             __cos_cav7_cpsr_get
    .global             __cos_cav7_spsr_get
    /* c0 */
    .global             __cos_cav7_midr_get
    .global             __cos_cav7_ctr_get
    .global             __cos_cav7_tcmtr_get
    .global             __cos_cav7_tlbtr_get
    .global             __cos_cav7_mpidr_get
    .global             __cos_cav7_revidr_get
    .global             __cos_cav7_id_pfr0_get
    .global             __cos_cav7_id_pfr1_get
    .global             __cos_cav7_id_dfr0_get
    .global             __cos_cav7_id_afr0_get
    .global             __cos_cav7_id_mmfr0_get
    .global             __cos_cav7_id_mmfr1_get
    .global             __cos_cav7_id_mmfr2_get
    .global             __cos_cav7_id_mmfr3_get
    .global             __cos_cav7_id_isar0_get
    .global             __cos_cav7_id_isar1_get
    .global             __cos_cav7_id_isar2_get
    .global             __cos_cav7_id_isar3_get
    .global             __cos_cav7_id_isar4_get
    .global             __cos_cav7_id_isar5_get
    .global             __cos_cav7_id_ccsidr_get
    .global             __cos_cav7_id_clidr_get
    .global             __cos_cav7_id_aidr_get
    .global             __cos_cav7_id_csselr_get
    .global             __cos_cav7_id_vpidr_get
    .global             __cos_cav7_id_vmpidr_get
    /* c1 */
    .global             __cos_cav7_sctlr_get
    .global             __cos_cav7_actlr_get
    .global             __cos_cav7_cpacr_get
    .global             __cos_cav7_scr_get
    .global             __cos_cav7_sder_get
    .global             __cos_cav7_nsacr_get
    .global             __cos_cav7_hsctlr_get
    .global             __cos_cav7_hactlr_get
    .global             __cos_cav7_hcr_get
    .global             __cos_cav7_hdcr_get
    .global             __cos_cav7_hcptr_get
    .global             __cos_cav7_hstr_get
    .global             __cos_cav7_hacr_get
    /* c2 */
    .global             __cos_cav7_ttbr0_get
    .global             __cos_cav7_ttbr1_get
    .global             __cos_cav7_ttbcr_get
    .global             __cos_cav7_htcr_get
    .global             __cos_cav7_vtcr_get
    .global             __cos_cav7_dacr_get
    /* c5 */
    .global             __cos_cav7_dfsr_get
    .global             __cos_cav7_ifsr_get
    .global             __cos_cav7_adfsr_get
    .global             __cos_cav7_aifsr_get
    .global             __cos_cav7_hadfsr_get
    .global             __cos_cav7_haifsr_get
    .global             __cos_cav7_hsr_get
    .global             __cos_cav7_dfar_get
    .global             __cos_cav7_ifar_get
    .global             __cos_cav7_hdfar_get
    .global             __cos_cav7_hifar_get
    .global             __cos_cav7_hpfar_get
    .global             __cos_cav7_par_get
    /* c9 */
    .global             __cos_cav7_pmcr_get
    .global             __cos_cav7_pmcntenset_get
    .global             __cos_cav7_pmcntenclr_get
    .global             __cos_cav7_pmovsr_get
    .global             __cos_cav7_pmselr_get
    .global             __cos_cav7_pmccntr_get
    .global             __cos_cav7_pmxevtyper_get
    .global             __cos_cav7_pmxevcntr_get
    .global             __cos_cav7_pmuserenr_get
    .global             __cos_cav7_pmintenset_get
    .global             __cos_cav7_pmintenclr_get
    /* c10 */
    .global             __cos_cav7_tlblr_get
    .global             __cos_cav7_prrr_get
    .global             __cos_cav7_nmrr_get
    .global             __cos_cav7_amair0_get
    .global             __cos_cav7_amair1_get
    .global             __cos_cav7_hmair0_get
    .global             __cos_cav7_hmair1_get
    .global             __cos_cav7_hamair0_get
    .global             __cos_cav7_hamair1_get
    /* c12 */
    .global             __cos_cav7_vbar_get
    .global             __cos_cav7_mvbar_get
    .global             __cos_cav7_isr_get
    .global             __cos_cav7_hvbar_get
    /* c13 */
    .global             __cos_cav7_fcseidr_get
    .global             __cos_cav7_contextidr_get
    .global             __cos_cav7_tpidrurw_get
    .global             __cos_cav7_tpidruro_get
    .global             __cos_cav7_tpidrurw_get
    .global             __cos_cav7_htpidr_get
    /* c14 */
    .global             __cos_cav7_cntfrq_get
    .global             __cos_cav7_cntkctl_get
    .global             __cos_cav7_cntp_tval_get
    .global             __cos_cav7_cntp_ctl_get
    .global             __cos_cav7_cntv_tval_get
    .global             __cos_cav7_cntv_ctl_get
    .global             __cos_cav7_cnthctl_get
    .global             __cos_cav7_cnthp_tval_get
    .global             __cos_cav7_cnthp_ctl_get
    /* c15 */
    .global 		__cos_cav7_sel_lkdn_read_set
    .global 		__cos_cav7_sel_lkdn_write_set
    .global 		__cos_cav7_main_tlb_va_set
    .global 		__cos_cav7_main_tlb_pa_set
    .global 		__cos_cav7_main_tlb_attrib_set
    /* double words */
    .global             __cos_cav7_cntpct_dw_get
    .global             __cos_cav7_cntvct_dw_get
    .global             __cos_cav7_cntp_cval_dw_get
    .global             __cos_cav7_cntv_cval_dw_get
    .global             __cos_cav7_cntvoff_dw_get
    .global             __cos_cav7_cnthp_cval_dw_get

    /* set register values */
    .global             __cos_cav7_cpsr_set
    .global             __cos_cav7_spsr_set
    /* c0 */
    .global             __cos_cav7_id_csselr_set
    .global             __cos_cav7_id_vpidr_set
    .global             __cos_cav7_id_vmpidr_set
    /* c1 */
    .global             __cos_cav7_sctlr_set
    .global             __cos_cav7_actlr_set
    .global             __cos_cav7_cpacr_set
    .global             __cos_cav7_scr_set
    .global             __cos_cav7_sder_set
    .global             __cos_cav7_nsacr_set
    .global             __cos_cav7_hsctlr_set
    .global             __cos_cav7_hactlr_set
    .global             __cos_cav7_hcr_set
    .global             __cos_cav7_hdcr_set
    .global             __cos_cav7_hcptr_set
    .global             __cos_cav7_hstr_set
    .global             __cos_cav7_hacr_set
    /* c2,c3 */
    .global             __cos_cav7_ttbr0_set
    .global             __cos_cav7_ttbr1_set
    .global             __cos_cav7_ttbcr_set
    .global             __cos_cav7_htcr_set
    .global             __cos_cav7_vtcr_set
    .global             __cos_cav7_dacr_set
    /* c5 */
    .global             __cos_cav7_dfsr_set
    .global             __cos_cav7_ifsr_set
    .global             __cos_cav7_adfsr_set
    .global             __cos_cav7_aifsr_set
    .global             __cos_cav7_hadfsr_set
    .global             __cos_cav7_haifsr_set
    .global             __cos_cav7_hsr_set
    .global             __cos_cav7_dfar_set
    .global             __cos_cav7_ifar_set
    .global             __cos_cav7_hdfar_set
    .global             __cos_cav7_hifar_set
    .global             __cos_cav7_hpfar_set
    /* c7 */
    .global             __cos_cav7_icialluis_set
    .global             __cos_cav7_bpiallis_set
    .global             __cos_cav7_par_set
    .global             __cos_cav7_iciallu_set
    .global             __cos_cav7_icimvau_set
    .global             __cos_cav7_cp15isb_set
    .global             __cos_cav7_bpiall_set
    .global             __cos_cav7_bpimva_set
    .global             __cos_cav7_dcimvac_set
    .global             __cos_cav7_dcisw_set
    .global             __cos_cav7_ats1cpr_set
    .global             __cos_cav7_ats1cpw_set
    .global             __cos_cav7_ats1cur_set
    .global             __cos_cav7_ats1cuw_set
    .global             __cos_cav7_ats12nsopr_set
    .global             __cos_cav7_ats12nsopw_set
    .global             __cos_cav7_ats12nsour_set
    .global             __cos_cav7_ats12nsouw_set
    .global             __cos_cav7_dccmvac_set
    .global             __cos_cav7_dccsw_set
    .global             __cos_cav7_cp15dsb_set
    .global             __cos_cav7_cp15dmb_set
    .global             __cos_cav7_dccmvau_set
    .global             __cos_cav7_dccimvac_set
    .global             __cos_cav7_dccisw_set
    .global             __cos_cav7_ats1hr_set
    .global             __cos_cav7_ats1hw_set
    /* c8 */
    .global             __cos_cav7_tlbiallis_set
    .global             __cos_cav7_tlbimvais_set
    .global             __cos_cav7_tlbiasidis_set
    .global             __cos_cav7_tlbimvaais_set
    .global             __cos_cav7_itlbiall_set
    .global             __cos_cav7_itlbimva_set
    .global             __cos_cav7_itlbiasid_set
    .global             __cos_cav7_dtlbiall_set
    .global             __cos_cav7_dtlbimva_set
    .global             __cos_cav7_dtlbiasid_set
    .global             __cos_cav7_tlbiall_set
    .global             __cos_cav7_tlbimva_set
    .global             __cos_cav7_tlbiasid_set
    .global             __cos_cav7_tlbimvaa_set
    .global             __cos_cav7_tlbiallhis_set
    .global             __cos_cav7_tlbimvahis_set
    .global             __cos_cav7_tlbiallnsnhis_set
    .global             __cos_cav7_tlbiallh_set
    .global             __cos_cav7_tlbimvah_set
    .global             __cos_cav7_tlbiallnsnh_set
    /* c9 */
    .global             __cos_cav7_pmcr_set
    .global             __cos_cav7_pmcntenset_set
    .global             __cos_cav7_pmcntenclr_set
    .global             __cos_cav7_pmovsr_set
    .global             __cos_cav7_pmswinc_set
    .global             __cos_cav7_pmselr_set
    .global             __cos_cav7_pmccntr_set
    .global             __cos_cav7_pmxevtyper_set
    .global             __cos_cav7_pmxevcntr_set
    .global             __cos_cav7_pmuserenr_set
    .global             __cos_cav7_pmintenset_set
    .global             __cos_cav7_pmintenclr_set
    /* c10 */
    .global             __cos_cav7_tlblr_set
    .global             __cos_cav7_prrr_set
    .global             __cos_cav7_nmrr_set
    .global             __cos_cav7_amair0_set
    .global             __cos_cav7_amair1_set
    .global             __cos_cav7_hmair0_set
    .global             __cos_cav7_hmair1_set
    .global             __cos_cav7_hamair0_set
    .global             __cos_cav7_hamair1_set
    /* c12 */
    .global             __cos_cav7_vbar_set
    .global             __cos_cav7_mvbar_set
    .global             __cos_cav7_hvbar_set
    /* c13 */
    .global             __cos_cav7_contextidr_set
    .global             __cos_cav7_tpidrurw_set
    .global             __cos_cav7_tpidruro_set
    .global             __cos_cav7_tpidrprw_set
    .global             __cos_cav7_htpidr_set
    /* c14 */
    .global             __cos_cav7_cntfrq_set
    .global             __cos_cav7_cntkctl_set
    .global             __cos_cav7_cntp_tval_set
    .global             __cos_cav7_cntp_ctl_set
    .global             __cos_cav7_cntv_tval_set
    .global             __cos_cav7_cntv_ctl_set
    .global             __cos_cav7_cnthctl_set
    .global             __cos_cav7_cnthp_tval_set
    .global             __cos_cav7_cnthp_ctl_set
    /* double words */
    .global             __cos_cav7_cntp_cval_dw_set
    .global             __cos_cav7_cntv_cval_dw_set
    .global             __cos_cav7_cntvoff_dw_set
    .global             __cos_cav7_cnthp_cval_dw_set

    /* booting specific stuff */
    .global             __cos_cav7_stack_start
    /* initial page table */
    .global             __cos_cav7_kern_pgtbl
    /* vector table */
    .global             __cos_cav7_vector_table
    /* fault handlers and user handlers are exported on their spot */

    /* the kernel entry of cos. this will be defined in c language. */
    .global             cos_kmain
    /* the fault handlerd of cos. these will be defined in c language. */
    .global             __cos_cav7_irq_handler
    /* the generic interrupt handler of cos. this will be defined in c language. */
    .global             __cos_cav7_generic_handler
    /* the system call handler of cos. this will be defined in c language. */
    .global             cos_syscall_handler
    /* the system tick handler of cos. this will be defined in c language. */
    .global             _cos_tick_handler
    /* the entry of smp after they have finished their initialization */
    .global             __cos_smp_low_level_init
    /* all other processor's timer interrupt handler */
    .global             __cos_cav7_smp_tick
    /* memory layout information - this is the actual page table mapping */
    .global             cos_cav7_mem_info

/* das u-boot header is appended by the u-boot through fit image generation.
 * we do not temper with this here, as it is unnecessary. for the .data section,
 * we are perfectly fine because it is in ram do not even need relocation, and
 * it is of course included in the resulting binary from elf. (actually cos
 * kernel code does not make use of the .data section anyway). as a result,
 * we just need to clean up the. bss section. the linux kernel, when booting,
 * always hacks the cpu to svc mode, perhaps to allow usage of many different
 * bootloaders; however, we know that u-boot have already set the cpu to svc
 * mode, which can spare us the ugly self-modifying code. */
    .global             __bss_start__
    .global             __bss_end__
    .global             __va_offset__
    .global             main
_start:
    .global             _start
    ldr                 r0,=__bss_start__
    ldr                 r1,=__bss_end__
    ldr                 r2,=__va_offset__
    sub                 r0,r0,r2
    sub                 r1,r1,r2
    ldr                 r2,=0x00
clear_bss:
    cmp                 r0,r1
    beq                 clear_done
    str                 r2,[r0]
    add                 r0,#0x04
    b                   clear_bss
clear_done:

    /* Clean up the memory for the components */
    /*
    ldr                 r0,=0x10000000
    ldr                 r1,=0x30000000
    ldr                 r2,=0x00
clear_comp:
    cmp                 r0,r1
    beq                 clear_comp_done
    str                 r2,[r0]
    add                 r0,#0x04
    b                   clear_comp
clear_comp_done: */

    /* set stacks for all modes except for usr */
    ldr                 r4,=__cos_cav7_stack_start
    add                 r4,r4,#65536
    sub                 r4,r4,#2048
    /* irq mode */
    ldr                 r0,=0x600f00d2
    msr                 cpsr,r0
    mov                 sp,r4
    /* abt mode */
    ldr                 r0,=0x600f00d7
    msr                 cpsr,r0
    mov                 sp,r4
    /* fiq mode */
    ldr                 r0,=0x600f00d1
    msr                 cpsr,r0
    mov                 sp,r4
    /* und mode */
    ldr                 r0,=0x600f00db
    msr                 cpsr,r0
    mov                 sp,r4
    /* sys mode */
    ldr                 r0,=0x600f00db
    msr                 cpsr,r0
    mov                 sp,r4
    /* svc mode */
    ldr                 r0,=0x600f00d3
    msr                 cpsr,r0
    mov                 sp,r4

    /* turn off the mmu and all cache if it is already enabled. there's no need
     * to turn cache off because we are not modifying the instruction stream at
     * all; the tlb walker will start walking from l1d if it is enabled.
     * original page table setup .... c0e. */
    cp15_get_init       crn=c1 op1=0 crm=c0 op2=0
    ldr                 r1,=~((1<<2)|(1<<0))
    and                 r0,r0,r1
    cp15_set_init       crn=c1 op1=0 crm=c0 op2=0 /* sctlr.afe,tre,i,c,m */
    isb
    /* flush tlb */
    ldr                 r0,=0x00
    cp15_set_init       crn=c8 op1=0 crm=c7 op2=0 /* tlbiall */
    isb

    /* set up the initial page table
     * r0: read base address
     * r1: write base address
     * r2: end read address
     * r3: pa address to map from
     * r4: va address to map into
     * r5: number of pages
     * r6: property mask
     * r7: page counter
     * r8: write index register
     * r9: write content register */
    ldr                 r0,=cos_cav7_mem_info
    ldr                 r1,=__cos_cav7_kern_pgtbl
    ldr                 r2,=__va_offset__
    /* calculate the actual address */
    sub                 r0,r0,r2
    sub                 r1,r1,r2
    /* calculate the configuration end address */
    ldr                 r3,[r0]
    add                 r2,r0,r3
    add                 r0,r0,#0x04

    /* load configurations and generate page table layout one by one */
load_config:
    ldmia               r0!,{r3-r6}
    mov                 r7,#0x00
    lsr                 r8,r4,#18
    add                 r8,r1,r8
    orr                 r9,r3,r6

fill_pgtbl:
    str                 r9,[r8]
    add                 r8,r8,#4
    add                 r7,r7,#1
    add                 r9,r9,#0x100000
    cmp                 r7,r5
    bne                 fill_pgtbl

    cmp                 r0,r2
    bne                 load_config

    dmb			sy

    /* set the registers */
    ldr                 r0,=0x02
    cp15_set_init       crn=c2 op1=0 crm=c0 op2=2 /* ttbcr, ttbr1 only use 1gb */
    isb

    ldr                 r0,=0xFFFFFFFF//0x55555555
    cp15_set_init       crn=c3 op1=0 crm=c0 op2=0 /* dacr */
    isb

    ldr                 r0,=0x000a00a4
    cp15_set_init       crn=c10 op1=0 crm=c2 op2=0 /* prrr */
    isb

    ldr                 r0,=0x006c006c
    cp15_set_init       crn=c10 op1=0 crm=c2 op2=1 /* nmrr */
    isb

    /* contextidr undefined on reset, set here! */
    ldr                 r0, =0x00
    cp15_set_init       crn=c13 op1=0 crm=c0 op2=1
    isb

    /* set base address */
    ldr                 r0,=__cos_cav7_kern_pgtbl
    ldr                 r1,=__va_offset__
    sub                 r0,r0,r1
    orr                 r0,r0,#0x4a /* 0x09 */

    cp15_set_init       crn=c2 op1=0 crm=c0 op2=0 /* ttbr0 */
    cp15_set_init       crn=c2 op1=0 crm=c0 op2=1 /* ttbr1 */
    /* load the main function address to r3 first to prepare for a long jump */
    ldr                 r3,=kmain
    isb
    
    /* flush BTAC and L1I */
    ldr		 r0, =0x00
    cp15_set_init       crn=c7 op1=0 crm=c5 op2=6
    cp15_set_init       crn=c7 op1=0 crm=c5 op2=0

    /* turn on paging and cache */
    cp15_get_init       crn=c1 op1=0 crm=c0 op2=0
    isb
    ldr                 r1,=~((1<<29)|(1<<28)|(1<<12)|(1<<11)|(1<<2)|(1<<0))
    and                 r0,r0,r1
    ldr                 r1,=(1<<29)|(1<<28)|(1<<12)|(1<<11)|(1<<2)|(1<<0) //(1<<12)|(1<<2)|(1<<0)
    orr                 r0,r0,r1
    cp15_set_init       crn=c1 op1=0 crm=c0 op2=0 /* sctlr.afe,tre,i,c,m */
    isb

    /* flush tlb again */
    ldr                 r0,=0x00
    cp15_set_init       crn=c8 op1=0 crm=c7 op2=0 /* tlbiall */
    isb

    /* branch to main function */
    bx                  r3
    .ltorg

    /*
    ldr                 r0,=0x00
    ldr                 r1,=0x11
    ldr                 r2,=0x22
    ldr 		r12,=0x00
    stmia               r12!,{r0-r11}

    cp15_set_init            crn=c7 op1=0 crm=c14 op2=1
    isb

    ldr                 r0,=0x41210000
    ldr                 r1,=0x55555555
    str                 r1,[r0]
    b                   .*/

/* das u-boot will set up us an initial page table with all identical mappings.
 * there's no need to do anything special here. also, we will not initialize
 * the serial port for similar reasons. but, we do need to set up initial stacks
 * for the cpus. because we know that there can be no more than 4 cpus in an armv7
 * chip, we can statically allocate them here. each core is associated with 64kb
 * stack, which should be more than sufficient. */
    .align              8
__cos_cav7_stack_start:
    .space              4*65536
__cos_cav7_stack_end:
    .space              4096
/* the kernel page table - the initialization sequence is totally controlled
 * by the configuration file, because there's no generic way to detect memory on
 * these devices. */
    .align              16
__cos_cav7_kern_pgtbl:
    .space              65536

/* vectors *******************************************************************/
    .align              8
__cos_cav7_vector_table:
    b                   reset_handler
    b                   undefined_handler
    b                   svc_handler
    b                   prefetch_abort_handler
    b                   data_abort_handler
    b                   unused_handler
    b                   irq_handler
    b                   fiq_handler

/* cpsr & spsr */
__cos_cav7_cpsr_get:
    mrs                 r0,cpsr
    bx                  lr
__cos_cav7_spsr_get:
    mrs                 r0,spsr
    bx                  lr

/* main id register */
__cos_cav7_midr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=0
/* cache type register */
__cos_cav7_ctr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=1
/* tcm type register */
__cos_cav7_tcmtr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=2
/* tlb type register */
__cos_cav7_tlbtr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=3
/* multiprocessor affinity register */
__cos_cav7_mpidr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=5
/* revision id register */
__cos_cav7_revidr_get:
    cp15_get            crn=c0 op1=0 crm=c0 op2=6
/* processor feature register 0 */
__cos_cav7_id_pfr0_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=0
/* processor feature register 1 */
__cos_cav7_id_pfr1_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=1
/* debug feature register 0 */
__cos_cav7_id_dfr0_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=2
/* auxiliary feature register 0 */
__cos_cav7_id_afr0_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=3
/* memory model feature register 0 */
__cos_cav7_id_mmfr0_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=4
/* memory model feature register 1 */
__cos_cav7_id_mmfr1_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=5
/* memory model feature register 2 */
__cos_cav7_id_mmfr2_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=6
/* memory model feature register 3 */
__cos_cav7_id_mmfr3_get:
    cp15_get            crn=c0 op1=0 crm=c1 op2=7
/* isa feature register 0 */
__cos_cav7_id_isar0_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=0
/* isa feature register 1 */
__cos_cav7_id_isar1_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=1
/* isa feature register 2 */
__cos_cav7_id_isar2_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=2
/* isa feature register 3 */
__cos_cav7_id_isar3_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=3
/* isa feature register 4 */
__cos_cav7_id_isar4_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=4
/* isa feature register 5 */
__cos_cav7_id_isar5_get:
    cp15_get            crn=c0 op1=0 crm=c2 op2=5
/* cache size id registers */
__cos_cav7_id_ccsidr_get:
    cp15_get            crn=c0 op1=1 crm=c0 op2=0
/* cache level id register */
__cos_cav7_id_clidr_get:
    cp15_get            crn=c0 op1=1 crm=c0 op2=1
/* auxiliary id register */
__cos_cav7_id_aidr_get:
    cp15_get            crn=c0 op1=1 crm=c0 op2=7
/* cache size selection register */
__cos_cav7_id_csselr_get:
    cp15_get            crn=c0 op1=2 crm=c0 op2=0
/* virtualization processor id register  */
__cos_cav7_id_vpidr_get:
    cp15_get            crn=c0 op1=4 crm=c0 op2=0
/* virtualization multiprocessor id register */
__cos_cav7_id_vmpidr_get:
    cp15_get            crn=c0 op1=4 crm=c0 op2=5

/* system control register */
__cos_cav7_sctlr_get:
    cp15_get            crn=c1 op1=0 crm=c0 op2=0
/* auxiliary control register */
__cos_cav7_actlr_get:
    cp15_get            crn=c1 op1=0 crm=c0 op2=1
/* coprocessor auxiliary control register */
__cos_cav7_cpacr_get:
    cp15_get            crn=c1 op1=0 crm=c0 op2=2
/* secure configuration register */
__cos_cav7_scr_get:
    cp15_get            crn=c1 op1=0 crm=c1 op2=0
/* secure debug enable register */
__cos_cav7_sder_get:
    cp15_get            crn=c1 op1=0 crm=c1 op2=1
/* non-secure access control register */
__cos_cav7_nsacr_get:
    cp15_get            crn=c1 op1=0 crm=c1 op2=2
/* hyp system control register */
__cos_cav7_hsctlr_get:
    cp15_get            crn=c1 op1=4 crm=c0 op2=0
/* hyp auxiliary control register */
__cos_cav7_hactlr_get:
    cp15_get            crn=c1 op1=4 crm=c0 op2=1
/* hyp configuration register */
__cos_cav7_hcr_get:
    cp15_get            crn=c1 op1=4 crm=c1 op2=0
/* hyp debug configuration register */
__cos_cav7_hdcr_get:
    cp15_get            crn=c1 op1=4 crm=c1 op2=1
/* hyp coprocessor trap register */
__cos_cav7_hcptr_get:
    cp15_get            crn=c1 op1=4 crm=c1 op2=2
/* hyp system trap register */
__cos_cav7_hstr_get:
    cp15_get            crn=c1 op1=4 crm=c1 op2=3
/* hyp auxiliary configuration register */
__cos_cav7_hacr_get:
    cp15_get            crn=c1 op1=4 crm=c1 op2=7

/* translation table base register 0 - 32bit. we do not support pae of any kind */
__cos_cav7_ttbr0_get:
    cp15_get            crn=c2 op1=0 crm=c0 op2=0
/* translation table base register 1 - 32bit. we do not support pae of any kind */
__cos_cav7_ttbr1_get:
    cp15_get            crn=c2 op1=0 crm=c0 op2=1
/* translation table base control register */
__cos_cav7_ttbcr_get:
    cp15_get            crn=c2 op1=0 crm=c0 op2=2
/* hyp translation control register */
__cos_cav7_htcr_get:
    cp15_get            crn=c2 op1=4 crm=c0 op2=2
/* virtualization translation control register */
__cos_cav7_vtcr_get:
    cp15_get            crn=c2 op1=4 crm=c1 op2=2
/* domain access control register */
__cos_cav7_dacr_get:
    cp15_get            crn=c3 op1=0 crm=c0 op2=0

/* data fault status register */
__cos_cav7_dfsr_get:
    cp15_get            crn=c5 op1=0 crm=c0 op2=0
/* instruction fault status register */
__cos_cav7_ifsr_get:
    cp15_get            crn=c5 op1=0 crm=c0 op2=1
/* auxiliary data fault status register */
__cos_cav7_adfsr_get:
    cp15_get            crn=c5 op1=0 crm=c1 op2=0
/* auxiliary instruction fault status register */
__cos_cav7_aifsr_get:
    cp15_get            crn=c5 op1=0 crm=c1 op2=1
/* hyp auxiliary data fault status register */
__cos_cav7_hadfsr_get:
    cp15_get            crn=c5 op1=4 crm=c1 op2=0
/* hyp auxiliary instruction fault status register */
__cos_cav7_haifsr_get:
    cp15_get            crn=c5 op1=4 crm=c1 op2=1
/* hyp syndrome register */
__cos_cav7_hsr_get:
    cp15_get            crn=c5 op1=4 crm=c2 op2=0
/* data fault address register */
__cos_cav7_dfar_get:
    cp15_get            crn=c6 op1=0 crm=c0 op2=0
/* instruction fault address register */
__cos_cav7_ifar_get:
    cp15_get            crn=c6 op1=0 crm=c0 op2=2
/* hyp data fault address register */
__cos_cav7_hdfar_get:
    cp15_get            crn=c6 op1=4 crm=c0 op2=0
/* hyp instruction fault address register */
__cos_cav7_hifar_get:
    cp15_get            crn=c6 op1=4 crm=c0 op2=2
/* hyp ipa fault address register */
__cos_cav7_hpfar_get:
    cp15_get            crn=c6 op1=4 crm=c0 op2=4

/* physical address register */
__cos_cav7_par_get:
    cp15_get            crn=c7 op1=0 crm=c4 op2=0

/* performance monitors control register */
__cos_cav7_pmcr_get:
    cp15_get            crn=c9 op1=0 crm=c12 op2=0
/* performance monitors count enable set register */
__cos_cav7_pmcntenset_get:
    cp15_get            crn=c9 op1=0 crm=c12 op2=1
/* performance monitors count enable clear register */
__cos_cav7_pmcntenclr_get:
    cp15_get            crn=c9 op1=0 crm=c12 op2=2
/* performance monitors overflow flag status register */
__cos_cav7_pmovsr_get:
    cp15_get            crn=c9 op1=0 crm=c12 op2=3
/* performance monitors event counter selection register */
__cos_cav7_pmselr_get:
    cp15_get            crn=c9 op1=0 crm=c12 op2=5
/* performance monitors cycle count register */
__cos_cav7_pmccntr_get:
    cp15_get            crn=c9 op1=0 crm=c13 op2=0
/* performance monitors event type select register */
__cos_cav7_pmxevtyper_get:
    cp15_get            crn=c9 op1=0 crm=c13 op2=1
/* performance monitors event count register */
__cos_cav7_pmxevcntr_get:
    cp15_get            crn=c9 op1=0 crm=c13 op2=2
/* performance monitors user enable register */
__cos_cav7_pmuserenr_get:
    cp15_get            crn=c9 op1=0 crm=c14 op2=0
/* performance monitors interrupt enable set register */
__cos_cav7_pmintenset_get:
    cp15_get            crn=c9 op1=0 crm=c14 op2=1
/* performance monitors interrupt enable clear register */
__cos_cav7_pmintenclr_get:
    cp15_get            crn=c9 op1=0 crm=c14 op2=2

/* tlb lockdown register - cortex-a9 */
__cos_cav7_tlblr_get:
    cp15_get            crn=c10 op1=0 crm=c0 op2=0
/* primary region remap register */
__cos_cav7_prrr_get:
    cp15_get            crn=c10 op1=0 crm=c2 op2=0
/* normal memory remap register */
__cos_cav7_nmrr_get:
    cp15_get            crn=c10 op1=0 crm=c2 op2=1
/* auxiliary memory attribute indirection register 0 */
__cos_cav7_amair0_get:
    cp15_get            crn=c10 op1=0 crm=c3 op2=0
/* auxiliary memory attribute indirection register 1 */
__cos_cav7_amair1_get:
    cp15_get            crn=c10 op1=0 crm=c3 op2=1
/* hyp memory attribute indirection register 0 */
__cos_cav7_hmair0_get:
    cp15_get            crn=c10 op1=4 crm=c2 op2=0
/* hyp memory attribute indirection register 1 */
__cos_cav7_hmair1_get:
    cp15_get            crn=c10 op1=4 crm=c2 op2=1
/* hyp auxiliary memory attribute indirection register 0 */
__cos_cav7_hamair0_get:
    cp15_get            crn=c10 op1=4 crm=c3 op2=0
/* hyp auxiliary memory attribute indirection register 1 */
__cos_cav7_hamair1_get:
    cp15_get            crn=c10 op1=4 crm=c3 op2=1

/* vector base address register */
__cos_cav7_vbar_get:
    cp15_get            crn=c12 op1=0 crm=c0 op2=0
/* vector base address register */
__cos_cav7_mvbar_get:
    cp15_get            crn=c12 op1=0 crm=c0 op2=1
/* interrupt status register */
__cos_cav7_isr_get:
    cp15_get            crn=c12 op1=0 crm=c1 op2=0
/* hyp vector base address register */
__cos_cav7_hvbar_get:
    cp15_get            crn=c12 op1=4 crm=c0 op2=0

/* fcse pid register */
__cos_cav7_fcseidr_get:
    cp15_get            crn=c13 op1=0 crm=c0 op2=0
/* context id register */
__cos_cav7_contextidr_get:
    cp15_get            crn=c13 op1=0 crm=c0 op2=1
/* user read/write software thread register */
__cos_cav7_tpidrurw_get:
    cp15_get            crn=c13 op1=0 crm=c0 op2=2
/* user read-only software thread register */
__cos_cav7_tpidruro_get:
    cp15_get            crn=c13 op1=0 crm=c0 op2=3
/* pl1-only software thread register */
__cos_cav7_tpidrprw_get:
    cp15_get            crn=c13 op1=0 crm=c0 op2=4
/* hyp read/write software thread register */
__cos_cav7_htpidr_get:
    cp15_get            crn=c13 op1=4 crm=c0 op2=2

/* counter frequency register */
__cos_cav7_cntfrq_get:
    cp15_get            crn=c14 op1=0 crm=c0 op2=0
/* timer pl1 control register */
__cos_cav7_cntkctl_get:
    cp15_get            crn=c14 op1=0 crm=c1 op2=0
/* pl1 physical timer value register */
__cos_cav7_cntp_tval_get:
    cp15_get            crn=c14 op1=0 crm=c2 op2=0
/* pl1 physical timer control register */
__cos_cav7_cntp_ctl_get:
    cp15_get            crn=c14 op1=0 crm=c2 op2=1
/* virtual timer value register */
__cos_cav7_cntv_tval_get:
    cp15_get            crn=c14 op1=0 crm=c3 op2=0
/* virtual timer control register */
__cos_cav7_cntv_ctl_get:
    cp15_get            crn=c14 op1=0 crm=c3 op2=1
/* timer pl2 control register */
__cos_cav7_cnthctl_get:
    cp15_get            crn=c14 op1=4 crm=c1 op2=0
/* pl2 physical timer value register */
__cos_cav7_cnthp_tval_get:
    cp15_get            crn=c14 op1=4 crm=c2 op2=0
/* pl2 physical timer control register */
__cos_cav7_cnthp_ctl_get:
    cp15_get            crn=c14 op1=4 crm=c2 op2=1

/* Cortex-A9 main TLB VA register */
__cos_cav7_main_tlb_va_get:
    cp15_get            crn=c15 op1=5 crm=c5 op2=2
/* Cortex-A9 main TLB PA register */
__cos_cav7_main_tlb_pa_get:
    cp15_get            crn=c15 op1=5 crm=c6 op2=2
/* Cortex-A9 main TLB attribute register */
__cos_cav7_main_tlb_attrib_get:
    cp15_get            crn=c15 op1=5 crm=c7 op2=2

/* physical count register */
__cos_cav7_cntpct_dw_get:
    cp15_get_double     crm=c14 op=0
/* virtual count register */
__cos_cav7_cntvct_dw_get:
    cp15_get_double     crm=c14 op=1
/* pl1 physical timer compare value register */
__cos_cav7_cntp_cval_dw_get:
    cp15_get_double     crm=c14 op=2
/* virtual timer compare value register */
__cos_cav7_cntv_cval_dw_get:
    cp15_get_double     crm=c14 op=3
/* virtual offset register */
__cos_cav7_cntvoff_dw_get:
    cp15_get_double     crm=c14 op=4
/* l2 physical timer compare value register */
__cos_cav7_cnthp_cval_dw_get:
    cp15_get_double     crm=c14 op=6

/* cpsr & spsr */
__cos_cav7_cpsr_set:
    msr                 cpsr,r0
    bx                  lr
__cos_cav7_spsr_set:
    msr                 spsr,r0
    bx                  lr

/* cache size selection register */
__cos_cav7_id_csselr_set:
    cp15_set            crn=c0 op1=2 crm=c0 op2=0
/* virtualization processor id register  */
__cos_cav7_id_vpidr_set:
    cp15_set            crn=c0 op1=4 crm=c0 op2=0
/* virtualization multiprocessor id register */
__cos_cav7_id_vmpidr_set:
    cp15_set            crn=c0 op1=4 crm=c0 op2=5

/* system control register */
__cos_cav7_sctlr_set:
    cp15_set            crn=c1 op1=0 crm=c0 op2=0
/* auxiliary control register */
__cos_cav7_actlr_set:
    cp15_set            crn=c1 op1=0 crm=c0 op2=1
/* coprocessor auxiliary control register */
__cos_cav7_cpacr_set:
    cp15_set            crn=c1 op1=0 crm=c0 op2=2
/* secure configuration register */
__cos_cav7_scr_set:
    cp15_set            crn=c1 op1=0 crm=c1 op2=0
/* secure debug enable register */
__cos_cav7_sder_set:
    cp15_set            crn=c1 op1=0 crm=c1 op2=1
/* non-secure access control register */
__cos_cav7_nsacr_set:
    cp15_set            crn=c1 op1=0 crm=c1 op2=2
/* hyp system control register */
__cos_cav7_hsctlr_set:
    cp15_set            crn=c1 op1=4 crm=c0 op2=0
/* hyp auxiliary control register */
__cos_cav7_hactlr_set:
    cp15_set            crn=c1 op1=4 crm=c0 op2=1
/* hyp configuration register */
__cos_cav7_hcr_set:
    cp15_set            crn=c1 op1=4 crm=c1 op2=0
/* hyp debug configuration register */
__cos_cav7_hdcr_set:
    cp15_set            crn=c1 op1=4 crm=c1 op2=1
/* hyp coprocessor trap register */
__cos_cav7_hcptr_set:
    cp15_set            crn=c1 op1=4 crm=c1 op2=2
/* hyp system trap register */
__cos_cav7_hstr_set:
    cp15_set            crn=c1 op1=4 crm=c1 op2=3
/* hyp auxiliary configuration register */
__cos_cav7_hacr_set:
    cp15_set            crn=c1 op1=4 crm=c1 op2=7

/* translation table base register 0 - 32bit. we do not support pae of any kind.
 * this operation also sets the page table of this architecture */
__cos_cav7_ttbr0_set:
__cos_cav7_pgtbl_set:
    cp15_set            crn=c2 op1=0 crm=c0 op2=0
/* translation table base register 1 - 32bit. we do not support pae of any kind */
__cos_cav7_ttbr1_set:
    cp15_set            crn=c2 op1=0 crm=c0 op2=1
/* translation table base control register */
__cos_cav7_ttbcr_set:
    cp15_set            crn=c2 op1=0 crm=c0 op2=2
/* hyp translation control register */
__cos_cav7_htcr_set:
    cp15_set            crn=c2 op1=4 crm=c0 op2=2
/* virtualization translation control register */
__cos_cav7_vtcr_set:
    cp15_set            crn=c2 op1=4 crm=c1 op2=2
/* domain access control register */
__cos_cav7_dacr_set:
    cp15_set            crn=c3 op1=0 crm=c0 op2=0

/* data fault status register */
__cos_cav7_dfsr_set:
    cp15_set            crn=c5 op1=0 crm=c0 op2=0
/* instruction fault status register */
__cos_cav7_ifsr_set:
    cp15_set            crn=c5 op1=0 crm=c0 op2=1
/* auxiliary data fault status register */
__cos_cav7_adfsr_set:
    cp15_set            crn=c5 op1=0 crm=c1 op2=0
/* auxiliary instruction fault status register */
__cos_cav7_aifsr_set:
    cp15_set            crn=c5 op1=0 crm=c1 op2=1
/* hyp auxiliary data fault status register */
__cos_cav7_hadfsr_set:
    cp15_set            crn=c5 op1=4 crm=c1 op2=0
/* hyp auxiliary instruction fault status register */
__cos_cav7_haifsr_set:
    cp15_set            crn=c5 op1=4 crm=c1 op2=1
/* hyp syndrome register */
__cos_cav7_hsr_set:
    cp15_set            crn=c5 op1=4 crm=c2 op2=0
/* data fault address register */
__cos_cav7_dfar_set:
    cp15_set            crn=c6 op1=0 crm=c0 op2=0
/* instruction fault address register */
__cos_cav7_ifar_set:
    cp15_set            crn=c6 op1=0 crm=c0 op2=2
/* hyp data fault address register */
__cos_cav7_hdfar_set:
    cp15_set            crn=c6 op1=4 crm=c0 op2=0
/* hyp instruction fault address register */
__cos_cav7_hifar_set:
    cp15_set            crn=c6 op1=4 crm=c0 op2=2
/* hyp ipa fault address register */
__cos_cav7_hpfar_set:
    cp15_set            crn=c6 op1=4 crm=c0 op2=4

/* instruction cache invalidate all to pou inner shareable */
__cos_cav7_icialluis_set:
    cp15_set            crn=c7 op1=0 crm=c1 op2=0
/* branch predictor invalidate all inner shareable */
__cos_cav7_bpiallis_set:
    cp15_set            crn=c7 op1=0 crm=c1 op2=6
/* physical address register */
__cos_cav7_par_set:
    cp15_set            crn=c7 op1=0 crm=c4 op2=6
/* instruction cache invalidate all to pou */
__cos_cav7_iciallu_set:
    cp15_set            crn=c7 op1=0 crm=c5 op2=0
/* invalidate instruction cache by mva to pou */
__cos_cav7_icimvau_set:
    cp15_set            crn=c7 op1=0 crm=c5 op2=1
/* isb register - deprecated */
__cos_cav7_cp15isb_set:
    cp15_set            crn=c7 op1=0 crm=c5 op2=4
/* invalidate entire branch predictor array */
__cos_cav7_bpiall_set:
    cp15_set            crn=c7 op1=0 crm=c5 op2=6
/* invalidate mva from branch predictors */
__cos_cav7_bpimva_set:
    cp15_set            crn=c7 op1=0 crm=c5 op2=7
/* invalidate data cache by mva to poc */
__cos_cav7_dcimvac_set:
    cp15_set            crn=c7 op1=0 crm=c6 op2=1
/* invalidate data cache line by set/way */
__cos_cav7_dcisw_set:
    cp15_set            crn=c7 op1=0 crm=c6 op2=2
/* priviledged read va to pa translation */
__cos_cav7_ats1cpr_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=0
/* priviledged write va to pa translation */
__cos_cav7_ats1cpw_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=1
/* user read va to pa translation */
__cos_cav7_ats1cur_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=2
/* user write va to pa translation */
__cos_cav7_ats1cuw_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=3
/* priviledged read va to pa translation, other security state */
__cos_cav7_ats12nsopr_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=4
/* priviledged write va to pa translation, other security state */
__cos_cav7_ats12nsopw_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=5
/* user read va to pa translation, other security state */
__cos_cav7_ats12nsour_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=6
/* user write va to pa translation, other security state */
__cos_cav7_ats12nsouw_set:
    cp15_set            crn=c7 op1=0 crm=c8 op2=7
/* clean data cache line by mva to poc */
__cos_cav7_dccmvac_set:
    cp15_set            crn=c7 op1=0 crm=c10 op2=1
/* clean data cache line by set/way */
__cos_cav7_dccsw_set:
    cp15_set            crn=c7 op1=0 crm=c10 op2=2
/* dsb register - deprecated */
__cos_cav7_cp15dsb_set:
    cp15_set            crn=c7 op1=0 crm=c10 op2=4
/* dmb register - deprecated */
__cos_cav7_cp15dmb_set:
    cp15_set            crn=c7 op1=0 crm=c10 op2=5
/* clean data cache line by mva to pou */
__cos_cav7_dccmvau_set:
    cp15_set            crn=c7 op1=0 crm=c11 op2=1
/* clean and invalidate data cache line by mva to poc */
__cos_cav7_dccimvac_set:
    cp15_set            crn=c7 op1=0 crm=c14 op2=1
/* clean and invalidate data cache line by set/way */
__cos_cav7_dccisw_set:
    cp15_set            crn=c7 op1=0 crm=c14 op2=2
/* hyp mode read translation */
__cos_cav7_ats1hr_set:
    cp15_set            crn=c7 op1=4 crm=c8 op2=0
/* hyp mode write translation */
__cos_cav7_ats1hw_set:
    cp15_set            crn=c7 op1=4 crm=c8 op2=1

/* invalidate entire tlb is */
__cos_cav7_tlbiallis_set:
    cp15_set            crn=c8 op1=0 crm=c3 op2=0
/* invalidate unified tlb entry by mva and asid is */
__cos_cav7_tlbimvais_set:
    cp15_set            crn=c8 op1=0 crm=c3 op2=1
/* invalidate unified tlb by asid match is */
__cos_cav7_tlbiasidis_set:
    cp15_set            crn=c8 op1=0 crm=c3 op2=2
/* invalidate unified tlb entry by mva all asid is */
__cos_cav7_tlbimvaais_set:
    cp15_set            crn=c8 op1=0 crm=c3 op2=3
/* invalidate instruction tlb */
__cos_cav7_itlbiall_set:
    cp15_set            crn=c8 op1=0 crm=c5 op2=0
/* invalidate instruction tlb entry by mva and asid */
__cos_cav7_itlbimva_set:
    cp15_set            crn=c8 op1=0 crm=c5 op2=1
/* invalidate instruction tlb by asid match */
__cos_cav7_itlbiasid_set:
    cp15_set            crn=c8 op1=0 crm=c5 op2=2
/* invalidate data tlb */
__cos_cav7_dtlbiall_set:
    cp15_set            crn=c8 op1=0 crm=c6 op2=0
/* invalidate data tlb entry by mva and asid */
__cos_cav7_dtlbimva_set:
    cp15_set            crn=c8 op1=0 crm=c6 op2=1
/* invalidate data tlb by asid match */
__cos_cav7_dtlbiasid_set:
    cp15_set            crn=c8 op1=0 crm=c6 op2=2
/* invalidate unified tlb */
__cos_cav7_tlbiall_set:
    cp15_set            crn=c8 op1=0 crm=c7 op2=0
/* invalidate unified tlb entry by mva and asid */
__cos_cav7_tlbimva_set:
    cp15_set            crn=c8 op1=0 crm=c7 op2=1
/* invalidate unified tlb by asid match */
__cos_cav7_tlbiasid_set:
    cp15_set            crn=c8 op1=0 crm=c7 op2=2
/* invalidate unified tlb entries by mva all asid */
__cos_cav7_tlbimvaa_set:
    cp15_set            crn=c8 op1=0 crm=c7 op2=3
/* invalidate entire hyp unified tlb is */
__cos_cav7_tlbiallhis_set:
    cp15_set            crn=c8 op1=4 crm=c3 op2=0
/* invalidate hyp unified tlb entry by mva is */
__cos_cav7_tlbimvahis_set:
    cp15_set            crn=c8 op1=4 crm=c3 op2=1
/* invalidate entire non-secure non-hyp unified tlb is */
__cos_cav7_tlbiallnsnhis_set:
    cp15_set            crn=c8 op1=4 crm=c3 op2=4
/* invalidate entire hyp unified tlb */
__cos_cav7_tlbiallh_set:
    cp15_set            crn=c8 op1=4 crm=c7 op2=0
/* invalidate hyp unified tlb entry by mva */
__cos_cav7_tlbimvah_set:
    cp15_set            crn=c8 op1=4 crm=c7 op2=1
/* invalidate entire non-secure non-hyp unified tlb */
__cos_cav7_tlbiallnsnh_set:
    cp15_set            crn=c8 op1=4 crm=c7 op2=4

/* performance monitors control register */
__cos_cav7_pmcr_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=0
/* performance monitors count enable set register */
__cos_cav7_pmcntenset_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=1
/* performance monitors count enable clear register */
__cos_cav7_pmcntenclr_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=2
/* performance monitors overflow flag status register */
__cos_cav7_pmovsr_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=3
/* performance monitors software increment register */
__cos_cav7_pmswinc_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=4
/* performance monitors event counter selection register */
__cos_cav7_pmselr_set:
    cp15_set            crn=c9 op1=0 crm=c12 op2=5
/* performance monitors cycle count register */
__cos_cav7_pmccntr_set:
    cp15_set            crn=c9 op1=0 crm=c13 op2=0
/* performance monitors event type select register */
__cos_cav7_pmxevtyper_set:
    cp15_set            crn=c9 op1=0 crm=c13 op2=1
/* performance monitors event count register */
__cos_cav7_pmxevcntr_set:
    cp15_set            crn=c9 op1=0 crm=c13 op2=2
/* performance monitors user enable register */
__cos_cav7_pmuserenr_set:
    cp15_set            crn=c9 op1=0 crm=c14 op2=0
/* performance monitors interrupt enable set register */
__cos_cav7_pmintenset_set:
    cp15_set            crn=c9 op1=0 crm=c14 op2=1
/* performance monitors interrupt enable clear register */
__cos_cav7_pmintenclr_set:
    cp15_set            crn=c9 op1=0 crm=c14 op2=2

/* tlb lockdown register - cortex-a9 */
__cos_cav7_tlblr_set:
    cp15_set            crn=c10 op1=0 crm=c0 op2=0
/* primary region remap register */
__cos_cav7_prrr_set:
    cp15_set            crn=c10 op1=0 crm=c2 op2=0
/* normal memory remap register */
__cos_cav7_nmrr_set:
    cp15_set            crn=c10 op1=0 crm=c2 op2=1
/* auxiliary memory attribute indirection register 0 */
__cos_cav7_amair0_set:
    cp15_set            crn=c10 op1=0 crm=c3 op2=0
/* auxiliary memory attribute indirection register 1 */
__cos_cav7_amair1_set:
    cp15_set            crn=c10 op1=0 crm=c3 op2=1
/* hyp memory attribute indirection register 0 */
__cos_cav7_hmair0_set:
    cp15_set            crn=c10 op1=4 crm=c2 op2=0
/* hyp memory attribute indirection register 1 */
__cos_cav7_hmair1_set:
    cp15_set            crn=c10 op1=4 crm=c2 op2=1
/* hyp auxiliary memory attribute indirection register 0 */
__cos_cav7_hamair0_set:
    cp15_set            crn=c10 op1=4 crm=c3 op2=0
/* hyp auxiliary memory attribute indirection register 1 */
__cos_cav7_hamair1_set:
    cp15_set            crn=c10 op1=4 crm=c3 op2=1

/* vector base address register */
__cos_cav7_vbar_set:
    cp15_set            crn=c12 op1=0 crm=c0 op2=0
/* vector base address register */
__cos_cav7_mvbar_set:
    cp15_set            crn=c12 op1=0 crm=c0 op2=1
/* hyp vector base address register */
__cos_cav7_hvbar_set:
    cp15_set            crn=c12 op1=4 crm=c0 op2=0

/* context id register */
__cos_cav7_contextidr_set:
    cp15_set            crn=c13 op1=0 crm=c0 op2=1
/* user read/write software thread register */
__cos_cav7_tpidrurw_set:
    cp15_set            crn=c13 op1=0 crm=c0 op2=2
/* user read-only software thread register */
__cos_cav7_tpidruro_set:
    cp15_set            crn=c13 op1=0 crm=c0 op2=3
/* pl1-only software thread register */
__cos_cav7_tpidrprw_set:
    cp15_set            crn=c13 op1=0 crm=c0 op2=4
/* hyp read/write software thread register */
__cos_cav7_htpidr_set:
    cp15_set            crn=c13 op1=4 crm=c0 op2=2

/* counter frequency register */
__cos_cav7_cntfrq_set:
    cp15_set            crn=c14 op1=0 crm=c0 op2=0
/* timer pl1 control register */
__cos_cav7_cntkctl_set:
    cp15_set            crn=c14 op1=0 crm=c1 op2=0
/* pl1 physical timer value register */
__cos_cav7_cntp_tval_set:
    cp15_set            crn=c14 op1=0 crm=c2 op2=0
/* pl1 physical timer control register */
__cos_cav7_cntp_ctl_set:
    cp15_set            crn=c14 op1=0 crm=c2 op2=1
/* virtual timer value register */
__cos_cav7_cntv_tval_set:
    cp15_set            crn=c14 op1=0 crm=c3 op2=0
/* virtual timer control register */
__cos_cav7_cntv_ctl_set:
    cp15_set            crn=c14 op1=0 crm=c3 op2=1
/* timer pl2 control register */
__cos_cav7_cnthctl_set:
    cp15_set            crn=c14 op1=4 crm=c1 op2=0
/* pl2 physical timer value register */
__cos_cav7_cnthp_tval_set:
    cp15_set            crn=c14 op1=4 crm=c2 op2=0
/* pl2 physical timer control register */
__cos_cav7_cnthp_ctl_set:
    cp15_set            crn=c14 op1=4 crm=c2 op2=1


/* Cortex-A9 select lockdown tlb entry for read */
__cos_cav7_sel_lkdn_read_set:
    cp15_set            crn=c15 op1=5 crm=c4 op2=2
/* Cortex-A9 select lockdown tlb entry for write */
__cos_cav7_sel_lkdn_write_set:
    cp15_set            crn=c15 op1=5 crm=c4 op2=4
/* Cortex-A9 main TLB VA register */
__cos_cav7_main_tlb_va_set:
    cp15_set            crn=c15 op1=5 crm=c5 op2=2
/* Cortex-A9 main TLB PA register */
__cos_cav7_main_tlb_pa_set:
    cp15_set            crn=c15 op1=5 crm=c6 op2=2
/* Cortex-A9 main TLB attribute register */
__cos_cav7_main_tlb_attrib_set:
    cp15_set            crn=c15 op1=5 crm=c7 op2=2

/* pl1 physical timer compare value register */
__cos_cav7_cntp_cval_dw_set:
    cp15_set_double     crm=c14 op=2
/* virtual timer compare value register */
__cos_cav7_cntv_cval_dw_set:
    cp15_set_double     crm=c14 op=3
/* virtual offset register */
__cos_cav7_cntvoff_dw_set:
    cp15_set_double     crm=c14 op=4
/* l2 physical timer compare value register */
__cos_cav7_cnthp_cval_dw_set:
    cp15_set_double     crm=c14 op=6

__cos_cav7_comp_swap:
    dmb                 sy
    ldrex               r3,[r0]
    cmp                 r3,r1
    bne                 __cos_cav7_comp_swap_fail
    strex               r3,r2,[r0]
    cmp                 r3,#0x00
    bne                 __cos_cav7_comp_swap
    mov                 r0,#0x01
    dmb                 sy
    bx                  lr
__cos_cav7_comp_swap_fail:
    clrex
    mov                 r0,#0x00
    bx                  lr

__cos_cav7_fetch_add:
    ldrex               r2,[r0]
    add                 r3,r2,r1
    strex               r12,r3,[r0]
    cmp                 r12,#0x00
    bne                 __cos_cav7_fetch_add
    mov                 r0,r2
    bx                  lr

__cos_cav7_fetch_and:
    ldrex               r2,[r0]
    and                 r3,r2,r1
    strex               r12,r3,[r0]
    cmp                 r12,#0x00
    bne                 __cos_cav7_fetch_add
    mov                 r0,r2
    bx                  lr

__cos_cav7_read_acquire:
    ldr                 r0,[r0]
    dmb
    bx                  lr

__cos_cav7_write_release:
    dmb
    str                 r1,[r0]
    bx                  lr

__cos_disable_int:
    cpsid               i
    bx                  lr

__cos_enable_int:
    cpsie               i
    bx                  lr

__cos_cav7_halt:
    /* wait for interrupt */
    wfi
    bx                  lr

__cos_cav7_msb_get:
    clz                 r1,r0
    mov                 r0,#31
    sub                 r0,r1
    bx                  lr

/* Now let's go into user mode...
 * R0 = User-level PC, R1 = TID+CPUID. r2-r4 are args.
 * The user-level code is responsible for setting up the user-level SP.
 * CPSR = 0x600F0010.
 */
__cos_enter_user_mode:
    push                {r0}
    mov                 r0,r1
    /* prepare the spsr for user-level */
    ldr                 r2,=0x600F0010
    msr                 spsr_cxsf,r2
    mov			r12,sp
    mov			r2,#0
    mov			r3,#0
    mov			r4,#0
    ldmia               r12!,{pc}^

reset_handler:
unused_handler:
    b                   .

/* save all general-purpose registers */
.macro save_gp_regs
    /* save user-mode pc */
    push                {lr}
    /* save user-mode sp and lr */
    stmdb               sp,{sp,lr}^
    sub                 sp,sp,#0x08
    /* save user-mode general-purpose registers */
    push                {r0-r12}
    /* save user-mode psr */
    mrs                 r0,spsr
    push                {r0}
    mov                 r0,sp
.endm

/* restore all general-purpose registers */
.macro restore_gp_regs
    /* restore user-mode psr */
    pop                 {r0}
    msr                 spsr_cxsf,r0
    /* restore user-mode general purpose registers */
    pop                 {r0-r12}
    /* restore user-mode sp and lr */
    ldmia               sp,{sp,lr}^
    add                 sp,sp,#0x08
    /* restore user-mode pc */
    ldmia               sp!,{pc}^
.endm

undefined_handler:
    save_gp_regs
    bl                  undefined_dbgprint
    b			.
    restore_gp_regs

prefetch_abort_handler:
    sub			lr,lr,#0x04
    save_gp_regs
    bl                  prefetch_abort_dbgprint
    b                   .
    restore_gp_regs

data_abort_handler:
    sub			lr,lr,#0x08
    save_gp_regs
    bl                  data_abort_dbgprint
    b			.
    restore_gp_regs

svc_handler:
    save_gp_regs
    bl                  composite_syscall_handler
    restore_gp_regs

irq_handler:
    sub			lr,lr,#0x04
    save_gp_regs
    bl                  __cos_cav7_irq_handler
    restore_gp_regs

fiq_handler:
    sub			lr,lr,#0x04
    save_gp_regs
    bl                  fiq_dbgprint
    b                   .
    restore_gp_regs
    .ltorg
