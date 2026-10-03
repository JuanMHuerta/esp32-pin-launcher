// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"
#include <stdio.h>
#include <string.h>
/* Fast commands with a short thinking pause; output arrives in serial bursts. */
#define OUT(t) {t, 200, CRT_OUTPUT}
#define FAST(t) {t, 85, CRT_OUTPUT}
#define READ(t) {t, 850, CRT_OUTPUT}
#define CMD(t) {"ops@atlas:$ " t, 360 + (sizeof(t) - 1) * 24, CRT_COMMAND}
#define WARN(t) {t, 700, CRT_WARN}
#define BAR(t) {t, 1550, CRT_PROGRESS}
#define TITLE(t) {t, 1100, CRT_TITLE}
#define TRACE(t) {t, 1700, CRT_TRACE}
static const crt_event_t startup[] = {TITLE("ORBITAL SYSTEMS / VT-220"),
                                      OUT("ROM 2.17 / MC68030 / 16 MHz"),
                                      BAR("RAM 8192K"),
                                      OUT("[ OK ] RAM parity / MMU / clock"),
                                      OUT("[ OK ] SCSI 0: ST125N / 20MB"),
                                      OUT("Loading ATLAS UNIX 4.2 ..."),
                                      OUT("sd0a: root filesystem clean"),
                                      OUT("tty0: 19200 baud / 8N1"),
                                      OUT("Starting syslogd, cron, relayd"),
                                      TITLE("ATLAS UNIX / STATION NODE-07"),
                                      {"login: operator", 720, CRT_COMMAND},
                                      {"Password: ********", 650, CRT_COMMAND},
                                      OUT("Last login: Fri Oct 01 03:14:08"),
                                      READ("Welcome, operator. Watch begins.")};
static const crt_event_t operations[] = {CMD("uname -a"),
                                         READ("ATLAS node-07 4.2 m68k"),
                                         CMD("uptime"),
                                         OUT("03:14 up 42 days, 1 user"),
                                         READ("load average: 0.12, 0.08, 0.03"),
                                         CMD("who"),
                                         OUT("operator tty0 Oct 01 03:14"),
                                         CMD("ls -l /var/log"),
                                         OUT("total 128"),
                                         FAST("-rw-r--r-- root 86016 kernel.log"),
                                         FAST("-rw-r----- ops  32768 uplink.log"),
                                         OUT("-rw-r--r-- root 12288 boot.log"),
                                         CMD("cat /etc/uplink.conf"),
                                         OUT("host=relay-03  port=4200"),
                                         OUT("mode=duplex   retry=3   crc=on"),
                                         OUT("device=/dev/ttyS1  baud=19200"),
                                         READ("window=8  timeout=5000"),
                                         CMD("ifconfig eth0"),
                                         OUT("eth0: flags=63<UP,BROADCAST,RUNNING>"),
                                         OUT("inet 10.24.7.2 netmask 0xffffff00"),
                                         OUT("ether 08:00:20:07:00:02"),
                                         CMD("netstat -rn"),
                                         OUT("Destination   Gateway      Flags"),
                                         FAST("default       10.24.7.1     UG"),
                                         OUT("10.24.7.0     10.24.7.2     U"),
                                         CMD("ping -c 4 10.24.7.1"),
                                         OUT("64 bytes: seq=0 ttl=64 time=2.1ms"),
                                         OUT("64 bytes: seq=1 ttl=64 time=1.8ms"),
                                         OUT("64 bytes: seq=2 ttl=64 time=2.0ms"),
                                         OUT("64 bytes: seq=3 ttl=64 time=1.9ms"),
                                         READ("4 sent, 4 received, 0% packet loss"),
                                         CMD("arp -a"),
                                         OUT("relay (10.24.7.1) at 08:00:20:03:00:01"),
                                         OUT("store (10.24.7.8) at 08:00:20:08:00:01"),
                                         CMD("netstat -an"),
                                         OUT("Proto Local address    State"),
                                         FAST("tcp   10.24.7.2.4200    ESTABLISHED"),
                                         FAST("tcp   *.23             LISTEN"),
                                         OUT("udp   *.514"),
                                         CMD("ps -ef"),
                                         OUT("UID  PID PPID TIME  CMD"),
                                         FAST("root   1    0 0:01  /sbin/init"),
                                         FAST("root  24    1 0:02  syslogd"),
                                         FAST("root  31    1 0:00  cron"),
                                         FAST("ops   71    1 0:13  relayd"),
                                         OUT("ops  108   92 0:00  -sh"),
                                         CMD("vmstat 1 4"),
                                         OUT(" r b   free  faults  in  cs  us sy id"),
                                         FAST(" 1 0   4096      12  32  54   3  2 95"),
                                         FAST(" 0 0   4080       0  29  41   2  1 97"),
                                         FAST(" 0 0   4080       0  31  43   2  2 96"),
                                         OUT(" 1 0   4064       2  34  58   4  2 94"),
                                         CMD("iostat 1 3"),
                                         OUT("      sd0           cpu"),
                                         OUT(" bps  tps msps    us sy id"),
                                         FAST("  24    2  8.1     3  2 95"),
                                         FAST("  16    1  7.9     2  1 97"),
                                         OUT("  32    3  8.2     4  2 94"),
                                         CMD("df -k"),
                                         OUT("Filesystem  kbytes  used avail cap"),
                                         OUT("/dev/sd0a    12288 10240  2048 83%"),
                                         CMD("du -sk /var/*"),
                                         FAST("128  /var/log"),
                                         FAST("64   /var/spool"),
                                         OUT("256  /var/telemetry"),
                                         CMD("tail -8 /var/log/relay"),
                                         FAST("03:14:11 relay: carrier detected"),
                                         FAST("03:14:11 relay: negotiating v2"),
                                         FAST("03:14:12 relay: window=8 mtu=256"),
                                         FAST("03:14:12 relay: channel 03 open"),
                                         FAST("03:14:13 relay: rx frame 0001"),
                                         FAST("03:14:13 relay: ack frame 0001"),
                                         FAST("03:14:14 relay: rx frame 0002"),
                                         OUT("03:14:14 relay: ack frame 0002"),
                                         CMD("relay --status"),
                                         OUT("peer relay-03 / channel 03"),
                                         OUT("RX 16384  TX 8192  dropped 0"),
                                         READ("carrier -67 dBm / BER 0.00002"),
                                         CMD("relay --sync"),
                                         WARN("carrier drift +180 Hz; adjusting"),
                                         OUT("[ OK ] phase lock restored"),
                                         BAR("Sync catalog"),
                                         OUT("index: 128 records / 3 changed"),
                                         FAST("RX 00007b  256 bytes  CRC OK"),
                                         FAST("RX 00007c  256 bytes  CRC OK"),
                                         FAST("RX 00007d  128 bytes  CRC OK"),
                                         READ("catalog committed / sequence 007d"),
                                         CMD("od -Ax -tx1 /tmp/frame"),
                                         FAST("000000 41 54 4c 53 02 07 03 00"),
                                         FAST("000008 00 80 00 7d 21 00 00 00"),
                                         FAST("000010 5a c3 18 7f 00 01 40 2a"),
                                         OUT("000018 8f 21 a0 6c 00 00 00 00"),
                                         CMD("sum /tmp/frame"),
                                         OUT("38412 1 /tmp/frame"),
                                         CMD("relay --verify"),
                                         BAR("Verify spool"),
                                         OUT("128 records / checksums match"),
                                         CMD("mail -p"),
                                         OUT("Mail spool: operator / 1 message"),
                                         OUT("/var/mail/operator: 1 message"),
                                         OUT("From watch@relay-03 Fri Oct 01"),
                                         OUT("Subject: next station pass"),
                                         READ("Window 03:20 UTC. Keep carrier warm."),
                                         CMD("cat /etc/motd"),
                                         READ("NODE-07 / KEEP THE NIGHT WATCH"),
                                         CMD("date -u"),
                                         OUT("Fri Oct 01 03:16:42 UTC 1993"),
                                         CMD("relay --poll"),
                                         OUT("poll 01: no pending messages"),
                                         OUT("poll 02: heartbeat acknowledged"),
                                         CMD("atq"),
                                         OUT("JOB  OWNER  WHEN         COMMAND"),
                                         FAST("1138 ops    03:18 UTC    survey"),
                                         FAST("1701 ops    03:21 UTC    ephemeris"),
                                         OUT("2049 ops    03:24 UTC    weather"),
                                         CMD("cat /etc/watch.conf"),
                                         OUT("shift=night  terminal=vt220"),
                                         OUT("fallback=manual  autopilot=off"),
                                         OUT("operator_override=required"),
                                         CMD("watch --dispatch 1138"),
                                         OUT("job 1138: acquiring survey queue"),
                                         FAST("task 01  clock reference      OK"),
                                         FAST("task 02  station ephemeris    OK"),
                                         FAST("task 03  receive spool        OK"),
                                         OUT("task 04  passive survey       RUN"),
                                         TRACE("survey IF"),
                                         OUT("survey complete / 4 carriers logged"),
                                         CMD("tail -6 /var/log/watch"),
                                         FAST("03:18:01 watch: survey 1138 complete"),
                                         FAST("03:18:02 mail: priority frame arrived"),
                                         FAST("03:18:02 relay: peer tag LV-426"),
                                         FAST("03:18:02 spool: custody transferred"),
                                         FAST("03:18:03 audit: operator notified"),
                                         OUT("03:18:03 watch: receive queue 1"),
                                         CMD("mail -p"),
                                         OUT("From: survey@outer-stations"),
                                         OUT("Subject: atmospheric soundings"),
                                         OUT("Origin LV-426 / recorder 04"),
                                         OUT("Wind 18 m/s. Visibility limited."),
                                         READ("Automated station. No reply needed."),
                                         CMD("relay --receipt 1138"),
                                         OUT("RX 3072 bytes / CRC verified"),
                                         READ("custody ACK sent / queue drained"),
                                         CMD("watch --schedule"),
                                         OUT("03:21  ephemeris refresh / queued"),
                                         OUT("03:24  weather digest    / queued"),
                                         OUT("03:30  contact handoff   / queued"),
                                         READ("watch active / next diagnostic pass")};
