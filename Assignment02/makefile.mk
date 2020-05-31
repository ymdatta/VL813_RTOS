default:
	cc client.c -lpthread -lpulse -lpulse-simple -o client
	cc server.c -lpthread -lpulse -lpulse-simple -o server
	cc pacat-simple.c -lpthread  -lpulse -lpulse-simple -o pacat
clean:
	rm server
	rm client
	rm pacat
