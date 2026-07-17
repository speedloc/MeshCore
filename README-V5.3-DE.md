# Solar-Repeater v5.3 – konservative Korrektur

Diese Version basiert auf dem zuletzt hochgeladenen, funktionierenden `solar-repeater`-Stand.

Geändert wurden ausschließlich:

- Abschaltung bei **3,30 V** statt 3,20 V
- Bootlock/Wiederstart bei **3,50 V** statt 3,40 V
- Statusmeldung mit sauber getrenntem Absender und Nachrichtentext

Die Boot-, SYSTEMOFF-, Board- und Initialisierungslogik wurde ansonsten nicht verändert.

## Anzeige im Kanal

Absender/Titel:

    <Repeatername>

Nachricht bei Abschaltung:

    ⚠️ Akku 3.29 V, Deep Sleep bis 3.50 V

Nachricht nach Wiederstart:

    ✅ Wieder online, Akku 3.52 V, Abschaltung bei 3.29 V

## Einspielen

Im Git-Arbeitsverzeichnis:

    cd ~/MeshCore-build/MeshCore
    git switch solar-repeater

Den Inhalt dieser ZIP in das Repository kopieren und die vorhandenen Dateien ersetzen.

Danach wie vereinbart bauen:

    chmod +x build.sh
    ./build.sh --clean

Nach erfolgreichem Flashen zuerst `statusmsg` testen.

Erst nach erfolgreichem Gerätetest veröffentlichen:

    ./publish.sh "Solar repeater v5.3 minimal"
