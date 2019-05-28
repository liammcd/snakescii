snakescii: snakescii.o
	$(CC) -o $@ snakescii.o -lncurses
	
snakescii.o: snakescii.c
	$(CC) -c $< 

clean:
	rm -f snakescii.o snakescii