static const crt_event_t maintenance[] = {CMD("dmesg | tail -8"),
                                          FAST("sd0: 20MB, 512 byte sectors"),
                                          FAST("sd0a: mounted on /, read-write"),
                                          FAST("le0: ethernet address 08:00:20:07:00:02"),
                                          FAST("tty0: console attached"),
                                          FAST("clock: reference synchronized"),
                                          FAST("relay: receive queue 0/32"),
                                          FAST("watchdog: last kick 03:16:42"),
                                          OUT("kernel: no faults since boot"),
                                          CMD("diag --disk sd0"),
                                          OUT("SCSI inquiry: SEAGATE ST125N"),
                                          OUT("sense key 0 / no pending errors"),
                                          BAR("Read surface"),
                                          OUT("LBA 000000-008191  read OK"),
                                          FAST("LBA 008192-016383  read OK"),
                                          FAST("LBA 016384-024575  read OK"),
                                          FAST("LBA 024576-032767  read OK"),
                                          OUT("LBA 032768-040959  read OK"),
                                          READ("40960 sectors / 0 read errors"),
                                          CMD("mount"),
                                          OUT("/dev/sd0a on / type ufs (rw)"),
                                          OUT("/dev/sd0b on /var type ufs (rw)"),
                                          CMD("ls -li /var/spool"),
                                          OUT("total 64"),
                                          FAST("4096 -rw-r----- ops 8192 rx.queue"),
                                          FAST("4097 -rw-r----- ops 4096 tx.queue"),
                                          OUT("4098 -rw-r----- ops  128 relay.lock"),
                                          CMD("relay --queue"),
                                          OUT("queue  job   bytes  attempts"),
                                          FAST("TX     007e    256      0"),
                                          FAST("TX     007f    128      1"),
                                          WARN("job 007f: ACK timeout / retry queued"),
                                          CMD("relay --retry 007f"),
                                          OUT("resending sequence 007f"),
                                          OUT("peer ACK 007f / duplicate suppressed"),
                                          READ("queue empty / spool consistent"),
                                          CMD("diag --memory"),
                                          OUT("testing unused pages only"),
                                          BAR("Walking bits"),
                                          FAST("pattern 00000000  OK"),
                                          FAST("pattern ffffffff  OK"),
                                          FAST("pattern aaaaaaaa  OK"),
                                          FAST("pattern 55555555  OK"),
                                          OUT("parity errors 0 / 4096K tested"),
                                          CMD("diag --serial ttyS1"),
                                          OUT("19200 baud / 8 data / no parity"),
                                          OUT("DTR=1 RTS=1 CTS=1 DCD=1"),
                                          OUT("RX overruns=0 framing=0 parity=0"),
                                          CMD("stty -a < /dev/ttyS1"),
                                          OUT("speed 19200 baud; line = 0;"),
                                          OUT("-parenb cs8 -cstopb cread clocal"),
                                          OUT("-ixon -ixoff -echo -icanon"),
                                          CMD("cat /etc/hosts"),
                                          FAST("127.0.0.1  localhost"),
                                          FAST("10.24.7.1  relay-03 relay"),
                                          FAST("10.24.7.2  node-07 atlas"),
                                          OUT("10.24.7.8  archive-01 store"),
                                          CMD("ping -c 2 archive-01"),
                                          OUT("64 bytes: seq=0 ttl=64 time=3.4ms"),
                                          OUT("64 bytes: seq=1 ttl=64 time=3.2ms"),
                                          READ("2 sent, 2 received, 0% packet loss"),
                                          CMD("rsh store df -k"),
                                          OUT("Filesystem  kbytes  used avail cap"),
                                          OUT("/dev/sd1a   131072 65536 65536 50%"),
                                          CMD("tar tvf /tmp/watch.t"),
                                          FAST("-rw-r----- ops 2048 etc/uplink.conf"),
                                          FAST("-rw-r----- ops 8192 var/log/watch"),
                                          FAST("-rw-r----- ops  256 var/relay/index"),
                                          OUT("-rw-r----- ops 4096 var/relay/state"),
                                          CMD("sum /tmp/watch.t"),
                                          OUT("17291 40 /tmp/watch.t"),
                                          CMD("cd /tmp"),
                                          CMD("rcp watch.t store:/inbox/"),
                                          CMD("cd"),
                                          BAR("Copy archive"),
                                          OUT("20480 bytes / 11.2 Kbytes/sec"),
                                          CMD("rsh store sum inbox/watch.t"),
                                          READ("17291 40 /inbox/watch.t"),
                                          CMD("cat /etc/crontab"),
                                          OUT("0 * * * * root /usr/sbin/logrotate"),
                                          OUT("*/5 * * * * ops /usr/bin/relay --poll"),
                                          OUT("30 3 * * * ops /usr/bin/archive"),
                                          CMD("ls -l /tmp"),
                                          FAST("-rw------- ops 20480 watch.t"),
                                          FAST("-rw------- ops    32 frame"),
                                          OUT("-rw------- ops   128 diag.report"),
                                          CMD("cat /tmp/diag.report"),
                                          OUT("CPU     instruction test  PASS"),
                                          OUT("MMU     translation       PASS"),
                                          OUT("RAM     parity            PASS"),
                                          OUT("SCSI    surface read      PASS"),
                                          OUT("SERIAL  handshake         PASS"),
                                          OUT("NET     loopback          PASS"),
                                          CMD("netstat -s | head -8"),
                                          FAST("tcp:"),
                                          FAST("  2048 packets sent"),
                                          FAST("  4096 packets received"),
                                          FAST("  2 retransmitted packets"),
                                          FAST("  0 bad checksums"),
                                          FAST("udp:"),
                                          FAST("  128 datagrams received"),
                                          OUT("  0 receive buffer errors"),
                                          CMD("tail -5 /var/log/auth.log"),
                                          FAST("03:14:09 login: operator on tty0"),
                                          FAST("03:17:10 rshd: archive-01 accepted"),
                                          FAST("03:17:12 rcp: transfer complete"),
                                          FAST("03:17:13 rshd: session closed"),
                                          OUT("03:17:14 audit: no failed logins"),
                                          CMD("sync"),
                                          CMD("relay --status"),
                                          OUT("peer relay-03 / session intact"),
                                          CMD("diag --watchdog"),
                                          OUT("test mode / supervisor notified"),
                                          OUT("primary heartbeat inhibited 120 ms"),
                                          WARN("supervisor: late heartbeat / isolated"),
                                          OUT("standby heartbeat accepted"),
                                          OUT("primary heartbeat restored"),
                                          READ("failover test PASS / no session lost"),
                                          CMD("diag --ae35"),
                                          OUT("AE35 antenna-control interface"),
                                          FAST("clock recovery        PASS"),
                                          FAST("line continuity       PASS"),
                                          FAST("parity check          PASS"),
                                          READ("prediction mismatch / no fault found"),
                                          CMD("diag --clock failover"),
                                          OUT("reference A -> reference B"),
                                          TRACE("clock IF"),
                                          OUT("phase settled / step +0.002 ms"),
                                          OUT("reference B -> reference A"),
                                          READ("both references within tolerance"),
                                          CMD("diag --rom-sum"),
                                          FAST("bank 00  32768 bytes  4e71  PASS"),
                                          FAST("bank 01  32768 bytes  6000  PASS"),
                                          OUT("service label: TYRELL / rev C"),
                                          READ("ROM checksum matches service record"),
                                          CMD("diag --fault-log"),
                                          OUT("ID    MODULE  RESULT   DISPOSITION"),
                                          FAST("1201  CPU     CLEARED  retry complete"),
                                          FAST("1202  MMU     CLEARED  queue drained"),
                                          OUT("1138  RADIO   CLEARED  carrier stable"),
                                          CMD("diag --report"),
                                          BAR("Commit test record"),
                                          OUT("27 checks / 0 open faults"),
                                          READ("maintenance complete / watch active")};
