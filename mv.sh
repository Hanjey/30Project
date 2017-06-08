#/bash/sh
tar -cvf walk_qemu_pg.tar.gz walk_qemu_pg
scp walk_qemu_pg.tar.gz root@223.129.0.236:/root/chenmeng/
rm -rf walk_qemu_pg.tar.gz
