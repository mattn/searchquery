#include "../include/searchquery/base.hxx"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void grep(const std::filesystem::path &path, const std::string &pattern) {
  std::ifstream ifs(path);
  if (!ifs) {
    throw std::runtime_error("cannot open file: " + path.string());
  }

  std::string line;
  int number = 1;
  while (std::getline(ifs, line)) {
    std::string err;
    if (searchquery::match_expression(line, pattern, err)) {
      std::cout << path.string() << ":" << number << ":" << line << std::endl;
    } else if (!err.empty()) {
      std::cerr << "error parsing pattern \"" << pattern << "\": " << err
                << std::endl;
      return;
    }
    number++;
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::cerr << "Usage: " << argv[0] << " [pattern]" << std::endl;
    return 1;
  }

  std::string pattern = argv[1];
  std::error_code ec;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           ".", std::filesystem::directory_options::skip_permission_denied,
           ec)) {
    if (entry.is_regular_file())
      grep(entry.path(), pattern);
  }
  return 0;
}