static const crt_event_t telemetry[] = {CMD("date -u"),
                                        OUT("Fri Oct 01 03:20:00 UTC 1993"),
                                        CMD("cat /proc/sensors"),
                                        OUT("TEMP   +21.4C    BUS   12.02V"),
                                        OUT("BATT   94%      SOLAR  0.00A"),
                                        OUT("ANT AZ 127.3    EL    +42.1"),
                                        READ("OSC    +0.8ppm  HEATER OFF"),
                                        CMD("relay --scan"),
                                        BAR("Sweep 400-480 MHz"),
                                        FAST("410.0 MHz  noise       -112 dBm"),
                                        FAST("420.0 MHz  beacon-03    -67 dBm"),
                                        FAST("436.5 MHz  beacon-09    -82 dBm"),
                                        OUT("468.0 MHz  noise       -109 dBm"),
                                        CMD("relay --lock beacon-03"),
                                        WARN("doppler offset +1.2 kHz"),
                                        OUT("tracking correction applied"),
                                        BAR("Carrier lock"),
                                        READ("PLL locked / phase error 0.03 rad"),
                                        CMD("antenna --track node-03"),
                                        OUT("ephemeris age 00:04:12 / valid"),
                                        OUT("slew AZ 127.3 -> 128.1"),
                                        OUT("slew EL 42.1 -> 42.7"),
                                        READ("tracking / rate 0.12 deg/sec"),
                                        CMD("relay --rx 12"),
                                        FAST("frame 0001 / CRC OK / 256 bytes"),
                                        FAST("frame 0002 / CRC OK / 256 bytes"),
                                        WARN("frame 0003 / CRC FAIL / NAK sent"),
                                        FAST("frame 0003 / CRC OK / 256 bytes"),
                                        FAST("frame 0004 / CRC OK / 256 bytes"),
                                        FAST("frame 0005 / CRC OK / 256 bytes"),
                                        FAST("frame 0006 / CRC OK / 256 bytes"),
                                        FAST("frame 0007 / CRC OK / 256 bytes"),
                                        FAST("frame 0008 / CRC OK / 256 bytes"),
                                        FAST("frame 0009 / CRC OK / 256 bytes"),
                                        FAST("frame 000a / CRC OK / 256 bytes"),
                                        FAST("frame 000b / CRC OK / 256 bytes"),
                                        OUT("frame 000c / CRC OK / 256 bytes"),
                                        READ("3072 bytes / 1 retry / no gaps"),
                                        CMD("od -Ax -tx1 /tmp/telemetry"),
                                        FAST("000000 54 4c 4d 02 03 00 00 0c"),
                                        FAST("000008 08 5c 04 b2 00 5e 00 00"),
                                        FAST("000010 05 01 01 ab 00 08 00 03"),
                                        FAST("000018 00 00 2a 10 7f 82 31 a6"),
                                        OUT("000020 00 01 00 00 00 00 00 00"),
                                        CMD("telemetry --decode"),
                                        OUT("node=03 sequence=000c version=2"),
                                        OUT("clock=03:20:14 UTC quality=LOCK"),
                                        OUT("temp=21.40C voltage=12.02V"),
                                        OUT("battery=94% heater=OFF"),
                                        OUT("attitude=0.12,-0.03,127.30 deg"),
                                        READ("status=NOMINAL alarms=0"),
                                        CMD("telemetry --history 8"),
                                        OUT("UTC       TEMP  VOLTS  RSSI"),
                                        FAST("03:18:30  21.3  12.01  -69"),
                                        FAST("03:18:45  21.3  12.02  -68"),
                                        FAST("03:19:00  21.4  12.01  -68"),
                                        FAST("03:19:15  21.4  12.02  -67"),
                                        FAST("03:19:30  21.4  12.02  -67"),
                                        FAST("03:19:45  21.4  12.03  -66"),
                                        FAST("03:20:00  21.4  12.02  -67"),
                                        OUT("03:20:15  21.4  12.02  -67"),
                                        CMD("relay --trace 8"),
                                        FAST("RX 03>07 DATA seq=000d len=256"),
                                        FAST("TX 07>03 ACK  seq=000d len=0"),
                                        FAST("RX 03>07 DATA seq=000e len=256"),
                                        FAST("TX 07>03 ACK  seq=000e len=0"),
                                        FAST("RX 03>07 DATA seq=000f len=128"),
                                        FAST("TX 07>03 ACK  seq=000f len=0"),
                                        FAST("TX 07>03 KEEP seq=0010 len=0"),
                                        OUT("RX 03>07 ACK  seq=0010 len=0"),
                                        CMD("relay --ber"),
                                        BAR("Sample bit errors"),
                                        OUT("bits 1048576 / errors 21"),
                                        READ("BER 0.000020 / link margin 8.4 dB"),
                                        CMD("cat /tmp/message"),
                                        OUT("From: watch@node-03"),
                                        OUT("To: operator@node-07"),
                                        OUT("Subject: station report 0042"),
                                        OUT("Position fixed. Awaiting sunrise."),
                                        READ("All systems holding. Send next pass."),
                                        CMD("relay --ack 0042"),
                                        OUT("ACK sent / sequence 0042"),
                                        READ("round trip 4.82 sec / peer confirmed"),
                                        CMD("orbit --next-pass"),
                                        OUT("NODE    AOS UTC   LOS UTC   MAX EL"),
                                        FAST("03      03:20:00  03:32:14   58.2"),
                                        FAST("09      04:01:08  04:09:42   31.7"),
                                        OUT("12      04:27:33  04:38:01   44.9"),
                                        CMD("antenna --limits"),
                                        OUT("AZ 0..360  EL 5..85  SOFT LIMITS ON"),
                                        OUT("AZ error +0.02  EL error -0.01"),
                                        OUT("motors IDLE / encoders valid"),
                                        CMD("telemetry --store"),
                                        BAR("Write watch record"),
                                        OUT("record 0042 / 4096 bytes committed"),
                                        CMD("ls -l /var/telemetry"),
                                        FAST("-rw-r----- ops 4096 pass-0040.dat"),
                                        FAST("-rw-r----- ops 4096 pass-0041.dat"),
                                        OUT("-rw-r----- ops 4096 pass-0042.dat"),
                                        CMD("clock --status"),
                                        OUT("reference node-03 / stratum 2"),
                                        READ("offset +0.002 sec / drift +0.8 ppm"),
                                        CMD("telemetry --alarms"),
                                        OUT("0 active / 1 cleared this watch"),
                                        CMD("relay --window"),
                                        READ("TX 8/8 free / RX 0/8 pending"),
                                        CMD("relay --poll"),
                                        OUT("beacon-03 heartbeat / status nominal"),
                                        OUT("beacon-09 queued for next pass"),
                                        CMD("orbit --occultation 03"),
                                        OUT("shadow entry 03:30:12 UTC"),
                                        OUT("expected dropout 4.8 seconds"),
                                        CMD("relay --handoff 09"),
                                        OUT("beacon-09 pilot detected / -82 dBm"),
                                        TRACE("backup pilot"),
                                        WARN("primary carrier below squelch"),
                                        OUT("session held / TX queue frozen"),
                                        FAST("buffer frame 0011 / slot 1 of 8"),
                                        FAST("buffer frame 0012 / slot 2 of 8"),
                                        FAST("buffer frame 0013 / slot 3 of 8"),
                                        OUT("backup carrier accepted / -76 dBm"),
                                        BAR("Align receive window"),
                                        FAST("replay frame 0011 / ACK"),
                                        FAST("replay frame 0012 / ACK"),
                                        OUT("replay frame 0013 / ACK"),
                                        READ("session resumed / no missing frames"),
                                        CMD("telemetry --weather"),
                                        OUT("STATION   TEMP   PRESSURE  STATE"),
                                        FAST("caladan   17.2C  101.3kPa  RAIN"),
                                        FAST("tycho    -18.4C   72.1kPa  CLEAR"),
                                        OUT("ceres-09 -42.0C   61.0kPa  CLOUD"),
                                        CMD("relay --message 1701"),
                                        OUT("From: navigation@survey-11"),
                                        OUT("Subject: chart revision 1701"),
                                        OUT("Local anomalies added to catalog."),
                                        READ("Use passive sensors near sector 7."),
                                        CMD("telemetry --radiation"),
                                        OUT("background 0.12 uSv/h / stable"),
                                        OUT("event count 42 / threshold clear"),
                                        CMD("relay --handoff-status"),
                                        OUT("active=09 standby=03 window=8"),
                                        READ("watch continues / carrier maintained")};
