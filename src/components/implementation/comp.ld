SECTIONS
{
	.text : { *(.text*) }
	.rodata : { *(.rodata*) }
	.ctors : {
		__CTOR_LIST__ = .;				
		LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
		*(.ctors)
		LONG(0)
		__CTOR_END__ = .;
	}
	.dtors : {
		__DTOR_LIST__ = .;				
		LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
		*(.dtors)
		LONG(0)
		__DTOR_END__ = .;
	}	
	.crecov : {
		__CRECOV_LIST__ = .;				
		LONG((__CRECOV_END__ - __CRECOV_LIST__) / 4 - 2)
		*(.crecov)
		LONG(0)
		__CRECOV_END__ = .;
	}	
	.kmem : { *(.kmem*) }
	.cinfo : { *(.cinfo*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) }
	.initonce : { *(.initonce*) }
	.initfile : { *(.initfile*) }
}
