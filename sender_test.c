#include <iostream>
#include <string>
#include <bitset>
#include <b15f/b15f.h>

/* 
----------------------------------------------------
Konstante Definitionen
 ----------------------------------------------------
*/
constexpr uint8_t CLOCK_PIN_MASK      = 0x08; // PA3
constexpr uint8_t DATA_MASK           = 0x03; // PA0, PA1
constexpr uint8_t RESPONSE_PIN_MASK   = 0x04; // PA2
constexpr int BIT_PAIRS_PER_PACKAGE   = 16;   // Jedes Paket enthält 16 Bitpaare (= 32 Bits)
constexpr int DATA_DELAY_MS           = 60;   // Wartezeit (ms) nach dem Setzen der Daten
constexpr int RESPONSE_DELAY_MS       = 30;   // Wartezeit (ms) zum erneuten Prüfen von ACK/NACK
constexpr int MAX_RETRY               = 3;    // Anzahl maximaler Wiederholversuche nach NACK

/* 
----------------------------------------------------
 Globale oder statische Hilfsfunktionen (könnten
 alternativ in einer Klasse gekapselt werden)

 ----------------------------------------------------
*/


// Fügt ein 2-Bit-Paar (bitPair) in ein 32-Bit-Paket ein
// (shiftet das Paket um 2 nach links und setzt die Bits)
void addBitPairToPackage(uint32_t& package, uint8_t bitPair) {
    package = (package << 2) | (bitPair & 0x03);
}

// Extrahiert das n-te 2-Bit-Paar (von links) aus package
// bitPairIndex = 0 → links (MSB), bitPairIndex = 15 → rechts (LSB)
uint8_t extractBitPair(uint32_t package, int bitPairIndex) {
    // Shift-Basis: das linkeste 2-Bit-Paar sitzt in den Bits 31..30
    // => SHIFT = 30 - (Index*2)
    int shift = 30 - (bitPairIndex * 2);
    return (package >> shift) & 0x03;
}

// Hilfs-Enum für den Antwortzustand
enum class ResponseState {
    NO_RESPONSE, // Weder ACK noch NACK
    ACK,
    NACK
};

/* Liest den Response-Pin und entscheidet, ob ACK oder NACK anliegt.
// Falls ein erster kurzer High-Puls kommt, warten wir noch RESPONSE_DELAY_MS ms
// und prüfen erneut. Bei erneutem High => ACK, sonst => NACK.
Falls komplett low => NO_RESPONSE.
*/ 
ResponseState readResponse(B15F& drv) {
    uint8_t response = drv.getRegister(&PINA) & RESPONSE_PIN_MASK;
    if (response) {
        // Erneut prüfen nach kurzer Wartezeit
        drv.delay_ms(RESPONSE_DELAY_MS);
        response = drv.getRegister(&PINA) & RESPONSE_PIN_MASK;
        if (response) {
            return ResponseState::ACK;
        } else {
            return ResponseState::NACK;
        }
    }
    return ResponseState::NO_RESPONSE;
}

// Kleine Hilfsfunktion, um Datenbits + Clock in einem Rutsch zu setzen
// clockHigh = true => Clock-Pin wird auf 1 gesetzt; false => auf 0
void setDataAndClock(B15F& drv, uint8_t data, bool clockHigh) {
    // Aktuellen Wert lesen
    uint8_t currentRegister = drv.getRegister(&PORTA);

    // Wir löschen die Datenbits (PA0,PA1) und den Clock-Pin (PA3),
    // damit wir anschließend die gewünschten Bits setzen können.
    currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);

    // Datenbits setzen
    currentRegister |= (data & DATA_MASK);

    // Clock-Pin setzen oder löschen
    if (clockHigh) {
        currentRegister |= CLOCK_PIN_MASK;
    }
    // Register schreiben
    drv.setRegister(&PORTA, currentRegister);
}

/* 
----------------------------------------------------
 Hauptprogramm für die Sende-Logik
----------------------------------------------------
*/
 