static const crt_event_t network[] = {CMD("hostname"),
                                      OUT("node-07"),
                                      CMD("netstat -i"),
                                      OUT("Name Mtu  Ipkts Ierrs Opkts Oerrs"),
                                      OUT("eth0 1500 16384     0  8192     0"),
                                      OUT("lo0  4096   128     0   128     0"),
                                      CMD("host archive-01"),
                                      OUT("archive-01 has address 10.24.7.8"),
                                      CMD("host relay-03"),
                                      OUT("relay-03 has address 10.24.7.1"),
                                      CMD("traceroute 10.42.3.2"),
                                      OUT("traceroute: 30 hops max, 40 bytes"),
                                      OUT("1  relay-03  2.1 ms  1.9 ms  2.0 ms"),
                                      OUT("2  gateway   8.4 ms  8.1 ms  8.3 ms"),
                                      READ("3  node-03   2410 ms  2409 ms  2411 ms"),
                                      CMD("route -n get 10.42.3.2"),
                                      OUT("destination: 10.42.3.2"),
                                      OUT("gateway: 10.24.7.1"),
                                      OUT("interface: eth0 / flags: UP,GATEWAY"),
                                      CMD("tcpdump -c 8 -n"),
                                      FAST("03:22:01 10.24.7.2.1025 > .1.4200: S"),
                                      FAST("03:22:01 10.24.7.1.4200 > .2.1025: S."),
                                      FAST("03:22:01 10.24.7.2.1025 > .1.4200: ."),
                                      FAST("03:22:02 10.24.7.1.4200 > .2.1025: P."),
                                      FAST("03:22:02 10.24.7.2.1025 > .1.4200: ."),
                                      FAST("03:22:02 10.24.7.1.4200 > .2.1025: P."),
                                      FAST("03:22:02 10.24.7.2.1025 > .1.4200: ."),
                                      OUT("03:22:03 10.24.7.2.1025 > .1.4200: P."),
                                      READ("8 packets received / 0 dropped"),
                                      CMD("netstat -m"),
                                      OUT("64/256 mbufs in use"),
                                      OUT("16/64 cluster buffers in use"),
                                      OUT("0 requests denied / 0 delayed"),
                                      CMD("cat /etc/resolv.conf"),
                                      OUT("domain orbital.local"),
                                      OUT("nameserver 10.24.7.1"),
                                      CMD("ping -c 3 10.24.7.8"),
                                      OUT("64 bytes: seq=0 ttl=64 time=3.2ms"),
                                      OUT("64 bytes: seq=1 ttl=64 time=3.4ms"),
                                      OUT("64 bytes: seq=2 ttl=64 time=3.1ms"),
                                      READ("3 sent, 3 received, 0% packet loss"),
                                      CMD("diag --loopback eth0"),
                                      BAR("Loopback packets"),
                                      OUT("64 byte frames    256/256 OK"),
                                      OUT("512 byte frames   256/256 OK"),
                                      OUT("1500 byte frames  256/256 OK"),
                                      CMD("netstat -s | tail -6"),
                                      FAST("icmp:"),
                                      FAST("  9 echo requests sent"),
                                      FAST("  9 echo replies received"),
                                      FAST("  0 unreachable messages"),
                                      FAST("  0 time exceeded messages"),
                                      OUT("  0 bad checksums"),
                                      CMD("relay --routes"),
                                      OUT("NODE  VIA   COST  AGE  STATE"),
                                      FAST("03    direct  1    12  UP"),
                                      FAST("09    03      2    31  UP"),
                                      OUT("12    09      3    48  STANDBY"),
                                      CMD("relay --probe 09"),
                                      OUT("probe sent / path 07>03>09"),
                                      READ("reply received / round trip 8.12 sec"),
                                      CMD("relay --probe 12"),
                                      WARN("node-12: outside contact window"),
                                      READ("probe deferred until next pass"),
                                      CMD("relay --clock"),
                                      OUT("local 03:23:16.002"),
                                      OUT("peer  03:23:16.000"),
                                      READ("offset +2 ms / synchronized"),
                                      CMD("cat /etc/inetd.conf"),
                                      OUT("telnet stream tcp nowait root telnetd"),
                                      OUT("shell  stream tcp nowait root rshd"),
                                      CMD("finger operator"),
                                      OUT("Login: operator   Name: Night Watch"),
                                      OUT("On since Oct 01 03:14 on tty0"),
                                      READ("Plan: keep relay-03 in contact."),
                                      CMD("relay --stats"),
                                      OUT("TX frames 8192 / RX frames 16384"),
                                      OUT("retries 3 / duplicates 1 / gaps 0"),
                                      CMD("relay --reset-stats"),
                                      OUT("interval counters cleared"),
                                      CMD("netstat -an | head -4"),
                                      OUT("Active Internet connections"),
                                      OUT("tcp 0 0 *.23          *.* LISTEN"),
                                      OUT("tcp 0 0 *.4200        *.* LISTEN"),
                                      OUT("tcp 0 0 10.24.7.2.1025    ESTABLISHED"),
                                      CMD("logger network-check-ok"),
                                      CMD("tail -3 /var/log/messages"),
                                      FAST("03:23:20 inetd: active services 2"),
                                      FAST("03:23:21 relay: route table stable"),
                                      OUT("03:23:22 ops: network-check-ok"),
                                      CMD("relay --keepalive"),
                                      OUT("heartbeat sent / ACK received"),
                                      CMD("relay --directory"),
                                      OUT("TAG       SERVICE        STATE"),
                                      FAST("tma-1     passive survey  QUIET"),
                                      FAST("wopr      route model     IDLE"),
                                      FAST("moya      courier         TRANSIT"),
                                      OUT("tycho     time reference  LOCK"),
                                      CMD("relay --route-test wopr"),
                                      OUT("model loaded / simulation only"),
                                      FAST("path 07>03>09     cost 2.1"),
                                      FAST("path 07>08>09     cost 2.4"),
                                      OUT("selected 07>03>09 / no changes sent"),
                                      CMD("netstat -i | tail -1"),
                                      OUT("eth0: 3 collisions / 16384 frames"),
                                      OUT("all recovered by exponential backoff"),
                                      CMD("relay --loop-check"),
                                      OUT("probe TTL 8 / sequence 1701"),
                                      FAST("node 03: hop 1 / seen once"),
                                      FAST("node 09: hop 2 / seen once"),
                                      FAST("node 12: hop 3 / seen once"),
                                      READ("no forwarding loops detected"),
                                      CMD("relay --listen 4"),
                                      FAST("RX 09>07 PRIORITY len=128 crc=OK"),
                                      FAST("RX 03>07 KEEP     len=0   crc=OK"),
                                      FAST("RX 08>07 CUSTODY  len=64  crc=OK"),
                                      OUT("RX 09>07 ACK      len=0   crc=OK"),
                                      CMD("relay --priority"),
                                      OUT("job 2049 / weather advisory"),
                                      OUT("priority 2 / expires in 30 minutes"),
                                      READ("forwarded to watch queue / receipt OK"),
                                      CMD("relay --traffic"),
                                      OUT("WINDOW UTC   RX    TX    LOSS"),
                                      FAST("03:30:00    ####  ##     0"),
                                      FAST("03:30:10    ####  ###    0"),
                                      FAST("03:30:20    ##    ##     0"),
                                      OUT("03:30:30    ##### ###    0"),
                                      READ("network pass complete / no faults")};
