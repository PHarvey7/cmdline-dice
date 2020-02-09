all:
	gcc dice.c -o dice

w-debug:
	gcc -g dice.c -o dice

no-bsd:
	gcc -DUSING_FALLBACK_RANDOM dice.c -o dice

no-bsd-debug:
	gcc -DUSING_FALLBACK_RANDOM -g dice.c -o dice

clean:
	rm -rf dice.dSYM
	rm dice
