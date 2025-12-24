#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

std::vector<std::string> read_lines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

int main() {
    mongocxx::instance inst{};

    mongocxx::client client{mongocxx::uri{}};

    auto db = client["my_database"];
    auto collection = db["documents"];

    auto links = read_lines("link.txt");
    auto texts = read_lines("db.txt");

    if (links.size() != texts.size()) {
        std::cerr << "Ошибка: количество ссылок и текстов не совпадает!\n";
        return 1;
    }

    for (size_t i = 0; i < links.size(); ++i) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        auto doc = document{}
            << "link" << links[i]
            << "text" << texts[i]
            << finalize;

        collection.insert_one(doc.view());
    }

    std::cout << "Вставлено " << links.size() << " документов.\n";
    return 0;
}
