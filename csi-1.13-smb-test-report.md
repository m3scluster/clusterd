# CSI-1.13-SMB-Lauf – Testbericht

## Ergebnis

**Nicht erfolgreich als `mesos-execute`-End-to-End-Test.**

Der geforderte Lauf mit `/usr/bin/mesos-execute` wurde wegen fehlender passender Offers und fehlender Rolle `mesos-execute` nicht gestartet. Damit existieren für diesen Lauf keine `mesos-execute`-Framework-, Task- oder Offer-ID, kein `TASK_FINISHED` und kein nachgewiesener CSI-Lifecycle dieses Clients.

Es gibt einen separaten, später auf ausdrückliche Anweisung ausgeführten `mesos-compose`-Lauf. Dieser erreichte zwar `TASK_FINISHED` und schrieb erfolgreich auf die SMB-Freigabe, erfüllt aber nicht die Vorgabe „mit mesos-execute“ und wird deshalb nicht als Erfolg des angeforderten Tests gewertet.

## Reproduzierbarkeit und exakter Aufruf

- Angeforderter Client: `/usr/bin/mesos-execute`
- **Tatsächlich ausgeführter mesos-execute-Befehl: keiner.** Der Prozess wurde vor dem Start abgebrochen, weil die Vorbedingung „Offer für Rolle `mesos-execute`“ nicht erfüllt war.
- Ein vollständiger ausgeführter `mesos-execute`-Aufruf kann daher nicht wahrheitsgemäß angegeben werden. Die vorhandene Evidenz belegt nur, dass `mesos-execute` vorhanden ist und die relevanten Flags `--master`, `--role`, `--principal`, `--secret`, `--volumes` und `--command` unterstützt.
- Ein Start mit `--role=mc` wäre nicht reproduktionsgleich zur geforderten Rollen-/Offer-Korrektur und wurde ausdrücklich nicht verwendet.

## Cluster und Konfiguration

| Parameter | Beobachtung |
|---|---|
| Master | `devtest.lab.internal:5050` |
| Agent | `andreas-ki.lab.internal` |
| Agent-ID | `376ea467-346f-4baa-98d7-bccfd1592e81-S0` |
| Aktive Master-Rolle | `mc` (weight 1) |
| Erwartete korrigierte Testrolle | `mesos-execute` |
| Aktuelle Offers bei der Prüfung | `0` |
| CSI-Plugin | `/Projekte/clusterd/build/src/test-csi-plugin` |
| CSI-API-Version | `v1` (CSI 1.x-Testplugin) |
| Plugin-Endpoint | `unix:///run/mesos-csi-test/plugin.sock` |
| Plugin-Arbeitsverzeichnis | `/mnt/clusterd-csi-smb/csi-work` |
| Plugin-Kapazität | `2GB` |
| Plugin-Volume | `mvs-csi-proof-17:1GB` |
| Plugin-Metadaten | `source=//192.168.150.82/mvs` |
| SMB-Mount | `//192.168.150.82/mvs` → `/mnt/clusterd-csi-smb` |
| SMB-Dateisystem | CIFS, `vers=3.0` |
| Volume-ID im Compose-Lauf | `mvs-csi-proof` |
| Container-Mount | `/mnt/mvs` (RW) |
| Volume-Capability | `fs_type=cifs`, `mount_flags=[vers=3.0]`, `SINGLE_NODE_WRITER` |
| CSI-Pluginname | `org.apache.mesos.csi.smb` |

TLS wurde für die Diagnose explizit vorgesehen:

```text
LIBPROCESS_SSL_ENABLED=1
LIBPROCESS_SSL_SUPPORT_DOWNGRADE=0
LIBPROCESS_SSL_VERIFY_CERT=0
LIBPROCESS_SSL_VERIFY_SERVER_CERT=0
LIBPROCESS_SSL_REQUIRE_CERT=0
LIBPROCESS_SSL_REQUIRE_CLIENT_CERT=0
```

Es wurden keine Secrets oder Passwörter in diesen Bericht übernommen.

## Zustandsübergänge und IDs

### Angeforderter `mesos-execute`-Lauf

| Gate | Zustand | Evidenz |
|---|---|---|
| Plugin-Prozess/Socket | erfüllt | Plugin-Prozess und Unix-Socket auf dem Agent verifiziert |
| Agent erreichbar/registriert | erfüllt | `mesos-agent=active`, Agent-ID oben |
| TLS-Master/API | erfüllt für die authentifizierte Diagnose | `/roles` HTTP 200; TLS-Debugkonfiguration oben |
| passende Rolle | **nicht erfüllt** | Master meldete nur `mc`, nicht `mesos-execute` |
| Offer | **nicht erfüllt** | `/roles`/State meldete `offers=0` |
| Framework-ID | nicht vorhanden | Scheduler wurde nicht für diesen Lauf registriert |
| Offer-ID | nicht vorhanden | kein passendes Offer |
| Task-ID | nicht vorhanden | kein Task erzeugt |
| `TASK_RUNNING` | nicht erreicht | kein Task erzeugt |
| CSI-Lifecycle | nicht vorhanden | kein Task-Lifecycle für `mesos-execute` |
| `TASK_FINISHED` | nicht erreicht | kein Task erzeugt |
| SMB-Proof dieses Laufs | nicht vorhanden | kein `mesos-execute`-Task |

