build:
	cc main.c -o ry -I./lua -L./lua -llua -ldl -lm

run: build
	./ry

clean:
	rm -rf ry
