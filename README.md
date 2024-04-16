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

Jak jsem již zmiňoval, tento projekt rozširuje projekt první tím, že implementuje opačnou stranu, tedy stranu serveru. Základním rysem a požadavkem na tento server byla podpora připojení několika klientů zároveň a umožnění vzájemné komunikace mezi nimi, nehledě na transportní protokol, který používají (UDP/TCP). Proto jsem se rozhodl, stejně jako u prvního projektu, zvolit multi-vláknový přístup, tedy že každého připojeného klienta spravuje samostatné vlákno (nikoliv proces).

Rozdíl mezi vláknem a procesem je, že vlákna sdílí jeden a ten samý adresní prostor v rámci programu, stejně tak i další systémové prostředky. Zatímco proces je už samotná spuštěná instance programu. Tedy můžeme spustit více vláken v jednom procesu.

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

Následuje tvorba instance samotného komunikačního serveru, který je uložen do globálního ukazatele typu `Server`, zobrazeno zde:
```
// Create Server class instance
Server server_inst(data_map);
server = &server_inst;
```

K běhovým chybám je přistupováno dvojím způsobem. Pro případ nefatálních chyb nebo chyb, které nemají vliv na fungování programu je realizován výpis obsahu chyby na standardní chybový výstup. Toto je realizováno statickou třídou [OutputClass][output-file-ref], která zároveň slouží také pro výpis zpráv přijatých a odeslaných od/na klienta/y. Řešení závažnějších chyb je realizováno pomocí vyvolání výjimek.

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

