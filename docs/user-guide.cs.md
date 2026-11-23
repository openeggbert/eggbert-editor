# Uživatelská příručka Eggbert Editoru

Eggbert Editor upravuje jednotlivé soubory světů her *Speedy Blupi* / *Speedy Eggbert* a *Speedy Blupi II* / *Speedy Eggbert 2*: soubory světů `.blp` a exportované soubory `.xch`, které mají stejné rozvržení. Mění pouze bajty, které upravíte. Všechno ostatní v souboru — rezervovaná pole, nepoužité sloty objektů, text za názvem, data za koncem rozvržení — se zapíše zpět přesně tak, jak bylo načteno.

Je to editor pro jeden svět najednou. Umí vypsat světy herní instalace, vložit do ní svět a zase jej z ní odebrat a spustit hru příkazem, který zadáte; umí zkontrolovat číslování kampaně a vyměňovat světy v podobě balíčků.

## Otevření světa

**Soubor → Otevřít svět** (Ctrl+O) otevře jeden nebo více souborů, každý na vlastní kartě. Otevřou se i soubory uvedené na příkazovém řádku a také soubory přetažené do okna. Dokud není nic otevřeno, okno zobrazuje úvodní stránku s položkami **Otevřít svět**, **Nový svět**, **Vybrat adresář hry** a se světy, které jste naposledy otevřeli.

**Soubor → Nový svět** (Ctrl+N) se zeptá na hru a na soubor a zapíše svět, jakým novou misi zakládá vlastní editor hry: prázdnou mapu plné velikosti s plošinou ze tří dlaždic, čtyřmi starty, jedním pokladem a cílem, hudbou 1 a pozadím 2. Soubor se zapíše v revizi, kterou používají původní světy dané hry (1.2 u první hry, 1.3 u druhé), a otevře se se zvolenou hrou. Soubor, který je otevřený v editoru, se nenahradí.

Soubor světa nenese žádnou signaturu. Editor přečte pole revize na začátku souboru a zvolí rozvržení:

| Revize | Vrstvy | Sloty objektů | Velikost | V tomto editoru |
| --- | --- | ---: | ---: | --- |
| 1.0 | popředí | 100 | 25,668 bajtů | prohlížení a zkoumání |
| 1.1 | popředí, bigDecor | 100 | 45,668 bajtů | prohlížení a zkoumání |
| 1.2, 1.3 | popředí, bigDecor | 200 | 50,468 bajtů | úpravy a ukládání |

Odpovídající revize a velikost znamenají, že soubor je se světem *konzistentní*; nejsou důkazem. Příliš krátký soubor se otevře s viditelnými úplnými částmi a je jen pro čtení; bajty navíc za rozvržením se zachovají a nikdy se nemění; neznámá revize se otevře pouze k šestnáctkovému prohlížení. Žlutý pruh uvádí, proč je dokument jen pro čtení. **Soubor → Vlastnosti dokumentu** zobrazí úplnou strukturální zprávu.

Ne každý soubor `.blp` je svět. Ve složce `DATA` hry jsou světy soubory `world*.blp`; `demo*.blp` (záznamy), `info*.blp`, `time.blp` a `bench.blp` jsou jiná data. Záznam se pozná podle obsahu a otevře se ve vlastním okně (viz **Herní záznamy**). U ostatních nic neoznačuje, čím jsou, takže to editor nedokáže určit: takový soubor se otevře jako velmi krátký „svět“ jen pro čtení nebo pouze k šestnáctkovému prohlížení a nikdy se nemění. Obrázkové soubory her ve složce `IMAGE08` mají také příponu `.blp`; jsou to bitmapy.

### Volba hry

Soubor neříká, ke které hře patří. Když otevřete svět, který leží ve složce `DATA` některé hry, editor podle této složky hru rozpozná (stejným způsobem jako u grafiky hry), zvolí ji a použije tento adresář hry pro její grafiku a hudbu, pokud jste už pro tuto hru nějaký nevybrali; ze složky se pouze čte. U každého jiného souboru zvolte na panelu nástrojů **Speedy Blupi / Eggbert 1** nebo **Speedy Blupi II / Eggbert 2**. Volba, kterou tam provedete, se pro daný soubor na tomto počítači zapamatuje a příště má přednost před složkou. Volba platí pro jednotlivou kartu, neukládá se do souboru a nikdy nezmění jediný bajt. Určuje předvolby objektů, adaptivní štětce, rozsahy nabízené pro hudbu a region a pravidla, která používá World Doctor. Dokud hru nezvolíte, nejsou tyto funkce závislé na hře dostupné; surové úpravy dlaždic, starty, název a prohlížení fungují bez ohledu na to.

## Plátno

Ve výchozím stavu plátno kreslí svět schematicky, bez jakékoli grafiky hry: každé uložené číslo dlaždice má vlastní stálou barvu (čísla menší než -1 sdílejí jednu výstražnou barvu), dlaždice bigDecor pokrývají dvě krát dvě buňky, objekty jsou obrysové rámečky se svým typem a slotem, starty hráčů jsou očíslované kotouče a dráhy pohybu jsou přerušované čáry s koncovým rámečkem. Dokud jsou dráhy pohybu zobrazeny, jsou oba konce dráhy označeny **1** (kde objekt začíná) a **2** (kde se obrací): schematicky očíslovanými štítky, až do přibližně pěti obrazovkových pixelů na buňku, a s grafikou hry dvěma vlastními obrázky her — žlutými rámečky s čísly 1 a 2, které editory her zobrazují u pohyblivých objektů — nakreslenými na uložených pozicích přes objekt. Plátno umí kreslit i grafikou hry, kterou máte (viz *Grafika hry*).

Plátno je **animované**, stejně jako vlastní editory úrovní her: hodiny her běží rychlostí 20 tiků za sekundu. S grafikou hry se láva, voda, vítr, větráky, pružiny a ostatní animované dlaždice střídají ve svých obrázcích (většina se mění každý druhý až čtvrtý tik) a objekty mění svůj obrázek tak jako ve hrách; v obou způsobech kreslení se objekty pohybují po svých drahách s uloženými dobami cesty a čekáními. Slouží to jen k prohlížení. Plynutí času svět nikdy nezmění: soubor ukládá začátek, konec a časování každého objektu a právě ty upravujete. Stejně jako v editorech her vychází každý objekt při otevření světa ze svého začátku a objekt, který jste právě upravili, odtud vyjde znovu. Je-li animace zapnutá, lze pohybující se objekt vybrat nástrojem Objekt tam, kde se právě nachází, na buňce jeho začátku nebo na jeho uložené pozici (střed kliknuté buňky musí ležet na objektu); při vypnuté animaci na jeho uložené pozici. **Zobrazení → Animovat** animaci vypíná a zapíná; při vypnuté animaci plátno zobrazuje přesně uložený stav. Exportované obrázky vždy zobrazují uložený stav.

- Posun pohledu: prostřední tlačítko myši, Alt+tažení, kolečko myši (Shift pro vodorovný posun) nebo nástroj **Prohlížení**.
- Zvětšení: Ctrl+kolečko mění zvětšení kolem ukazatele; posuvník na pravém konci stavového řádku pokrývá celý rozsah; **Zobrazení → Přiblížit / Oddálit / Skutečná velikost / Celý svět / Celý výběr**.
- Klávesnice: šipky posouvají zvýrazněný kurzor buňky (Shift o deset buněk, Ctrl posouvá pohled); mezerník nebo Enter použije aktivní nástroj na kurzoru.
- Nabídka **Zobrazení** zapíná a vypíná mřížku, popisky hodnot, jednotlivé vrstvy, objekty, starty a dráhy.

Stavový řádek zobrazuje buňku pod ukazatelem s uloženou hodnotou a offsetem v souboru pro každou vrstvu, využití slotů objektů, revizi a zvětšení.

## Grafika hry (volitelná)

Editor neobsahuje žádnou grafiku hry a plně funguje i bez ní. Pokud některou hru máte, **Zobrazení → Adresář s grafikou hry** vám umožní vybrat její adresář (ten, který obsahuje `DATA`, `IMAGE08` a soubor `.exe` hry). Editor ukáže, co rozpoznal — adresář první hry obsahuje `DATA/demo200.blp`, adresář druhé hry `DATA/demo300.blp` — a hru můžete opravit. Pro každou hru se pamatuje jeden adresář jako místní předvolba; nikdy se neukládá do souboru světa.

