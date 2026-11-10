.phony: all clean

all: 
	gcc -ogencharset gencharset.c
	rm -rf charset
	./gencharset
	{ printf '// Auto Generated in support/coco/gen\n//\n'; \
	  printf '// CoCo 1/2 only. The CoCo 3 build uses the MSDOS-derived 4-color font\n'; \
	  printf '// in charset_coco3.h, which hires.c includes directly.\n#ifndef COCO3\n\n'; \
	  xxd -c 8 -i charset; printf '\n#endif /* !COCO3 */\n'; } >../../../src/coco/charset.c

 
	