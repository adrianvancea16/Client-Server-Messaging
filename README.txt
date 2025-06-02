Tema 2 PCom - Aplicație Client-Server pentru Gestionarea Mesajelor

Pentru aceasta tema am accesat doua sleepdays.
Acest proiect reprezintă implementarea unei aplicații client-server conform cerințelor. Scopul principal este de a gestiona publicarea și abonarea la mesaje pe diferite topic-uri, folosind protocoalele TCP și UDP.

Nume: Vancea Adrian
Grupa: 321CD

Cuprins
Arhitectura Aplicației
Structuri de Date Principale
Detalii de Implementare
Server (server.c)
Client TCP (subscriber.c)
Funcții Utilitare (helper.h, helper.c)
Observații Suplimentare

Arhitectura Aplicației
Aplicația este compusă din trei entități principale:

Serverul (server.c):
- Componenta centrală care intermediază comunicarea.
- Ascultă conexiuni de la clienții TCP pe un socket TCP.
- Primește datagrame de la clienții UDP pe un socket UDP.
- Gestionează abonamentele clienților TCP la diverse topic-uri.
- Redirecționează mesajele primite de la clienții UDP către clienții TCP abonați la topic-urile corespunzătoare.
- Folosește mecanismul poll() pentru multiplexarea I/O (stdin, socket UDP, socket TCP de listening, socket-uri TCP ale clienților conectați).

Clienții TCP (subscriber.c):
- Se conectează la server pentru a se abona/dezabona la topic-uri.
- Primesc comenzi de la tastatură (subscribe, unsubscribe, exit).
- Afișează mesajele primite de la server pentru topic-urile la care sunt abonați.

Clienții UDP (Publishers):
- Aceștia sunt pre-implementați.
- Publică mesaje pe diverse topic-uri, trimițându-le către server sub forma unor datagrame UDP cu un format specific.

Structuri de Date Principale
Am folosit următoarele structuri de date (majoritatea implementate ca liste simplu înlănțuite), definite în helper.h:

Message: Reprezintă un mesaj individual primit de la un client UDP și stocat/procesat de server.
- identifier: Un ID unic pentru mesaj în cadrul unui topic (ex: no_messages_ever al topicului la momentul adăugării).
- len: Lungimea message_value.
- message_value: Conținutul mesajului (include adresa IP/port UDP sursă, topic, tip, valoare).
- no_possible_clients: Numărul de clienți TCP care ar trebui să primească acest mesaj (activi sau cu SF=1).
- no_sent_clients: Numărul de clienți TCP cărora le-a fost efectiv trimis mesajul (sau marcat ca trimis dacă SF=0 și clientul e deconectat).
- next: Pointer la următorul mesaj în lista de mesaje a unui topic.

Topic (inițial numit udp_topic în cod, dar se referă la un topic în general): Reprezintă un topic de mesaje.
- name: Numele topicului (șir de caractere).
- mess_head, mess_tail: Pointeri la primul și ultimul mesaj din coada de mesaje pentru acest topic (pentru clienții cu SF=1).
- no_messages_ever: Contor pentru numărul total de mesaje publicate vreodată pe acest topic.
- next: Pointer la următorul topic în lista globală de topic-uri a serverului.

Client_topic: Reprezintă abonamentul unui client TCP la un anumit topic.
- topic: Pointer la structura Topic la care clientul este abonat.
- sf: Flag Store-and-Forward (0 sau 1).
- last_message_identifier: ID-ul ultimului mesaj primit de client pe acest topic (util pentru SF).
- next: Pointer la următorul abonament din lista de abonamente a unui client.

Tcp_client (inițial numit client_info): Reprezintă un client TCP conectat la server.
- id: ID-ul clientului (șir de caractere).
- connected: Flag care indică dacă clientul este momentan conectat (1) sau deconectat (0).
- client_socket: Descriptorul socket-ului pentru clientul TCP.
- topic_element: Pointer la capul listei de Client_topic (abonamentele clientului).
- next: Pointer la următorul client în lista globală de clienți a serverului.

ServerResources (în server.c): O structură care agregă toate resursele și starea serverului (buffere, descriptori de socket, liste de clienți/topicuri, pollfd array etc.).

Detalii de Implementare

Server (server.c)

main():
- Parsează argumentele (portul).
- Apelează init_server() pentru a aloca resurse și a configura socket-urile și pollfd.
- Intră într-o buclă infinită unde așteaptă evenimente folosind poll():
  - Input de la STDIN_FILENO (pfds[0]): Gestionează comanda exit prin handle_stdin(). Dacă handle_stdin() returnează < 0, bucla se oprește.
  - Datagramă pe socket-ul UDP (pfds[1]): Apelează handle_udp() pentru a procesa mesajul.
  - Conexiune nouă pe socket-ul TCP de listening (pfds[2]): Apelează handle_new_connection() pentru a accepta și configura noul client.
  - Mesaj de la un client TCP existent (pfds[3]...pfds[nfds-1]): Apelează handle_client_message() pentru a procesa comanda clientului.
