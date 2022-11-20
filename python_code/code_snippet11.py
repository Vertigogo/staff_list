
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from os import system
from optparse import OptionParser

queries_liste = {}
quiet = False
databaseConn = None
databaseCursor = None


def process(pkt):
    global quiet
    global databaseConn
    ip46 = IPv6 if IPv6 in pkt else IP
    if pkt.haslayer(DNSQR) and UDP in pkt and pkt[UDP].sport == 53 and ip46 in pkt:
        # pkt[IP].dst == IP source of the DNS request
        # pkt[IP].src == IP of the DNS server
        # pkt[DNS].qd.qname == DNS name
        query = pkt[DNS].qd.qname.decode("utf-8") if pkt[DNS].qd != None else "?"

        if not pkt[ip46].dst in queries_liste:
            queries_liste[pkt[ip46].dst] = {}

        if not pkt[ip46].src in queries_liste[pkt[ip46].dst]:
            queries_liste[pkt[ip46].dst][pkt[ip46].src] = {}

        if not query in queries_liste[pkt[ip46].dst][pkt[ip46].src]:
            queries_liste[pkt[ip46].dst][pkt[ip46].src][query] = 1
        else:
            queries_liste[pkt[ip46].dst][pkt[ip46].src][query] += 1

        if databaseConn and query != None and query != "?":
            databaseCursor.execute("INSERT OR IGNORE INTO domains (domain) VALUES (?);", (query,))
            databaseConn.commit()

            databaseCursor.execute("SELECT idDomain FROM domains WHERE domain=?;", (query,))
            domainId = databaseCursor.fetchone()[0]

            databaseCursor.execute("SELECT count, idWhoAsk FROM whoAsk WHERE ipFrom=? AND ipTo=? AND domainId=?;",
                                   (pkt[ip46].src, pkt[ip46].dst, domainId))
            whoAsk = databaseCursor.fetchone()

            if whoAsk:
                databaseCursor.execute("UPDATE whoAsk SET count=? WHERE idWhoAsk=?",
                                       (whoAsk[0] + 1 if whoAsk[0] else 2, whoAsk[1]))
            else:
                databaseCursor.execute("INSERT INTO whoAsk (ipFrom, ipTo, domainId, count) VALUES (?,?,?,1);",
                                       (pkt[ip46].src, pkt[ip46].dst, domainId))

            databaseConn.commit()

        if not quiet:
            system('clear')
            print("{:15s} | {:15s} | {:15s} | {}".format("IP source", "DNS server", "Count DNS request", "Query"))
            for ip in queries_liste:
                print("{:15s}".format(ip))  # IP source
                for query_server in queries_liste[ip]:
                    print(" " * 18 + "{:15s}".format(query_server))  # IP of DNS server
                    for query in queries_liste[ip][query_server]:
                        print(" " * 36 + "{:19s} {}".format(str(queries_liste[ip][query_server][query]),
                                                            query))  # Count DNS request | DNS


def init_db(databasePath):
    global databaseConn
    global databaseCursor
    databaseConn = sqlite3.connect(databasePath)
    databaseCursor = databaseConn.cursor()

    databaseCursor.execute("""CREATE TABLE if not exists domains (
							idDomain INTEGER PRIMARY KEY AUTOINCREMENT,
							domain TEXT DEFAULT NULL,
							UNIQUE(domain)
						);""")
    databaseCursor.execute("""CREATE TABLE if not exists whoAsk (
							idWhoAsk INTEGER PRIMARY KEY AUTOINCREMENT,
							ipFrom TEXT DEFAULT NULL,
							ipTo TEXT DEFAULT NULL,
							domainId INTEGER,
							count INTEGER,
							UNIQUE(ipFrom, ipTo, domainId),
							FOREIGN KEY(domainId) REFERENCES domains(id)
						);""")