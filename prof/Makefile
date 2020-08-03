CC = fccpx -Nclang -ffj-fjprof
CFLAGS = -O3 -ffj-ilfunc -fbuiltin -fomit-frame-pointer -finline-functions -g

test_profile: test_profile.c
	$(CC) $(CFLAGS) -o $@  $^
clean:
	rm -f test_profile