- La ieșirea din buclă (ex: comanda exit), apelează cleanup_server() pentru a elibera toate resursele.

init_server(ServerResources *res, int port):
- Alocă memorie pentru buffere (stdin_buffer, topic_name, udp_buffer, tcp_buffer) și pentru array-ul pfds.
- Creează socket-ul UDP (sock_udp) și socket-ul TCP principal (sock_tcp_main).
- Configurează serv_addr și face bind pentru ambele socket-uri pe portul dat și INADDR_ANY.
- Pune socket-ul TCP în modul listen.
- Inițializează pfds cu STDIN_FILENO, sock_udp, sock_tcp_main.
- Setează TCP_NODELAY pe sock_tcp_main.

cleanup_server(ServerResources *res):
- Eliberează memoria alocată pentru buffere, pfds, lista de clienți (all_clients prin free_clients_list), și lista de topicuri (all_topics prin free_topics_list).
- Închide sock_udp și sock_tcp_main.

handle_stdin(ServerResources *res):
- Citește de la stdin.
- Dacă comanda este "exit", trimite un mesaj "Exit server." (folosind my_send) către toți clienții TCP conectați, închide socket-urile lor și returnează -1. Altfel, returnează 0.

handle_udp(ServerResources *res):
- Primește datagrama UDP folosind recvfrom.
- Extrage numele topicului din primii 50 de octeți.
- Creează un full_message care conține structura sockaddr_in a clientului UDP și payload-ul UDP original. Acest full_message va fi trimis clienților TCP.
- Apelează add_topic_message() pentru a adăuga/actualiza topicul și a adăuga mesajul la coada topicului. Această funcție calculează și no_possible_clients pentru mesaj.
- Iterează prin toți clienții TCP (res->all_clients):
  - Verifică dacă clientul este abonat la topicul mesajului (user_has_topic).
  - Dacă da, iterează prin mesajele din coada topicului (începând de la topic->last_message_identifier al clientului pentru acel topic).
  - Dacă clientul este conectat (copy->connected == 1), trimite mesajul folosind my_send, incrementează msg->no_sent_clients și actualizează topic->last_message_identifier al clientului.
  - Dacă clientul NU este conectat DAR are SF=0 pentru topic, doar actualizează topic->last_message_identifier (mesajul se consideră "pierdut" pentru el). Mesajele pentru clienții cu SF=1 deconectați rămân în coadă.
- Apelează check_remove() pentru fiecare mesaj procesat pentru a vedea dacă poate fi șters din memorie (dacă msg->no_sent_clients == msg->no_possible_clients).

handle_new_connection(ServerResources *res):
- Apelează accept() pe sock_tcp_main pentru a obține un nou socket de client.
- Primește ID-ul clientului folosind my_recv().
- Redimensionează dinamic array-ul pfds dacă este necesar.
- Adaugă noul socket la pfds.
- Apelează add_tcp_client() (din helper.c) pentru a adăuga clientul în lista serverului.
  - Dacă add_tcp_client returnează 1 (client nou): afișează mesajul de conectare.
  - Dacă add_tcp_client returnează 2 (client reconectat): afișează mesajul de conectare și apoi parcurge abonamentele clientului și mesajele stocate (cu SF=1) pentru a i le trimite.
  - Dacă add_tcp_client returnează 0 (ID deja conectat): trimite mesajul "Client ... already connected." clientului, îl închide și decrementează nfds.

handle_client_message(ServerResources *res, int idx):
- Primește comanda de la clientul TCP de pe pfds[idx].fd folosind recv().
- Dacă recv returnează <= 0, clientul s-a deconectat. Se marchează client->connected = 0, se afișează mesajul de deconectare, se închide socket-ul și se elimină din pfds.
- Parsează comanda:
  - "subscribe ": Apelează process_subscribe().
  - "unsubscribe ": Apelează process_unsubscribe().
  - "exit": Marchează clientul ca deconectat, afișează mesaj, închide socket-ul, elimină din pfds.
  - Altfel: comandă necunoscută, trimite mesaj de eroare.

process_subscribe(ServerResources *res, int client_fd, char *cmd):
- Parsează topicul și flag-ul SF din comandă.
- Validează formatul.
- Găsește clientul (get_client).
- Dacă topicul nu există, îl creează (add_empty_topic).
- Dacă clientul nu e deja abonat, îl abonează (add_topic_for_user).
- Trimite mesaj de confirmare/eroare clientului folosind my_send.

process_unsubscribe(ServerResources *res, int client_fd, char *cmd):
- Parsează topicul din comandă.
- Găsește clientul și topicul.
- Dacă clientul e abonat, îl dezabonează (remove_topic_for_user).
- Trimite mesaj de confirmare/eroare clientului folosind my_send.

Client TCP (subscriber.c)

