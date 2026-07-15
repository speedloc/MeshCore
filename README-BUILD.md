# Solar-Repeater unter Linux bauen

## Firmware kompilieren

Im Projektordner künftig immer das mitgelieferte Skript verwenden:

```bash
chmod +x build.sh publish.sh
./build.sh --clean
```

Die fertigen Dateien liegen danach unter:

```text
firmware-output/
```

## Bauen und GitHub aktualisieren

```bash
./publish.sh "Beschreibung der Änderung"
```

`publish.sh` führt zuerst einen sauberen Build aus. Nur bei erfolgreichem Build werden die Änderungen committed und in den aktuell ausgecheckten Branch gepusht. Erwartet wird der Branch `solar-repeater`.

## Funktionen dieser Version

- Low-Battery-Prüfung einmal pro Stunde
- Abschaltung bei 3,20 V
- automatischer Wiederstart ab 3,40 V
- USB-Versorgung umgeht die Boot-Sperre für Wartung und Updates
- Abschaltmeldung an `#lkgr-info`
- verzögerte Wieder-online-Meldung, nachdem die MeshCore-Uhr gültig ist
- persistenter Low-Battery-Marker im internen Flash
- Offline-Dauer in einer einzeiligen Meldung
- `statusmsg` über USB und Remote-Admin-CLI
