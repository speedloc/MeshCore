# Solar-Repeater v5

Diese Version basiert auf dem vom Nutzer hochgeladenen Branch `solar-repeater`.

## Verhalten

- Akkuspannung wird einmal pro Stunde geprüft.
- Bei höchstens 3,20 V wird die Abschaltmeldung gesendet und anschließend SYSTEMOFF aktiviert.
- Die LPCOMP-/Boot-Sperre startet das Board ab etwa 3,40 V wieder.
- Bei USB-Versorgung wird die Boot-Sperre übersprungen.
- Der Abschaltzustand wird als strukturierter Marker im internen Flash gespeichert.
- Die Wieder-online-Meldung wird erst versendet, wenn die MeshCore-Uhr wieder gültig ist.
- Der Marker wird erst nach dem Sendezeitfenster der Wieder-online-Meldung gelöscht.

## Meldungen

```text
⚠️ <Repeatername>, Akku 3,19 V, Deep Sleep bis 3,40 V
✅ <Repeatername>, wieder online, Akku 3,46 V, offline 4h 06m
```

Falls beim Abschalten noch keine gültige Uhrzeit vorhanden war, wird `offline unbekannt` ausgegeben.

## Test

Über USB oder die authentifizierte Remote-Admin-CLI:

```text
statusmsg
```

## Build

```bash
chmod +x build.sh publish.sh
./build.sh --clean
```

Nicht direkt `pio run` verwenden, sondern künftig `./build.sh`.

## Veröffentlichen

```bash
./publish.sh "Solar repeater v5"
```
