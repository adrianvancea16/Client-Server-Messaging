#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define _GNU_SOURCE
#include <poll.h>
#include <sys/poll.h>
#include <netinet/tcp.h>

/* Structuri de date */
typedef struct message {
    int identifier;
    int len;
    char *message_value;
    int no_possible_clients;
    int no_sent_clients;
    struct message *next;
} Message;

typedef struct udp_topic {
    char *name;
    Message *mess_head;
    Message *mess_tail;
    int no_messages_ever;
    struct udp_topic *next;
} Topic;

typedef struct client_topic {
    Topic *topic;
    int sf;
    int last_message_identifier;
    struct client_topic *next;
} Client_topic;

typedef struct client_info {
    char *id;
    int connected;
    int client_socket;
    Client_topic *topic_element;
    struct client_info *next;
} Tcp_client;

/* ================= Funcții de trimitere și recepție ================= */

/* FFunctie pentru primirea mesajului */
short my_recv(int sock, char *mess);

/* Functie pentru a trimite mesaj de lungime len */
int my_send(int sock, char *mess, short len);

/* ================= Funcții de eliberare resurse ================= */

void free_client(Tcp_client *client);
void free_clients_list(Tcp_client *head);
void free_message(Message *msg);
void free_topic_messages(Message *head);
void free_topic(Topic *topic);
void free_topics_list(Topic *head);

/* ================= Gestionare topicuri și clienți ================= */

Client_topic *user_has_topic(char *name, Client_topic **topics_list);
void add_topic_for_user(Client_topic **topic_list, int sf, Topic *topic_to_add);
void remove_topic_for_user(Client_topic **topic_list, Topic *topic_to_remove);
int add_tcp_client(Tcp_client **clients_list, char *new_id, int new_socket);
void add_topic_message(Topic **topics_list, char *topic_name, char *mess, int mess_len, Tcp_client *all_clients);
void add_empty_topic(Topic **topics_list, char *topic_name);
Topic *is_topic(char *name, Topic *topics_list);
Tcp_client *get_client(int fd, Tcp_client *clients_list);

/* ================= Funcții auxiliare ================= */

void print_clients_subscriptions(Tcp_client **all_clients);
void check_remove(Topic *topic, Message *mess);

#endif /* HELPER_H */