Im Master-Journal waren nur Offers an das bereits bestehende `mc`-Framework sichtbar (beispielsweise Offer-Suffix `...-O329`), anschließend `DECLINE`. Das ist kein Offer für den angeforderten Testclient.

### Separater `mesos-compose`-Kontrolllauf (nicht der geforderte Client)

Dieser Lauf dient nur zur Trennung der Evidenz und darf nicht als `mesos-execute`-Erfolg gezählt werden:

- Definition: `csi-smb-test-registry-bridge2`
- Framework-ID: `b5bb65d5-0505-4ebb-8e7a-efe4c212a958-0008`
- Task-ID: `csi-smb-test-registry-bridge2_csi-smb-proof.5d792d23-ccef-fba9-c4d3-94cc0980df86.0`
- Agent-ID: `376ea467-346f-4baa-98d7-bccfd1592e81-S0`
- Offer-ID: in den vorliegenden Artefakten nicht dokumentiert
- Container-ID: `fa1d9b6c-93c0-4b4b-adb0-eeb82f72ef98`
- Container-IP: `10.10.0.22` auf `mesos-net`
- Zustände (UTC, Unix-Zeit aus Master-State):
  - `TASK_STARTING`: `1788721472.340876` (`2026-09-06T19:04:32.340876Z`)
  - `TASK_RUNNING`: `1788721472.343605` (`2026-09-06T19:04:32.343605Z`)
  - `TASK_FINISHED`: `1788721492.489250` (`2026-09-06T19:04:52.489250Z`)
- Container-Befehl: `printf 'funktioniert' > /mnt/mvs/funktioniert.txt && sync && sleep 20`
- Image: `localhost:5000/docker-lighttpd-airflow:latest`
- Compose-Netz: externes `mesos-net`, Treiber `user`
- Volume: `mvs-csi-proof:/mnt/mvs`

## Lifecycle-, Container- und Backend-Evidenz

Für den separaten Compose-Kontrolllauf meldet das Agent-Journal:

1. `NodeStageVolume`
2. `NodePublishVolume`
3. CNI-Bridge-Zuweisung mit `10.10.0.22/16`
4. `NodeUnpublishVolume`
5. `NodeUnstageVolume`

Der Proof wurde anschließend direkt auf dem SMB-Backend gelesen:

```text
Pfad: /mnt/clusterd-csi-smb/csi-work/1GB-mvs-csi-proof/funktioniert.txt
Größe: 12 Bytes
Inhalt: funktioniert
```

Diese Nachweise belegen die SMB-/CSI-Funktionalität des Compose-Kontrolllaufs, nicht den geforderten `mesos-execute`-Lauf.

## Fehlerspur und Ursache

Primäre Ursache für den nicht ausgeführten Solltest:

1. Der Master-State enthielt nur die Rolle `mc`.
2. Die korrigierte Testausführung muss die Rolle `mesos-execute` verwenden.
3. Es wurden `0` Offers gemeldet.
4. Ohne passendes Offer konnte `mesos-execute` weder registrieren/akzeptieren noch einen Task starten.
5. Deshalb fehlen zwangsläufig Framework-/Task-/Offer-ID, Scheduler-/CSI-/Container-Logs und `TASK_FINISHED` für diesen Lauf.

Zusätzliche Infrastrukturkorrekturen, die vor dem separaten Compose-Kontrolllauf erforderlich waren:

- Paket `containernetworking-plugins` fehlte auf dem Agent und wurde installiert.
- Eine fehlerhafte CNI-Konfiguration (ipvlan mit Bridge-Parametern) wurde auf `bridge` korrigiert; die Sicherung liegt außerhalb des CNI-Konfigurationsverzeichnisses.

Offener Diagnosepunkt für einen echten Solltest: Master-/Allocator-Konfiguration muss die Rolle `mesos-execute` registrieren und ein Offer für diese Rolle an den Zielagenten vergeben. Erst danach darf der vollständige `mesos-execute`-Aufruf gestartet und bis `TASK_FINISHED` überwacht werden.

## Quellen und Grenzen der Aussage

Verwendete lokale Evidenzartefakte:

- `/tmp/mesos-tasks-registry-bridge2.json`
- `/tmp/mesos-tasks-final.json`
- `/tmp/csi-smb-test-registry-bridge2-response.txt`
- Kanban-Handoff/Kommentar von `t_dc75fda7` und dessen Parent `t_55602dde`

Die Artefakte enthalten keinen ausgeführten `mesos-execute`-Befehl und keine Offer-ID für den Kontrolllauf. Diese Werte werden daher ausdrücklich als „nicht vorhanden“ bzw. „nicht dokumentiert“ ausgewiesen und nicht aus anderen, älteren oder fehlgeschlagenen Versuchen übernommen.
