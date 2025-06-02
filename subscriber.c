#include "helper.h"

// Prototipul funcției de procesare a mesajelor
int process_incoming_message(char *incoming_data, int bytes_read) {
    // Variabila udp_origin este acum definită doar în această funcție unde este folosită
    struct sockaddr_in udp_origin;
    
    // Verifică dacă este un mesaj de confirmare abonare
    if (strstr(incoming_data, "Subscribed ") == incoming_data) {
        printf("%s\n", incoming_data);
        return 0;
    } 
    
    // Verifică dacă este un mesaj de închidere conexiune
    if (strstr(incoming_data, "Client ") == incoming_data ||
        strstr(incoming_data, "Exit ") == incoming_data) {
        return 1;  // Semnal pentru închidere conexiune
    }
    
    // Verifică dacă este un mesaj cu date UDP
    if (bytes_read > 50) {
        // Extragere informații sursă UDP
        memcpy(&udp_origin, incoming_data, sizeof(struct sockaddr_in));
        
        int topic_offset = sizeof(struct sockaddr_in);
        
        // Afișare adresa și portul sursei UDP
        printf("%s:%d", inet_ntoa(udp_origin.sin_addr), ntohs(udp_origin.sin_port));
        
        // Afișare topic (asigurăm terminarea corectă a șirului)
        char saved = incoming_data[50 + topic_offset];
        incoming_data[50 + topic_offset] = '\0';
        printf(" - %s - ", incoming_data + topic_offset);
        incoming_data[50 + topic_offset] = saved;
        
        // Procesare tip date și afișare în format corespunzător
        uint8_t data_type = incoming_data[50 + topic_offset];
        
        switch (data_type) {
            case 0: { // INT
                uint8_t sign = incoming_data[51 + topic_offset];
                int32_t value = ntohl(*(int32_t *)(incoming_data + 52 + topic_offset));
                printf("INT - %s%d\n", (sign ? "-" : ""), value);
                break;
            }
            case 1: { // SHORT_REAL
                uint16_t value = ntohs(*(uint16_t *)(incoming_data + 51 + topic_offset));
                printf("SHORT_REAL - %.2f\n", value / 100.0);
                break;
            }
            case 2: { // FLOAT
                uint8_t sign = incoming_data[51 + topic_offset];
                int32_t value = ntohl(*(int32_t *)(incoming_data + 52 + topic_offset));
                uint8_t power = incoming_data[56 + topic_offset];
                printf("FLOAT - %s%.*f\n", 
                       (sign ? "-" : ""),
                       power,
                       value / pow(10, power));
                break;
            }
            case 3: { // STRING
                printf("STRING - ");
                puts(incoming_data + 51 + topic_offset);
                break;
            }
            default:
                printf("Unknown type: %d\n", data_type);
                break;
        }
    } else {
        printf("%s\n", incoming_data);
    }
    
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int connection_fd = -1;
    int disable_delay = 1;
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));

    char *user_input = NULL;
    char *incoming_data = NULL;
    struct pollfd *fds = NULL;
    int poll_result;
    int bytes_read;

    // Verificare număr corect de argumente
    if (argc != 4) {
        fprintf(stderr, "[CLIENT_NODE] Format: %s <user_tag> <ip_addr> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Alocare buffer pentru intrare utilizator
    user_input = calloc(256, sizeof(char));
    if (!user_input) {
        perror("[CLIENT_NODE] Input buffer allocation failed");
        return EXIT_FAILURE;
    }

    // Alocare buffer pentru date primite
    incoming_data = calloc(2048, sizeof(char));
    if (!incoming_data) {
        perror("[CLIENT_NODE] Receive buffer allocation failed");
        free(user_input);
        return EXIT_FAILURE;
    }

    // Creare socket TCP
    connection_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection_fd < 0) {
        perror("[CLIENT_NODE] Cannot create TCP socket");
        goto release_buffers;
    }

    // Dezactivare algoritm Nagle pentru performanță mai bună
    if (setsockopt(connection_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&disable_delay, sizeof(int)) < 0) {
        perror("[CLIENT_NODE] TCP_NODELAY set failed");
        goto cleanup;
    }

    // Configurare adresă server
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(argv[3]));
    srv_addr.sin_addr.s_addr = inet_addr(argv[2]);

    // Conectare la server
    if (connect(connection_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("[CLIENT_NODE] Failed to connect to server");
        goto cleanup;
    }

    // Verificare lungime tag utilizator
    if (strlen(argv[1]) >= 256) {
        fprintf(stderr, "[CLIENT_NODE] Tag too long (max 255 chars)\n");
        goto cleanup;
    }

    // Copiere tag utilizator în buffer
    memcpy(user_input, argv[1], strlen(argv[1]));

    // Trimitere tag utilizator către server
    if (my_send(connection_fd, user_input, strlen(user_input)) < 0) {
        perror("[CLIENT_NODE] Unable to transmit tag");
        goto cleanup;
    }

    // Alocare și configurare structuri pentru poll
    fds = malloc(10 * sizeof(struct pollfd));
    if (!fds) {
        perror("[CLIENT_NODE] Failed to allocate poll descriptors");
        goto cleanup;
    }

    int descriptor_count = 0;

    // Adăugare descriptor stdin pentru poll
    fds[descriptor_count].fd = STDIN_FILENO;
    fds[descriptor_count++].events = POLLIN;

    // Adăugare descriptor socket server pentru poll
    fds[descriptor_count].fd = connection_fd;
    fds[descriptor_count++].events = POLLIN;

    while (1) {
        poll_result = poll(fds, descriptor_count, -1);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("[CLIENT_NODE] poll() error");
            break;
        }

        // Procesare intrare utilizator
        if (fds[0].revents & POLLIN) {
            memset(user_input, 0, 256);
            bytes_read = read(STDIN_FILENO, user_input, 100);
            if (bytes_read < 0) {
                perror("[CLIENT_NODE] Failed reading stdin");
                break;
            }

            if (bytes_read == 0) {
                fprintf(stderr, "[CLIENT_NODE] stdin closed\n");
                break;
            }

            if (strcmp(user_input, "exit\n") == 0) {
                if (send(connection_fd, user_input, strlen(user_input), 0) < 0) {
                    perror("[CLIENT_NODE] Couldn't notify server for exit");
                }
                close(connection_fd);
                break;
            }

            // Trimitere mesaj către server
            if (send(connection_fd, user_input, strlen(user_input), 0) < 0) {
                perror("[CLIENT_NODE] Message send failure");
                break;
            }
        }

        // Procesare date primite de la server
        if (fds[1].revents & POLLIN) {
            memset(incoming_data, 0, 2048);
            bytes_read = my_recv(connection_fd, incoming_data);
            if (bytes_read < 0) {
                perror("[CLIENT_NODE] Data reception error");
                break;
            }

            if (bytes_read == 0) {
                fprintf(stderr, "[CLIENT_NODE] Server closed connection\n");
                break;
            }

            // Procesare mesaj folosind funcția factorizată
            if (process_incoming_message(incoming_data, bytes_read)) {
                close(connection_fd);
                break;
            }
        }
    }

cleanup:
    if (connection_fd > 0) {
        close(connection_fd);
    }

release_buffers:
    free(user_input);
    free(incoming_data);
    free(fds);

    return EXIT_SUCCESS;
}