int main() {
    // Instanz des B15F-Boards
    B15F& drv = B15F::getInstance();

    // PortA konfigurieren: PA0, PA1, PA3 als Ausgang (1),
    // PA2 als Eingang (0). In Binär: 0b1011 = 0x0B
    drv.setRegister(&DDRA, 0x0B);

    // Zustandsvariablen
    uint32_t package = 0;      // aktuelles 32-Bit-Paket
    int bitPairCount = 0;      // wie viele 2-Bit-Paare wurden in dieses Paket geschrieben?
    int retryCount   = 0;      // wie oft wurde ein Paket wiederholt?

    // Wir wechseln zwischen "neue Bitpaare senden" und "altes Paket wiederholen"
    // je nachdem, ob wir zuletzt ACK oder NACK bekommen haben.
    bool lastWasAck  = true;   // Zu Beginn erwarten wir, dass wir neue Daten lesen
    bool lastWasNack = false;  // falls wir ein NACK haben, wiederholen wir

    std::string bitPair; // Puffer zum Einlesen von STDIN (2 Bit pro Zeile)

    // Wir laufen so lange, wie wir Daten bekommen oder nach NACK
    // potenziell noch zuende senden wollen
    while (lastWasAck || lastWasNack) 
    {
        if (lastWasAck) {
            // 1) Versuche, neues Bitpaar von STDIN zu lesen
            if (std::getline(std::cin, bitPair)) {
                // 2) Konvertiere Bitpaar in Integer (0..3)
                uint8_t data = 0;
                try {
                    data = std::stoi(bitPair, nullptr, 2); // "01" -> 1 etc.
                } catch (const std::exception&) {
                    std::cerr << "Fehlerhafte Eingabe: " << bitPair << std::endl;
                    // Wir überspringen dieses Bitpaar oder beenden
                    break;
                }

                // 3) Bitpaar in Paket integrieren
                addBitPairToPackage(package, data);

                // 4) Senden: Clock hoch -> warten -> Clock runter
                setDataAndClock(drv, data, true);
                drv.delay_ms(DATA_DELAY_MS);
                setDataAndClock(drv, data, false);

                bitPairCount++;

                // 5) Falls wir 16 Bitpaare (32 Bits) gesendet haben, 
                //    prüfen wir auf ACK/NACK
                if (bitPairCount == BIT_PAIRS_PER_PACKAGE) {
                    bitPairCount = 0; // zurücksetzen
                    // Jetzt Response abfragen
                    auto resp = readResponse(drv);
                    if (resp == ResponseState::ACK) {
                        // Paket war erfolgreich
                        std::cerr << "ACK erhalten. Paket gesendet: "
                                  << std::bitset<32>(package) << std::endl;
                        // Reset
                        package    = 0;
                        retryCount = 0;
                        lastWasAck = true;  // Wir bleiben bei "Neues Paket"
                        lastWasNack = false;
                    } else if (resp == ResponseState::NACK) {
                        std::cerr << "NACK erhalten. Paket muss wiederholt werden." << std::endl;
                        lastWasAck  = false;
                        lastWasNack = true;
                        // retryCount wird erst nach dem Wiederholversuch erhöht
                    } else {
                        // Kein ACK und kein NACK => Timeout
                        std::cerr << "TIMEOUT (kein ACK/NACK) bei Paketende" << std::endl;
                        // Wir können hier entscheiden, abzubrechen
                        break;
                    }
                } // Ende if (bitPairCount == 16)
            } 
            else {
                // Keine neuen Bitpaare mehr in STDIN => Wir sind fertig
                std::cerr << "Keine weiteren Daten. Beende Übertragung." << std::endl;
                break;
            }
        } 
        else if (lastWasNack) {

            /*  Wir müssen das alte Paket wiederholen
                Wir haben bitPairCount == 0, also wir fangen wieder an, 
                die 16 Bitpaare aus "package" neu zu senden.
                (Oder wir merken uns, bei welchem 2-Bit-Paar wir weitermachen.)
            */

            int localCount = 0; // Zählt von 0..15
            retryCount++;

            // Falls wir zu oft wiederholt haben => Abbruch
            if (retryCount > MAX_RETRY) {
                std::cerr << "Zu viele NACKs => Abbruch" << std::endl;
                break;
            }

            // Wiederhole das Paket
            while (localCount < BIT_PAIRS_PER_PACKAGE) {
                uint8_t data = extractBitPair(package, localCount);
                std::cout << "Resende Bitpaar: " << std::bitset<2>(data) << std::endl;

                // Takt und Daten setzen
                setDataAndClock(drv, data, true);
                drv.delay_ms(DATA_DELAY_MS);
                setDataAndClock(drv, data, false);

                localCount++;
            }

            // Nach dem 16. Bitpaar erneut Response checken
            auto resp = readResponse(drv);
            if (resp == ResponseState::ACK) {
                // Paket war jetzt erfolgreich
                std::cerr << "ACK nach Wiederholung. Paket gesendet: "
                          << std::bitset<32>(package) << std::endl;
                // Reset
                package     = 0;
                retryCount  = 0;
                lastWasAck  = true;
                lastWasNack = false;
                bitPairCount = 0; // Vorbereitung auf neues Paket
            } else if (resp == ResponseState::NACK) {
                std::cerr << "Erneut NACK erhalten." << std::endl;
                // Wir bleiben in Schleife => retryCount++ war schon
                lastWasAck  = false;
                lastWasNack = true;
            } else {
                // Kein ACK, kein NACK
                std::cerr << "TIMEOUT (kein ACK/NACK) nach Wiederholung." << std::endl;
                break;
            }
        }
    }

    // Zum Schluss: Daten- und Taktleitungen auf LOW setzen
    uint8_t currentRegister = drv.getRegister(&PORTA);
    currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);
    drv.setRegister(&PORTA, currentRegister);

    return 0;
}
