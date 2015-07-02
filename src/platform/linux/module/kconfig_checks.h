#ifndef CONFIG_X86_32
#error "Composite only supports x86, 32bit (set CONFIG_X86_32 and CONFIG_X86)"
#endif

#ifndef CONFIG_X86_32_LAZY_GS
#error "Linux kernel must be configured with lazy gs (set CONFIG_X86_32_LAZY_GS)"
#endif

#ifndef CONFIG_MMU
#error "Composite requires an MMU (set CONFIG_MMU)"
#endif

#ifdef CONFIG_X86_PAE
#error "Composite does not support PAE (unset CONFIG_X86_PAE)"
#endif

#ifndef CONFIG_NOHIGHMEM
#warning "Not sure if Composite will work with Linux high-memory (set CONFIG_NOHIGHMEM)"
#endif

#ifdef CONFIG_NO_HZ
#error "Composite currently requires a fixed HZ (undo CONFIG_NO_HZ)"
#endif