Z adresáře se pouze čte, nikdy se nemění. Editor čte archy spritů (`object.blp`, `element.blp`, `explo.blp`, `blupi000.blp` až `blupi003.blp`, což jsou obyčejné bitmapy) a pozici každého spritu vyhledá ve vlastním spustitelném souboru hry. Tabulka pozic se použije jen tehdy, když odpovídá archu, ke kterému patří (výška poslední položky tabulky může být zkrácena, aby se vešla). **Zobrazení → Zpráva o grafice hry** vypíše, co bylo nalezeno a co ne.

**Zobrazení → Kreslit grafikou hry** přepíná mezi grafikou hry a schematickým kreslením. Karta používá grafiku jen tehdy, když její zvolená hra odpovídá načtenému adresáři. Obrázek pozadí světa, bigDecor, objekty, dlaždice a starty se kreslí v pořadí, jaké používají hry (obyčejné dlaždice, výtahy, potom navrchu láva, voda, vítr, větráky a pasti; ve druhé hře jdou dvě čísla dlaždic za objekty), s čistě modrou jako průhlednou barvou; vše, co grafika neposkytuje, si ponechá svůj schematický zástupný obrázek. Pozadí se dlaždicově opakuje a na plátně se posouvá dvoutřetinovou rychlostí mapy jako ve hrách; **Zobrazení → Zobrazit obrázek pozadí** jej na plátně vypne. Minimapa zůstává vždy schematická.

Kreslení grafikou hry bylo ověřeno s instalacemi her Speedy Eggbert 1 a Speedy Eggbert 2: každá dlaždice, obrázek bigDecor, objekt i start uložený v jejich světech se najde a vykreslí. Jiná vydání vyzkoušena nebyla. Pokud se něco vykreslí na špatném místě, přepněte na schematické kreslení, které ukazuje přesně to, co je uloženo, a nahlaste to prosím.

## Paleta: výběr podle obrázku

Panel **Paleta** vlevo (**Zobrazení → Paleta**) je místo, kde podle obrázku, nikoli podle čísla, vybíráte, co nástroje malují a umisťují. Má čtyři karty.

**Dlaždice.** Pruh nahoře uvádí skupiny; mřížka pod ním zobrazuje dlaždice zvolené skupiny, počínaje prázdnou buňkou. Jakmile je zvolena hra, existuje jedna skupina pro každé tlačítko vlastního editoru úrovní této hry, které nabízí dlaždice — v první hře například *Scenérie* (stromy, domy a další velké obrázky), *Skála a technické bloky*, *Budovy a dekorace* — s jejich druhy ve stejném pořadí, a za nimi *Ostatní dlaždice* (dlaždice, které nejsou v žádné skupině). *Všechny dlaždice* a *Vše z bigDecor* jsou k dispozici vždy. Najetím ukazatele na dlaždici zobrazíte její druh a číslo. Kliknutím na dlaždici jí začnete malovat: vrstva, do které patří, se stane aktivní vrstvou a vybere se Tužka, pokud už není aktivní některý nástroj pro malování dlaždic. Dlaždice, které editory her přizpůsobují sousedům (skála, tráva, sýr a tak dále), se malují adaptivním štětcem, který dělá totéž, pokud jsou vybrány z některé skupiny editoru; ve skupinách *Ostatní dlaždice* a *Všechny dlaždice* je každé číslo surovou dlaždicí.

**Objekty.** Stejným způsobem pro vše, co umí umístit editor zvolené hry: *Všechny objekty*, potom jedna skupina pro každé tlačítko editoru, které nabízí objekty, například *Výtahy, bedny a nepřátelé* nebo *Bonusy a cíl* v první hře. Klikněte na objekt a potom kliknutím na buňku mapy jej umístěte (u objektů, které se pohybují po dráze, táhněte).

**Okraje.** Adaptivní štětce okrajů zvolené hry. Kliknutím na některý jím začnete malovat.

**Svět.** Nastavení celého světa; každá volba je jeden krok zpět.

