// Sollte ich mir aufgrund dieser schrecklichen Aufgabe mein Leben nehmen, 
// erhält Bruno Kunert einen Geldwert von 2 Red Bulls in grün

#include <iostream>
#include <string>
#include <bitset>

// B15F-spezifische Header
#include <b15f/b15f.h>

// Globale Konstanten und Masken
static const uint8_t CLOCK_PIN_MASK    = 0x08;  // PA3 als Taktleitung
static const uint8_t DATA_MASK         = 0x03;  // PA0 und PA1 als Datenleitungen
static const uint8_t RESPONSE_PIN_MASK = 0x04;  // PA2 als Antwortleitung

// Globale Zustände
static bool ack = true;
static bool nack = false;
static uint32_t package = 0;   // Aktuelles Paket (32 Bits)
static int retryCount = 0;

// Hilfsfunktionen
/**
 * @brief Hilfsfunktion zum Hinzufügen eines Bitpaares in ein uint32_t (verschieben um 2 Bits)
 */
void addBitPairToPackage(uint32_t& package, uint8_t bitPair) {
    // Schiebe das Paket um 2 Bits nach links und füge das Bitpaar hinzu
    package = (package << 2) | (bitPair & 0x03);
}

/**
 * @brief Extrahiert ein 2-Bit-Paar an Position bitPairCount (0-15) aus einem 32-Bit-Paket
 */
uint8_t extractBitPair(uint32_t package, int bitPairCount) {
    // Position der (MSB-)Bits berechnen, die wir auslesen wollen
    int shift = 30 - (bitPairCount * 2);
    return (package >> shift) & 0x03;
}

/**
 * @brief Prüft den RESPONSE-Pin auf ACK/NACK und setzt Variablen entsprechend
 */
void checkForAckOrNack(int& bitPairCount, bool& ackFlag, bool& nackFlag, B15F& drv, uint32_t& packageRef) {
    ackFlag = false;
    nackFlag = false;

    // Erste Abfrage
    uint8_t response = drv.getRegister(&PINA) & RESPONSE_PIN_MASK;
    if (response) {
        // Kurze Verzögerung, um ein stabiles Signal zu bekommen
        drv.delay_ms(30);
        response = drv.getRegister(&PINA) & RESPONSE_PIN_MASK;

        if (response) {
            ackFlag  = true;
            nackFlag = false;

            std::cerr << "Response-Pin ACK erkannt nach 16 Bitpaaren." << std::endl;
            std::cerr << "Package: " << std::bitset<32>(packageRef) << std::endl;

            // Paket zurücksetzen
            packageRef = 0;
            retryCount = 0;
        } else {
            std::cerr << "Response-Pin NACK erkannt nach 16 Bitpaaren." << std::endl;
            ackFlag  = false;
            nackFlag = true;
        }
    } else {
        ackFlag  = false;
        nackFlag = false;
        std::cerr << "TIMEOUT weil weder ACK noch NACK." << std::endl;
    }

    // Bitpaar-Zähler zurücksetzen
    bitPairCount = 0;
}

/**
 * @brief Versendet ein einzelnes 2-Bit-Paar (Taktleitung toggeln, Datenbits setzen)
 */
void sendBitPair(B15F& drv, uint8_t data) {
    // Lese aktuellen Wert von PORTA
    uint8_t currentRegister = drv.getRegister(&PORTA);

    // Maskiere Daten- und Taktleitung (PA0, PA1, PA3) -> verändere PA2 nicht
    currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);

    // Setze Datenbits
    currentRegister |= (data & DATA_MASK);

    // Taktleitung auf HIGH
    currentRegister |= CLOCK_PIN_MASK;
    drv.setRegister(&PORTA, currentRegister);

    // Wartezeit, damit der Empfänger die Daten lesen kann
    drv.delay_ms(60);

    // Taktleitung auf LOW
    currentRegister &= ~CLOCK_PIN_MASK;
    drv.setRegister(&PORTA, currentRegister);
}

/**
 * @brief Sendet ein neues Bitpaar aus der stdin, fügt es dem package hinzu, 
 *        und prüft ggf. auf ACK/NACK
 */
void sendPackage(B15F& drv, int& bitPairCount, std::string& bitPair, uint32_t& packageRef) {
    // Prüfe, ob neue Bits verfügbar sind
    if (std::getline(std::cin, bitPair)) {
        // Debug-Ausgabe
        std::cout << "Bitpaar: " << bitPair << std::endl;

        // String "00"/"01"/"10"/"11" -> uint8_t
        uint8_t data = std::stoi(bitPair, nullptr, 2);
        addBitPairToPackage(packageRef, data);

        // Sende dieses Bitpaar
        sendBitPair(drv, data);

        // Einen Bitpaar-Zähler hochzählen
        bitPairCount++;

        // Nach jedem 16. Bitpaar => Response-Pin prüfen
        if (bitPairCount == 16) {
            checkForAckOrNack(bitPairCount, ack, nack, drv, packageRef);
        }
    } else {
        // Keine neuen Bits mehr -> Datenleitung und Taktleitung löschen
        uint8_t currentRegister = drv.getRegister(&PORTA);
        currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);
        drv.setRegister(&PORTA, currentRegister);
    }
}

/**
 * @brief Sendet bereits übertragene Daten (package) erneut nach NACK 
 *        oder bis retryCount >= 3
 */
void resendPackage(B15F& drv, int& bitPairCount, uint32_t& packageRef) {
    // Wir befinden uns gerade bei bitPairCount < 16 und retryCount < 3
    if (bitPairCount < 16 && retryCount < 3) {
        // Hole das aktuelle Bitpaar aus dem Package
        uint8_t data = extractBitPair(packageRef, bitPairCount);
        std::cout << "Bitpaar: " << std::bitset<2>(data) << std::endl;

        // Dasselbe Prozedere wie beim Senden
        sendBitPair(drv, data);
        bitPairCount++;

        // Nach jedem 16. Bitpaar => Response-Pin prüfen
        if (bitPairCount == 16) {
            retryCount++;
            checkForAckOrNack(bitPairCount, ack, nack, drv, packageRef);
            std::cout << "Retry: " << retryCount << std::endl;
        }
    }
    else if (retryCount == 3) {
        std::cerr << "TIMEOUT: zu oft NACK!" << std::endl;
        ack = false;
        nack = false;
        // Daten- und Taktleitung löschen
        uint8_t currentRegister = drv.getRegister(&PORTA);
        currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);
        drv.setRegister(&PORTA, currentRegister);
    } else {
        // Alle Bitpaare übertragen oder Abbruch
        uint8_t currentRegister = drv.getRegister(&PORTA);
        currentRegister &= ~(DATA_MASK | CLOCK_PIN_MASK);
        drv.setRegister(&PORTA, currentRegister);
    }
}

int main() {
    // B15 Board-Instanz
    B15F& drv = B15F::getInstance();

    // PortA konfigurieren: PA0, PA1, PA3 als Ausgang; PA2 als Eingang
    drv.setRegister(&DDRA, 0x0B); // 0x0B = 00001011 (binär)

    std::string bitPair;
    int bitPairCount = 0;

    // Hauptschleife
    while (ack || nack) {
        if (ack) {
            // Falls ACK -> Neue Daten von stdin lesen und senden
            sendPackage(drv, bitPairCount, bitPair, package);
        } else {
            // Falls NACK -> Versuche das Paket erneut zu senden
            resendPackage(drv, bitPairCount, package);
        }
    }

    return 0;
}
