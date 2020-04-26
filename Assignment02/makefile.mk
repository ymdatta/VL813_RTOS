default:
	cc client.c -lpthread -lpulse -lpulse-simple -o client
	cc server.c -lpthread -lpulse -lpulse-simple -o server