static const crt_event_t archive[] = {CMD("pwd"),
                                      OUT("/home/operator"),
                                      CMD("ls -la"),
                                      OUT("total 12"),
                                      FAST("drwxr-x--- ops 512 ."),
                                      FAST("drwxr-xr-x root 512 .."),
                                      FAST("-rw-r----- ops 128 .profile"),
                                      OUT("drwxr-x--- ops 512 watch"),
                                      CMD("cat .profile"),
                                      OUT("PATH=/bin:/usr/bin:/usr/local/bin"),
                                      OUT("TERM=vt220; export TERM PATH"),
                                      OUT("umask 027"),
                                      CMD("ls -l watch"),
                                      FAST("-rw-r----- ops 4096 pass-0040.dat"),
                                      FAST("-rw-r----- ops 4096 pass-0041.dat"),
                                      OUT("-rw-r----- ops 4096 pass-0042.dat"),
                                      CMD("wc -c watch/*"),
                                      FAST("4096 watch/pass-0040.dat"),
                                      FAST("4096 watch/pass-0041.dat"),
                                      FAST("4096 watch/pass-0042.dat"),
                                      OUT("12288 total"),
                                      CMD("sum watch/*"),
                                      FAST("19204 8 watch/pass-0040.dat"),
                                      FAST("28413 8 watch/pass-0041.dat"),
                                      OUT("31027 8 watch/pass-0042.dat"),
                                      CMD("cat /var/relay/index"),
                                      OUT("PASS  NODE  FRAMES  CRC       STATE"),
                                      FAST("0040  03    16      3a81c502  STORED"),
                                      FAST("0041  09     8      78e2441f  STORED"),
                                      OUT("0042  03    16      8f21a06c  STORED"),
                                      CMD("find watch -type f"),
                                      FAST("watch/pass-0040.dat"),
                                      FAST("watch/pass-0041.dat"),
                                      OUT("watch/pass-0042.dat"),
                                      CMD("tar cf /tmp/pass.tar watch"),
                                      BAR("Pack watch data"),
                                      OUT("3 files / 12288 payload bytes"),
                                      CMD("tar tf /tmp/pass.tar"),
                                      FAST("watch/"),
                                      FAST("watch/pass-0040.dat"),
                                      FAST("watch/pass-0041.dat"),
                                      OUT("watch/pass-0042.dat"),
                                      CMD("archive --compress 0042"),
                                      BAR("Compress archive"),
                                      OUT("writing /var/spool/archive/pass-0042.Z"),
                                      READ("20480 -> 7142 bytes / 65% saved"),
                                      CMD("ls -l /var/spool/archive"),
                                      OUT("-rw-r----- ops 7142 pass-0042.Z"),
                                      CMD("archive --verify 0042"),
                                      BAR("Check archive"),
                                      OUT("header magic 1f 9d / 16-bit codes"),
                                      OUT("3 members / all checksums match"),
                                      CMD("archive --send 0042"),
                                      FAST("TX block 0000 1024 bytes ACK"),
                                      FAST("TX block 0001 1024 bytes ACK"),
                                      FAST("TX block 0002 1024 bytes ACK"),
                                      WARN("TX block 0003 ACK late / waiting"),
                                      FAST("TX block 0003 1024 bytes ACK"),
                                      FAST("TX block 0004 1024 bytes ACK"),
                                      FAST("TX block 0005 1024 bytes ACK"),
                                      OUT("TX block 0006  998 bytes ACK"),
                                      READ("7142 bytes stored on archive-01"),
                                      CMD("archive --receipt 0042"),
                                      OUT("peer=archive-01 sequence=0042"),
                                      OUT("bytes=7142 checksum=8f21a06c"),
                                      READ("receipt committed / remote copy good"),
                                      CMD("cat /var/relay/manifest"),
                                      OUT("0040 node-03 local remote verified"),
                                      OUT("0041 node-09 local remote verified"),
                                      OUT("0042 node-03 local remote verified"),
                                      CMD("archive --check-manifest"),
                                      OUT("3 matches / no unverified records"),
                                      CMD("archive --retention"),
                                      OUT("policy: keep last 32 passes locally"),
                                      OUT("oldest 0011 / newest 0042"),
                                      READ("remote retention 365 days"),
                                      CMD("archive --space"),
                                      OUT("local spool 64K / limit 1024K"),
                                      OUT("remote disk 50% / 65536K available"),
                                      CMD("archive --dry-run"),
                                      OUT("0 pending / 0 expired / 0 orphaned"),
                                      CMD("ls -l /var/log/archive"),
                                      OUT("-rw-r----- ops 2048 archive"),
                                      CMD("tail -4 /var/log/archive"),
                                      FAST("03:24:10 archive: packed pass 0042"),
                                      FAST("03:24:11 archive: checksum verified"),
                                      FAST("03:24:14 archive: remote ACK 0042"),
                                      OUT("03:24:14 archive: manifest committed"),
                                      CMD("df -k /var"),
                                      OUT("Filesystem kbytes used avail cap"),
                                      OUT("/dev/sd0b    8192 2048 6144 25%"),
                                      CMD("sync"),
                                      CMD("archive --status"),
                                      CMD("archive --catalog"),
                                      OUT("ID    CONTENT             COPIES"),
                                      FAST("0937  special-order.dat       2"),
                                      FAST("1138  passive-survey.dat      2"),
                                      FAST("1701  navigation-chart.dat    2"),
                                      OUT("2049  offworld-weather.dat    2"),
                                      CMD("archive --scrub 1138"),
                                      BAR("Read redundant copy"),
                                      WARN("block 0031: local checksum mismatch"),
                                      OUT("remote copy checksum valid"),
                                      OUT("local block recovered from mirror"),
                                      READ("catalog 1138 verified / 2 good copies"),
                                      CMD("archive --parity"),
                                      FAST("stripe 0000  data 4/4  parity OK"),
                                      FAST("stripe 0001  data 4/4  parity OK"),
                                      FAST("stripe 0002  data 4/4  parity OK"),
                                      OUT("stripe 0003  data 4/4  parity OK"),
                                      CMD("archive --metadata 2049"),
                                      OUT("origin: offworld weather bureau"),
                                      OUT("category: c-beam calibration"),
                                      OUT("district: tannhauser / station 02"),
                                      READ("instrument record / no active alerts"),
                                      CMD("archive --duplicates"),
                                      OUT("6 matching blocks / shared safely"),
                                      OUT("saved 3072 bytes / no records removed"),
                                      CMD("archive --restore-test"),
                                      BAR("Restore scratch copy"),
                                      FAST("header   magic       PASS"),
                                      FAST("index    128 records PASS"),
                                      FAST("payload  CRC32       PASS"),
                                      OUT("mirror   comparison  PASS"),
                                      READ("scratch copy discarded / test complete"),
                                      CMD("archive --receipt-log"),
                                      FAST("0937 remote accepted / sealed"),
                                      FAST("1138 remote accepted / repaired"),
                                      FAST("1701 remote accepted / sealed"),
                                      OUT("2049 remote accepted / sealed"),
                                      READ("archive current / waiting for frames")};
