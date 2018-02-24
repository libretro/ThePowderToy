add_library(bzip2 STATIC
        bzlib.c
        bzlib.h
        bzlib_private.h
        compress.c
        decompress.c
        randtable.c
        crctable.c
        blocksort.c
        huffman.c
        )

create_target_directory_groups(bzip2)
