#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>


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

std::string utf8_to_lower(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            if (c2 >= 0x90 && c2 <= 0xAF) {
                result += char(0xD0);
                result += char(c2 + 0x20);
                i++;
                continue;
            } else if (c2 == 0x81) {
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
    if (word.size() < 2) return word;

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
        if (word.size() > suf.size() && word.substr(word.size() - suf.size()) == suf) {
            std::string candidate = word.substr(0, word.size() - suf.size());
            if (!candidate.empty() && is_valid_utf8(candidate)) {
                return candidate;
            }
        }
    }
    return word;
}


struct QueryParts {
    std::vector<std::string> must;
    std::vector<std::string> must_not;
};

QueryParts parse_query(std::string query) {
    QueryParts parts;

    if (!query.empty() && query.front() == '[') query = query.substr(1);
    if (!query.empty() && query.back() == ']') query.pop_back();
    query.erase(std::remove_if(query.begin(), query.end(), ::isspace), query.end());

    size_t pos = 0;
    while (pos < query.size()) {
        if (query[pos] == '!') {
            pos++;
            size_t start = pos;
            while (pos < query.size() && query[pos] != '!' && query[pos] != '&') {
                pos++;
            }
            if (pos > start) {
                std::string term = query.substr(start, pos - start);
                parts.must_not.push_back(russian_stem(term));
            }
        } else {
            size_t start = pos;
            while (pos < query.size() && query[pos] != '!' && query[pos] != '&') {
                pos++;
            }
            if (pos > start) {
                std::string term = query.substr(start, pos - start);
                term.erase(std::remove(term.begin(), term.end(), '&'), term.end());
                if (!term.empty()) {
                    parts.must.push_back(russian_stem(term));
                }
            }
        }
    }

    return parts;
}


int main() {
    mongocxx::instance inst{};
    mongocxx::client client{mongocxx::uri{}};
    auto db = client["my_database"];
    auto collection = db["documents"];

    std::string query;
    std::cout << "Введите запрос (пример: [книги !статьи]): ";
    std::getline(std::cin, query);

    auto start = std::chrono::steady_clock::now();

    auto parts = parse_query(query);

    bsoncxx::builder::basic::document filter;

    if (!parts.must.empty()) {
        if (parts.must.size() == 1) {
            filter.append(bsoncxx::builder::basic::kvp("tokens", parts.must[0]));
        } else {
            bsoncxx::builder::basic::array all_array;
            for (const auto& term : parts.must) {
                all_array.append(term);
            }
            bsoncxx::builder::basic::document all_doc;
            all_doc.append(bsoncxx::builder::basic::kvp("$all", all_array.extract()));
            filter.append(bsoncxx::builder::basic::kvp("tokens", all_doc.extract()));
        }
    }

    if (!parts.must_not.empty()) {
        bsoncxx::builder::basic::array nin_array;
        for (const auto& term : parts.must_not) {
            nin_array.append(term);
        }

        bsoncxx::builder::basic::document nin_condition;
        nin_condition.append(bsoncxx::builder::basic::kvp("$nin", nin_array.extract()));

        bsoncxx::builder::basic::document not_doc;
        not_doc.append(bsoncxx::builder::basic::kvp("tokens", nin_condition.extract()));

        if (filter.view().empty() == false) {
            bsoncxx::builder::basic::array and_array;
            and_array.append(filter.extract());
            and_array.append(not_doc.extract());

            bsoncxx::builder::basic::document final_doc;
            final_doc.append(bsoncxx::builder::basic::kvp("$and", and_array.extract()));
            filter = std::move(final_doc);
        } else {
            filter = std::move(not_doc);
        }
    }

    try {
        auto cursor = collection.find(filter.view(), mongocxx::options::find{}.limit(50));
        int count = 0;
        for (auto&& doc : cursor) {
            count++;
            auto link_view = doc["link"].get_string().value;
            std::string link(link_view.data(), link_view.length());
            std::cout << "\nРезультат " << count << ":\n"
                      << "link: " << link << "\n";
        }
        if (count == 0) {
            std::cout << "Ничего не найдено.\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка поиска: " << e.what() << "\n";
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nПоиск выполнен за " << duration.count() << " мс.\n";

    return 0;
}