static const crt_event_t signal[] = {CMD("receiver --status"),
                                     OUT("RX channel 03 / center 420.000 MHz"),
                                     OUT("filter 12.5 kHz / gain 28 dB"),
                                     READ("AGC settled / squelch open"),
                                     CMD("receiver --spectrum"),
                                     OUT("OFFSET kHz   LEVEL    POWER"),
                                     FAST("-6.0        -108 dBm  ."),
                                     FAST("-4.0        -101 dBm  .."),
                                     FAST("-2.0         -86 dBm  #####"),
                                     FAST(" 0.0         -67 dBm  ##########"),
                                     FAST("+2.0         -85 dBm  #####"),
                                     FAST("+4.0        -100 dBm  .."),
                                     OUT("+6.0        -109 dBm  ."),
                                     CMD("receiver --noise"),
                                     BAR("Measure noise floor"),
                                     OUT("mean -110.2 dBm / peak -104.1 dBm"),
                                     READ("signal-to-noise 43.2 dB"),
                                     CMD("receiver --agc"),
                                     OUT("target -12 dBFS / actual -12.3 dBFS"),
                                     OUT("attack 10 ms / release 250 ms"),
                                     CMD("receiver --afc"),
                                     OUT("offset +1180 Hz / correction -1180"),
                                     OUT("residual +2 Hz / loop stable"),
                                     CMD("antenna --position"),
                                     OUT("AZ 128.10 deg / EL 42.70 deg"),
                                     OUT("command 128.12 / 42.69"),
                                     CMD("antenna --encoders"),
                                     FAST("AZ raw 23319 / index valid"),
                                     FAST("EL raw 07773 / index valid"),
                                     OUT("missed counts 0 / limits clear"),
                                     CMD("orbit --elements 03"),
                                     OUT("epoch 1993:274:03:00:00 UTC"),
                                     OUT("inclination 51.60 deg"),
                                     OUT("eccentricity 0.000312"),
                                     READ("mean motion 15.52 rev/day"),
                                     CMD("orbit --propagate 03"),
                                     BAR("Propagate track"),
                                     OUT("UTC       AZ     EL    RANGE km"),
                                     FAST("03:26:00  130.2  48.1   612.4"),
                                     FAST("03:27:00  135.8  54.0   558.2"),
                                     FAST("03:28:00  143.1  58.2   531.7"),
                                     OUT("03:29:00  151.4  56.9   540.2"),
                                     CMD("receiver --samples 8"),
                                     FAST("I +0124 Q -0083  phase -0.59"),
                                     FAST("I +0142 Q -0041  phase -0.28"),
                                     FAST("I +0148 Q +0004  phase +0.03"),
                                     FAST("I +0138 Q +0049  phase +0.34"),
                                     FAST("I +0116 Q +0089  phase +0.65"),
                                     FAST("I +0081 Q +0123  phase +0.99"),
                                     FAST("I +0039 Q +0144  phase +1.31"),
                                     OUT("I -0007 Q +0149  phase +1.62"),
                                     CMD("receiver --pll"),
                                     OUT("phase error 0.03 rad / LOCK"),
                                     OUT("loop bandwidth 25 Hz"),
                                     CMD("relay --fec"),
                                     OUT("convolutional decoder / rate 1/2"),
                                     OUT("decoded blocks 128 / corrected 7"),
                                     READ("uncorrectable 0 / interleave depth 8"),
                                     CMD("relay --frame-sync"),
                                     BAR("Correlate sync word"),
                                     OUT("sync 1acffc1d / correlation 32/32"),
                                     OUT("byte alignment 0 / bit slips 0"),
                                     CMD("receiver --levels"),
                                     OUT("RF -67 dBm / IF -12.3 dBFS"),
                                     OUT("ADC clipping 0 / headroom 12.3 dB"),
                                     CMD("cat /proc/power"),
                                     OUT("BUS 12.02V / CURRENT 0.42A"),
                                     OUT("RX  0.18A  CPU 0.12A  AUX 0.12A"),
                                     CMD("cat /proc/thermal"),
                                     OUT("CPU 31.2C  RF 28.4C  CASE 21.4C"),
                                     READ("fan OFF / heater OFF / limits OK"),
                                     CMD("clock --compare 03"),
                                     OUT("8 samples / median offset +2 ms"),
                                     FAST("sample 01 +2.1 ms"),
                                     FAST("sample 02 +1.9 ms"),
                                     FAST("sample 03 +2.0 ms"),
                                     OUT("jitter 0.2 ms / reference valid"),
                                     CMD("antenna --predict"),
                                     OUT("next correction AZ +0.12 EL +0.08"),
                                     OUT("tracking rate within motor limits"),
                                     CMD("receiver --calibrate"),
                                     BAR("Reference channel"),
                                     OUT("reference 10.000000 MHz"),
                                     READ("trim +0.8 ppm / calibration valid"),
                                     CMD("relay --quality"),
                                     OUT("carrier LOCK / frame sync LOCK"),
                                     OUT("SNR 43.2 dB / BER 0.000020"),
                                     OUT("latency 4.82 sec / margin 8.4 dB"),
                                     CMD("relay --watch"),
                                     CMD("receiver --interference"),
                                     TRACE("RF 420 MHz"),
                                     WARN("narrow spur detected / +4.2 kHz"),
                                     OUT("source local oscillator / not uplink"),
                                     CMD("receiver --notch 4200"),
                                     OUT("notch enabled / width 120 Hz"),
                                     TRACE("filtered IF"),
                                     READ("spur suppressed 18 dB / carrier valid"),
                                     CMD("receiver --pulse-test"),
                                     OUT("injecting five reference pulses"),
                                     FAST("01  rise 12 us  width 250 us  OK"),
                                     FAST("02  rise 11 us  width 250 us  OK"),
                                     FAST("03  rise 12 us  width 250 us  OK"),
                                     FAST("04  rise 11 us  width 250 us  OK"),
                                     OUT("05  rise 12 us  width 250 us  OK"),
                                     CMD("receiver --drift 6"),
                                     OUT("UTC       OFFSET Hz   LOOP"),
                                     FAST("03:32:00  +1180       LOCK"),
                                     FAST("03:32:10  +1172       LOCK"),
                                     FAST("03:32:20  +1164       LOCK"),
                                     FAST("03:32:30  +1156       LOCK"),
                                     FAST("03:32:40  +1148       LOCK"),
                                     OUT("03:32:50  +1140       LOCK"),
                                     CMD("receiver --catalog"),
                                     OUT("TAG      CLASS        ACTION"),
                                     FAST("b-612    survey buoy   RECORD"),
                                     FAST("tma-1    silent marker PASSIVE"),
                                     OUT("r-42     time beacon   TRACK"),
                                     CMD("receiver --record b-612"),
                                     OUT("bearing 127.3 / range unresolved"),
                                     TRACE("survey carrier"),
                                     READ("sample saved / no interrogation sent"),
                                     CMD("antenna --trim"),
                                     OUT("AZ +0.02 / EL -0.01 / settled"),
                                     READ("automatic tracking / watch continues")};
