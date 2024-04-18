[main-file-ref]: main.cpp
[udp-helper-ref]: UDPHelper.cpp
[tcp-helper-ref]: TCPHelper.cpp
[output-file-ref]: OutputClass.h
[server-file-ref]: ServerClass.cpp

# **Projektová dokumentace druhého projektu do předmětu IPK 2023/24**
**Autor:** Tomáš Daniel \
**Login:** xdanie14 \
**Zadání:** IOTA - Chat Server

## Obsah
1. [Teorie](#teorie)
2. [Struktura a implementace programu](#struct)
3. [Testování](#test)
4. [Rozšíření](#bonus)
5. [Zdroje](#source)

## Teorie <a name="teorie"></a>
V této sekci se zaměřím na základní popis teorie potřebné k ilustraci a pochopení mechanismů implementovaných v rámci tohoto projektu.
Vzhledem k tomu, že tento projekt do velké míry navazuje na projekt první ([chat klient](https://git.fit.vutbr.cz/xdanie14/IPK-1.Project)), bude zde sekce věnovaná teorii velmi stručná. To stěžejní, tedy popis použitých protokolů UDP a TCP můžete nalézt v sekci [teorie prvního projektu](https://git.fit.vutbr.cz/xdanie14/IPK-1.Project#teorie) a pro tento projekt se žádným způsobem neměnila.

Jak jsem již zmiňoval, tento projekt rozširuje první projekt tím, že implementuje opačnou stranu, tedy stranu serveru. Základním rysem a požadavkem na tento server byla podpora připojení několika klientů zároveň a umožnění vzájemné komunikace mezi nimi, nehledě na transportní protokol, které používají (UDP/TCP). Proto jsem se rozhodl, stejně jako u prvního projektu, zvolit multivláknový přístup, tedy že každého připojeného klienta spravuje samostatné vlákno (nikoliv proces).

Rozdíl mezi vláknem a procesem je, že vlákna sdílí jeden a ten samý adresní prostor v rámci programu, stejně jako i další systémové prostředky. Zatímco proces je už samotná spuštěná instance programu. Tedy můžeme spustit více vláken v jednom procesu.

## Struktura a implementace programu <a name="struct"></a>
Program je logicky členěn na jednotlivé soubory a funkce/metody v rámci souborů.

Na začátku každého běhu aplikace dochází ke načtení a validovaní poskytnutých CLI (Command Line Interface) argumentů a jejich následné vložení do neseřazené mapy `data_map`, ze které si následně konstruktor třídy reprezentující server tyto data načte a inicializuje své atributy.

Ukázka načtení a vložení argumentu v [hlavním souboru][main-file-ref], udávajícího komunikační port serveru, do mapy:
```
// Parse cli args
for (int index = 1; index < argc; ++index) {
    std::string cur_val(argv[index]);
    ...
    else if (cur_val == std::string("-p"))
        data_map.insert({"port", std::string(argv[++index])});
    ...
}
```

Následuje tvorba instance samotného komunikačního serveru, který je uložen do globálního ukazatele `server` typu `Server`, zobrazeno zde:
```
// Create Server class instance
Server server_inst(data_map);
server = &server_inst;
```

K běhovým chybám je přistupováno dvojím způsobem. Pro případ nefatálních chyb nebo chyb, které nemají vliv na fungování programu, je realizován výpis obsahu chyby na standardní chybový výstup. Toto je realizováno statickou třídou [OutputClass][output-file-ref], která zároveň slouží také pro výpis zpráv přijatých a odeslaných od/na klienta/y. Řešení závažnějších chyb je realizováno pomocí vyvolání výjimek.

Příklad zpracování takové výjimky v [hlavním souboru][main-file-ref] při spouštění serveru:
```
// Try starting the server
try {
    server->start_server();
} catch (const std::logic_error& e) {
    OutputClass::out_err_intern(std::string(e.what()));
    return EXIT_FAILURE;
}
```

Po úspěšném vytvoření socketů pro oba podporované protokoly (UDP/TCP) v rámci procedury `start_server()`, dochází v vytvoření pomocného [jthread](https://en.cppreference.com/w/cpp/thread/jthread) vlákna, představeného v rámci standardu C++20. Jmenovitě se jedná o vlákno `accept_thread` deklarované v rámci třídy `Server`. Toto vlákno se stará o přijimání nových klientů, kteří mají zájem služby serveru využít.

Určitou výzvu při implementaci představovalo zajištění mezivláknové synchronizace a zabránění konfliktů při čtení a zápisu do vnitřních attributů třídy a struktury `ServerClient`, která reprezentuje právě jedno připojeného klienta. Zejména pak fronty zpráv čekající na odeslání `msg_queue`, separátní pro každého připojeného klienta. Pro zajištění tohoto jsou v programu použity [mutexy](https://en.cppreference.com/w/cpp/thread/mutex), [podmínečné proměnné](https://en.cppreference.com/w/cpp/thread/condition_variable) (anglicky conditional variables) a [atomické proměnné](https://en.cppreference.com/w/cpp/atomic/atomic).

Po úspěšném spuštění serveru a připojení klientů, je chování serveru pro tyto klienty individuální a závislé na zprávách od nich přijatých a to v souladu s definovaných konečným automatem ze [zadání projektu](https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/media/branch/master/Project%202/iota/diagrams/protocol_fsm_server.svg).

Možností pro ukončení spojení serveru s klientem je několik:
1. Uživatel se rozhodne ukončit program (server) zasláním interrupt signálu (`CTRL+c`), kdy dojde k odpojení všech právě připojených klientů
2. Server podle své vnitřní logiky rozhodne u ukončení spojení s daným klientem (vyvolání metody *session_end(...);*) s argumentem ve formě ukazatele na konkrétního uživatele
3. Server obdrží od klienta `BYE` zprávu
4. **(TCP Specific)** Klient uzavře navázané spojení (socket) ještě před úspěšnou autentizací

Určitou výzvu pro výše zmíněné představovala skutečnost, že jedinou možností pro ukončení celého serveru je zaslání interrupt signálu (`CTRL+c`) ze strany uživatele. Tento signál je ovšem přijat a zpracováván v hlavním souboru [main.cpp][main-file-ref], kde metoda třídy `Server`, `stop_server()`, která je při tomto signálu z hlavního souboru vyvolána, po plošném zaslání `BYE` zpráv všem připojeným uživatelů čeká až všechny klientská vlákna dokončí svůj běh a až poté je celý program ukončen.

Ukázka tohoto mechanismu v [ServerClass.cpp][server-file-ref]:
```
void Server::stop_server () {
    ...
    // Wait for all clients threads to finish
    for (auto& thread : this->client_threads)
        thread.join();
    ...
}
```

Moje implementace v současné době podporuje pouze jedno autentizované spojení na jednoho klienta. Důvodem není ani tak usnadnění implementace, jako spíše bezpečnostní aspekt. Jelikož v současném stavu věcí je veškerá komunikace mezi klientem a serverem nešifrovaná a tudíž nezabezpečená, mohlo by ze strany útočníka dojít k tzv. reply útoku, kdy by útočník zachytil `secret` hodnotu (v tomto kontextu heslo) skutečného uživatele z jeho `AUTH` zprávy a to následně mohl použít a vydávat se za onoho uživatele.

## Testování <a name="test"></a>
### Testovací prostředí
Uskutečnění níže popsaných testů probíhalo v domácím prostředí v rámci lokální sítě `WLAN`, prostřednictvím internetového protokolu `IPv4`. V době testování se v síti nacházely dva následující aktivní síťové prvky:
1. Notebook hostující testovanou aplikaci `ipk24chat-server`
2. Kabelový modem [CBN CH7465](https://pics.vodafone.cz/2/kabel/compal_ch7465lg/Compal_CH7465_podrobnynavod.pdf)

V případě notebooku se jednalo o [Samsung Galaxy Book2 Pro 360](https://www.samsung.com/hk_en/computers/galaxy-book/galaxy-book2-pro-360-15inch-i7-16gb-1tb-np950qed-ka1hk/#specs), model **950QED**.

#### Systémové detaily zařízení
1. **Název operačního systému:** Microsoft Windows 11 Home
2. **Verze operačního systému:** 10.0.22631 Build 22631
3. **Výrobce operačního systému:** Microsoft Corporation
4. **Výrobce zařízení:** SAMSUNG ELECTRONICS CO., LTD.
4. **Typ systému:** x64-based PC
5. **Síťová karta:**  Intel(R) Wi-Fi 6E AX211 160MHz
    * **Connection Name:** WiFi
    * **DHCP Enabled:**    Yes

#### Testovací prostředí
Testování probíhalo v rámci hostujícího notebooku v prostředí Windows Subsystem for Linux (`WSL`), ve kterém byla spuštěna Linuxová distribuce **Kali Linux** (*Release:* 2023.4; *Codename:* kali-rolling). Pro simulování druhého účastníka komunikace, tedy klienta, posloužil implementovaný klient z první úlohy.

**Pokud nebude v rámci jednotlivých testů uvedeno jinak, je za testovací prostředí implicitně považováno výše uvedené prostředí.**

### Test ukončení programu uživatelem kombinací `CTRL+c` - žádný připojený klient
* Popis testu: Ověření reakce programu na interrupt signál vyvolaný uživatelem
* Důvody testování: Požadováno dle zadání
* Způsob testování: Uživatel na začátku běhu programu provede klávesovou zkratku `CTRL+c`
* Očekávaný výstup:
    * Uvolnění veškeré alokované paměti a korektní ukončení programu s návratovou hodnotou rovnou 0
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ valgrind ./ipk24chat-server -l 127.0.0.1 -p 4567
        ==12368== Memcheck, a memory error detector
        ==12368== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
        ==12368== Using Valgrind-3.20.0 and LibVEX; rerun with -h for copyright info
        ==12368== Command: ./ipk24chat-server -l 127.0.0.1 -p 4567
        ==12368==
        ^C==12368==
        ==12368== HEAP SUMMARY:
        ==12368==     in use at exit: 0 bytes in 0 blocks
        ==12368==   total heap usage: 39,967 allocs, 39,967 frees, 6,946,394 bytes allocated
        ==12368==
        ==12368== All heap blocks were freed -- no leaks are possible
        ==12368==
        ==12368== For lists of detected and suppressed errors, rerun with: -s
        ==12368== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ echo $?
        0
        ```

### Test komunikace mezi uživateli
* Popis testu: Ověření schopnosti programu zajistit připojeným klientům vzájemnou komunikaci
* Důvody testování: Základní funkcionalita
* Způsob testování: K serveru se připojí dva testovací klienti (jeden pomocí UDP a druhý pomocí TCP), následně dojde ke vzájemné výměně zpráv mezi těmito klienty.
* Očekávaný výstup:
    * Viditelná vzájemná výměna zpráv mezi zůčastněnými stranami.
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ ./ipk24chat-server -l 127.0.0.1 -p 4567
        RECV: 127.0.0.1:33948 | AUTH                                          (příchozí AUTH od prvního klienta[TCP])
        SENT: 127.0.0.1:33948 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        SENT: 127.0.0.1:33948 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        RECV: 127.0.0.1:39198 | AUTH                                          (příchozí AUTH od druhého klienta[UDP])
        SENT: 127.0.0.1:39198 | CONFIRM                                       (zaslání potvrzení přijetí druhému klientovi)
        SENT: 127.0.0.1:39198 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        RECV: 127.0.0.1:39198 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:39198 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        SENT: 127.0.0.1:33948 | MSG                                           (--||--)
        RECV: 127.0.0.1:39198 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        RECV: 127.0.0.1:39198 | MSG                                           (MSG od UDP klienta určená pro ostatní členy default kanálu)
        SENT: 127.0.0.1:39198 | CONFIRM                                       (zaslání potvrzení přijetí UDP klientovi)
        SENT: 127.0.0.1:33948 | MSG                                           (zaslání MSG od UDP klienta TCP klientovi (jediný další připojený))
        RECV: 127.0.0.1:33948 | MSG                                           (MSG od TCP klienta určená pro ostatní členy default kanálu)
        SENT: 127.0.0.1:39198 | MSG                                           (zaslání MSG od TCP klienta UDP klientovi (jediný další připojený))
        RECV: 127.0.0.1:39198 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        ```

### Test ukončení programu uživatelem kombinací `CTRL+c` - několik připojených klientů
* Popis testu: Ověření reakce programu na interrupt signál vyvolaný uživatelem v momentě dvou připojených klientů
* Důvody testování: Základní funkcionalita
* Způsob testování: K serveru se připojí dva testovací klienti (jeden pomocí UDP a druhý pomocí TCP), následně uživatel serveru provede klávesovou zkratku `CTRL+c`
* Očekávaný výstup:
    * `BYE` zpráva zaslaná serverem oběma připojeným klientům, kdy server vyčká s ukončením do momentu, kdy mu UDP klient doručí `CONFIRM` zprávu. Poté následuje korektní ukončení programu s návratovou hodnotou rovnou 0
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ ./ipk24chat-server -l 127.0.0.1 -p 4567
        RECV: 127.0.0.1:46036 | AUTH                                          (příchozí AUTH od prvního klienta[TCP])
        SENT: 127.0.0.1:46036 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        SENT: 127.0.0.1:46036 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        RECV: 127.0.0.1:49914 | AUTH                                          (příchozí AUTH od druhého klienta[UDP])
        SENT: 127.0.0.1:49914 | CONFIRM                                       (zaslání potvrzení přijetí druhému klientovi)
        SENT: 127.0.0.1:49914 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        RECV: 127.0.0.1:49914 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:49914 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        SENT: 127.0.0.1:46036 | MSG                                           (--||--)
        RECV: 127.0.0.1:49914 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        ^CSENT: 127.0.0.1:46036 | BYE                                         (zaslání BYE zprávy TCP klientovi)
        SENT: 127.0.0.1:49914 | BYE                                           (zaslání BYE zprávy UDP klientovi)
        RECV: 127.0.0.1:49914 | CONFIRM                                       (potvrzení přijetí ze strany klienta => ukončení programu)

        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ echo $?
        0
        ```

### Test odpojení uživatele
* Popis testu: Ověření reakce programu na odpojení klienta
* Důvody testování: Základní funkcionalita
* Způsob testování: K serveru se připojí dva testovací klienti (jeden pomocí UDP a druhý pomocí TCP), následně dojde k odpojení UDP uživatele.
* Očekávaný výstup:
    * Zbývající připojení klienti v rámci stejného kánalu obdrží od serveru zprávu oznamující odpojení UDP klienta.
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ ./ipk24chat-server -l 127.0.0.1 -p 4567
        RECV: 127.0.0.1:44118 | AUTH                                          (příchozí AUTH od prvního klienta[TCP])
        SENT: 127.0.0.1:44118 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        SENT: 127.0.0.1:44118 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        RECV: 127.0.0.1:41173 | AUTH                                          (příchozí AUTH od druhého klienta[UDP])
        SENT: 127.0.0.1:41173 | CONFIRM                                       (zaslání potvrzení přijetí druhému klientovi)
        SENT: 127.0.0.1:41173 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        RECV: 127.0.0.1:41173 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:41173 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        SENT: 127.0.0.1:44118 | MSG                                           (--||--)
        RECV: 127.0.0.1:41173 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        RECV: 127.0.0.1:41173 | BYE                                           (BYE zpráva signalizující serveru, že se tento klient odpojil)
        SENT: 127.0.0.1:41173 | CONFIRM                                       (zaslání potvrzení přijetí končícímu klientovi)
        SENT: 127.0.0.1:44118 | MSG                                           (MSG všem klientům v default kanálu oznamující odpojení UDP člena (TCP klient je jediný další připojený))
        ```

### Test reakce na nedoručenou `BYE` zprávu pro `UDP` klienta
* Popis testu: Ověření, že server korektně reaguje na nepřijatou `CONFIRM` zprávu od připojeného UDP klienta a pokouší se zprávu zaslat opakovaně
* Důvody testování: Charakteristika chování u UDP klientů
* Způsob testování: K serveru se připojí testovací UDP klient. Následně se tento klient odmlčí.
* Očekávaný výstup:
    * Opakované zaslání požadované zprávy a následné označení klienta za neaktivního a ukončení spojení s ním.
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ ./ipk24chat-server -l 127.0.0.1 -p 4567
        RECV: 127.0.0.1:56883 | AUTH                                          (příchozí AUTH od UDP klienta)
        SENT: 127.0.0.1:56883 | CONFIRM                                       (zaslání potvrzení přijetí klientovi)
        SENT: 127.0.0.1:56883 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        RECV: 127.0.0.1:56883 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:56883 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        RECV: 127.0.0.1:56883 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        ^CSENT: 127.0.0.1:56883 | BYE                                         (zaslání BYE zprávy UDP klientovi [server byl ukončen])
        SENT: 127.0.0.1:56883 | BYE                                           (opětovné zaslání BYE, jelikož klient nepotvrdil přijetí na první pokus)
        SENT: 127.0.0.1:56883 | BYE                                           (--||--)
        SENT: 127.0.0.1:56883 | BYE                                           (--||--)
        ERR: No response from client, ending connection                     (dosaženo vnitřní podmíky serveru a ukončení spojení s tímto klientem)
        ```

### Test připojení jednoho z uživatelů do nového kanálu
* Popis testu: Ověření reakce programu na odpojení klienta z výchozího kanálu a připojení do nového kanálu
* Důvody testování: Základní funkcionalita
* Způsob testování: K serveru se připojí dva testovací klienti (jeden pomocí UDP a druhý pomocí TCP), následně dojde k odpojení TCP uživatele z výchozího kanálu a připojení do nového kanálu.
* Očekávaný výstup:
    * Zbývající připojení klienti v rámci stejného kánalu obdrží od serveru zprávu oznamující odpojení klienta.
* Skutečný výstup:
    * Server:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/2.Project]
        └─$ ./ipk24chat-server -l 127.0.0.1 -p 4567
        RECV: 127.0.0.1:59374 | AUTH                                          (příchozí AUTH od prvního klienta[TCP])
        SENT: 127.0.0.1:59374 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        SENT: 127.0.0.1:59374 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        RECV: 127.0.0.1:59067 | AUTH                                          (příchozí AUTH od druhého klienta[UDP])
        SENT: 127.0.0.1:59067 | CONFIRM                                       (zaslání potvrzení přijetí druhému klientovi)
        SENT: 127.0.0.1:59067 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení)
        RECV: 127.0.0.1:59067 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:59067 | MSG                                           (MSG všem klientům v default kanálu oznamující připojení nového člena)
        SENT: 127.0.0.1:59374 | MSG                                           (--||--)
        RECV: 127.0.0.1:59067 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        RECV: 127.0.0.1:59067 | JOIN                                          (JOIN zpráva signalizující žádost UDP klienta o připojení do nového kanálu)
        SENT: 127.0.0.1:59067 | CONFIRM                                       (zaslání potvrzení přijetí UDP klientovi)
        SENT: 127.0.0.1:59067 | REPLY                                         (REPLY zaslaná tomuto klientovi signalizující úspěšné připojení do nového kanálu)
        SENT: 127.0.0.1:59374 | MSG                                           (MSG zpráva zbylému TCP klientu v default kanálu oznamující odpojení UDP klienta z tohoto kanálu)
        RECV: 127.0.0.1:59067 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        SENT: 127.0.0.1:59067 | MSG                                           (MSG všem klientům v kanálu, kam se právě připojil UDP klient, oznamující jeho připojení)
        RECV: 127.0.0.1:59067 | CONFIRM                                       (potvrzení přijetí ze strany klienta)
        ```

## Rozšíření <a name="bonus"></a>
V rámci tohoto projektu jsem žádná rozšíření funkcionality nad rámec zadání **neprováděl**.

## Bibliografie <a name="source"></a>

Přispěvatelé Wikipedie, Vlákno (informatika) [online], Wikipedie: Otevřená encyklopedie, c2023, Datum poslední revize 15. 01. 2023, 17:39 UTC, [citováno 16. 04. 2024] <https://cs.wikipedia.org/w/index.php?title=Vl%C3%A1kno_(informatika)&oldid=22344963>

Přispěvatelé Wikipedie, Proces (informatika) [online], Wikipedie: Otevřená encyklopedie, c2024, Datum poslední revize 15. 04. 2024, 12:20 UTC, [citováno 16. 04. 2024] <https://cs.wikipedia.org/w/index.php?title=Proces_(informatika)&oldid=23825584>
