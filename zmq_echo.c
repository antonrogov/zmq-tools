#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zmq.h>

char *zmq_recv_string(void *socket, int64_t *more) {
    int size;
    size_t more_size = sizeof(*more);
    char *string;
    zmq_msg_t message;
    
    zmq_msg_init(&message);
    zmq_recv(socket, &message, 0);
    if (more) {
        zmq_getsockopt(socket, ZMQ_RCVMORE, more, &more_size);
    }
    
    size = zmq_msg_size(&message);
    string = (char *)malloc(size + 1);
    memcpy(string, zmq_msg_data(&message), size);
    string[size] = 0;
    
    zmq_msg_close(&message);
    
    return string;
}

int main(int argc, char *argv []) {
    void *context, *socket;
    char *topic, *string;
    int64_t more;
    
    if (argc < 2) {
        fprintf(stderr, "USAGE: zmq_echo <address> [topic]\n");
        return 1;
    }

    context = zmq_init(1);

    printf("connecting to %s...\n", argv[1]);
    socket = zmq_socket(context, ZMQ_SUB);
    zmq_connect(socket, argv[1]);
    
    if (argc > 2) {
        topic = argv[2];
        printf("subscribing to \"%s\" topic...\n", topic);
    } else {
        topic = "";
        printf("subscribing to all topics...\n");
    }
    zmq_setsockopt(socket, ZMQ_SUBSCRIBE, topic, strlen(topic));
    
    printf(">> ");
    fflush(stdout);
    
    while (1) {
        string = zmq_recv_string(socket, &more);
        printf(more ? "%s" : "%s\n>> ", string);
        fflush(stdout);
        free(string);
    }

    zmq_close(socket);
    zmq_term(context);

    return 0;
}
