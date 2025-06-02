#include "helper.h"

short my_recv(int sock, char *mess) {
    short len_to_rec, total_ret = 0, ret;
    recv(sock, &len_to_rec, sizeof(len_to_rec), 0);
    while (len_to_rec > total_ret) {
        ret = recv(sock, mess + total_ret, len_to_rec - total_ret, 0);
        total_ret += ret;
    }
    return total_ret;
}

int my_send(int sock, char *mess, short len) {
    int ret;
    short lng = len;
    ret = send(sock, (char *)&lng, sizeof(lng), 0);
    if (ret < 0) return ret;
    ret = send(sock, mess, len, 0);
    if (ret < 0) return ret;
    return 1;
}

//=== file: server.c ===
#include "helper.h"

int add_tcp_client(Tcp_client **clients_list, char *new_id, int new_socket) {
    if (!clients_list || !new_id) return -1;
    Tcp_client **head = clients_list;
    Tcp_client *new_client;
    int flag = 1;

    while ((*head) != NULL && strcmp((*head)->id, new_id) != 0)
        head = &(*head)->next;

    if ((*head) != NULL) {
        if ((*head)->connected == 1) return 0;
        (*head)->connected = 1;
        (*head)->client_socket = new_socket;
        setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
        return 2;
    }

    new_client = malloc(sizeof(Tcp_client));
    if (!new_client) return -1;
    new_client->connected = 1;
    new_client->client_socket = new_socket;
    new_client->id = calloc(strlen(new_id) + 1, sizeof(char));
    if (!new_client->id) {
        free(new_client);
        return -1;
    }

    strcpy(new_client->id, new_id);
    new_client->topic_element = NULL;
    new_client->next = NULL;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    *head = new_client;
    return 1;
}

Tcp_client *get_client(int fd, Tcp_client *clients_list) {
    Tcp_client *head = clients_list;
    while (head != NULL) {
        if (fd == head->client_socket) return head;
        head = head->next;
    }
    return NULL;
}

void free_client(Tcp_client *client) {
    if (!client) return;
    free(client->id);
    Client_topic *current = client->topic_element, *next;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(client);
}

void free_clients_list(Tcp_client *head) {
    Tcp_client *current = head, *next;
    while (current) {
        next = current->next;
        free_client(current);
        current = next;
    }
}

void print_clients_subscriptions(Tcp_client **all_clients) {
    Tcp_client *copy = *all_clients;
    while (copy != NULL) {
        printf("Client %s are topic-urile: ", copy->id);
        Client_topic *lst = copy->topic_element;
        while (lst != NULL) {
            if (lst->topic && lst->topic->name) {
                printf("%s, ", lst->topic->name);
            }
            lst = lst->next;
        }
        printf("\n");
        copy = copy->next;
    }
}

#include "helper.h"

void add_empty_topic(Topic **topics_list, char *topic_name) {
    Topic **head = topics_list;
    while ((*head) != NULL) head = &(*head)->next;

    Topic *new_topic = malloc(sizeof(Topic));
    if (!new_topic) return;
    new_topic->name = calloc(strlen(topic_name) + 1, sizeof(char));
    if (!new_topic->name) {
        free(new_topic);
        return;
    }

    strcpy(new_topic->name, topic_name);
    new_topic->no_messages_ever = 0;
    new_topic->mess_tail = NULL;
    new_topic->mess_head = NULL;
    new_topic->next = NULL;
    *head = new_topic;
}

Topic *is_topic(char *name, Topic *topics_list) {
    Topic *head = topics_list;
    while (head != NULL) {
        if (head->name && !strcmp(name, head->name)) return head;
        head = head->next;
    }
    return NULL;
}

void free_topic(Topic *topic) {
    if (!topic) return;
    free(topic->name);
    free_topic_messages(topic->mess_head);
    free(topic);
}

void free_topics_list(Topic *head) {
    Topic *current = head, *next;
    while (current) {
        next = current->next;
        free_topic(current);
        current = next;
    }
}

