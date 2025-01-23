#include <iostream>
#include <bitset>
#include <string>
#include <queue>
#include <vector>

/* 
----------------------------------------------------
 Paket-Konstanten
----------------------------------------------------
*/

constexpr uint8_t START_SIGN = 0x02; // STX
constexpr uint8_t END_SIGN   = 0x03; // ETX

/*
----------------------------------------------------
 Hilfsfunktion: Wandelt ein Byte in vier 2-Bit-Paare um
 und liefert sie als Strings (z.B. "00", "01", "10", "11")
----------------------------------------------------
*/

std::vector<std::string> byteToTwoBitPairs(uint8_t byteValue) {
    std::vector<std::string> pairs;
    std::bitset<8> bits(byteValue);  // z.B. 0xA5 => "10100101"
    std::string bitString = bits.to_string(); // "10100101"

    // Alle 2 Bits als String abtrennen, z.B. (0,1), (2,3), (4,5), (6,7)
    for (size_t i = 0; i < bitString.size(); i += 2) {
        pairs.push_back(bitString.substr(i, 2));
    }
    return pairs;
}

int main() {
    std::string inputLine;

    // Wir lesen Zeile für Zeile von STDIN
    while (true) {
        if (!std::getline(std::cin, inputLine)) {
            // EOF oder Fehler => Schleife verlassen
            break;
        }

        // Prüfsumme für diese Zeile zurücksetzen
        uint8_t checksum = 0;
        /*
        Für jede Zeile gehen wir jedes Zeichen durch
        und erzeugen ein "Paket" [START, data, checksum, END].
        Falls du nur EIN Paket pro Zeile willst, muss das
        zusammengefasst werden.
        */
        
        for (char c : inputLine) {
            uint8_t data = static_cast<uint8_t>(c);

            // Prüfsumme aktualisieren
            checksum = (checksum + data) % 256;

            // Erzeuge das Paket
            // (Momentan "ein Paket pro Zeichen": [0x02, data, checksum, 0x03])
            std::vector<uint8_t> packet = {START_SIGN, data, checksum, END_SIGN};

            // Zerlege das Paket Byte für Byte in 2-Bit-Paare
            std::queue<std::string> bitQueue;
            for (uint8_t byteVal : packet) {
                auto pairs = byteToTwoBitPairs(byteVal);
                for (const auto& p : pairs) {
                    bitQueue.push(p);
                }
            }

            // Gebe die 2-Bit-Paare aus (jedes Pair in einer neuen Zeile)
            while (!bitQueue.empty()) {
                std::cout << bitQueue.front() << std::endl;
                bitQueue.pop();
            }
        }
        
        /*
        Optional: Nach einer Zeile ggf. ein Trenner oder
        Meldung ausgeben, um deutlich zu machen,
        dass diese Zeile abgeschlossen ist.
        */
        
        std::cerr << "Fertig mit Zeile: \"" << inputLine << "\"" << std::endl;
    }

    return 0;
}
