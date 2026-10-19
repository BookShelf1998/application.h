app: app.c
	cc -Wall -Wextra -o app app.c -lm -lX11 -lGL

.PHONY: run-app clean

run-app: app
	./app

clean:
	rm app
