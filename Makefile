pingpong-disco1:
	gcc -o ppos-test -Wall ppos-core-aux.c ppos_disk.c disk.c pingpong-disco1.c libppos_static.a -lrt

pingpong-disco2:
	gcc -o ppos-test -Wall ppos-core-aux.c ppos_disk.c disk.c pingpong-disco2.c libppos_static.a -lrt