#include "helper.h"

void add_topic_message(Topic **topics_list, char *topic_name, char *mess, int mess_len, Tcp_client *all_clients) {
    Topic **head = topics_list;
    while ((*head) != NULL && strcmp((*head)->name, topic_name)) head = &(*head)->next;

    Topic *topic;
    if (*head == NULL) {
        topic = malloc(sizeof(Topic));
        if (!topic) return;
        topic->name = calloc(strlen(topic_name) + 1, sizeof(char));
        if (!topic->name) { free(topic); return; }
        strcpy(topic->name, topic_name);
        topic->no_messages_ever = 1;
        topic->mess_head = NULL;
        topic->mess_tail = NULL;
        topic->next = NULL;
        *head = topic;
    } else {
        topic = *head;
        topic->no_messages_ever++;
    }

    Message *new_message = malloc(sizeof(Message));
    if (!new_message) return;
    new_message->identifier = topic->no_messages_ever;
    new_message->len = mess_len;
    new_message->message_value = calloc(2000, sizeof(char));
    memcpy(new_message->message_value, mess, mess_len);
    new_message->no_sent_clients = 0;
    new_message->no_possible_clients = 0;
    new_message->next = NULL;

    Tcp_client *copy = all_clients;
    while (copy != NULL) {
        Client_topic *this_topic = user_has_topic(topic_name, &(copy->topic_element));
        if (this_topic && (copy->connected || this_topic->sf == 1)) {
            new_message->no_possible_clients++;
        }
        copy = copy->next;
    }

    if (topic->mess_tail) {
        topic->mess_tail->next = new_message;
        topic->mess_tail = new_message;
    } else {
        topic->mess_head = topic->mess_tail = new_message;
    }
}

void free_message(Message *msg) {
    if (!msg) return;
    free(msg->message_value);
    free(msg);
}

void free_topic_messages(Message *head) {
    Message *current = head, *next;
    while (current) {
        next = current->next;
        free_message(current);
        current = next;
    }
}

void check_remove(Topic *topic, Message *mess) {
    if (mess->no_possible_clients != mess->no_sent_clients) return;
    Message *hd = topic->mess_head;

    if (hd == mess && hd == topic->mess_tail) {
        topic->mess_head = topic->mess_tail = NULL;
        free_message(hd);
    } else if (hd == mess) {
        topic->mess_head = hd->next;
        free_message(hd);
    } else {
        while (hd && hd->next) {
            if (hd->next == mess) {
                Message *to_remove = hd->next;
                hd->next = to_remove->next;
                if (topic->mess_tail == to_remove) topic->mess_tail = hd;
                free_message(to_remove);
                return;
            }
            hd = hd->next;
        }
    }
}

//=== file: subscribe.c ===
#include "helper.h"

Client_topic *user_has_topic(char *name, Client_topic **topics_list) {
    Client_topic *head = *topics_list;
    while (head != NULL) {
        if (head->topic && head->topic->name && !strcmp(name, head->topic->name)) return head;
        head = head->next;
    }
    return NULL;
}

void add_topic_for_user(Client_topic **topic_list, int sf, Topic *topic_to_add) {
    Client_topic **head = topic_list;
    while ((*head) != NULL) head = &(*head)->next;
    Client_topic *new_topic = malloc(sizeof(Client_topic));
    if (!new_topic) return;
    new_topic->last_message_identifier = topic_to_add->no_messages_ever;
    new_topic->sf = sf;
    new_topic->topic = topic_to_add;
    new_topic->next = NULL;
    *head = new_topic;
}

void remove_topic_for_user(Client_topic **topic_list, Topic *topic_to_remove) {
    Client_topic *head = *topic_list, *to_free;
    if (head->topic == topic_to_remove) {
        *topic_list = head->next;
        free(head);
        return;
    }
    while (head->next != NULL) {
        if (head->next->topic == topic_to_remove) {
            to_free = head->next;
            head->next = to_free->next;
            free(to_free);
            return;
        }
        head = head->next;
    }
}
