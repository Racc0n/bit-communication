#include <iostream>
#include <bitset>
#include <string>
#include <queue>
#include <vector>

// Paket-Konstanten
static const uint8_t START_SIGN = 0x02; // STX
static const uint8_t END_SIGN   = 0x03; // ETX

/**
 * @brief Bildet ein Paket aus Startzeichen, Datenbyte, Checksumme und Endzeichen
 *
 * @param data     Nutzdaten (ein Byte)
 * @param checksum Referenz auf die laufende Prüfsumme
 * @return Vector mit 4 Bytes [START, data, checksum, END]
 */
std::vector<uint8_t> createPacket(uint8_t data, uint8_t& checksum) {
    checksum = (checksum + data) % 256; // Einfache Prüfsumme aktualisieren

    // Debug-Ausgabe
    std::cerr << "Aktuelle Prüfsumme: " << std::bitset<8>(checksum)
              << " (" << static_cast<int>(checksum) << ")" << std::endl;

    // Zusammenstellen des Pakets
    return { START_SIGN, data, checksum, END_SIGN };
}

/**
 * @brief Zerlegt ein Byte in vier 2-Bit-Paare und hängt sie der Queue an
 *
 * @param byte      Das zu zerlegende Byte
 * @param bitQueue  Queue, in welche die 2-Bit-Paare als String eingefügt werden
 */
void enqueue2BitPairs(uint8_t byte, std::queue<std::string>& bitQueue) {
    std::bitset<8> binary(byte);          // z.B. 0xAB -> "10101011"
    std::string binaryStr = binary.to_string();

    // Alle 2 Bits als String ins bitQueue packen
    for (size_t i = 0; i < binaryStr.size(); i += 2) {
        bitQueue.push(binaryStr.substr(i, 2));
    }
}

/**
 * @brief Liest den kompletten Input-String von stdin, erstellt alle Pakete 
 *        und zerlegt sie in 2-Bit-Paare
 *
 * @return Queue mit allen 2-Bit-Paaren
 */
std::queue<std::string> processInputAndCreateBitQueue() {
    std::queue<std::string> bitQueue;
    std::string input;
    uint8_t checksum = 0;

    // Kompletten Text von stdin lesen
    std::getline(std::cin, input);

    // Für jedes Zeichen ein Paket bilden und in 2-Bit-Paare zerlegen
    for (char c : input) {
        uint8_t data = static_cast<uint8_t>(c);

        // Paket erstellen
        std::vector<uint8_t> packet = createPacket(data, checksum);

        // Jedes Byte im Paket in 2-Bit-Paare aufsplitten
        for (uint8_t byte : packet) {
            enqueue2BitPairs(byte, bitQueue);
        }
    }

    return bitQueue;
}

int main() {
    // Queue mit allen 2-Bit-Paaren erstellen
    std::queue<std::string> bitQueue = processInputAndCreateBitQueue();

    // Ausgabe: 2-Bit-Paare der Reihe nach ausgeben
    while (!bitQueue.empty()) {
        std::cout << bitQueue.front() << std::endl;
        bitQueue.pop();
    }

    return 0;
}
