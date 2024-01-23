Continut arhiva:
clientFolder: folder utilizat in cazul functiei Download
serverFolder: folder care functioneaza ca un /home/student pentru server
logger.txt : fisier logger
Makefile
client.c
server.c

Mod de functionare:
Clientul trimite comanda catre server la "foc automat" ( totul se desfasoara intr-o bucla while)
in care fiecare client este gestionat de cate un thread de conexiune.
Comenzile sunt intalnite la nivelul serverului prin intermediul functiilor de tipul
execute_[nume_comanda], in timp ce thread-ul specializat de update foloseste variabile conditionate
pentru a "se trezi", respectiv "a adormi".Atunci cand "se trezeste" executa functii specifice, de update,
upload, delete...
De asemenea se foloseste si epol pentru asteptare concurentiala, utilizat pentru a observa modificarile aduse pe socket-ul server,
dar si pe signalfd ( la primirea semnalelor SIGTERM si SIGINT , se intra in handler-ul acestora de semnal, urmand sa fie utilizat in
epol).
Operatiile sunt marcate in logger(Aici exista o problema, la fflush se afiseaza de 2 ori aceeasi actiune)
Functia search verifica intai in lista de 10 cuvinte frecventate, apoi in fisier.

Structura:

1. Functii execute
2. Structura fisiere declarate global
3. Variabile de tip mutex pentru diferite operatii(update, executii in listaFisiere care duc modificari, sincronizare thread-uri)
4. Functii de sincronizare citire, scriere pe fisiere.
5. Functii de update, remove, move si add pentru structura de tip listaFisiere
6. Functie care depisteaza protocolul utilizat
7. Variabile de conditie folosite pentru adormire threads
8. Functie de constructie a vectorului cu cele mai frecvente 10 cuvinte


!!!
Se recomanda:
La download descarcarea oricarui fisier cu mentiunea ca trebuie sa contina in cale serverFolder/
La upload se poate uploada orice fisier
MOVE prezinta unele probleme, dar functioneaza cum trebuie in modul debug

