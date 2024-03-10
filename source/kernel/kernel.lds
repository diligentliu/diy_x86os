/* 参考文档： https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_chapter/ld_3.html */
SECTIONS
{
	PROVIDE(kernel_base = .);
    . = 0x00010000;

	PROVIDE(s_text = .);
	.text : {
		*(.text)
	} 

	.rodata : {
		*(.rodata)
	}
	PROVIDE(e_text = .);

	. = ALIGN(4096);
	PROVIDE(s_data = .);
	.data : {
		*(.data)
	}
	.bss : {
		*(.bss)
	}

	PROVIDE(kernel_end = .);
}
