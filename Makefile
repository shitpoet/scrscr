#compile:;tcc  -O0 -w s2.c -o scrscr `pkg-config --cflags --libs gtk+-2.0`
compile:;gcc  -O0 -w scrscr.c -o scrscr `pkg-config --cflags --libs gtk+-2.0`

run:;rm scrscr;tcc  -O0 -w main.c -o scrscr `pkg-config --cflags --libs gtk+-2.0`;./scrscr &




