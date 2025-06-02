# Tema 2 PCom - Aplicație Client-Server pentru Gestionarea Mesajelor

Acest proiect reprezintă implementarea unei aplicații client-server conform cerințelor. Scopul principal este de a gestiona publicarea și abonarea la mesaje pe diferite topic-uri, folosind protocoalele TCP și UDP.

**Autor:** Vancea Adrian  
**Grupa:** 321CD

---

## Cuprins

- [Arhitectura Aplicației](#arhitectura-aplicației)
- [Structuri de Date Principale](#structuri-de-date-principale)
- [Detalii de Implementare](#detalii-de-implementare)
  - [Server (`server.c`)](#server-serverc)
  - [Client TCP (`subscriber.c`)](#client-tcp-subscriberc)
  - [Funcții Utilitare (`helper.h`, `helper.c`)](#funcții-utilitare-helperh-helperc)
- [Observații Suplimentare](#observații-suplimentare)

---

## Arhitectura Aplicației

Aplicația este compusă din trei entități principale:

### Serverul (`server.c`)
- Componenta centrală care intermediază comunicarea.
- Ascultă conexiuni de la clienții TCP.
- Primește datagrame de la clienții UDP.
- Gestionează abonamentele clienților la topic-uri.
- Redirecționează mesajele primite de la UDP către clienții TCP abonați.
- Folosește `poll()` pentru multiplexarea I/O: `stdin`, `socket UDP`, `socket TCP listening`, `socket-uri TCP clienți`.

### Clienții TCP (`subscriber.c`)
- Se conectează la server pentru abonare/dezabonare.
- Primesc comenzi de la tastatură: `subscribe`, `unsubscribe`, `exit`.
- Afișează mesajele primite de la server.

### Clienții UDP (Publishers)
- Sunt preimplementați.
- Trimit mesaje UDP cu format specific către server.

---

## Structuri de Date Principale

Definite în `helper.h`, majoritatea ca liste simplu înlănțuite:

### `Message`
- `identifier`: ID unic în cadrul unui topic.
- `len`: Lungimea mesajului.
- `message_value`: Conținutul mesajului.
- `no_possible_clients`: Nr. de clienți care ar trebui să primească mesajul.
- `no_sent_clients`: Nr. de clienți care au primit efectiv mesajul.
- `next`: Pointer la următorul mesaj.

### `Topic`
- `name`: Numele topicului.
- `mess_head`, `mess_tail`: Capetele cozii de mesaje.
- `no_messages_ever`: Contor mesaje publicate.
- `next`: Pointer la următorul topic.

### `Client_topic`
- `topic`: Pointer la structura `Topic`.
- `sf`: Store-and-Forward flag (0 sau 1).
- `last_message_identifier`: Ultimul mesaj primit.
- `next`: Pointer la următorul abonament.

### `Tcp_client`
- `id`: ID-ul clientului.
- `connected`: 1 dacă e conectat, 0 altfel.
- `client_socket`: Socket-ul clientului.
- `topic_element`: Pointer la lista de `Client_topic`.
- `next`: Pointer la următorul client.

### `ServerResources`
- Agregă toate resursele și starea serverului.

---

## Detalii de Implementare

### Server (`server.c`)

#### `main()`
- Parsează argumente.
- Apelează `init_server()`.
- Buclă infinită cu `poll()`:
  - `stdin`: `handle_stdin()`
  - `UDP`: `handle_udp()`
  - `TCP listening`: `handle_new_connection()`
  - `TCP clienți existenți`: `handle_client_message()`
- La ieșire: `cleanup_server()`.

#### `init_server(ServerResources *res, int port)`
- Alocă buffere și `pfds`.
- Creează socket-uri UDP și TCP.
- Configurează bind și listen.
- Adaugă descriptori în `pfds`.

#### `cleanup_server(ServerResources *res)`
- Eliberează memorie și închide socket-uri.

#### `handle_stdin(ServerResources *res)`
- Dacă comanda este `exit`, trimite "Exit server." la toți clienții și returnează `-1`.

#### `handle_udp(ServerResources *res)`
- Primește datagramă UDP.
- Creează `full_message` pentru clienți.
- Apelează `add_topic_message()`.
- Trimite mesajul la clienții TCP abonați.
- Apelează `check_remove()`.

#### `handle_new_connection(ServerResources *res)`
- Acceptă conexiune.
- Primește ID client.
- Apelează `add_tcp_client()`:
  - Nou: afisează conectare.
  - Reconectat: re-trimite mesaje SF.
  - ID duplicat: respinge clientul.

#### `handle_client_message(ServerResources *res, int idx)`
- Primește comenzi:
  - `subscribe`: `process_subscribe()`
  - `unsubscribe`: `process_unsubscribe()`
  - `exit`: deconectează clientul.
  - Necunoscut: trimite eroare.

#### `process_subscribe(...)`
- Parsează topic și SF.
- Găsește/creează topic.
- Adaugă abonament dacă nu există.

#### `process_unsubscribe(...)`
- Șterge abonamentul clientului de la un topic.

---

### Client TCP (`subscriber.c`)

#### `main()`
- Parsează argumente.
- Creează socket TCP.
- Trimite ID client.
- Buclă cu `poll()`:
  - `stdin`: trimite comenzi.
  - `server`: primește mesaje.
- La ieșire: închide socket și eliberează memorie.

#### `process_incoming_message(...)`
- Afișează confirmări/erori.
- Dacă e mesaj UDP:
  - Parsează IP/port.
  - Afișează topic, tip și valoare.

---

## Funcții Utilitare (`helper.h`, `helper.c`)

### Mesagerie TCP
- `my_send()`, `my_recv()`: framing TCP.

### Clienți
- `add_tcp_client()`: adaugă sau reconectează client.
- `get_client()`, `free_client()`, `free_clients_list()`

### Topicuri
- `add_empty_topic()`, `is_topic()`, `free_topic()`, `free_topics_list()`

### Mesaje
- `add_topic_message()`: adaugă mesaj într-un topic.
- `check_remove()`: verifică dacă poate fi șters un mesaj.

### Abonamente
- `user_has_topic()`, `add_topic_for_user()`, `remove_topic_for_user()`

---

## Observații Suplimentare

- **Buffering stdout**: Dezactivat cu `setvbuf(...)`.
- **Algoritmul Nagle**: Dezactivat (`TCP_NODELAY`) pentru latență scăzută.
- **Gestionare Erori**: Se verifică toate apelurile critice.
- **Mesaje STDOUT**: Doar cele cerute de enunț, fără debug în plus.

---