Po úspěšném vytvoření socketů pro oba podporované protokoly (UDP/TCP) v rámci procedury `start_server()` dochází v vytvoření pomocného [jthread](https://en.cppreference.com/w/cpp/thread/jthread) vlákna, představeného v rámci standardu C++20. Jmenovitě se jedná o vlákno `accept_thread` deklarované v rámci třídy `Server`. Toto vlákno se stará o přijimání nových klientů, kteří mají zájem služby serveru využít.

Určitou výzvu při implementaci představovalo zajištění mezivláknové synchronizace a zabránění konfliktů při čtení a zápisu do vnitřních attributů třídy a struktury `ServerClient`, která reprezentuje právě jedno připojeného klienta. Zejména pak fronty zpráv čekající na odeslání `msg_queue`, separátní pro každého připojeného klienta. Pro zajištění tohoto jsou v programu použity [mutexy](https://en.cppreference.com/w/cpp/thread/mutex), [podmínečné proměnné](https://en.cppreference.com/w/cpp/thread/condition_variable) (anglicky conditional variables) a [atomické proměnné](https://en.cppreference.com/w/cpp/atomic/atomic).

Po úspěšném spuštění serveru a připojení klientů, je chování serveru pro tyto klienty individuální a závislé na zprávách od nich přijatých a to v souladu s definovaných konečným automatem ze [zadání projektu](https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/media/branch/master/Project%202/iota/diagrams/protocol_fsm_server.svg).

Možností pro ukončení spojení serveru s klientem je několik:
1. Uživatel se rozhodne ukončit program (server) zasláním interrupt signálu (`CTRL+c`), kdy dojde k odpojení všech právě připojených klientů
2. Server podle své vnitřní logiky rozhodne u ukončení spojeni (vyvolání metody *session_end(...);*) s argumentem ve formě ukazatele na konkrétního uživatele
3. Server obdrží od klienta `BYE` zprávu
4. **(TCP Specific)** Klient uzavře navázané spojení ještě před úspěšnou autentizací

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
Testování probíhalo v rámci hostujícího notebooku v prostředí Windows Subsystem for Linux (`WSL`), ve které byla spuštěna Linuxová distribuce **Kali Linux** (*Release:* 2023.4; *Codename:* kali-rolling). Pro simulování druhého účastníka komunikace, tedy klienta, posloužil implementovný klient z první úlohy.

**Pokud nebude v rámci jednotlivých testů uvedeno jinak, je za testovací prostředí implicitně považováno výše uvedené prostředí.**

**Symbol `->` značí příchozí zprávu na server a symbol `<-` naopak značí odchozí zprávu z serveru ke klientovi**

### Test chybějícího povinného argumenty programu
* Popis testu: Uživatel vynechá povinný argument spouštění programu *-t*, pro specifikaci typu komunikačního protokolu
* Důvody testování: Ověření schopnosti programu validovat uživatelské vstupy
* Způsob testování: Uživatel argument během spouštění vynechá
* Vstupy:
    * `./ipk24chat-client -s 127.0.0.1 -p 4567`
* Očekávaný výstup:
    * `ERROR: Compulsory values are missing` společně s návratovou hodnotou != 0
* Skutečný výstup:
```
┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
└─$ ./ipk24chat-client -s 127.0.0.1 -p 4567
ERROR: Compulsory values are missing

┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
└─$ echo $?
1
```

### Test nevalidní hodnoty pro hostname
* Popis testu: Uživatel zadá neexistující název serveru
* Důvody testování: Ověření schopnosti programu validovat uživatelské vstupy
* Způsob testování: Uživatel zadá špatnou hodnotu pro název serveru
* Vstupy:
    * `./ipk24chat-client -t udp -s NONSENSE -p 4567`
    * `./ipk24chat-client -t tcp -s NONSENSE -p 4567`
* Očekávaný výstup:
    * `ERROR: Unknown or invalid hostname provided` společně s návratovou hodnotou != 0
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s NONSENSE -p 4567
        ERROR: Unknown or invalid hostname provided

        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        1
        ```
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s NONSENSE -p 4567
        ERROR: Unknown or invalid hostname provided

        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        1
        ```

### Test ukončení programu uživatelem kombinací `CTRL+c`
* Popis testu: Ověření reakce programu na interrupt signál vyvolaný uživatelem
* Důvody testování: Požadováno dle zadání
* Způsob testování: Uživatel na začátku běhu programu provede klávesovou zkratku `CTRL+c`
* Očekávaný výstup:
    * `BYE` zpráva zaslaná serveru a korektní ukončení programu s návratovou hodnotou rovnou 0
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
        ^C                                                      -> \xff\x00\x00 [BYE Message]
                                                                <- \x00\x00\x00 [CONFIRM Message]
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        0
        ```
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        ^C                                                      -> BYE\r\n
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        0
        ```

### Test ukončení programu uživatelem kombinací `CTRL+d`
* Popis testu: Ověření reakce programu na interrupt signál vyvolaný uživatelem
* Důvody testování: Požadováno dle zadání
* Způsob testování: Uživatel na začátku běhu programu provede klávesovou zkratku `CTRL+d`
* Očekávaný výstup:
    * `BYE` zpráva zaslaná serveru a korektní ukončení programu s návratovou hodnotou rovnou 0
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
                                                                -> \xff\x00\x00 [BYE Message]
                                                                <- \x00\x00\x00 [CONFIRM Message]
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        0
        ```
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
                                                                -> BYE\r\n
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ echo $?
        0
        ```

### Test reakce na nedoručenou `REPLY` zprávu pro `AUTH` zprávu (**UDP Specific**)
* Popis testu: Ověření, že klient korektně reaguje na nedodanou, ovšem vyžadovanou, `REPLY` zprávu
* Důvody testování: Charakteristika UDP protokolu, jeden z možných edge-casů
* Způsob testování: Testovací server [mockUDPserver.py][mockudp-file-ref] při reakci na přijatou `AUTH` nezašle klientem očekávanou `REPLY` zprávu
* Očekávaný výstup:
    * Tisk chybové zprávy oznamující že server nereaguje a ukončení spojení ve formě zaslané `BYE` zprávy
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> \x02\x00\x00tom\x00tom\x00tom\x00 [AUTH Message]
                                                                <- \x00\x00\x00 [CONFIRM Message]
        ERR: Timeout for server response, ending connection     -> \xff\x00\x01 [BYE Message]
                                                                <- \x00\x00\x01 [CONFIRM Message]
        ```

### Test reakce na negativní `REPLY` zprávu pro `AUTH` zprávu
* Popis testu: Ověření a demonstrace chování klienta na negativní `REPLY` zprávu pro zaslanou `AUTH` zprávu
* Důvody testování: Jedna z běžných situací, které v rámci užívání mohou nastat
* Způsob testování: Uživatel zašle `AUTH` na kterou přijímá negativní `REPLY` odpověď
* Očekávaný výstup:
    * Tisk přijaté `REPLY` zprávy, na kterou může uživatel reagovat několika způsoby:
        * Ukončení spojení
        * Opětovnému zaslání `AUTH` zprávy
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> \x02\x00\x00tom\x00tom\x00tom\x00 [AUTH Message]
                                                                <- \x00\x00\x00 [CONFIRM Message]
                                                                <- \x01\x00\x00\x00\x00\x00nene\x00 [NEGATIVE REPLY]
        Failure: nene                                           -> \x00\x00\x00 [CONFIRM Message]
        -- MOZNOST PRO UZIVATELE ROZHODNOUT SE, CO DAL --
        ```
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> AUTH tom AS tom USING tom\r\n [AUTH Message]
                                                                <- REPLY NOK IS nene\r\n [NEGATIVE REPLY]
        Failure: nene
        -- MOZNOST PRO UZIVATELE ROZHODNOUT SE, CO DAL --
        ```

### Test reakce na neočekávanou zprávu v `OPEN` stavu
* Popis testu: Ověření reakce klienta na neočekávanou zprávu pro jeho aktuální stav
* Důvody testování: Jedna z běžných situací, které v rámci užívání mohou nastat
* Způsob testování: Klient úspěšně naváže spojení se serverem a přejde do `OPEN` stavu, ve kterém od serveru přijímá neočekávanou a tedy chybnou `AUTH` zprávu
Očekávaný výstup:
    * Tisk přijaté `REPLY` zprávy, následované výpisem chybové zprávy informující o přijetí zprávy nevalidní pro aktuální klientův stav a následné přepnutí do `ERROR` stavu, zaslání `BYE` a ukončení spojení
* Skutečný výstup:
    * UDP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> \x02\x00\x00tom\x00tom\x00tom\x00 [AUTH Message]
                                                                <- \x00\x00\x00 [CONFIRM Message]
                                                                <- \x01\x00\x00\x01\x00\x00jojo\x00 [POSITIVE REPLY]
        Success: jojo                                           -> \x00\x00\x00 [CONFIRM Message]
                                                                <- \x02\x00\x01tom\x00tom\x00tom\x00 [(Unexpected) AUTH Message]
                                                                -> \x00\x00\x01 [CONFIRM Message]
        ERR: Unexpected message received                        -> \xfe\x00\x01tom\x00Unexpected message received\x00 [ERROR Message]
                                                                <- \x00\x00\x01 [CONFIRM Message]
        ```
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> AUTH tom AS tom USING tom\r\n [AUTH Message]
                                                                <- REPLY OK IS jojo\r\n [POSITIVE REPLY]
        Success: jojo                                           <- AUTH tom AS tom USING tom\r\n [AUTH Message]
        ERR: Unexpected message received                        -> ERR FROM tom IS Unexpected message received\r\n [ERROR Message]
                                                                -> BYE\r\n
        ```

### Test reakce na přijetí vícera zpráv najednou (**TCP Specific**)
* Popis testu: Ověření, že je klient schopen rozeznat a zpracovat vícero zpráv obsažených v bufferu z funkce *recv()*
* Důvody testování: Charakteristika TCP protokolu, jeden z možných edge-casů
* Způsob testování: Testovací server [mockTCPserver.py][mocktcp-file-ref] při reakci na přijatou `AUTH` zprávu odešle klientovi zpět dvě zprávy `REPLY` a `MSG` v rámci jednoho zaslání
* Očekávaný výstup:
    * Tisk obou přijatých zpráv na standardní výstup
* Skutečný výstup:
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> AUTH tom AS tom USING tom\r\n [AUTH Message]
                                                                <- REPLY OK IS VSE JE OK\r\nMSG FROM tom IS ahoj svete\r\n [POSITIVE REPLY + MSG Message]
        Success: VSE JE OK
        tom: ahoj svete
        ```

### Test reakce na přijetí nekompletní zprávy (**TCP Specific**)
* Popis testu: Částečně navazuje na předchozí test, ověření že klient detekuje nekompletní zprávu od serveru a počká na zaslání zbytku od serveru
* Důvody testování: Charakteristika TCP protokolu, jeden z možných edge-casů
* Způsob testování: Testovací server [mockTCPserver.py][mocktcp-file-ref] při reakci na přijatou `AUTH` zprávu odešle klientovi první část zprávy `REPLY` a po 2 sekundách zbylou část
* Očekávaný výstup:
    * Tisk přijaté zprávy v jednom celku
* Skutečný výstup:
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> AUTH tom AS tom USING tom\r\n [AUTH Message]
                                                                <- REPLY OK I
                                                                ...wait 2 secs...
                                                                <- S VSE JE OK\r\n
        Success: VSE JE OK
        ```

### Test reakce na přijetí case-insensitive zprávy (**TCP Specific**)
* Popis testu: Ověření že klient na přijaté zprávy pohlíží jako na case-insensitive
* Důvody testování: Charakteristika TCP protokolu, jeden z možných edge-casů
* Způsob testování: Testovací server [mockTCPserver.py][mocktcp-file-ref] při reakci na přijatou `AUTH` zprávu odešle klientovi `REPLY` zprávu s nahodilou kombinací velkých a malých písmen (`RePlY Ok iS VsE je OK\r\n`)
* Očekávaný výstup:
    * Tisk přijaté zprávy bez jakékoliv chybové zprávy
* Skutečný výstup:
    * TCP:
        ```
        ┌──(dandys㉿DandysComp)-[~/Dandys-Kingdom/IPK-Projects/1.Project]
        └─$ ./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
        /auth tom tom tom                                       -> AUTH tom AS tom USING tom\r\n [AUTH Message]
                                                                <- RePlY Ok iS VsE je OK\r\n [POSITIVE REPLY Message]
        Success: VsE je OK
        ```

## Rozšíření <a name="bonus"></a>
V rámci tohoto projektu jsem žádná rozšíření funkcionality nad rámec zadání **neprováděl**.

## Bibliografie <a name="source"></a>

Přispěvatelé Wikipedie, Vlákno (informatika) [online], Wikipedie: Otevřená encyklopedie, c2023, Datum poslední revize 15. 01. 2023, 17:39 UTC, [citováno 16. 04. 2024] <https://cs.wikipedia.org/w/index.php?title=Vl%C3%A1kno_(informatika)&oldid=22344963>

Přispěvatelé Wikipedie, Proces (informatika) [online], Wikipedie: Otevřená encyklopedie, c2024, Datum poslední revize 15. 04. 2024, 12:20 UTC, [citováno 16. 04. 2024] <https://cs.wikipedia.org/w/index.php?title=Proces_(informatika)&oldid=23825584>