- *Velikost světa*: jedna obrazovka, široký (posouvá se vodorovně), vysoký (posouvá se svisle) nebo velký (posouvá se oběma směry).
- *Pozadí*: obrázek pozadí, který hra zobrazuje za světem. Kliknutím na některý jej použijete.
- *Hudba*: bez hudby, nebo některá ze skladeb hry. Je-li zaškrtnuto **Přehrát po výběru**, výběr skladby ji také přehraje; **Přehrát**/**Zastavit** to udělá pro označenou skladbu. Hudba her je ve formátu MIDI a editor nemá vlastní syntezátor: použije příkaz uložený pod `music/command` v souboru nastavení editoru, pokud tam nějaký je (`%f` zastupuje soubor), jinak první nainstalovaný z programů FluidSynth (se zvukovým fontem v systémovém adresáři zvukových fontů), TiMidity++ a WildMIDI. Přehrávač se hledá při spuštění editoru. Pokud žádný není, panel to oznámí. K poslechu je nutné mít zvolený adresář hry (Zobrazení → Adresář s grafikou hry), protože soubory s hudbou patří hře; její obrázky použitelné být nemusí.

Je-li načtena grafika hry a zapnuto **Zobrazení → Kreslit grafikou hry**, jsou obrázky vlastní grafikou hry a ve skupinách *Ostatní dlaždice*, *Všechny dlaždice* a *Vše z bigDecor* jsou vynechána čísla bez viditelného obrázku; jinak je každá položka barevný vzorek se svým číslem, ve stejných barvách jako schematické plátno, a jakmile je zvolena hra, panel nabídne tlačítko **Vybrat adresář hry**. Stejné obrázky se objevují ve výběru objektů na panelu nástrojů, v nabídce **Objekt → Vytvořit ověřený typ** a před každým záznamem v panelu Objekty. Výběr dlaždice, objektu nebo štětce svět nikdy nezmění; mění jej pouze nástroje a karta Svět.

Skupiny slouží k hledání, nejsou omezením: pole **ID dlaždice** na panelu nástrojů zůstává pro zadání libovolného surového čísla. Starty hráčů se umisťují nástrojem Start.

### Encyklopedie prvků

**Nápověda → Encyklopedie prvků** vypíše vše, co katalogy editoru vědí o hře aktuálního světa: každou položku dlaždic a každý objekt ze skupin editoru hry a každou rodinu adaptivních okrajů. Psaním do vyhledávacího pole filtrujete podle názvu nebo skupiny; číslo najde položky, které ukládají právě tuto hodnotu dlaždice nebo typ objektu. U zvolené položky okno ukazuje, co se pro ni ukládá (čísla dlaždic, nebo typ, kanál, ikonu, umístění a časování objektu), a doklad, o který se to opírá. **Vybrat v editoru** (nebo dvojklik) zvolí položku stejně jako kliknutí v Paletě: obyčejnou dlaždici pro Tužku, dlaždici, kterou hra přetváří podle sousedů, jako její Adaptivní štětec, objekt pro nástroj Umístit, rodinu okrajů pro Adaptivní štětec. Obrázky jsou ty, které ukazuje Paleta: vlastní obrázky hry, dokud je zapnuto **Zobrazení → Kreslit grafikou hry** a je zvolen adresář hry, jinak očíslované zástupné obrázky. Okno zůstává při práci otevřené a řídí se hrou aktuální karty.

## Úpravy dlaždic

Nástroje působí na **aktivní vrstvu** (panel nástrojů nebo **Vrstva → Upravovat popředí / Upravovat bigDecor**) a zapisují uložená čísla dlaždic; nástroj Adaptivní pracuje vždy na popředí. Hodnota -1 je prázdná buňka. Dlaždici vyberte v panelu **Paleta** nebo zadejte její číslo.

| Nástroj | Klávesa | Co dělá |
| --- | --- | --- |
| Prohlížení | H | posouvá pohled; nic neupravuje |
| Výběr | S | tažením vyberete obdélník buněk |
| Tužka | B | zapisuje ID dlaždice do buněk, přes které táhnete |
| Guma | E | zapisuje -1 pouze do aktivní vrstvy |
| Kapátko | I | převezme uloženou hodnotu pod ukazatelem jako ID dlaždice |
| Čára | L | tažením nakreslíte rovnou čáru |
| Obdélník | R | tažením nakreslíte vyplněný obdélník |
| Výplň | F | nahradí souvislou oblast stejných hodnot |
| Adaptivní | A | maluje a přepočítává tvary okrajů (viz níže) |

S výjimkou nástroje Adaptivní jsou tyto nástroje *surové*: nikdy se nedotknou druhé vrstvy ani žádného objektu. Výběr omezuje buňky, které nástroj maluje (Adaptivní může přesto přetvořit sousedy mimo výběr a příkaz Vložit jím omezen není). Nabídka **Úpravy** obsahuje příkazy Vyjmout, Kopírovat, Vložit (na kurzoru buňky), Smazat, Vybrat vše / Zrušit výběr / Invertovat výběr; nabídka **Vrstva** obsahuje Vyplnit výběr, Nahradit hodnotu, Vymazat výběr a Převrátit výběr. Vložení, které přesahuje okraj mapy, se ořízne a oznámí, kolik buněk bylo vynecháno. Dlaždice lze vkládat v rámci jedné karty nebo mezi kartami nastavenými na stejnou hru; čísla dlaždic znamenají v obou hrách něco jiného, proto jsou jiná vložení odmítnuta.

Vrstvu lze zamknout (**Vrstva → Zamknout**) nebo zobrazit samostatně (**Izolovat aktivní vrstvu**). Před úpravou, která mění více než 64 buněk, editor ukáže, kolik buněk a bajtů a které offsety se změní.

### Razítka

Razítko je kus světa, který si uchováte k opakovanému použití. Vyberte buňky a potom **Úpravy → Uložit výběr jako razítko** uloží pod zadaným názvem ohraničující obdélník výběru: obě vrstvy dlaždic a, necháte-li políčko zaškrtnuté, i objekty, které v něm začínají (s jejich dráhami). Uložení ve světě nic nezmění. **Úpravy → Umístit razítko** zobrazí uložená razítka, každé s malým schematickým obrázkem (dlaždice v barvách editoru, tečka za každý objekt; nikdy grafika hry). Zvolené razítko je pak *připravené*: na mapě se zobrazí obrys buněk, kterých by se dotklo, a sleduje ukazatel nebo klávesnicový kurzor, podle toho, co se pohnulo naposledy, a nic se nezapíše, dokud nekliknete nebo na buňce kurzoru nestisknete Enter či mezerník tam, kam má přijít jeho levý horní roh. Tím se razítko umístí jako jeden krok zpět. Esc (má-li mapa fokus) nebo zvolení libovolného nástroje razítko odloží; umístění, které je odmítnuto, jej ponechá připravené, takže můžete zkusit jiné místo. Prázdné buňky razítka jsou díry: ponechají to, co tam je. Dlaždice, které by padly mimo mapu, se oříznou a spočítají. Objekty razítka se umístí buď všechny, nebo žádný — není-li dost volných slotů nebo by některý objekt ležel mimo svět, neumístí se nic a je uveden důvod. Razítko vytvořené pod jednou hrou se neumístí do světa druhé hry. Objekty razítka jsou kopie uložených záznamů bez ohledu na jejich typ, včetně typů, pro které editor nemá předvolbu. Razítka jsou malé textové soubory ve vašem uživatelském profilu (`stamps` v datové složce editoru), jeden na razítko; soubor, který nelze přečíst jako razítko, se nenabízí.

### Nahrazení hodnoty ve všech otevřených světech

**Vrstva → Nahradit hodnotu ve všech otevřených světech** se zeptá na hodnotu a její náhradu stejně jako **Nahradit hodnotu** a potom pro každý otevřený svět ukáže, kolik buněk aktuální vrstvy tuto hodnotu obsahuje — nebo proč je svět přeskočen (jiná hra, jen pro čtení, zamčená vrstva). Tento seznam po jednotlivých světech nahrazuje dotaz, který editor jinak klade před úpravou více než 64 buněk. Po potvrzení dostane každý dotčený svět jeden vlastní krok zpět; nic se neukládá. Prohledává se celá vrstva bez ohledu na to, co je vybráno.

### Adaptivní štětec

Některé rodiny dlaždic mají několik tvarů v závislosti na sousedech: skála, trojúhelníková skála, tráva a tyče v obou hrách; cihly, zelený sliz, palác a trubky ve hře Speedy Blupi II. Vlastní editory úrovní her volí tvar automaticky. Nástroj Adaptivní dělá totéž: zapíše rodinu do buňky a potom přepočítá tvary této buňky a jejích čtyř přímých sousedů podle pravidel zvolené hry, včetně jejích náhodných variací.

Ve hře Speedy Blupi II vkládají dlaždice sýra a jeskyně okrajové kusy do prázdných buněk vedle sebe a zdroj blesku vyplní svým paprskem až čtyři prázdné buňky pod sebou; nástroj Adaptivní dělá i to. Seznam štětců nabízí také *ID dlaždice, obnovit sousedy* a *Mazat, obnovit sousedy*.

Najetím ukazatele se zobrazí každá buňka, kterou by kliknutí změnilo, včetně buněk mimo štětec. Jeden tah je jeden krok zpět a příkaz Znovu obnoví přesně tytéž dlaždice. Na rozdíl od editorů her tento nástroj nikdy nesmaže objekt. Stejně jako ve hrách se obnovují jen přímí sousedé a při otevření ani uložení světa se nic nepřepočítává.

## Název, hudba, region a starty

**Svět → Metadata** upravuje název, hudbu, region (pozadí), to, zda se svět posouvá vodorovně a svisle, a uloženou pozici posunu. **Svět → Starty hráčů** upravuje čtyři startovní pozice a směry.

- Název se ukládá jako jednotlivé bajty (Windows-1252) do pole o 100 bajtech. Hry dovolí hráči napsat 40 znaků; delší název je povolen s poznámkou. Bajty za koncem názvu zůstávají tak, jak jsou. Hry čtou název pouze ze souborů revize 1.3.
- Hudba a region jsou omezeny na hodnoty, které nabízí zvolená hra (hudba 0–10, kde 0 znamená žádnou; region 0–23 v první hře a 0–31 ve druhé). Uložená hodnota mimo tento rozsah zůstane zachována, dokud ji nezměníte.
- Svět, který se v některém směru neposouvá, má v tomto směru jednu obrazovku: 10 buněk na šířku nebo 480 pixelů na výšku.
- Nástroj **Start** (P) přesune start vybraný na panelu nástrojů na kliknutou buňku, na stejnou pozici uvnitř buňky, jakou používají editory her; kliknutí na buňku, kde start na této pozici už je, jej otočí. **Svět → Kopírovat vybraný start do všech čtyř** zkopíruje jeden start do ostatních. Hry pro jednoho hráče používají start 1.

## Objekty

Svět ukládá až 200 záznamů objektů (100 v revizích 1.0 a 1.1). Záznam, jehož typ je 0, je volný. Panel **Objekty** vypisuje používané záznamy se slotem, typem, začátkem, koncem a aktuální pozicí, kanálem a ikonou a ukazuje, kolik slotů je uloženo, obsazeno a volných.

- **Umístit** (N) vytvoří objekt zvolený v Paletě (nebo v seznamu na panelu nástrojů) na kliknuté buňce. U pohyblivých objektů stiskněte tlačítko na buňce začátku a táhněte na buňku konce. Předvolby existují jen pro typy objektů, které umí umístit vlastní editor úrovní zvolené hry, a nový objekt dostane přesně ty hodnoty, které tento editor zapisuje. Přijde do nejnižšího volného slotu; dřívější zbylé bajty slotu se nahradí. Jsou-li všechny sloty obsazené, nic se nevytvoří.
- **Objekt** (O) vybere záznam kliknutím na něj; dalším kliknutím se prochází překrývající se záznamy v pořadí slotů. Tažením jej přesunete o celé buňky; tažením červeného koncového rámečku dráhy přesunete jen konec. Přesun zachovává pozici uvnitř buňky přesně tak, jak je uložena (několik původních světů ukládá vejce o čtyři pixely výš, než je umisťují editory her; zůstanou tak, dokud čísla nezměníte). **Objekt → Posunout o buňku** (Alt+šipky) objekt postrčí, **Duplikovat** (Ctrl+D) zkopíruje záznam o jednu buňku doprava, **Smazat objekt** (Shift+Delete) uvolní slot vymazáním pouze jeho typu, jak to dělají hry.
- Panel Objekty upravuje přesná čísla: doby cesty, čekání, tři pozice, kanál a ikonu. Uložený typ, dvě neznámá pole a pole používaná za běhu se zobrazují, ale neupravují. U dvou nepřátel kreslených jako hráč (ve vrtulníku, v tanku) je kanálem barva: editory her nabízejí 11, 12 a 13.
- Záznam, jehož typ editor zvolené hry neumí umístit (typy vznikající jen při hraní nebo neznámá čísla), se zobrazuje se svými surovými hodnotami a lze jej smazat, ale ne změnit.

Limit 200 slotů je pevný limit formátu. Hry navíc potřebují volné sloty při hraní, pro výbuchy, šplouchnutí, střely a podobné efekty; panel a World Doctor upozorňují na téměř vyčerpané sloty jako na *odhad*.

### Zarovnání objektů

**Objekt → Zarovnat objekty ve výběru** přesune objekty, jejichž začátek leží ve vybraných buňkách, jako skupinu: k nejlevějšímu, nejpravějšímu, nejhornějšímu nebo nejspodnějšímu začátku mezi nimi, nebo je rovnoměrně rozmístí mezi dva krajní (alespoň tři objekty). Začátek, konec a aktuální pozice objektu se posunou o stejnou hodnotu, takže jeho dráha si zachová tvar. Jako všude jinde se přesouvají jen objekty typu ověřeného pro zvolenou hru; ostatní ve výběru zůstanou tak, jak jsou uloženy, a spočítají se ve stavovém řádku. Je to jeden krok zpět.

## Zpět, ukládání a soubory změněné na disku

Každá karta má vlastní historii Zpět/Znovu (Ctrl+Z, Ctrl+Shift+Z); každý tah, tvar, vložení, dialog nebo příkaz je jeden krok. Zpět funguje i za poslední uložení.

**Uložit** (Ctrl+S) zapíše soubor přes dočasný soubor, který původní soubor nahradí v jediném kroku, takže neúspěšné uložení ponechá originál nedotčený. Před zápisem editor porovná soubor na disku s tím, co naposledy přečetl nebo zapsal. Pokud jej něco jiného změnilo, zkrátilo, prodloužilo nebo odstranilo, nic se nezapíše a vy si vyberete mezi opětovným načtením verze z disku (pokud soubor ještě existuje), uložením své verze pod jiným názvem a zrušením. **Uložit jako** zapíše do jiného souboru, **Uložit vše** uloží každou změněnou kartu, **Vrátit k uloženému** načte znovu z disku.

Dokumenty jen pro čtení lze zkopírovat příkazem Uložit jako; kopie je bajt po bajtu shodná.

### Převod do jiné revize

**Soubor → Převést revizi** zapíše kopii aktuálního světa (s jeho neuloženými změnami) v revizi 1.0, 1.1, 1.2 nebo 1.3 do nového souboru a otevře ji. Soubor, ze kterého jste vyšli, se nemění a kopii nelze nikdy zapsat přes něj ani přes žádný soubor otevřený v editoru. Takto se také upravuje svět revize 1.0 nebo 1.1: převeďte jej do 1.2 nebo 1.3, upravte kopii a převeďte zpět, pokud je stará revize potřeba.

Hlavička, dlaždice a záznamy objektů se kopírují bajt po bajtu; mění se jen číslo revize. Při převodu nahoru se to, co starší revize neukládá, doplní tak, jak to hry vyplňují při načítání takového světa: prázdná zadní vrstva u 1.0, sto volných slotů objektů u 1.0 a 1.1. Při převodu dolů dialog vypíše, co cílová revize neumí uložit — dlaždice zadní vrstvy v 1.0, objekty nebo zbylé bajty ve slotech 100 až 199 v 1.0 a 1.1 — a převede až poté, co zaškrtnete **Převést přesto a ztratit, co cílová revize neumí uložit**. Zadní vrstva, která je prázdná, a sloty, které jsou zcela nulové, se jednoduše vynechají, takže převod na vyšší revizi lze vždy přesně vrátit. Bajty za koncem světa se přenesou. Mezi 1.2 a 1.3 se liší jen číslo; hry čtou název pouze z revize 1.3. Zkrácené soubory a neznámé revize převést nelze.

### Převod pro druhou hru

Obě hry čtou stejné rozvržení souboru a sdílejí téměř každé uložené číslo, takže svět jedné hry většinou funguje i ve druhé. **Soubor → Převést pro druhou hru** (dostupné, jakmile je zvolena hra karty) zapíše kopii pro druhou hru do nového souboru, v revizi, kterou používají původní světy této hry, a otevře ji s touto hrou zvolenou. Soubor, ze kterého jste vyšli, se nemění.

Než se cokoli zapíše, seznam ukáže každou uloženou hodnotu, kterou druhá hra nezná nebo s ní zachází jinak, s tím, jak často a kde se poprvé vyskytuje (doklad je v bublinové nápovědě každého řádku):

- **neznámé** — druhá hra pro to nemá obrázek ani kód. Převod pro první hru: dlaždice druhé hry 246 až 440, obrázky zadní vrstvy 54 až 99, typy objektů, které má jen druhá hra (barevné klíče, vosa, příšera, pásové výtahy, vznášedlo, dynamit, naváděná bomba a další), a pozadí 24 až 31. Převod pro druhou hru: pouze typ objektu 58, efekt, který první hra vytváří při hraní a který soubor světa běžně neukládá.
- **liší se** — obě hry dané číslo mají, ale zobrazují je různě: čísla dlaždic 48 až 67 (bloky v první hře; druhá hra místo nich zobrazuje dlaždici 215), v obou směrech; výtah nebo bedna kreslené obrázkem dlaždice, který druhé hře chybí; a sedm čísel pozadí, která jsou v obou hrách různými obrázky.

Když je něco neznámé, volíte mezi ponecháním těchto hodnot uložených (druhá hra je ignoruje a převod zpět obnoví svět přesně) a jejich odstraněním (tyto buňky se vyprázdní, tyto sloty objektů se uvolní, pozadí se nastaví na 2). Hodnoty, které se pouze liší, se nikdy nemění. Ničeho jiného se převod nedotkne: kromě čísla revize je kopie bajt po bajtu shodná se zdrojem.

### Sloučení změn z jiné kopie

Když byly dvě kopie světa upravovány odděleně, **Soubor → Sloučit změny** přenese změny druhé kopie do aktuální karty. Zvolíte společný základ, ze kterého obě kopie vznikly, a druhou kopii; aktuální karta se svými neuloženými změnami je vaše strana. Všechny tři musí být úplné světy stejné revize a délky.

Sloučení pracuje prvek po prvku — jedna hodnota hlavičky, jedna buňka, jeden celý záznam objektu: co změnila jen druhá kopie, se převezme, co jste změnili jen vy, zůstane, a co obě strany změnily stejně, se prostě ponechá. Prvky, které obě strany změnily různě, se vypíší s názvem, offsetem a oběma verzemi; zaškrtněte ty, které se mají převzít z druhé kopie (dvě tlačítka je všechny zaškrtnou nebo jejich zaškrtnutí zruší). Nic se nerozhoduje za vás. Výsledek se použije jako jeden krok zpět a nic se neukládá. Bajty, které editor zachovává (rezervovaná pole hlavičky a bajty za světem), se nikdy z jiného souboru nepřebírají: ponechají si hodnoty tohoto světa a stavový řádek uvede, kolik se jich lišilo. Totéž sloučení bez okna provádí příkaz `merge`.

### Pokud se editor neočekávaně ukončí

Několik sekund po úpravě se neuložený stav každého změněného světa zkopíruje do složky obnovy ve vašem uživatelském profilu (nikdy vedle souboru světa). Kopie se odstraní, když svět uložíte, vrátíte k uloženému nebo zavřete. Pokud se editor nebo počítač zastaví dříve, při příštím spuštění se editor zeptá, co s každou kopií udělat: **Obnovit** otevře svět a znovu použije změny jako jeden krok zpět, stále neuložené; **Uložit kopii jako** je zapíše do souboru podle vašeho výběru; **Zahodit** je smaže; **Zeptat se později** je ponechá. Obnovení se nabízí jen tehdy, když je soubor světa bajt po bajtu tím, čím byl při vytvoření kopie; pokud se soubor změnil nebo zmizel, lze změny stále uložit jako kopii. Poškozená kopie se ohlásí a nikdy se nepoužije.

### Snímky

**Soubor → Pořídit snímek** uloží aktuální stav světa pod názvem, který zadáte, do téže složky ve vašem uživatelském profilu. **Soubor → Snímky** vypíše snímky světa, od nejnovějšího; obnovení snímku je jediný krok zpět a nedotkne se souboru na disku a snímek tam lze i smazat. Uchovává se dvacet nejnovějších snímků každého souboru světa.

## Minimapa a statistiky

**Minimapa** zobrazuje celý svět a obrysem vyznačuje část viditelnou na plátně; kliknutím nebo tažením v ní plátno posunete. Může zobrazovat obě vrstvy nebo jednu.

**Statistiky světa** uvádějí velikost souboru a revizi, obsazené a prázdné buňky a hodnoty použité v každé vrstvě, využití slotů objektů a počty podle uloženého typu, kanálu a ikony, čtyři starty, hranice použitých buněk a rozsahy bajtů, které se liší od uloženého souboru a od souboru při otevření. Dvojklikem na řádek najdete jeho buňky, záznamy nebo bajty na plátně. Jsou to uložená fakta; pro žádnou hru se neinterpretují.

**Úpravy → Najít** (Ctrl+F) přejde na souřadnici buňky, uloženou hodnotu dlaždice v aktivní vrstvě, typ objektu nebo slot objektu.

## World Doctor

**Svět → Zkontrolovat** (F8) otevře panel Diagnostika. World Doctor kontroluje strukturu a známé limity; svět nikdy nemění a neposuzuje, zda lze svět dohrát (s výjimkou jednoho označeného odhadu) nebo zda je zábavný. Seznam se obnovuje po každé úpravě a shrnutí se zobrazí při každém použití příkazů Uložit a Uložit jako, aniž by je blokovalo.

Každý nález má ID pravidla, závažnost (chyba, varování nebo poznámka), hru, pro kterou byl posuzován, a podklad pravidla: strukturální fakt, něco přečteného ve zdrojovém kódu hry, hodnotu mimo ověřený rozsah, něco převzatého, ale znovu neověřeného, nebo odhad. Dvojklik na nález jej ukáže na plátně, v panelu Objekty a v Binárním inspektoru.

Mezi nálezy patří: zkrácený soubor, bajty navíc, neznámá revize; hudba nebo region mimo rozsah hry; start mimo svět nebo s neobvyklým směrem; název bez ukončovacího znaku, delší než 40 bajtů nebo hrou ignorovaný; čísla dlaždic menší než -1, bez obrázku nebo bez kolizních dat ve zvolené hře; nahrazení uložených hodnot 48–67 hodnotou 215, které Speedy Blupi II provádí při načítání světa; okrajové dlaždice, jejichž tvar se liší od toho, co by vytvořil editor hry; objekty typu, který vzniká jen při hraní nebo je hře neznámý, s neočekávaným kanálem, mimo svět nebo s dráhou a nulovou dobou cesty; zcela obsazené sloty objektů a téměř vyčerpané sloty.

Jeden nález je odhadem týkajícím se hraní, nikoli souboru: **REACH-CUT-OFF** hlásí poklad nebo cíl, jehož buňka není s žádným startem spojena buňkami bez zcela pevných dlaždic (nahoru, dolů, doleva, doprava). Takový objekt lze sebrat jen tehdy, když hra mapu při hraní změní. Odhad neví nic o skákání, padání, vozidlech ani výtazích, takže nepřítomnost této poznámky neznamená, že svět lze dohrát; a neprovádí se pro světy Speedy Blupi II, které obsahují teleporty nebo dveře na klíč. Je to poznámka označená jako odhad a u původních světů žádné z her nehlásí nic.

Nabízí se jen jedna oprava: ukončení názvu, který zaplňuje celé své pole. Zobrazí se s přesným bajtem, který mění, a po vašem potvrzení se použije jako jeden krok zpět. **Svět → Porovnat s původním** a **Porovnat s uloženým** vypíší každý změněný rozsah bajtů se starými a novými bajty.

## Binární inspektor

**Zobrazení → Binární inspektor** (Ctrl+B) zobrazuje celý soubor jako šestnáctkový výpis. Nemůže nic změnit. Hlavička, popředí, bigDecor, záznamy objektů a případné bajty navíc jsou podbarveny různě; bajty změněné od otevření souboru jsou zvýrazněny a neuložené bajty podtrženy.

U bajtu pod kurzorem ukazuje offset, strukturální název (například `header.music`, `foreground[3][70]`, `objects[137].posEnd.y`), výpočet, který k němu vede, hodnotu jako číslo little-endian se znaménkem a bez znaménka, původní hodnotu a u bajtu s neuloženými změnami uloženou hodnotu. Buňky se ukládají po sloupcích: buňka (x, y) vrstvy je na `layer start + 2 × (100 × x + y)`.

Pro skok zadejte offset (`1608` nebo `0x648`), `cell 3,70`, `big 3,70` nebo `slot 137`; vyberte sekci ze seznamu; hledejte šestnáctkové bajty nebo text ASCII; nastavujte záložky; zvolte 8 až 32 bajtů na řádek; zobrazte původní bajty nebo původní a aktuální vedle sebe. Inspektor sleduje kurzor buňky a vybraný objekt a kliknutí na bajt tam přesune plátno.

## Práce s herní instalací

**Zobrazení → Instalace** zobrazí dokovací panel (na kartě společně s Paletou) pro adresář hry, který jste vybrali pro hru aktuálního světa (**Zobrazení → Adresář s grafikou hry**). Vypisuje každý svět ve složce `DATA` tohoto adresáře s malým obrázkem jeho použité části: vlastní světy hry (`world012.blp`) s rolí, kterou má jejich číslo ve hře — první svět, rozcestník skupiny, mise, závěrečný svět, síťová hra, demo — a světy, které navrhl hráč (`u001-003.blp`: hráč 1, svět 3), s názvem tam, kde jej soubor ukládá. Soubor, který není úplným světem, je označen. Pod světy panel vypisuje herní záznamy instalace se světem, na kterém se každý přehrává, a s jeho délkou; dvojklikem na záznam zobrazíte jeho klávesy (viz **Herní záznamy**). Dvojklikem na svět jej otevřete. Hry nevedou žádný seznam navržených světů: k tomu, aby jej obrazovka návrhu ve hře našla, stačí soubor se správným názvem.

**Nasadit aktuální svět** (také **Soubor → Nasadit do instalace**) vloží svět z aktuální karty — tak jak teď je, včetně neuložených změn — do této složky. Místo volíte vy: svět navržený hráčem 1 až 8, číslo 1 až 20 (nabídne se místo, kterému odpovídá název vlastního souboru karty, pokud takové je, jinak první volné číslo hráče 1), nebo jedno z vlastních čísel světů hry, takže hra bude hrát váš svět na jeho místě. Dialog říká, zda se soubor vytvoří, nebo nahradí. Nahrazovaný soubor se nejprve zkopíruje do složky ve vašem uživatelském profilu (nikdy uvnitř adresáře hry) a zkontroluje; váš svět se potom zapíše přes dočasný soubor, který v jediném kroku zaujme místo cíle, a přečte se zpět. Pokud některý krok selže, adresář hry obsahuje přesně to, co obsahoval předtím. Nasazení neukládá vlastní soubor karty. Soubor, který je otevřený v editoru, nemůže být cílem nasazení.

**Vrátit poslední nasazení** po dotazu zruší nejnovější nasazení: uchovaný soubor se vrátí zpět a ověří, nebo se vytvořený soubor odstraní. Pokud byl soubor od nasazení změněn (například vlastním editorem hry), zůstane nedotčen a jste o tom informováni.

**Zkušební hra** (F5) spustí příkaz, který jste pro hru zadali — její program nebo například `wine` následované programem — v adresáři hry. Na příkaz se editor zeptá napoprvé a lze jej změnit v **Úpravy → Příkaz zkušební hry**. Hry nemají žádnou volbu, která by otevřela konkrétní svět, takže jej vyberete ve hře: na titulní obrazovce zvolte hráče, potom tlačítko návrhu a přejděte na číslo světa.

### Kampaně a balíčky

**Soubor → Přehled kampaně** vypíše světy instalace v pořadí, v jakém se k nim hra dostává — první svět, rozcestník každé skupiny následovaný jejími misemi, závěrečný svět, síťové a demo světy, ostatní čísla, potom navržené světy — a uvede, co ukazuje samotné číslování: chybějící svět 1, rozcestník, jehož mise chybějí, nebo mise, jejichž rozcestník chybí, čísla mimo schéma. Pod tím sleduje dlaždice dveří oficiálních světů — z prvního světa vedou dveře do svého čísla krát deset, jinde do rozcestníku skupiny plus své číslo, a výstupní dveře zpět do prvního světa — a hlásí dveře, které vedou do souboru světa, který neexistuje, a rozcestník nebo misi, do kterých žádné dveře nevedou. Před těmito nálezy přehled vypisuje samotná propojení, jeden řádek na každý svět, který má dveře: `World 1 -> 10, 20` znamená, že dveře prvního světa vedou do rozcestníků 10 a 20.

**Soubor → Exportovat balíček kampaně** zapíše světy instalace s manifestem (název, hra a u každého světa jeho místo, velikost a SHA-256) jako jeden soubor ZIP nebo jako složku. Navržené světy a světy, které tento editor sám nasadil, se nabízejí zaškrtnuté. Soubory na vlastních číslech světů hry, které tam editor nevložil, jsou s největší pravděpodobností originály hry: jsou vypsány, ale nelze je zaškrtnout, dokud nezaškrtnete políčko s prohlášením, že balíček, který je obsahuje, se nesmí šířit dál. ZIP se zapisuje bez komprese (světy jsou malé) a otevře se v libovolném programu pro ZIP.

**Soubor → Importovat balíček kampaně** přečte balíček ZIP nebo balíček ve složce, když v ní vyberete soubor `eggbert-campaign.json`, a než se cokoli zapíše, vše zkontroluje: manifest, že každá položka je uvedena a odpovídá své velikosti a kontrolnímu součtu, že každý svět je úplný a nese název souboru místa, které hry používají. Názvy položek smějí obsahovat jen písmena, číslice, tečku, spojovník a podtržítko, takže nic v balíčku nemůže ukazovat mimo balíček. Položky mohou být uloženy tak, jak jsou, nebo komprimovány metodou Deflate, což programy pro ZIP dělají ve výchozím nastavení, takže se přečte i balíček, jehož soubory někdo rozbalil a znovu zazipoval, pokud jsou soubory v nejvyšší úrovni archivu (složka uvnitř něj je odmítnuta jako každá jiná cesta); při dekompresi položka nikdy nevytvoří víc dat, než jakou velikost pro ni archiv uvádí. Šifrované nebo příliš velké položky, jiné metody komprese a rozdělené archivy nebo archivy ZIP64 jsou odmítnuty s uvedením důvodu. Balíček pro druhou hru je odmítnut. Potom uvidíte světy a to, které existující soubory nahrazují; po potvrzení se každý svět nasadí přesně jako příkazem **Nasadit aktuální svět** (uchovaná kopie, nahrazení v jediném kroku, zpětné přečtení). Pokud některý svět nelze umístit, ty už umístěné se zase odeberou, takže import proběhne buď celý, nebo vůbec; poté **Vrátit poslední nasazení** odebírá importované světy po jednom.

## Experimentální úpravy

Ve výchozím stavu vypnuto. Zapíná se v **Úpravy → Předvolby → Experimentální úpravy**; stavový řádek to pak uvádí. Umožňuje zapisovat hodnoty, pro které editor nemá žádný doklad, takže výsledkem může být něco, co žádná hra neočekává. Zpřístupní se tři věci:

- **Úpravy → Upravit bajty** nahradí bajty na offsetu, který zadáte (desítkově, nebo šestnáctkově s `0x`; výchozí je kurzor Binárního inspektoru), šestnáctkovými bajty, které zadáte, a ukáže, co je tam uloženo nyní. Úprava je jeden krok zpět jako každá jiná. Pokud zasahuje do rozsahu, který editor jinak zachovává — rezervovaná pole hlavičky, pole `world` nebo bajty za koncem světa — vyjmenuje tyto offsety a před jejich změnou se zeptá znovu. Pole revize (offsety 0 až 3) se nikdy neupravuje, protože rozhoduje o tom, jak se čte zbytek souboru, a soubor nikdy nemění délku.
- **Svět → Metadata** přijme libovolné číslo hudby a regionu a upozorní, když je číslo mimo rozsah ověřený pro hru.
- **Úpravy → Načíst vlastní pravidla okrajů** přečte textový soubor s vašimi vlastními pravidly okrajů a přidá je do výběru štětců nástroje Adaptivní jako **Vlastní: název**. Pravidlo je šestnáct čísel dlaždic, jedno pro každou kombinaci připojených sousedů:

  ```
  # mask order: +1 the cell below joins, +2 above, +4 right, +8 left
  [Stone wall]
  forms = 153 147 148 146 40 151 150 144 39 152 149 145 38 36 37 35
  ```

  Malování tímto pravidlem zapíše tvar pro žádné sousedy a potom dá namalované buňce a jejím čtyřem sousedům tvar odpovídající tomu, kteří z jejich sousedů obsahují číslo téhož pravidla; okraj mapy se počítá jako připojený. Mění se pouze vrstva popředí. Tato pravidla jsou vaše, ne pravidla hry: nic nekontroluje, zda je hra kreslí jako okraj. Soubor se zapamatuje a při příštím spuštění se přečte znovu, dokud experimentální úpravy zůstávají zapnuté.

Světy jen pro čtení zůstávají jen pro čtení.

## Export obrázku

**Soubor → Exportovat svět do PNG** (Ctrl+E) zapíše jeden obrázek celého světa: 6,400 × 6,400 pixelů při nativních 64 pixelech na buňku, nebo menší. Zvolte celý svět nebo jen vybrané buňky, vrstvy a překryvy, plné nebo průhledné pozadí a, s grafikou hry, zda se kreslí obrázek pozadí světa (dlaždicově opakovaný od levého horního rohu, bez posunu daného posouváním plátna); dialog nejprve ukáže velikost a potřebnou paměť. Obrázek se kreslí stejným kódem jako plátno a do svých metadat zaznamená profil hry, způsob kreslení a nastavení. Je schematický, pokud nezaškrtnete **Kreslit grafikou hry** (dostupné, když je načtena grafika pro hru karty). Obrázky nakreslené grafikou hry obsahují autorsky chráněný materiál vlastníků hry: ponechte si je pro vlastní potřebu, pokud nemáte svolení je sdílet.

**Soubor → Exportovat svět do SVG (schematicky)** místo toho zapíše škálovatelnou kresbu: jeden čtverec na každou obsazenou buňku, obrysy pro bigDecor, kruh na každý objekt s přerušovanou čárou podél jeho dráhy a čtyři starty. Nikdy neobsahuje grafiku hry, takže ji lze volně sdílet, a nakreslené prvky říkají, co představují — `data-tile` na čtverci dlaždice, `data-slot` a `data-type` na kruhu objektu, `data-path-of` (slot) na čáře jeho dráhy, `data-start` na startu — díky čemuž je soubor použitelný pro další nástroje. Soubor bez úplné vrstvy dlaždic nakreslit nelze.

## Regresní scénáře

Scénář je malý textový soubor s věcmi, které mají o světě zůstat pravdivé, zatímco na něm dál pracujete: kolik pokladů obsahuje, že World Doctor nenajde žádnou chybu, kde je první start. **Soubor → Zaznamenat scénář** zapíše vše, co o světě na kartě platí právě teď, jedno očekávání na řádek:

```
# Eggbert Editor world scenario: delete or loosen (>=, <=) lines as needed.
doctor.errors = 0
foreground.occupied = 412
objects.type.5 = 12
start.1.x = 194
```

Smažte řádky, na kterých vám nezáleží, a jiné uvolněte pomocí `>=` nebo `<=` (například `objects.type.5 >= 10`). **Soubor → Zkontrolovat podle scénáře** potom porovná svět na kartě — včetně neuložených úprav — s takovým souborem a vypíše každé očekávání, které už neplatí, s jeho řádkem a oběma čísly. Název, pro který svět nic nemá (typ objektu, který se nevyskytuje, pravidlo, které nic nehlásí), se počítá jako 0. Zaznamenávanými fakty jsou revize, velikost souboru, hudba, region, posouvání a čtyři starty z hlavičky, obsazené buňky každé vrstvy dlaždic, objekty používané, volné a podle uloženého typu a nálezy World Doctoru podle závažnosti a podle pravidla (závisejí na hře zvolené pro kartu). Příkazy `scenario-record` a `scenario-check` dělají totéž bez okna, takže složku světů může zkontrolovat skript.

## Vzhled a rozvržení

Panely lze přesouvat, uvolňovat do samostatných oken, skládat na sebe a zavírat; nabídka **Zobrazení** je vypisuje. **Zobrazení → Rozvržení** je uspořádá pro mapování, práci s objekty nebo kontrolu a **Obnovit rozvržení** obnoví výchozí stav. **Zobrazení → Motiv** přepíná mezi vzhledem podle systému, světlým a tmavým. Rozvržení okna, motiv a naposledy otevřené soubory se pamatují.

**Úpravy → Předvolby → Jazyk** volí jazyk rozhraní: jazyk systému, angličtinu nebo češtinu (Čeština). Projeví se při příštím spuštění editoru. Český překlad pokrývá vlastní texty rozhraní, názvy dlaždic, objektů, skupin a rolí světů z katalogů her a věty, které skládá kód kontrol a převodů: nálezy World Doctoru a Přehledu kampaně, poznámky převodů, fakta encyklopedie, problémy textového světa Mobile Eggbert, řádky kontroly podle scénáře a zprávy o balíčcích a nasazeních. Česky jsou i standardní tlačítka dialogů (OK, Zrušit, Uložit, Zahodit a podobně), nabídka vstupního pole a tlačítka doků. Anglicky zůstávají strukturální názvy a výpočty Binárního inspektoru (jsou to vlastní pojmy formátu), citace dokladů, název souboru, který je odmítnut jako položka balíčku, diagnostický soubor a příkazový řádek. Je-li zvolena čeština, **Nápověda → Uživatelská příručka** zobrazí tuto příručku v češtině.

Každý ovládací prvek hlavního okna, který může získat klávesnicový fokus, má název pro čtečky obrazovky, včetně obrázkových seznamů Palety a polí panelu Objekty, a stejně tak seznamy a výběry v dialozích převodu, snímků a balíčků. Ostatní dialogy spoléhají na své viditelné popisky.

**Nápověda → Diagnostický balíček** uloží textovou zprávu, kterou lze přiložit k hlášení problému: verze programu, Qt a platformy, strukturální zprávu aktuálního dokumentu (s názvem jeho souboru, ale ne s jeho složkou, a s dekódovaným názvem a hodnotami hlavičky), nálezy World Doctoru a rozsahy bajtů změněné od otevření a od posledního uložení s jejich starými a novými bajty. Zbytek dat světa zahrnut není.

## Příkazový řádek

Kromě otevírání souborů (`eggbert-editor world.blp ...`) má program příkazy, které neotevírají žádné okno a nikdy nemění soubor světa, který čtou:

- `eggbert-editor info FILE [--events]` vypíše strukturální zprávu a dekódované shrnutí. U textového světa Mobile Eggbert nebo herního záznamu popíše místo toho tento soubor; `--events` přidá každou událost záznamu.
- `eggbert-editor validate FILE [--game 1|2]` vypíše nálezy World Doctoru, jeden na řádek s pravidlem, rozsahem bajtů a zprávou; návratový kód je 1, pokud jsou nalezeny chyby. Textový svět Mobile Eggbert se kontroluje jako svět této hry a `--game` se u něj nezadává; totéž platí pro `export-png`, `export-batch` a příkazy scénářů.
- `eggbert-editor diff FILE OTHER` vypíše lišící se rozsahy bajtů s jejich strukturálními názvy a starými a novými bajty; návratový kód je 1, pokud se soubory liší.
- `eggbert-editor export-png FILE OUT.png` zapíše obrázek, který zapisuje **Soubor → Exportovat svět do PNG**. Volby: `--scale N` (pixely na buňku, 1 až 64), `--crop x,y,width,height` (v buňkách), `--game 1|2`, `--game-data DIR` (kreslit grafikou této hry; u textového světa Mobile Eggbert složka portu), `--no-foreground`, `--no-bigdecor`, `--no-objects`, `--no-starts`, `--no-paths`, `--no-backdrop`, `--grid`, `--labels`, `--transparent`.
- `eggbert-editor convert FILE OUT --to 1.0|1.1|1.2|1.3 [--allow-loss]` zapíše kopii v jiné revizi, stejně jako **Soubor → Převést revizi**. `OUT` nesmí existovat. Když cílová revize neumí uložit vše, nic se nezapíše a návratový kód je 1, pokud není zadáno `--allow-loss`.
- `eggbert-editor convert-game FILE OUT --from 1|2 --to 2|1 [--unknown refuse|keep|remove]` zapíše kopii pro druhou hru, stejně jako **Soubor → Převést pro druhou hru**, a vypíše seznam. `OUT` nesmí existovat. Bez `--unknown keep` nebo `--unknown remove` se svět s hodnotami neznámými cílové hře nepřevede: seznam jde na chybový výstup a návratový kód je 1.
- `eggbert-editor merge BASE MINE THEIRS OUT [--prefer mine|theirs]` spojí to, co dva světy změnily, vycházejíce ze stejného základu. Tři soubory musí být úplné světy jedné revize a stejné délky. Sloučení probíhá prvek po prvku — jedna hodnota hlavičky, jedna buňka, jeden celý záznam objektu, nebo jinde jeden bajt: prvek změněný na jedné straně převezme bajty této strany, prvek změněný na obou stranách stejně je převezme a prvek změněný na obou stranách různě je konflikt. Při konfliktech bez `--prefer` se konflikty vypíší (offset, název, tři verze), nic se nezapíše a návratový kód je 1; s `--prefer` se pro každý konflikt převezme celý prvek této strany. `OUT` nesmí existovat. **Soubor → Sloučit změny** dělá totéž v okně, přičemž se o každém konfliktu rozhoduje zvlášť. Tři textové světy Mobile Eggbert se slučují stejně, hodnotu po hodnotě, a `OUT` je pak text souboru `MINE` se sloučenými hodnotami; textové světy a světy `.blp` se nemíchají.
- `eggbert-editor new OUT --game 1|2|mobile` zapíše svět, který zapisuje **Soubor → Nový svět**; s `mobile` textový svět Mobile Eggbert. `OUT` nesmí existovat.
- `eggbert-editor export-batch OUTDIR FILE... [options of export-png]` zapíše jeden obrázek na svět do existujícího adresáře, každý pojmenovaný podle svého světa (z `world001.blp` se stane `world001.png`) a shodný s tím, co pro něj zapisuje `export-png`. Dva světy se stejným názvem jsou odmítnuty. Svět, který nelze nakreslit, ostatní nezastaví; návratový kód je pak 1. Chybná volba se ohlásí jednou, nic se nezapíše a návratový kód je 2.
- `eggbert-editor export-svg FILE OUT.svg [--scale N] [--no-foreground] [--no-bigdecor] [--no-objects] [--no-starts]` zapíše schematickou kresbu, kterou zapisuje **Soubor → Exportovat svět do SVG**; `--scale` je velikost jedné buňky (1 až 64, 16, není-li zadáno).
- `eggbert-editor scenario-record FILE OUT.txt [--game 1|2]` zapíše scénář, který zapisuje **Soubor → Zaznamenat scénář**. `OUT` nesmí existovat.
- `eggbert-editor scenario-check FILE SCENARIO.txt [--game 1|2]` zkontroluje svět podle scénáře a vypíše každé očekávání, které není splněno; návratový kód je 1, když některá splněna nejsou, a 2, když soubor není scénář.

Návratové kódy: 0 úspěch, 1 nálezy, rozdíly, odmítnutá ztráta, konflikty při sloučení, obrázky, které se nepodařilo zapsat, nebo nesplněná očekávání, 2 chybné použití nebo soubor, který nelze přečíst nebo zapsat. `eggbert-editor --help` vypíše vše.

## Herní záznamy

Hry se dodávají se záznamy něčího hraní (`demo200.blp` a následující v první hře, `demo300.blp` a následující ve druhé) a přehrávají je z titulní obrazovky. Otevření takového souboru — přes Soubor → Otevřít svět, na příkazovém řádku nebo přetažením — nevytvoří kartu: okno jen pro čtení ukáže, co soubor obsahuje.

- **Časová osa** s jedním řádkem na klávesu (Vlevo, Vpravo, Nahoru, Dolů, Skok, Akce) a pruhem pro každý úsek, kdy je klávesa držena. Přejeďte přes ni ukazatelem nebo jí předejte klávesnicový fokus a použijte šipky vlevo a vpravo (Page Up/Down posouvají o dvacetinu záznamu, Home/End na jeho konce) a řádek pod ní uvede krok, přibližný čas a klávesy, které jsou tam drženy.
- **Tabulka** s tím, kolikrát byla každá klávesa stisknuta a jak dlouho byla držena.
- **Svět**, ke kterému záznam patří: hry přehrávají `demoNNN.blp` na `worldNNN.blp` z téže složky a **Otevřít svět NNN** tento soubor otevře, pokud tam je.

Záznam ukládá klávesy, ne to, kde hráč byl, takže z něj nelze nakreslit žádnou cestu světem ani teplotní mapu: to by vyžadovalo pravidla pohybu her, která editor nemá. Časy jsou v herních krocích; sekundy se uvádějí jako „asi“, pro běžné tempo her 50 ms na krok. Soubor se pozná podle obsahu: hlavička o 204 bajtech nesoucí revizi 1.0 a nic jiného, potom nejvýše 10,000 čtyřbajtových událostí, jejichž kroky začínají na 0 nebo později a nikdy nejdou zpět a jejichž klávesy patří mezi oněch šest. Cokoli jiného — a každý úplný svět revize 1.0 — se považuje za soubor světa. Záznamy nelze upravovat ani zapisovat.

`eggbert-editor info demo200.blp` vypíše stejná čísla a s `--events` každou zaznamenanou událost.

## Světy Mobile Eggbert

Mobile Eggbert, port hry pro telefony, ukládá své světy jako textové soubory (`world011.txt`): jeden řádek `DescFile:` s poli `name=value`, blok `Decor:` a blok `BigDecor:` s jedním řádkem hodnot oddělených čárkami na každý sloupec mapy a jeden řádek `MoveObject:` na objekt. Takový soubor obsahuje totéž, co svět her pro Windows — dvě vrstvy 100 × 100 čísel dlaždic, až 200 objektů se stejnými třinácti čísly, start, hudbu, pozadí a pozici posunu — proto jej editor otevře jako běžnou kartu a funguje na něm vše popsané výše: Paleta, tužka, tvary, výplň a výběry, štětce okrajů, objekty a jejich dráhy, razítka, minimapa, statistiky, World Doctor, encyklopedie, snímky, obnova po pádu, Sloučit změny, export PNG a SVG i scénáře.

- **Hra** takové karty je vždy **Mobile Eggbert** a pole hry v panelu nástrojů je zamčené. Port obsahuje stejné tabulky dlaždic a objektů jako druhá hra pro Windows, takže názvy, skupiny, štětce okrajů a předvolby objektů jsou její. Tři rozdíly jsou doloženy a uplatněny: port při načítání světa nenahrazuje dlaždice 48 až 67, dlaždice 49 až 67 v něm nejsou pevné a číslo dlaždice 0 čte jako prázdnou buňku, na což World Doctor upozorní. Doby pohyblivých objektů se zobrazují tak, jak je soubor ukládá; port je při načítání světa přepočítá na své tempo a objektům typu 54 dá pevné čekání na obou koncích.
- **Obrázky** pocházejí z vlastní složky `Content` portu (`icons` a `backgrounds`). Leží-li svět ve složce `worlds` portu, najdou se samy; jinak zvolte složku portu přes **Zobrazení → Adresář s grafikou hry**. Port nemá žádné hudební soubory, takže není co přehrávat.
- **Jeden start.** Soubor má jeden start a jeden směr: Metadata světa upravují jen první start, nástroj startu jej přesouvá a název nelze zadat, protože pro něj soubor nemá místo.
- **Ukládání zapisuje text.** Přepíší se jen hodnoty, které jste změnili, každá na svém místě; všechno ostatní — mezery, neznámá pole a řádky, pořadí řádků objektů — se zapíše zpět bajt po bajtu a uložení světa, který jste nezměnili, zapíše totožný soubor. Smazání objektu odebere jeho řádek; nový objekt dostane řádek na konci. Ukládá se přes dočasný soubor a uložení je odmítnuto, pokud se soubor na disku změnil, jako u každého světa. Kopie uchovávané pro obnovu a jako snímky jsou také text. Soubor s konci řádků Windows (CR LF) si je ponechá a jeho karta zobrazí upozornění: čtení světů v portu dělí řádky jen u znaku LF, takže pokud hra znaky CR předtím neodstraní, nepřečte poslední hodnotu každého řádku dlaždic, tedy spodní řadu mapy.
- **Dok Instalace** vypisuje světy ze složky `worlds` portu s malým obrázkem a jejich rolí (port čísluje kampaň jako druhá hra: svět 1, rozcestníky skupin 10, 20 a tak dále se svými misemi a 199 jako poslední). **Nasadit aktuální svět** tam zapíše svět karty jako `worldNNN.txt` pod číslem, které zadáte, a to, co nahradí, uchová pro **Vrátit poslední nasazení**. **Zkušební hra** spustí příkaz, který jste pro tuto hru zadali, ve složce obsahující `Content`. **Soubor → Přehled kampaně** vypíše tyto světy a zkontroluje jejich číslování a dveře podle pravidel portu, která jsou stejná jako u druhé hry.
- **Soubor → Nový svět** nabízí Mobile Eggbert jako třetí hru a zapíše stejný malý výchozí svět jako textový soubor.
- **Nenabízí se** pro tyto světy, protože se týkají formátů `.blp`: Převést revizi, Převést pro druhou hru, Binární inspektor, zadávané úpravy bajtů a balíčky kampaní. Razítko vytvořené ve světě jiné hry se sem neumístí, stejně jako mezi oběma hrami pro Windows.
- **Soubor, který editor neumí rozložit** — více než 200 řádků objektů, objekt typu 0 v seznamu, číslo příliš velké pro rozložení, nebo chybějící blok — se místo toho otevře v prostém vlastním okně a stavový řádek uvede proč. Toto okno zobrazuje očíslované čtverce, maluje, kreslí čáry a obdélníky a vyplňuje výběr číslem dlaždice, upravuje hlavičku a každý řádek objektu v tabulkách, na stránce Problémy vypisuje s číslem řádku, co je špatně, a při každé úpravě nahradí přesně jeden token textu.

`eggbert-editor info FILE` popíše takový soubor bez okna a ostatní příkazy s ním zacházejí tak, jak je uvedeno v části **Příkazový řádek**. Textový svět se nikdy nezapíše jako soubor `.blp` ani svět `.blp` jako text: oba formáty patří různým hrám. Editor pouze drží textový svět v paměti v rozložení, které používá pro každý svět; toto rozložení se nikdy neukládá.

## Co editor nedělá

Neupravuje revize 1.0 a 1.1 na místě (místo toho převeďte kopii), neupravuje bajty přímo v šestnáctkovém zobrazení (zadávané úpravy bajtů viz **Experimentální úpravy**), nenabízí objekty, pro které nemá doklad (surová čísla dlaždic a číselná pole ověřeného objektu můžete vždy zadat sami; cokoli jiného jen jako zadané bajty při zapnutých experimentálních úpravách), nenabízí štětce okrajů ani hodnoty hudby a regionu, pro které nemá doklad, pokud nejsou zapnuty experimentální úpravy, nepřečíslovává kampaně a nepřevádí do formátu Mobile Eggbert ani z něj.
