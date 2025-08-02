#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <percorso_reale>\n";
        return 1;
    }

    std::string inputPath = argv[1];

    // Crea directory temp se non esiste
    fs::path tempDir = "./temp";
    if (!fs::exists(tempDir)) {
        fs::create_directory(tempDir);
    }

    // Trova un ID libero per il nuovo file
    int id = 1;
    fs::path outputPath;
    do {
        outputPath = tempDir / ("output_" + std::to_string(id) + ".txt");
        id++;
    } while (fs::exists(outputPath));

    // Scrive il percorso assoluto nel file
    std::ofstream outFile(outputPath);
    if (!outFile) {
        std::cerr << "Errore nella creazione del file: " << outputPath << "\n";
        return 2;
    }

    try {
        fs::path absPath = fs::absolute(inputPath);
        outFile << absPath << "\n";
        std::cout << "Creato: " << outputPath << " con contenuto: " << absPath << "\n";
    } catch (fs::filesystem_error& e) {
        std::cerr << "Errore nel calcolo del percorso assoluto: " << e.what() << "\n";
        return 3;
    }

    return 0;
}
