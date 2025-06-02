#include "helper.h"

typedef struct {
    char *stdin_buffer, *topic_name, *udp_buffer, *tcp_buffer;
    struct pollfd *pfds;
    int sock_udp, sock_tcp_main;
    int nfds, reach;
    Tcp_client *all_clients;
    Topic *all_topics;
} ServerResources;

int init_server(ServerResources *res, int port) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    /* Alocare buffere */
    if (!(res->stdin_buffer = malloc(50)) || !(res->topic_name = malloc(50)) || 
        !(res->udp_buffer = malloc(2000)) || !(res->tcp_buffer = malloc(200)) || 
        !(res->pfds = malloc(50 * sizeof(struct pollfd)))) {
        perror("[SERVER] Failed to allocate resources");
        return -1;
    }
    memset(res->tcp_buffer, 0, 200);
    
    /* Inițializare socketi */
    struct sockaddr_in serv_addr;
    if ((res->sock_udp = socket(PF_INET, SOCK_DGRAM, 0)) < 0 || 
        (res->sock_tcp_main = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[SERVER] Socket creation failed");
        return -1;
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    if (bind(res->sock_udp, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ||
        bind(res->sock_tcp_main, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ||
        listen(res->sock_tcp_main, 10) != 0) {
        perror("[SERVER] Socket setup failed");
        return -1;
    }
    
    /* Configurare poll */
    res->nfds = 0;
    res->reach = 50;
    res->pfds[res->nfds].fd = STDIN_FILENO;
    res->pfds[res->nfds++].events = POLLIN;
    res->pfds[res->nfds].fd = res->sock_udp;
    res->pfds[res->nfds++].events = POLLIN;
    res->pfds[res->nfds].fd = res->sock_tcp_main;
    res->pfds[res->nfds++].events = POLLIN;
    
    int flag = 1;
    setsockopt(res->sock_tcp_main, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    
    return 0;
}

/* Eliberează resursele serverului */
void cleanup_server(ServerResources *res) {
    free(res->stdin_buffer);
    free(res->topic_name);
    free(res->udp_buffer);
    free(res->tcp_buffer);
    free(res->pfds);
    free_clients_list(res->all_clients);
    free_topics_list(res->all_topics);
    close(res->sock_udp);
    close(res->sock_tcp_main);
}

/* Gestionează intrarea de la stdin */
int handle_stdin(ServerResources *res) {
    int ret;
    memset(res->stdin_buffer, 0, 50);
    
    if ((ret = read(STDIN_FILENO, res->stdin_buffer, 50)) <= 0) {
        perror("[SERVER] Stdin read failed");
        return -1;
    }
    
    if (!strcmp(res->stdin_buffer, "exit\n")) {
        for (int i = 3; i < res->nfds; i++) {
            strcpy(res->tcp_buffer, "Exit server.");
            my_send(res->pfds[i].fd, res->tcp_buffer, strlen(res->tcp_buffer));
            close(res->pfds[i].fd);
        }
        return -1;
    }
    return 0;
}

/* Gestionează mesaje UDP */
void handle_udp(ServerResources *res) {
    struct sockaddr_in udp_client_addr;
    socklen_t len_udp = sizeof(udp_client_addr);
    
    int bytes_from = recvfrom(res->sock_udp, res->udp_buffer, 2000, MSG_WAITALL, 
                          (struct sockaddr*)&udp_client_addr, &len_udp);
    if (bytes_from <= 50) {
        return;
    }
    
    res->udp_buffer[bytes_from] = '\0';
    char save = res->udp_buffer[50];
    res->udp_buffer[50] = '\0';
    strcpy(res->topic_name, res->udp_buffer);
    res->udp_buffer[50] = save;
    
    /* Creează mesajul complet cu adresa clientului */
    char *full_message = calloc(2000, sizeof(char));
    if (!full_message) return;
    
    memcpy(full_message, &udp_client_addr, len_udp);
    memcpy(full_message + len_udp, res->udp_buffer, bytes_from);
    
    /* Adaugă mesajul la topic și trimite la clienți abonați */
    add_topic_message(&res->all_topics, res->topic_name, full_message, 
                     bytes_from + len_udp, res->all_clients);
    
    /* Livrare mesaje la clienți */
    for (Tcp_client *copy = res->all_clients; copy; copy = copy->next) {
        Client_topic *topic = user_has_topic(res->topic_name, &copy->topic_element);
        if (topic) {
            for (Message *msg = topic->topic->mess_head; msg; ) {
                if (msg->identifier > topic->last_message_identifier) {
                    if (copy->connected) {
                        my_send(copy->client_socket, msg->message_value, msg->len);
                        msg->no_sent_clients++;
                        topic->last_message_identifier = msg->identifier;
                    } else if (topic->sf == 0) {
                        topic->last_message_identifier = msg->identifier;
                    }
                }
                Message *old = msg;
                msg = msg->next;
                check_remove(topic->topic, old);
            }
        }
    }
    
    free(full_message);
    memset(res->udp_buffer, 0, 2000);
}

/* Gestionează conexiuni TCP noi */
void handle_new_connection(ServerResources *res) {
    struct sockaddr_in tcp_client_addr;
    socklen_t len_tcp = sizeof(tcp_client_addr);
    
    int sock_new_client = accept(res->sock_tcp_main, (struct sockaddr*)&tcp_client_addr, &len_tcp);
    if (sock_new_client < 0) return;
    
    if (my_recv(sock_new_client, res->tcp_buffer) < 0) {
        close(sock_new_client);
        return;
    }
    
    /* Extinde array-ul pfds dacă e necesar */
    if (res->nfds == res->reach) {
        struct pollfd *new_pfds = realloc(res->pfds, 2 * res->reach * sizeof(struct pollfd));
        if (!new_pfds) {
            close(sock_new_client);
            return;
        }
        res->pfds = new_pfds;
        res->reach *= 2;
    }
    
    res->pfds[res->nfds].fd = sock_new_client;
    res->pfds[res->nfds++].events = POLLIN;
    
    /* Salvăm ID-ul clientului înainte de a modifica buffer-ul */
    char client_id[200];
    strcpy(client_id, res->tcp_buffer);
    
    int status = add_tcp_client(&res->all_clients, client_id, sock_new_client);
    if (status > 0) {
        printf("New client %s connected from %s:%d.\n", client_id,
               inet_ntoa(tcp_client_addr.sin_addr), ntohs(tcp_client_addr.sin_port));
        
        /* Dacă clientul s-a reconectat, trimite mesajele stocate */
        if (status == 2) {
            for (Tcp_client *client = res->all_clients; client; client = client->next) {
                if (client->client_socket == sock_new_client) {
                    for (Client_topic *topic = client->topic_element; topic; topic = topic->next) {
                        for (Message *msg = topic->topic->mess_head; msg; ) {
                            if (msg->identifier > topic->last_message_identifier) {
                                if (my_send(client->client_socket, msg->message_value, msg->len) >= 0) {
                                    msg->no_sent_clients++;
                                    topic->last_message_identifier = msg->identifier;
                                }
                            }
                            Message *old = msg;
                            msg = msg->next;
                            check_remove(topic->topic, old);
                        }
                    }
                    break;
                }
            }
        }
    } else if (status == 0) {
        /* Client deja conectat */
        sprintf(res->tcp_buffer, "Client %s already connected.", client_id);
        my_send(sock_new_client, res->tcp_buffer, strlen(res->tcp_buffer));
        puts(res->tcp_buffer);
        close(sock_new_client);
        
        /* Corectăm și pfds deoarece am crescut nfds dar apoi am închis socket-ul */
        res->nfds--;
    } else {
        close(sock_new_client);
        /* Corectăm și pfds deoarece am crescut nfds dar apoi am închis socket-ul */
        res->nfds--;
    }
    
    memset(res->tcp_buffer, 0, 200);
}

/* Procesează comanda subscribe */
void process_subscribe(ServerResources *res, int client_fd, char *cmd) {
    char *topic_name, *sf_str;
    
    if (!(topic_name = strtok(cmd + 10, " \n")) || !(sf_str = strtok(NULL, " \n")) || 
        strlen(sf_str) != 1 || (sf_str[0] != '0' && sf_str[0] != '1')) {
        sprintf(res->tcp_buffer, "Invalid subscribe format. Use: subscribe <topic> <SF>");
        my_send(client_fd, res->tcp_buffer, strlen(res->tcp_buffer));
        return;
    }
    
    Tcp_client *client = get_client(client_fd, res->all_clients);
    if (!client) return;
    
    /* Adaugă topic dacă nu există */
    if (!is_topic(topic_name, res->all_topics))
        add_empty_topic(&res->all_topics, topic_name);
    
    /* Abonează clientul */
    if (!user_has_topic(topic_name, &client->topic_element)) {
        Topic *topic = is_topic(topic_name, res->all_topics);
        if (topic) {
            add_topic_for_user(&client->topic_element, sf_str[0] - '0', topic);
            sprintf(res->tcp_buffer, "Subscribed to topic.");
        }
    } else {
        sprintf(res->tcp_buffer, "Already subscribed to %s.", topic_name);
    }
    
    my_send(client_fd, res->tcp_buffer, strlen(res->tcp_buffer));
}

/* Procesează comanda unsubscribe */
void process_unsubscribe(ServerResources *res, int client_fd, char *cmd) {
    char *topic_name;
    
    if (!(topic_name = strtok(cmd + 12, " \n"))) {
        sprintf(res->tcp_buffer, "Missing topic name. Use: unsubscribe <topic>");
        my_send(client_fd, res->tcp_buffer, strlen(res->tcp_buffer));
        return;
    }
    
    Tcp_client *client = get_client(client_fd, res->all_clients);
    if (!client) return;
    
    Topic *topic = is_topic(topic_name, res->all_topics);
    Client_topic *client_topic = user_has_topic(topic_name, &client->topic_element);
    
    if (topic && client_topic) {
        remove_topic_for_user(&client->topic_element, topic);
        sprintf(res->tcp_buffer, "Unsubscribed from %s.", topic_name);
    } else if (topic) {
        sprintf(res->tcp_buffer, "User is not subscribed to %s.", topic_name);
    } else {
        sprintf(res->tcp_buffer, "Topic %s does not exist.", topic_name);
    }
    
    my_send(client_fd, res->tcp_buffer, strlen(res->tcp_buffer));
}

/* Gestionează mesaje de la clienți TCP */
void handle_client_message(ServerResources *res, int idx) {
    char cmd_copy[200];
    int recv_result = recv(res->pfds[idx].fd, res->tcp_buffer, 200, 0);
    
    if (recv_result <= 0) {
        /* Clientul s-a deconectat */
        for (Tcp_client *client = res->all_clients; client; client = client->next) {
            if (client->client_socket == res->pfds[idx].fd) {
                printf("Client %s disconnected %s.\n", client->id, 
                       recv_result == 0 ? "unexpectedly" : "");
                client->connected = 0;
                break;
            }
        }
        
        close(res->pfds[idx].fd);
        res->nfds--;
        res->pfds[idx].fd = res->pfds[res->nfds].fd;
        return;
    }
    
    strcpy(cmd_copy, res->tcp_buffer);
    
    if (strstr(res->tcp_buffer, "subscribe ") == res->tcp_buffer) {
        process_subscribe(res, res->pfds[idx].fd, cmd_copy);
    } else if (strstr(res->tcp_buffer, "unsubscribe ") == res->tcp_buffer) {
        process_unsubscribe(res, res->pfds[idx].fd, cmd_copy);
    } else if (strstr(res->tcp_buffer, "exit") == res->tcp_buffer) {
        /* Clientul se deconectează */
        for (Tcp_client *client = res->all_clients; client; client = client->next) {
            if (client->client_socket == res->pfds[idx].fd) {
                printf("Client %s disconnected.\n", client->id);
                client->connected = 0;
                break;
            }
        }
        
        close(res->pfds[idx].fd);
        res->nfds--;
        res->pfds[idx].fd = res->pfds[res->nfds].fd;
    } else {
        sprintf(res->tcp_buffer, "Unknown command. Valid commands: subscribe <topic> <SF>, unsubscribe <topic>, exit");
        my_send(res->pfds[idx].fd, res->tcp_buffer, strlen(res->tcp_buffer));
    }
    
    memset(res->tcp_buffer, 0, 200);
}

int main(int argc, char **argv) {
    ServerResources res = {0};
    
    if (argc < 2) {
        fprintf(stderr, "[SERVER] Missing port number\n");
        return -1;
    }
    
    int port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "[SERVER] Invalid port number\n");
        return -1;
    }
    
    if (init_server(&res, port) < 0) {
        return -1;
    }
    
    while (1) {
        if (poll(res.pfds, res.nfds, -1) < 0) {
            perror("[SERVER] Poll failed");
            break;
        }
        
        if ((res.pfds[0].revents & POLLIN) && handle_stdin(&res) < 0) {
            break;
        } else if (res.pfds[1].revents & POLLIN) {
            handle_udp(&res);
        } else if (res.pfds[2].revents & POLLIN) {
            handle_new_connection(&res);
        } else {
            for (int i = 3; i < res.nfds; i++) {
                if (res.pfds[i].revents & POLLIN) {
                    handle_client_message(&res, i);
                }
            }
        }
    }
    
    cleanup_server(&res);
    return 0;
}