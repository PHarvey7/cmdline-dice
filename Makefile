all:
	gcc -g dice.c -o dice

clean:
	rm -rf dice.dSYM
	rm dice