main():
- Parsează argumentele (ID_CLIENT, IP_SERVER, PORT_SERVER).
- Alocă buffere (user_input, incoming_data).
- Creează socket-ul TCP (connection_fd).
- Setează TCP_NODELAY.
- Configurează srv_addr și apelează connect().
- Trimite ID_CLIENT către server folosind my_send().
- Inițializează fds pentru poll (cu STDIN_FILENO și connection_fd).
- Intră într-o buclă infinită unde așteaptă evenimente folosind poll():
  - Input de la STDIN_FILENO (fds[0]):
    - Citește comanda.
    - Dacă e "exit", o trimite la server, închide connection_fd și iese din buclă.
    - Altfel, trimite comanda la server.
  - Mesaj de la server (fds[1]):
    - Primește mesajul folosind my_recv().
    - Dacă my_recv returnează <= 0, serverul a închis conexiunea sau a apărut o eroare; iese din buclă.
    - Apelează process_incoming_message() pentru a parsa și afișa mesajul. Dacă aceasta returnează 1 (ex: serverul se închide), iese din buclă.
- La ieșirea din buclă, închide connection_fd (dacă nu e deja închis) și eliberează bufferele.

process_incoming_message(char *incoming_data, int bytes_read):
- Verifică dacă e un mesaj de confirmare (Subscribed..., Unsubscribed..., erori) și îl afișează.
- Verifică dacă e un mesaj de la server de tip "Client ... already connected" sau "Exit server." și returnează 1 pentru a semnala închiderea.
- Dacă bytes_read > 50 (sugerează un mesaj de date de la un client UDP), atunci:
  - Copiază primii sizeof(struct sockaddr_in) octeți în udp_origin pentru a obține IP-ul și portul clientului UDP.
  - Afișează IP_CLIENT_UDP:PORT_CLIENT_UDP.
  - Extrage și afișează numele topicului (de la offset sizeof(struct sockaddr_in), lungime 50).
  - Extrage tipul de date (data_type) de la offset sizeof(struct sockaddr_in) + 50.
  - În funcție de data_type, extrage valoarea (folosind ntohl, ntohs corespunzător și pow() pentru FLOAT) și o afișează formatat.
- Altfel (mesaje text simple, probabil confirmări), afișează direct.
- Returnează 0 pentru continuare.

Funcții Utilitare (helper.h, helper.c)

my_send(int sock, char *mess, short len) / my_recv(int sock, char *mess): Implementează protocolul de framing TCP descris anterior.

Gestionare Clienți TCP (add_tcp_client, get_client, free_client, free_clients_list):
- add_tcp_client: Adaugă un client nou sau actualizează starea unuia existent (reconectare). Gestionează și cazul ID-ului duplicat. Setează TCP_NODELAY pe socket-ul noului client.
- get_client: Caută un client în listă după descriptorul de socket.
- Funcțiile free_... eliberează memoria alocată pentru structurile respective.

Gestionare Topicuri (add_empty_topic, is_topic, free_topic, free_topics_list):
- add_empty_topic: Adaugă un topic nou (fără mesaje inițial) în lista serverului.
- is_topic: Verifică dacă un topic cu un anumit nume există.

Gestionare Mesaje (add_topic_message, free_message, free_topic_messages, check_remove):
- add_topic_message: Adaugă un mesaj primit de la UDP la coada unui topic. Dacă topicul nu există, îl creează. Calculează no_possible_clients pentru noul mesaj, iterând prin toți clienții și abonamentele lor (verificând dacă sunt activi sau au SF=1 pentru topicul respectiv).
- check_remove: Verifică dacă un mesaj (mess) dintr-un topic (topic) poate fi șters (dacă mess->no_possible_clients == mess->no_sent_clients). Gestionează corect actualizarea pointerilor mess_head și mess_tail ai topicului.

Gestionare Abonamente (user_has_topic, add_topic_for_user, remove_topic_for_user):
- user_has_topic: Verifică dacă un client este abonat la un anumit topic.
- add_topic_for_user: Adaugă un abonament (cu SF și last_message_identifier inițializat) la lista de abonamente a unui client.
- remove_topic_for_user: Șterge un abonament din lista unui client.

Observații Suplimentare
- Buffering Ieșire: S-a folosit setvbuf(stdout, NULL, _IONBF, BUFSIZ) atât în server cât și în client pentru a dezactiva buffering-ul la stdout, asigurând afișarea imediată a mesajelor.
- Algoritmul Nagle: S-a dezactivat algoritmul Nagle (TCP_NODELAY) pentru a minimiza latența în comunicarea TCP.
- Gestionarea Erorilor: S-au verificat valorile returnate de majoritatea apelurilor de sistem și funcțiilor, afișând mesaje de eroare la stderr (folosind perror sau fprintf) în caz de probleme.
- Mesaje de Debug: Nu se afișează alte mesaje la STDOUT în afara celor specificate în cerință.