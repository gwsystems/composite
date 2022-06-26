PHDRS
{
	text PT_LOAD ;
	data PT_LOAD ;
}

SECTIONS
{
	ENTRY(__cosrt_upcall_entry)

	. = SIZEOF_HEADERS;

	/* start the text/read-only sections */
	.text : ALIGN(4096) { *(.text*) } :text
	.rodata : { *(.rodata*) }
	.eh_frame : { *(.eh_frame*) }

	. = ALIGN(4096);
	/* start the data sections */
	.ctors : {

		__CTOR_LIST__ = .;
		QUAD((__CTOR_END__ - __CTOR_LIST__) / 8 - 2)
		*(.ctors)
		QUAD(0)
		__CTOR_END__ = .;
	} :data
	.dtors : {

		__DTOR_LIST__ = .;
		QUAD((__DTOR_END__ - __DTOR_LIST__) / 8 - 2)
		*(.dtors)
		QUAD(0)
		__DTOR_END__ = .;
	}
	.init_array : {

		__init_array_start = .;
		__INIT_ARRAY_LIST__ = .;
		QUAD((__INIT_ARRAY_END__ - __INIT_ARRAY_LIST__) / 8 - 2)
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		QUAD(0)
		__INIT_ARRAY_END__ = .;
		__init_array_end = .;
	}
	.fini_array : {

		__fini_array_start = .;
		__FINI_ARRAY_LIST__ = .;
		QUAD((__FINI_ARRAY_END__ - __FINI_ARRAY_LIST__) / 8 - 2)
		KEEP (*(.fini_array))
		KEEP (*(SORT(.fini_array.*)))
		QUAD(0)
		__FINI_ARRAY_END__ = .;
		__fini_array_end = .;
	}
	.crecov : {

		__CRECOV_LIST__ = .;
		QUAD((__CRECOV_END__ - __CRECOV_LIST__) / 8 - 2)
		*(.crecov)
		QUAD(0)
		__CRECOV_END__ = .;

	}
	.data : {
	      *(.data*);
	      *(.ucap*);
	      *(.initonce*);
	      *(.initfile*);
	}
	.bss : { *(.bss*) }

	__crt_static_heap_ptr = .;
}
