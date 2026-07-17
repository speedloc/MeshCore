# XIAO nRF52840 Solar-Repeater – Änderungen

Diese Variante basiert auf EasySkyMesh/MeshCore PowerSaving v16.

## Verhalten

- Akkuspannung wird im laufenden Betrieb einmal pro Stunde geprüft.
- Bei **3,30 V oder weniger** wird eine Statusmeldung an `#lkgr-info` eingeplant, der SX1262 abgeschaltet und der nRF52840 in `SYSTEMOFF` versetzt.
- Der XIAO startet nach Erholung des Akkus und der Bootprüfung wieder ab **3,50 V**.
- Bei angeschlossener **USB-Versorgung wird die Boot-Sperre übersprungen**. Damit kann das Gerät auch bei niedrigem Akku für Wartung und Updates gestartet werden.
- Das vorhandene Akku-BMS bleibt als zusätzlicher Schutz sinnvoll.

## Statusmeldungen

Vorgesehen sind Meldungen mit dem konfigurierten Repeater-Namen:

- `Repeatername: Akku 3.29 V, Deep Sleep bis 3.50 V`
- `Repeatername: wieder online, Akku 3.52 V, offline 4h 06m`

Der Testbefehl lautet:

```text
statusmsg
```

Er funktioniert:

- lokal über USB-Serial mit 115200 Baud;
- remote über die authentifizierte MeshCore-Admin-CLI.

## Kompilieren

Bitte künftig das mitgelieferte Skript verwenden:

```bash
chmod +x build.sh
./build.sh --clean
```

Die fertigen Dateien werden nach `firmware-output/` kopiert.

Direkter USB-Upload mit PlatformIO:

```bash
./build.sh --upload
```

Das Skript erwartet PlatformIO standardmäßig unter `~/platformio-venv`. Ein anderer Pfad kann so gesetzt werden:

```bash
PLATFORMIO_VENV=/anderer/pfad ./build.sh --clean
```
