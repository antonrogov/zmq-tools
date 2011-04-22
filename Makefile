CC=gcc
CFLAGS=
LDFLAGS=-lzmq
all: zmq_echo zmq_forward
zmq_echo: zmq_echo.o
zmq_forward: zmq_forward.o
clean:
	rm -f zmq_echo zmq_echo.o zmq_forward zmq_forward.o
