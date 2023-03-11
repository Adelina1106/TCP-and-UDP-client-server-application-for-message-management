# TCP-and-UDP-client-server-application-for-message-management

Rizea Adelina-Maria

------------------------------------------------------------------------------

    Am folosit 7 vectori pentru a retine următoarele: clienții
conectați(connected), topic-urile cu abonati(topics), subscriberii pentru
aceste topic-uri(subscribers_of_topic), id-urile clientilor care au abonari
cu SF = 1(clients_id), topic-urile la care clientii sunt abonati cu SF = 1
(topics_SF) si mesajele pe care un client ar trebui sa le primeasca la
reconectare(to_receive).
    Structura tcp_message este folosita pentru reprezentarea mesajelor,
aceasta avand primul camp message ce retine mesajul ce va fi transmis si
campul id_client ce retine id-ul clientului care a trimis mesajul
(sau poate fi gol).
    udp_message este structura folosita pentru formatarea mesajelor ce vor
fi trimise catre clienti pentru a fi afisate. topic contine topic-ul, 
tip_date contine tipul de date(0/1/2/2), iar payload-ul contine valoarea
propriu zisa a mesajului.
    client este structura pentru un client, folosita in vectorul de clienti
conectati si in cel de abonati.

------------------------------------------------------------------------------
                                Server

    Creez si setez socket-ii udp si tcp.
    Dacă se primesc mesaje pe socket-ul de TCP, înseamnă că un nou subscriber
încearcă sa se conecteze. Verific dacă este un nou client sau unul care
se reconecteaza. Pentru cel de-al doilea caz, ii trimit mesajele reținute
în vectorul to_receive(mesajele la indicele corespunzător id-ului clientului
din vectorul clientsID) si sterg aceste mesaje din vector. Adaug noul client
conectat la vectorul de clienți conectați(connected).

    Dacă se primește un mesaj pe socket-ul UDP, populez câmpurile structurii
udp_message din care voi forma mesajul ce trebuie trimis către subscriber.
Formez acest mesaj folosind sprintf și câmpurile structurii. Dau de 2 ori send
catre client. Trimit prima oara dimensiunea mesajului ce va trebui afișat în
client, urmat de dlimitatorul END. Trimit apoi structura de tip tcp_message
ce are în câmpul message mesajul ce va trebui afisat(cel format mai sus).

    Dacă se primește mesaj de la tastatura, se accepta doar mesajul exit.
Dacă acesta este primit, se trimit structuri de tip TCP către clienți în
același mod descris anterior(prima oara dimensiunea - 4 în acest caz, urmat
de structura cu mesajul în ea - exit in acest caz).

    Daca se primesc mesaje de pe socket-ii clientilor, verific ce fel de
mesaj s-a primit(subscribe/unsubscribe/exit). Pentru subscribe, in functie de
valoarea avuta de SF, adaug topic-ul la vectorul de topic-uri cu abonati(daca
nu exista deja) sau la vectorul de topic-uri cu abonati cu SF = 1. Daca
clientul se aboneaza pentru prima oara, il adaug si pe el in lista clientilor
cu abonati. Pentru unsubscribe, elimin clientul din vectorul clientilor abonati.

-------------------------------------------------------------------------------
                                subscriber
    Trimit catre server o structura de tip tcp ce contine id-ul clientului
care s-a conectat.
    In cazul unui mesaj de la tastatura, verific ce fel de mesaj e pentru 
a afisa corespunzator si trimit catre server structura ce contine tipul
de comanda(subscribe/unsubscribe). In cazul primirii "exit", inchid socket-ul
si clientul, anuntand server-ul prin trimiterea unei structuri cu mesajul
exit.
    Daca se primeste mesaj de la server, primesc cate un octet pana cand
am primit delimitatorul pentru lungimea mesajului : END. Dupa ce acesta
a fost primit, iau lungimea mesajului din buffer-ul extras. Cer mesaje
de la server de aceasta dimensiune, pana cand dimensiunea primita(retinuta prin
adunarea cu valoarea intoarsa de apelul lui recv) este egala cu dimensiunea
extrasa mai sus(stiu ca s-a primit tot mesajul).Afisez de fiecare data
buffer-ul extras. 

-------------------------------------------------------------------------------

    Astfel, conform descrierii de mai sus, protocolul pentru incadrarea
mesajelor consta in trimiterea dimensiunii mesajului ce va trebui afisat de
catre client urmat de delimitatorul END si apoi a structurii care are in campul
message mesajul ce va trebui afisat. Aceasta structura are acest camp pe prima
pozitie, iar cand primesc mesaje in subscriber, acesta va fi citit prima oara.
Odata cu primirea delimitatorului END stiu ca lungimea mesajului a fost trimisa
si ca urmeaza primirea lui. Primesc pana cand dimensiunea totala primita
este egala cu cea data anterior. Stiu, astfel, ca s-a primit tot mesajul.