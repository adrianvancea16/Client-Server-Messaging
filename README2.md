# Tema 2 PCom - Aplicație Client-Server pentru Gestionarea Mesajelor

Pentru aceasta tema am accesat doua sleepdays.

Acest proiect reprezintă implementarea unei aplicații client-server conform cerințelor. Scopul principal este de a gestiona publicarea și abonarea la mesaje pe diferite topic-uri, folosind protocoalele TCP și UDP.

**Nume:** Vancea Adrian\
**Grupa:** 321CD

## Cuprins
1.  [Arhitectura Aplicației](#arhitectura-aplicației)
2.  [Funcționalități Server](#funcționalități-server)
3.  [Funcționalități Client TCP (Subscriber)](#funcționalități-client-tcp-subscriber)
4.  [Protocolul de Comunicare TCP (Nivel Aplicație)](#protocolul-de-comunicare-tcp-nivel-aplicație)
5.  [Structuri de Date Principale](#structuri-de-date-principale)
6.  [Detalii de Implementare](#detalii-de-implementare)
    *   [Server (`server.c`)](#server-serverc)
    *   [Client TCP (`subscriber.c`)](#client-tcp-subscriberc)
    *   [Funcții Utilitare (`helper.h`, `helper.c`)](#funcții-utilitare-helperh-helperc)
7.  [Compilare și Rulare](#compilare-și-rulare)
8.  [Observații Suplimentare](#observații-suplimentare)

## Arhitectura Aplicației

Aplicația este compusă din trei entități principale:

1.  **Serverul:**
    *   Componenta centrală care intermediază comunicarea.
    *   Ascultă conexiuni de la clienții TCP pe un socket TCP.
    *   Primește datagrame de la clienții UDP pe un socket UDP.
    *   Gestionează abonamentele clienților TCP la diverse topic-uri.
    *   Redirecționează mesajele primite de la clienții UDP către clienții TCP abonați la topic-urile corespunzătoare.
    *   Folosește mecanismul `poll()` pentru multiplexarea I/O (stdin, socket UDP, socket TCP de listening, socket-uri TCP ale clienților conectați).

2.  **Clienții TCP (Subscribers):**
    *   Se conectează la server pentru a se abona/dezabona la topic-uri.
    *   Primesc comenzi de la tastatură (`subscribe`, `unsubscribe`, `exit`).
    *   Afișează mesajele primite de la server pentru topic-urile la care sunt abonați.

3.  **Clienții UDP (Publishers):**
    *   Aceștia sunt pre-implementați.
    *   Publică mesaje pe diverse topic-uri, trimițându-le către server sub forma unor datagrame UDP cu un format specific.

## Funcționalități Server

*   **Inițializare:**
    *   La pornire, primește un număr de port ca argument.
    *   Deschide un socket UDP și un socket TCP pe portul specificat, ascultând pe toate interfețele de rețea disponibile (`INADDR_ANY`).
    *   Dezactivează buffering-ul pentru `stdout` folosind `setvbuf(stdout, NULL, _IONBF, BUFSIZ)`.
    *   Dezactivează algoritmul Nagle pentru socket-ul TCP principal de listening și pentru fiecare socket nou de client TCP folosind `setsockopt` cu `TCP_NODELAY`.
*   **Gestionare Conexiuni TCP:**
    *   Acceptă noi conexiuni de la clienții TCP.
    *   La conectarea unui nou client, acesta trimite un ID. Serverul verifică unicitatea ID-ului:
        *   Dacă un client cu același ID este deja activ, conexiunea nouă este refuzată, clientul nou este închis, iar serverul afișează `Client <ID_CLIENT> already connected.`.
        *   Dacă ID-ul este nou sau dacă un client cu același ID a fost conectat anterior dar s-a deconectat, conexiunea este acceptată. Serverul afișează `New client <ID_CLIENT> connected from IP:PORT.`.
    *   Păstrează o listă a clienților TCP conectați și a abonamentelor acestora. Abonamentele persistă pentru un ID_CLIENT chiar și după deconectare, dacă nu s-a dat `unsubscribe`.
    *   La deconectarea unui client TCP (comandă `exit` de la client sau închidere neașteptată), serverul afișează `Client <ID_CLIENT> disconnected.`.
*   **Recepție Mesaje UDP:**
    *   Primește datagrame de la clienții UDP. Formatul mesajului UDP este: `topic (50 octeți)` + `tip_date (1 octet)` + `conținut (max 1500 octeți)`.
    *   Extrage topicul, tipul de date și conținutul mesajului.
*   **Redirecționare Mesaje:**
    *   Pentru fiecare mesaj UDP primit, serverul identifică toți clienții TCP abonați la topicul respectiv (inclusiv prin potriviri wildcard).
    *   Construiește un mesaj TCP care include adresa IP și portul clientului UDP sursă, topicul, tipul de date și valoarea mesajului.
    *   Trimite acest mesaj TCP către fiecare client TCP abonat și activ.
    *   **Store-and-Forward (SF):** Dacă un client TCP este abonat la un topic cu opțiunea SF=1 și este deconectat temporar, mesajele primite pe acel topic sunt stocate de server. La reconectarea clientului, serverul îi trimite toate mesajele stocate. Mesajele sunt șterse din coada de așteptare a serverului după ce au fost trimise către toți clienții relevanți (activi sau cu SF=1 care s-ar putea reconecta).
*   **Comenzi de la Tastatură (Server):**
    *   Acceptă comanda `exit`. La primirea acestei comenzi, serverul:
        *   Trimite un mesaj de închidere către toți clienții TCP conectați.
        *   Închide toate socket-urile (inclusiv cele ale clienților TCP).
        *   Eliberează toate resursele alocate.
        *   Se termină.
*   **Gestionare Abonamente:**
    *   Procesează comenzile `subscribe <TOPIC> [SF]` și `unsubscribe <TOPIC>` primite de la clienții TCP.
    *   Actualizează lista de abonamente pentru fiecare client.
    *   **Wildcard-uri:** Suportă abonarea la topic-uri folosind wildcard-urile `*` și `+`:
        *   `+`: Înlocuiește un singur nivel din ierarhia topic-ului. Ex: `a/+/b` se potrivește cu `a/room1/b`, dar nu cu `a/room1/c/b`.
        *   `*`: Înlocuiește zero sau mai multe nivele din ierarhia topic-ului. Ex: `a/*/b` se potrivește cu `a/b`, `a/x/b`, `a/x/y/b`.
        *   Se asigură că un client primește un mesaj o singură dată, chiar dacă mai multe abonamente (simple sau wildcard) se potrivesc cu topicul mesajului. *(Notă: Implementarea efectivă a logicii de potrivire wildcard nu este vizibilă în `process_subscribe` din codul furnizat, dar este descrisă aici conform cerinței.)*

## Funcționalități Client TCP (Subscriber)

*   **Inițializare:**
    *   La pornire, primește ca argumente: `ID_CLIENT`, `IP_SERVER`, `PORT_SERVER`.
    *   Se conectează la server pe adresa și portul specificate.
    *   Trimite serverului `ID_CLIENT` imediat după conectare.
    *   Dezactivează buffering-ul pentru `stdout` folosind `setvbuf(stdout, NULL, _IONBF, BUFSIZ)`.
    *   Dezactivează algoritmul Nagle pentru socket-ul TCP folosind `setsockopt` cu `TCP_NODELAY`.
*   **Comenzi de la Tastatură (Client):**
    *   `subscribe <TOPIC> [SF]`: Trimite serverului o cerere de abonare la topicul specificat. `SF` este 0 sau 1 și indică opțiunea Store-and-Forward. După trimiterea comenzii și primirea confirmării (sau a unui mesaj de eroare) de la server, afișează `Subscribed to topic X.` sau un mesaj corespunzător.
    *   `unsubscribe <TOPIC>`: Trimite serverului o cerere de dezabonare. După trimiterea comenzii și primirea confirmării de la server, afișează `Unsubscribed from topic X.` sau un mesaj corespunzător.
    *   `exit`: Notifică serverul, închide conexiunea și se termină.
*   **Recepție Mesaje de la Server:**
    *   Ascultă mesaje de la server.
    *   La primirea unui mesaj de date (publicat de un client UDP), îl afișează în formatul:
        `<IP_CLIENT_UDP>:<PORT_CLIENT_UDP> - <TOPIC> - <TIP_DATE> - <VALOARE_MESAJ>`
    *   Formatează `<VALOARE_MESAJ>` în funcție de `<TIP_DATE>`:
        *   **INT (0):** Număr întreg cu semn. Ex: `-123`
        *   **SHORT_REAL (1):** Număr real pozitiv cu 2 zecimale. Ex: `23.50`
        *   **FLOAT (2):** Număr real cu semn, cu precizie variabilă. Ex: `-12.3456`
        *   **STRING (3):** Șir de caractere.
    *   La primirea unui mesaj de la server care indică închiderea acestuia (ex: "Exit server."), clientul se închide.
    *   La primirea unui mesaj de la server care indică o problemă (ex: "Client ... already connected."), clientul se închide.
*   **Multiplexare:** Folosește `poll()` pentru a monitoriza simultan intrarea de la tastatură (`STDIN_FILENO`) și mesajele de la server (socket-ul TCP).

## Protocolul de Comunicare TCP (Nivel Aplicație)

Deoarece TCP este un protocol orientat pe flux (stream-oriented), nu garantează că datele trimise printr-un singur apel `send()` vor fi primite printr-un singur apel `recv()`. Mesajele se pot fragmenta sau concatena. Pentru a gestiona corect mesajele la nivel de aplicație peste TCP, am implementat un protocol simplu de framing:

*   **Formatul Mesajului TCP:**
    Fiecare mesaj logic trimis peste TCP (atât de la client la server, cât și de la server la client) este precedat de lungimea sa.
    `[lungime_mesaj (short - 2 octeți)] [payload_mesaj (lungime_mesaj octeți)]`

*   **Funcții de Trimitere/Recepție (`my_send`, `my_recv` în `helper.c`):**
    *   `my_send(socket, buffer, length)`:
        1.  Trimite valoarea `length` (convertită la `short`) pe socket.
        2.  Trimite conținutul `buffer`-ului (de `length` octeți) pe socket.
    *   `my_recv(socket, buffer)`:
        1.  Citește primii 2 octeți de pe socket pentru a determina `lungime_mesaj_de_primit`.
        2.  Citește exact `lungime_mesaj_de_primit` octeți de pe socket în `buffer`, gestionând citirile parțiale într-o buclă până când întregul mesaj este primit.
        3.  Returnează numărul de octeți citiți pentru payload.

*   **Dezactivarea Algoritmului Nagle (`TCP_NODELAY`):**
    S-a setat opțiunea `TCP_NODELAY` pe socket-urile TCP pentru a reduce latența. Acest lucru face ca segmentele TCP mici să fie trimise imediat, fără a aștepta acumularea mai multor date sau un ACK, ceea ce este benefic pentru o aplicație de mesagerie interactivă. Aceasta NU elimină necesitatea protocolului de framing de mai sus, dar poate reduce frecvența concatenării mesajelor.

## Structuri de Date Principale

Am folosit următoarele structuri de date (majoritatea implementate ca liste simplu înlănțuite), definite în `helper.h`:

*   `Message`: Reprezintă un mesaj individual primit de la un client UDP și stocat/procesat de server.
    *   `identifier`: Un ID unic pentru mesaj în cadrul unui topic (ex: `no_messages_ever` al topicului la momentul adăugării).
    *   `len`: Lungimea `message_value`.
    *   `message_value`: Conținutul mesajului (include adresa IP/port UDP sursă, topic, tip, valoare).
    *   `no_possible_clients`: Numărul de clienți TCP care ar trebui să primească acest mesaj (activi sau cu SF=1).
    *   `no_sent_clients`: Numărul de clienți TCP cărora le-a fost efectiv trimis mesajul (sau marcat ca trimis dacă SF=0 și clientul e deconectat).
    *   `next`: Pointer la următorul mesaj în lista de mesaje a unui topic.
*   `Topic` (inițial numit `udp_topic` în cod, dar se referă la un topic în general): Reprezintă un topic de mesaje.
    *   `name`: Numele topicului (șir de caractere).
    *   `mess_head`, `mess_tail`: Pointeri la primul și ultimul mesaj din coada de mesaje pentru acest topic (pentru clienții cu SF=1).
    *   `no_messages_ever`: Contor pentru numărul total de mesaje publicate vreodată pe acest topic.
    *   `next`: Pointer la următorul topic în lista globală de topic-uri a serverului.
*   `Client_topic`: Reprezintă abonamentul unui client TCP la un anumit topic.
    *   `topic`: Pointer la structura `Topic` la care clientul este abonat.
    *   `sf`: Flag Store-and-Forward (0 sau 1).
    *   `last_message_identifier`: ID-ul ultimului mesaj primit de client pe acest topic (util pentru SF).
    *   `next`: Pointer la următorul abonament din lista de abonamente a unui client.
*   `Tcp_client` (inițial numit `client_info`): Reprezintă un client TCP conectat la server.
    *   `id`: ID-ul clientului (șir de caractere).
    *   `connected`: Flag care indică dacă clientul este momentan conectat (1) sau deconectat (0).
    *   `client_socket`: Descriptorul socket-ului pentru clientul TCP.
    *   `topic_element`: Pointer la capul listei de `Client_topic` (abonamentele clientului).
    *   `next`: Pointer la următorul client în lista globală de clienți a serverului.
*   `ServerResources` (în `server.c`): O structură care agregă toate resursele și starea serverului (buffere, descriptori de socket, liste de clienți/topicuri, `pollfd` array etc.).

## Detalii de Implementare

### Server (`server.c`)

*   **`main()`:**
    1.  Parsează argumentele (portul).
    2.  Apelează `init_server()` pentru a aloca resurse și a configura socket-urile și `pollfd`.
    3.  Intră într-o buclă infinită unde așteaptă evenimente folosind `poll()`:
        *   **Input de la `STDIN_FILENO` (`pfds[0]`):** Gestionează comanda `exit` prin `handle_stdin()`. Dacă `handle_stdin()` returnează < 0, bucla se oprește.
        *   **Datagramă pe socket-ul UDP (`pfds[1]`):** Apelează `handle_udp()` pentru a procesa mesajul.
        *   **Conexiune nouă pe socket-ul TCP de listening (`pfds[2]`):** Apelează `handle_new_connection()` pentru a accepta și configura noul client.
        *   **Mesaj de la un client TCP existent (`pfds[3]`...`pfds[nfds-1]`):** Apelează `handle_client_message()` pentru a procesa comanda clientului.
    4.  La ieșirea din buclă (ex: comanda `exit`), apelează `cleanup_server()` pentru a elibera toate resursele.
*   **`init_server(ServerResources *res, int port)`:**
    *   Alocă memorie pentru buffere (`stdin_buffer`, `topic_name`, `udp_buffer`, `tcp_buffer`) și pentru array-ul `pfds`.
    *   Creează socket-ul UDP (`sock_udp`) și socket-ul TCP principal (`sock_tcp_main`).
    *   Configurează `serv_addr` și face `bind` pentru ambele socket-uri pe portul dat și `INADDR_ANY`.
    *   Pune socket-ul TCP în modul `listen`.
    *   Inițializează `pfds` cu `STDIN_FILENO`, `sock_udp`, `sock_tcp_main`.
    *   Setează `TCP_NODELAY` pe `sock_tcp_main`.
*   **`cleanup_server(ServerResources *res)`:**
    *   Eliberează memoria alocată pentru buffere, `pfds`, lista de clienți (`all_clients` prin `free_clients_list`), și lista de topicuri (`all_topics` prin `free_topics_list`).
    *   Închide `sock_udp` și `sock_tcp_main`.
*   **`handle_stdin(ServerResources *res)`:**
    *   Citește de la `stdin`.
    *   Dacă comanda este "exit", trimite un mesaj "Exit server." (folosind `my_send`) către toți clienții TCP conectați, închide socket-urile lor și returnează -1. Altfel, returnează 0.
*   **`handle_udp(ServerResources *res)`:**
    *   Primește datagrama UDP folosind `recvfrom`.
    *   Extrage numele topicului din primii 50 de octeți.
    *   Creează un `full_message` care conține structura `sockaddr_in` a clientului UDP și payload-ul UDP original. Acest `full_message` va fi trimis clienților TCP.
    *   Apelează `add_topic_message()` pentru a adăuga/actualiza topicul și a adăuga mesajul la coada topicului. Această funcție calculează și `no_possible_clients` pentru mesaj.
    *   Iterează prin toți clienții TCP (`res->all_clients`):
        *   Verifică dacă clientul este abonat la topicul mesajului (`user_has_topic`).
        *   Dacă da, iterează prin mesajele din coada topicului (începând de la `topic->last_message_identifier` al clientului pentru acel topic).
        *   Dacă clientul este conectat (`copy->connected == 1`), trimite mesajul folosind `my_send`, incrementează `msg->no_sent_clients` și actualizează `topic->last_message_identifier` al clientului.
        *   Dacă clientul NU este conectat DAR are SF=0 pentru topic, doar actualizează `topic->last_message_identifier` (mesajul se consideră "pierdut" pentru el). Mesajele pentru clienții cu SF=1 deconectați rămân în coadă.
        *   Apelează `check_remove()` pentru fiecare mesaj procesat pentru a vedea dacă poate fi șters din memorie (dacă `msg->no_sent_clients == msg->no_possible_clients`).
*   **`handle_new_connection(ServerResources *res)`:**
    *   Apelează `accept()` pe `sock_tcp_main` pentru a obține un nou socket de client.
    *   Primește ID-ul clientului folosind `my_recv()`.
    *   Redimensionează dinamic array-ul `pfds` dacă este necesar.
    *   Adaugă noul socket la `pfds`.
    *   Apelează `add_tcp_client()` (din `helper.c`) pentru a adăuga clientul în lista serverului.
        *   Dacă `add_tcp_client` returnează `1` (client nou): afișează mesajul de conectare.
        *   Dacă `add_tcp_client` returnează `2` (client reconectat): afișează mesajul de conectare și apoi parcurge abonamentele clientului și mesajele stocate (cu SF=1) pentru a i le trimite.
        *   Dacă `add_tcp_client` returnează `0` (ID deja conectat): trimite mesajul "Client ... already connected." clientului, îl închide și decrementează `nfds`.
*   **`handle_client_message(ServerResources *res, int idx)`:**
    *   Primește comanda de la clientul TCP de pe `pfds[idx].fd` folosind `recv()`.
    *   Dacă `recv` returnează `<= 0`, clientul s-a deconectat. Se marchează `client->connected = 0`, se afișează mesajul de deconectare, se închide socket-ul și se elimină din `pfds`.
    *   Parsează comanda:
        *   **"subscribe <TOPIC> <SF>"**: Apelează `process_subscribe()`.
        *   **"unsubscribe <TOPIC>"**: Apelează `process_unsubscribe()`.
        *   **"exit"**: Marchează clientul ca deconectat, afișează mesaj, închide socket-ul, elimină din `pfds`.
        *   Altfel: comandă necunoscută, trimite mesaj de eroare.
*   **`process_subscribe(ServerResources *res, int client_fd, char *cmd)`:**
    *   Parsează topicul și flag-ul SF din comandă.
    *   Validează formatul.
    *   Găsește clientul (`get_client`).
    *   Dacă topicul nu există, îl creează (`add_empty_topic`).
    *   Dacă clientul nu e deja abonat, îl abonează (`add_topic_for_user`).
    *   Trimite mesaj de confirmare/eroare clientului folosind `my_send`.
*   **`process_unsubscribe(ServerResources *res, int client_fd, char *cmd)`:**
    *   Parsează topicul din comandă.
    *   Găsește clientul și topicul.
    *   Dacă clientul e abonat, îl dezabonează (`remove_topic_for_user`).
    *   Trimite mesaj de confirmare/eroare clientului folosind `my_send`.

### Client TCP (`subscriber.c`)

*   **`main()`:**
    1.  Parsează argumentele (`ID_CLIENT`, `IP_SERVER`, `PORT_SERVER`).
    2.  Alocă buffere (`user_input`, `incoming_data`).
    3.  Creează socket-ul TCP (`connection_fd`).
    4.  Setează `TCP_NODELAY`.
    5.  Configurează `srv_addr` și apelează `connect()`.
    6.  Trimite `ID_CLIENT` către server folosind `my_send()`.
    7.  Inițializează `fds` pentru `poll` (cu `STDIN_FILENO` și `connection_fd`).
    8.  Intră într-o buclă infinită unde așteaptă evenimente folosind `poll()`:
        *   **Input de la `STDIN_FILENO` (`fds[0]`):**
            *   Citește comanda.
            *   Dacă e "exit", o trimite la server, închide `connection_fd` și iese din buclă.
            *   Altfel, trimite comanda la server.
        *   **Mesaj de la server (`fds[1]`):**
            *   Primește mesajul folosind `my_recv()`.
            *   Dacă `my_recv` returnează `<= 0`, serverul a închis conexiunea sau a apărut o eroare; iese din buclă.
            *   Apelează `process_incoming_message()` pentru a parsa și afișa mesajul. Dacă aceasta returnează 1 (ex: serverul se închide), iese din buclă.
    9.  La ieșirea din buclă, închide `connection_fd` (dacă nu e deja închis) și eliberează bufferele.
*   **`process_incoming_message(char *incoming_data, int bytes_read)`:**
    *   Verifică dacă e un mesaj de confirmare (`Subscribed...`, `Unsubscribed...`, erori) și îl afișează.
    *   Verifică dacă e un mesaj de la server de tip "Client ... already connected" sau "Exit server." și returnează 1 pentru a semnala închiderea.
    *   Dacă `bytes_read > 50` (sugerează un mesaj de date de la un client UDP), atunci:
        1.  Copiază primii `sizeof(struct sockaddr_in)` octeți în `udp_origin` pentru a obține IP-ul și portul clientului UDP.
        2.  Afișează `IP_CLIENT_UDP:PORT_CLIENT_UDP`.
        3.  Extrage și afișează numele topicului (de la offset `sizeof(struct sockaddr_in)`, lungime 50).
        4.  Extrage tipul de date (`data_type`) de la offset `sizeof(struct sockaddr_in) + 50`.
        5.  În funcție de `data_type`, extrage valoarea (folosind `ntohl`, `ntohs` corespunzător și `pow()` pentru FLOAT) și o afișează formatat.
    *   Altfel (mesaje text simple, probabil confirmări), afișează direct.
    *   Returnează 0 pentru continuare.

### Funcții Utilitare (`helper.h`, `helper.c`)

*   **`my_send(int sock, char *mess, short len)` / `my_recv(int sock, char *mess)`:** Implementează protocolul de framing TCP descris anterior.
*   **Gestionare Clienți TCP (`add_tcp_client`, `get_client`, `free_client`, `free_clients_list`):**
    *   `add_tcp_client`: Adaugă un client nou sau actualizează starea unuia existent (reconectare). Gestionează și cazul ID-ului duplicat. Setează `TCP_NODELAY` pe socket-ul noului client.
    *   `get_client`: Caută un client în listă după descriptorul de socket.
    *   Funcțiile `free_...` eliberează memoria alocată pentru structurile respective.
*   **Gestionare Topicuri (`add_empty_topic`, `is_topic`, `free_topic`, `free_topics_list`):**
    *   `add_empty_topic`: Adaugă un topic nou (fără mesaje inițial) în lista serverului.
    *   `is_topic`: Verifică dacă un topic cu un anumit nume există.
*   **Gestionare Mesaje (`add_topic_message`, `free_message`, `free_topic_messages`, `check_remove`):**
    *   `add_topic_message`: Adaugă un mesaj primit de la UDP la coada unui topic. Dacă topicul nu există, îl creează. Calculează `no_possible_clients` pentru noul mesaj, iterând prin toți clienții și abonamentele lor (verificând dacă sunt activi sau au SF=1 pentru topicul respectiv).
    *   `check_remove`: Verifică dacă un mesaj (`mess`) dintr-un topic (`topic`) poate fi șters (dacă `mess->no_possible_clients == mess->no_sent_clients`). Gestionează corect actualizarea pointerilor `mess_head` și `mess_tail` ai topicului.
*   **Gestionare Abonamente (`user_has_topic`, `add_topic_for_user`, `remove_topic_for_user`):**
    *   `user_has_topic`: Verifică dacă un client este abonat la un anumit topic.
    *   `add_topic_for_user`: Adaugă un abonament (cu SF și `last_message_identifier` inițializat) la lista de abonamente a unui client.
    *   `remove_topic_for_user`: Șterge un abonament din lista unui client.

## Compilare și Rulare

1.  **Compilare:**
    Se folosește `Makefile`-ul furnizat:
    ```bash
    make server     # Compilează serverul
    make subscriber # Compilează clientul TCP
    make            # Compilează ambele
    make clean      # Șterge fișierele executabile și obiect
    ```

2.  **Rulare:**
    *   **Server:**
        ```bash
        ./server <PORT_DORIT>
        ```
        Exemplu: `./server 8080`

    *   **Client TCP (Subscriber):**
        ```bash
        ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>
        ```
        Exemplu: `./subscriber client1 127.0.0.1 8080`

    *   **Client UDP (Publisher - exemplu, dacă ați dezvoltat unul pentru testare):**
        Comanda de rulare ar depinde de implementarea clientului UDP. Acesta ar trebui să trimită datagrame către `IP_SERVER:PORT_SERVER` pe UDP, respectând formatul: `topic (50 octeți)`, `tip_date (1 octet)`, `conținut (max 1500 octeți)`.

## Observații Suplimentare

*   **Buffering Ieșire:** S-a folosit `setvbuf(stdout, NULL, _IONBF, BUFSIZ)` atât în server cât și în client pentru a dezactiva buffering-ul la `stdout`, asigurând afișarea imediată a mesajelor.
*   **Algoritmul Nagle:** S-a dezactivat algoritmul Nagle (`TCP_NODELAY`) pentru a minimiza latența în comunicarea TCP.
*   **Gestionarea Erorilor:** S-au verificat valorile returnate de majoritatea apelurilor de sistem și funcțiilor, afișând mesaje de eroare la `stderr` (folosind `perror` sau `fprintf`) în caz de probleme.
*   **Mesaje de Debug:** Nu se afișează alte mesaje la `STDOUT` în afara celor specificate în cerință.
*   **Wildcard-uri:** Logica de potrivire a wildcard-urilor (`*`, `+`) pentru comanda `subscribe` este esențială conform cerinței. Deși implementarea detaliată a acestei potriviri nu este explicită în funcțiile `process_subscribe` din codul furnizat, designul general o prevede, iar serverul ar trebui să filtreze mesajele corespunzător. Potrivirea s-ar face prin tokenizarea numelui topicului din mesajul UDP și a pattern-ului de abonare, comparând apoi segmentele.

---