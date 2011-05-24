#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <zmq.h>


int is_running;


void signal_handler(int sig) {
	switch (sig) {
	case SIGHUP:
		break;
	case SIGTERM:
        is_running = 0;
        break;
	}
}

void daemonize(const char *name) {
    pid_t pid, sid;
    
    pid = fork();
    if (pid < 0) {
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }

    umask(0);
    
    openlog(name, LOG_NOWAIT | LOG_PID, LOG_USER);

    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Could not create process group\n");
        exit(1);
    }
    
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Could not change working directory to /\n");
        exit(1);
    }

    close(stdin);
    close(stdout);
    close(stderr);
    
    signal(SIGCHLD,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    
    is_running = 1;
}


int forward(void *from_socket, void *to_socket) {
    int result;
    int64_t more;
    size_t size;
    zmq_msg_t msg;
    
    result = zmq_msg_init(&msg);
    if (result < 0) {
        syslog(LOG_ERR, "Could not initialize message buffer: %s\n", strerror(errno));
        return 1;
    }

    size = sizeof(more);

    while (is_running) {
        result = zmq_recv(from_socket, &msg, 0);
        if (result < 0) {
            syslog(LOG_ERR, "Error while receiving message: %s\n", strerror(errno));
            return 1;
        }

        result = zmq_getsockopt(from_socket, ZMQ_RCVMORE, &more, &size);
        if (result < 0) {
            syslog(LOG_ERR, "Error in getsockopt: %s\n", strerror(errno));
            return 1;
        }

        result = zmq_send(to_socket, &msg, more ? ZMQ_SNDMORE : 0);
        if (result < 0) {
            syslog(LOG_ERR, "Error while sending message: %s\n", strerror(errno));
            return 1;
        }
    }
    
    return 0;
}


int run(const char *sub_address, const char *pub_address) {
    int result;
    void *context, *sub_socket, *pub_socket;
    
    context = zmq_init(1);

    sub_socket = zmq_socket(context, ZMQ_SUB);
    if (!sub_socket) {
        syslog(LOG_ERR, "Could not create SUB socket: %s\n", strerror(errno));
        return 1;
    }
    
    result = zmq_bind(sub_socket, sub_address);
    if (result < 0) {
        syslog(LOG_ERR, "Could not bind SUB socket to %s: %s\n", sub_address, strerror(errno));
        return 1;
    }
    
    result = zmq_setsockopt(sub_socket, ZMQ_SUBSCRIBE, "", 0);
    if (result < 0) {
        syslog(LOG_ERR, "Could not subscribe SUB socket: %s\n", strerror(errno));
        return 1;
    }
    
    pub_socket = zmq_socket(context, ZMQ_PUB);
    if (!pub_socket) {
        syslog(LOG_ERR, "Could not create PUB socket: %s\n", strerror(errno));
        return 1;
    }
    
    result = zmq_bind(pub_socket, pub_address);
    if (result < 0) {
        syslog(LOG_ERR, "Could not bind PUB socket to %s: %s\n", pub_address, strerror(errno));
        return 1;
    }
    
    syslog(LOG_NOTICE, "Forwarding form %s to %s...", sub_address, pub_address);
    
    forward(sub_socket, pub_socket);
    
    syslog(LOG_NOTICE, "Shutting down...");
    
    zmq_close(sub_socket);
    zmq_close(pub_socket);
    zmq_term(context);
    
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("USAGE: %s <SUB bind address> <PUB bind address>\n", argv[0]);
        return 1;
    }

    daemonize("zmq_forward");
    
    return run(argv[1], argv[2]);
}
