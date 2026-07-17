# Update auf Solar-Repeater v5.2

Geändert wurden nur die Spannungsschwellen:

- Abschaltung: **3,30 V oder weniger**
- Wiederstart/Bootlock: **3,50 V**
- Prüfintervall: weiterhin einmal pro Stunde
- USB-Bypass: weiterhin aktiv

## Einspielen

Im Git-Arbeitsverzeichnis:

```bash
cd ~/MeshCore-build/MeshCore
git switch solar-repeater
```

Den Inhalt dieser ZIP in den Projektordner kopieren und vorhandene Dateien ersetzen.
Danach kompilieren:

```bash
./build.sh --clean
```

Nach erfolgreichem Flashen und Testen veröffentlichen:

```bash
./publish.sh "Solar repeater v5.2: 3.30 V shutdown, 3.50 V restart"
```