const crt_event_t *crt_boot_script(unsigned *count)
{
    *count = sizeof(startup) / sizeof(*startup);
    return startup;
}
const crt_event_t *crt_script(unsigned profile, unsigned *count)
{
    switch (profile % CRT_PROFILES) {
    case 1:
        *count = sizeof(maintenance) / sizeof(*maintenance);
        return maintenance;
    case 2:
        *count = sizeof(telemetry) / sizeof(*telemetry);
        return telemetry;
    case 3:
        *count = sizeof(network) / sizeof(*network);
        return network;
    case 4:
        *count = sizeof(archive) / sizeof(*archive);
        return archive;
    case 5:
        *count = sizeof(signal) / sizeof(*signal);
        return signal;
    default:
        *count = sizeof(operations) / sizeof(*operations);
        return operations;
    }
}
static uint32_t cycle_time(unsigned profile)
{
    unsigned n;
    const crt_event_t *e = crt_script(profile, &n);
    uint32_t t = 0;
    for (unsigned i = 0; i < n; i++) {
        t += e[i].duration;
    }
    return t;
}
static const crt_event_t *current(const crt_t *s, unsigned *count)
{
    return s->booting ? crt_boot_script(count) : crt_script(s->profile, count);
}
static void append(crt_t *s, crt_kind_t kind)
{
    if (s->visible == CRT_ROWS) {
        memmove(s->lines, s->lines + 1, (CRT_ROWS - 1) * sizeof(*s->lines));
        s->visible--;
    }
    crt_line_t *l = &s->lines[s->visible++];
    memset(l, 0, sizeof(*l));
    l->kind = kind;
}
static unsigned prompt_length(const char *text)
{
    const char *prompt = strchr(text, '$');
    if (!prompt) {
        prompt = strchr(text, '#');
    }
    return prompt ? (unsigned)(prompt - text) + 2 : 0;
}
static void update_line(crt_t *s, const crt_event_t *e, bool complete)
{
    crt_line_t *line = &s->lines[s->visible - 1];
    if (e->kind == CRT_TRACE) {
        /* A deterministic narrow carrier peak with a shifting noise floor. Keeping
         * the end snapshot fixed makes large and small time steps agree exactly. */
        uint32_t tick = (complete ? e->duration : s->event_ms) / 90;
        char trace[19];
        for (int k = 0; k < 18; k++) {
            int distance = k - 9 - (int)(tick % 3) + 1;
            if (distance < 0) {
                distance = -distance;
            }
            unsigned noise = s->seed + tick * 26699u + (unsigned)k * 1973u;
            noise ^= noise << 13;
            noise ^= noise >> 17;
            int height = 9 - distance + (int)(noise & 1);
            if (height < 0) {
                height = 0;
            }
            if (height > 10) {
                height = 10;
            }
            trace[k] = ".:-=+#"[height / 2];
        }
        trace[18] = 0;
        snprintf(line->text, sizeof(line->text), "%.*s |%s|", 16, e->text, trace);
    } else if (e->kind == CRT_PROGRESS) {
        unsigned pct = complete ? 100 : (unsigned)((uint64_t)s->event_ms * 100 / e->duration);
        char bar[11];
        for (unsigned k = 0; k < 10; k++) {
            bar[k] = k < pct / 10 ? '#' : '-';
        }
        bar[10] = 0;
        snprintf(line->text, sizeof(line->text), "%.*s [%s] %3u%%", 19, e->text, bar, pct);
    } else {
        unsigned length = (unsigned)strlen(e->text), visible = length;
        if (e->kind == CRT_COMMAND && !complete) {
            visible = prompt_length(e->text) + (s->event_ms > 100 ? (s->event_ms - 100) / 18 : 0);
            if (visible > length) {
                visible = length;
            }
        }
        snprintf(line->text, sizeof(line->text), "%.*s", (int)visible, e->text);
    }
}
void crt_init(crt_t *s, unsigned profile, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->profile = profile % CRT_PROFILES;
    s->generation = 1;
    s->seed = seed;
    s->phosphor = seed & 1;
    s->booting = true;
    s->cycle_ms = cycle_time(s->profile);
}
void crt_next(crt_t *s, uint32_t seed)
{
    unsigned generation = s->generation + 1, profile = s->profile + 1;
    crt_init(s, profile, seed);
    s->generation = generation;
}
void crt_step(crt_t *s, uint32_t ms)
{
    uint64_t previous = s->elapsed_ms;
    s->elapsed_ms += ms;
    /* Warm-up and login happen once. Automatic boundaries retain every visible
     * line, the phosphor color, and the clock; only the command playlist changes. */
    if (previous < CRT_WARMUP_MS) {
        uint32_t warmup = CRT_WARMUP_MS - (uint32_t)previous;
        if (ms < warmup) {
            return;
        }
        ms -= warmup;
    }
    unsigned count;
    const crt_event_t *events = current(s, &count);
    if (!s->visible) {
        append(s, events[s->event].kind);
    }
    while (ms >= events[s->event].duration - s->event_ms) {
        ms -= events[s->event].duration - s->event_ms;
        update_line(s, &events[s->event], true);
        s->event_ms = 0;
        s->event++;
        if (s->event == count) {
            s->event = 0;
            if (s->booting) {
                s->booting = false;
            } else {
                s->profile = (s->profile + 1) % CRT_PROFILES;
                s->generation++;
            }
            s->cycle_ms = cycle_time(s->profile);
            events = current(s, &count);
        }
        append(s, events[s->event].kind);
    }
    s->event_ms += ms;
    update_line(s, &events[s->event], false);
    s->cursor = events[s->event].kind == CRT_COMMAND && (s->elapsed_ms / 320) % 2 == 0;
}
/* All phosphor intensities use the same hue, including warnings and glow. */
static uint16_t phosphor(unsigned theme, int level)
{
    return theme == CRT_AMBER ? pin_rgb(level, level * 3 / 5, 0)
                              : pin_rgb(level / 7, level, level / 6);
}
static uint16_t ink(unsigned theme, crt_kind_t kind)
{
    int level = kind == CRT_COMMAND || kind == CRT_TITLE ? 255 : kind == CRT_WARN ? 240 : 205;
    return phosphor(theme, level);
}
static uint16_t terminal_pixels[PIN_W * PIN_H];
void crt_paint(const crt_t *s, uint16_t *p)
{
    memset(terminal_pixels, 0, sizeof(terminal_pixels));
    for (unsigned i = 0; i < s->visible; i++) {
        pin_text(terminal_pixels, CRT_TEXT_X, CRT_TEXT_Y + (int)i * CRT_ROW_PITCH, s->lines[i].text,
                 ink(s->phosphor, s->lines[i].kind));
    }
    if (s->cursor && s->visible) {
        int x = CRT_TEXT_X + (int)strlen(s->lines[s->visible - 1].text) * 6;
        if (x > 248) {
            x = 248;
        }
        pin_rect(terminal_pixels, x, CRT_TEXT_Y + ((int)s->visible - 1) * CRT_ROW_PITCH, 5, 7,
                 ink(s->phosphor, CRT_COMMAND));
    }
    int roll = (int)((s->elapsed_ms / 32) % 160) - 20;
    unsigned tick = (unsigned)(s->elapsed_ms / 80);
    /* Mild line wobble and defocused phosphor bloom, rather than garbled text. */
    int wobble = (s->elapsed_ms / 1100) % 9 == 0 ? 1 : 0;
    for (int y = 0; y < PIN_H; y++) {
        for (int x = 0; x < PIN_W; x++) {
            int edge = x < 12 ? 12 - x : x > 255 ? x - 255 : 0;
            int ey = y < 7 ? 7 - y : y > 112 ? y - 112 : 0;
            if (x < 8 || x > 259 || y < 4 || y > 115 || edge * edge + ey * ey > 70) {
                int level = 12;
                p[y * PIN_W + x] = pin_rgb(level, level, level);
                continue;
            }
            int dx = x - 134, dy = y - 60;
            int sx = x + dx * dy * dy / (56 * 56 * 29) + (y > roll && y < roll + 3 ? wobble : 0);
            int sy = y + dy * dx * dx / (126 * 126 * 28);
            int level = 0;
            if (sx > 1 && sx < PIN_W - 2 && sy > 1 && sy < PIN_H - 2) {
                int offset = sy * PIN_W + sx;
                /* Extract brightness, then combine a tight halo and a softer two-pixel glow. */
                int center = terminal_pixels[offset] ? 1 : 0;
                int near = !!terminal_pixels[offset - 1] + !!terminal_pixels[offset + 1] +
                           !!terminal_pixels[offset - PIN_W] + !!terminal_pixels[offset + PIN_W];
                int far = !!terminal_pixels[offset - 2] + !!terminal_pixels[offset + 2] +
                          !!terminal_pixels[offset - 2 * PIN_W] +
                          !!terminal_pixels[offset + 2 * PIN_W];
                uint16_t c = terminal_pixels[offset];
                level =
                    center ? (s->phosphor == CRT_AMBER ? ((c >> 11) & 31) * 8 : ((c >> 5) & 63) * 4)
                           : 0;
                level += near * 13 + far * 4;
                if (level > 255) {
                    level = 255;
                }
            }
            unsigned noise = (unsigned)x * 1973u + (unsigned)y * 9277u + tick * 26699u + s->seed;
            noise ^= noise << 13;
            noise ^= noise >> 17;
            int shade = (y & 1) ? 168 : 250;
            shade -= 40 * dx * dx / (126 * 126) + 38 * dy * dy / (56 * 56);
            if (y >= roll && y < roll + 7) {
                shade -= 24;
            }
            /* Very small whole-screen hum; the refresh band stays subdued. */
            shade -= (int)(tick % 3) * 2 + (int)(noise & 7);
            if (shade < 70) {
                shade = 70;
            }
            level = level * shade / 256;
            int ambient = 4 + (int)(noise & 3);
            ambient = ambient * (256 - 70 * dx * dx / (126 * 126) - 65 * dy * dy / (56 * 56)) / 256;
            p[y * PIN_W + x] = phosphor(s->phosphor, level + ambient);
        }
    }
    if (s->elapsed_ms < CRT_WARMUP_MS) {
        unsigned gain = (unsigned)(s->elapsed_ms * 256 / CRT_WARMUP_MS);
        if (gain < 38) {
            memset(p, 0, PIN_W * PIN_H * sizeof(*p));
            if (gain > 5) {
                pin_rect(p, 80, 58, 108, 1, ink(s->phosphor, CRT_TITLE));
            }
        } else if (gain < 102) {
            memset(p, 0, PIN_W * PIN_H * sizeof(*p));
            int half = (int)(gain - 38) * 190 / 256;
            pin_rect(p, 12, 59 - half, 244, half * 2 + 1, pin_dim(ink(s->phosphor, CRT_TITLE), 80));
        } else {
            for (int i = 0; i < PIN_W * PIN_H; i++) {
                p[i] = pin_dim(p[i], gain);
            }
        }
    }
}
