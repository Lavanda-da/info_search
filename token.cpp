#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <chrono>


bool is_valid_utf8(const std::string& s) {
    int count = 0;
    for (unsigned char c : s) {
        if (count == 0) {
            if ((c & 0x80) == 0) continue;
            else if ((c & 0xE0) == 0xC0) count = 1;
            else if ((c & 0xF0) == 0xE0) count = 2;
            else if ((c & 0xF8) == 0xF0) count = 3;
            else return false;
        } else {
            if ((c & 0xC0) != 0x80) return false;
            count--;
        }
    }
    return count == 0;
}

bool is_utf8_letter_start(unsigned char c) {
    return (c >= 0x41 && c <= 0x5A) ||        // A-Z
           (c >= 0x61 && c <= 0x7A) ||        // a-z
           (c >= 0xD0 && c <= 0xD1);           // Начало кириллицы в UTF-8
}

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        if (is_utf8_letter_start(c)) {
            current += c;
            if ((c == 0xD0 || c == 0xD1) && i + 1 < text.size()) {
                current += text[++i];
            }
        } else if ((c & 0xC0) == 0x80) {
            if (!current.empty()) {
                current += c;
            }
        } else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string utf8_to_lower(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);

        if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            if (c2 >= 0x90 && c2 <= 0xAF) {       // А–Я → а–я
                result += char(0xD0);
                result += char(c2 + 0x20);
                i++;
                continue;
            } else if (c2 == 0x81) {              // Ё → ё
                result += char(0xD1);
                result += char(0x91);
                i++;
                continue;
            }
        } else if (c >= 'A' && c <= 'Z') {
            result += static_cast<char>(c + 32);
            continue;
        }
        result += c;
    }
    return result;
}

std::string russian_stem(const std::string& word_utf8) {
    std::string word = utf8_to_lower(word_utf8);
    if (word.size() < 4) return word;

    std::vector<std::string> suffixes = {
        "\xD0\xB0\xD0\xBC\xD0\xB8",         // ами
        "\xD1\x8F\xD0\xBC\xD0\xB8",         // ями
        "\xD0\xBE\xD0\xB3\xD0\xBE",         // ого
        "\xD0\xB5\xD0\xB3\xD0\xBE",         // его
        "\xD0\xBE\xD0\xBC\xD1\x83",         // ому
        "\xD0\xB5\xD0\xBC\xD1\x83",         // ему
        "\xD0\xB8\xD0\xBC\xD0\xB8",         // ими
        "\xD1\x8B\xD0\xBC\xD0\xB8",         // ыми
        "\xD0\xB5\xD1\x88\xD1\x8C",         // ешь
        "\xD0\xB5\xD1\x82\xD0\xB5",         // ете
        "\xD0\xB8\xD1\x88\xD1\x8C",         // ишь
        "\xD0\xB8\xD1\x82\xD0\xB5",         // ите
        "\xD0\xBE\xD0\xB9",                 // ой
        "\xD0\xB5\xD0\xB9",                 // ей
        "\xD0\xBE\xD0\xBC",                 // ом
        "\xD0\xB5\xD0\xBC",                 // ем
        "\xD0\xBE\xD0\xB2",                 // ов
        "\xD0\xB5\xD0\xB2",                 // ев
        "\xD0\xB0\xD0\xBC",                 // ам
        "\xD1\x8F\xD0\xBC",                 // ям
        "\xD0\xB0\xD1\x85",                 // ах
        "\xD1\x8F\xD1\x85",                 // ях
        "\xD1\x8B\xD0\xB9",                 // ый
        "\xD0\xB8\xD0\xB9",                 // ий
        "\xD0\xBE\xD0\xBC",                 // ом
        "\xD0\xB5\xD0\xBC",                 // ем
        "\xD0\xB0\xD1\x8F",                 // ая
        "\xD1\x8F\xD1\x8F",                 // яя
        "\xD0\xBE\xD0\xB9",                 // ой
        "\xD0\xB5\xD0\xB9",                 // ей
        "\xD1\x83\xD1\x8E",                 // ую
        "\xD1\x8E\xD1\x8E",                 // юю
        "\xD0\xBE\xD0\xB5",                 // ое
        "\xD0\xB5\xD0\xB5",                 // ее
        "\xD0\xB8\xD0\xB5",                 // ие
        "\xD1\x8B\xD0\xB5",                 // ые
        "\xD0\xB8\xD1\x85",                 // их
        "\xD1\x8B\xD1\x85",                 // ых
        "\xD0\xB8\xD0\xBC",                 // им
        "\xD1\x8B\xD0\xBC"                  // ым
        "\xD0\xB5\xD1\x82",                 // ет
        "\xD1\x83\xD1\x82",                 // ут
        "\xD1\x8E\xD1\x82",                 // ют
        "\xD0\xB8\xD1\x82",                 // ит
        "\xD0\xB8\xD0\xBC",                 // им
        "\xD0\xB0\xD1\x82",                 // ат
        "\xD1\x8F\xD1\x82"                  // ят
        "\xD0\xB0",                         // а
        "\xD1\x8F",                         // я
        "\xD0\xB8",                         // и
        "\xD1\x8B",                         // ы
        "\xD0\xB5",                         // е
        "\xD1\x83",                         // у
        "\xD1\x8E",                         // ю
        "\xD0\xBE"                          // о
    };

    for (const auto& suf : suffixes) {
        if (word.size() > suf.size() && 
            word.substr(word.size() - suf.size()) == suf) {
            std::string candidate = word.substr(0, word.size() - suf.size());
            if (candidate.size() >= 1 && is_valid_utf8(candidate)) {
                return candidate;
            }
        }
    }
    return word;
}

int main() {
    auto start = std::chrono::steady_clock::now();
    mongocxx::instance inst{};
    mongocxx::client client{mongocxx::uri{}};
    auto db = client["my_database"];
    auto collection = db["documents"];

    auto cursor = collection.find({});
    int updated = 0;

    for (auto&& doc : cursor) {
        try {
            auto id = doc["_id"].get_oid().value;

            auto text_view = doc["text"].get_string().value;
            std::string text{text_view.data(), text_view.length()};

            if (!is_valid_utf8(text)) {
                std::cerr << "Пропущен документ с невалидным UTF-8 в поле 'text'\n";
                continue;
            }

            auto words = tokenize(text);
            std::set<std::string> unique_stems;
            for (const auto& word : words) {
                std::string stem = russian_stem(word);
                if (is_valid_utf8(stem)) {
                    unique_stems.insert(stem);
                }
            }

            bsoncxx::builder::basic::array tokens_builder;
            for (const auto& stem : unique_stems) {
                tokens_builder.append(stem);
            }

            bsoncxx::builder::basic::document update_doc;
            update_doc.append(
                bsoncxx::builder::basic::kvp("$set",
                    [&](bsoncxx::builder::basic::sub_document sub_doc) {
                        sub_doc.append(bsoncxx::builder::basic::kvp("tokens", tokens_builder.extract()));
                    }
                )
            );

            bsoncxx::builder::basic::document filter_doc;
            filter_doc.append(bsoncxx::builder::basic::kvp("_id", bsoncxx::types::b_oid{id}));

            collection.update_one(filter_doc.extract(), update_doc.extract());
            updated++;

        } catch (const std::exception& e) {
            std::cerr << "Ошибка при обработке документа: " << e.what() << "\n";
        }
    }

    std::cout << "Успешно обновлено документов: " << updated << "\n";
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nТокенизация выполнена за " << duration.count() << " мс.\n";

    return 0;
}
