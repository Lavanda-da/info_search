#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

int main() {
    mongocxx::instance inst{};
    mongocxx::client client{mongocxx::uri{}};
    auto db = client["my_database"];
    auto collection = db["documents"];

    // Словарь: слово -> частота
    std::unordered_map<std::string, int> freq;

    std::cout << "Сбор статистики по всем документам...\n";
    auto cursor = collection.find({});
    int doc_count = 0;

    for (auto&& doc : cursor) {
        doc_count++;
        try {
            // Получаем массив tokens
            auto tokens_array = doc["tokens"].get_array().value;
            for (auto&& elem : tokens_array) {
                std::string word{elem.get_utf8().value.data(), elem.get_utf8().value.length()};
                freq[word]++;
            }
        } catch (const std::exception& e) {
            // Пропускаем документы без tokens
        }
    }

    std::cout << "Обработано документов: " << doc_count << "\n";
    std::cout << "Уникальных слов: " << freq.size() << "\n";

    // Преобразуем в вектор и сортируем по частоте (убывание)
    std::vector<std::pair<std::string, int>> sorted_freq(freq.begin(), freq.end());
    std::sort(sorted_freq.begin(), sorted_freq.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    // Сохраняем в CSV: ранг, слово, частота, freq * rank (должно быть ~const)
    std::ofstream csv("word_frequency.csv");
    csv << "rank,word,frequency,freq_times_rank\n";
    int rank = 1;
    for (const auto& [word, count] : sorted_freq) {
        csv << rank << "," << word << "," << count << "," << (count * rank) << "\n";
        rank++;
    }
    csv.close();

    std::cout << "Данные сохранены в word_frequency.csv\n";

    return 0;
}
