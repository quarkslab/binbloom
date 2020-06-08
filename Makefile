binbloom:	binbloom.c
	gcc -O3 -Wall -Wextra binbloom.c -o binbloom

install:
	sudo cp -a binbloom /usr/local/bin/

uninstall:
	sudo rm -f /usr/local/bin/binbloom

clean:
	rm binbloom

style:
	astyle --formatted --mode=c --suffix=none \
	    --indent=spaces=4 --indent-switches \
	    --keep-one-line-blocks --max-instatement-indent=60 \
	    --style=google --pad-oper --unpad-paren --pad-header \
	    --align-pointer=name binbloom.c
