#include "xxh3.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

struct RROptions {
  bool verbose = false;
  bool dryRun = false;

  fs::path rootSearchDirectory;
};

[[nodiscard]] RROptions readOptions(int argc, char** argv) {
  RROptions options;

  opterr = false;// Let us handle all error output for command line options
  int choice;
  int index = 0;
  std::vector<option> long_options = {
          {"dry-run", no_argument, nullptr, 'y'},
          {"verbose", no_argument, nullptr, 'v'},
          {"dir", required_argument, nullptr, 'd'},
          {"help", no_argument, nullptr, 'h'},
          {nullptr, 0, nullptr, '\0'},
  };

  std::unordered_set<std::string> requiredOptionsStillNeeded = {"dir"};

  while ((choice = getopt_long(argc, argv, "yvd:h", long_options.data(), &index)) != -1) {
    auto optionIterator = std::find_if(long_options.begin(), long_options.end(),
                                       [choice](option const& opt) {
                                         return opt.val == choice;
                                       });

    if (optionIterator != long_options.end()) {
      requiredOptionsStillNeeded.erase(optionIterator->name);
    }

    switch (choice) {
      case 'h':
        std::cerr << "Help is not implemented!" << std::endl;
        exit(0);
      case 'y':
        options.dryRun = true;
        break;
      case 'v':
        options.verbose = true;
        break;
      case 'd':
        options.rootSearchDirectory = optarg;
        break;
      default:
        throw std::runtime_error("Error: invalid option");
    }
  }

  if (!requiredOptionsStillNeeded.empty()) {
    throw std::runtime_error("Option <" + *requiredOptionsStillNeeded.begin() +
                             "> must be supplied!");
  }

  return options;
}

XXH64_hash_t hashFile(fs::path const& path) {
  XXH3_state_t hashState;
  XXH3_64bits_reset(&hashState);

  std::ifstream inputFileStream;
  inputFileStream.open(path, std::ios::in | std::ios::binary);

  // https://stackoverflow.com/a/50491948
  constexpr size_t BUFFER_SIZE = 128;
  std::array<char, BUFFER_SIZE> bytes{};
  std::fill(bytes.begin(), bytes.end(), 0);

  while (inputFileStream.good()) {
    inputFileStream.read(bytes.data(), BUFFER_SIZE);
    XXH3_64bits_update(&hashState, bytes.data(), inputFileStream.gcount());
  }

  if (!inputFileStream.eof()) {
    std::string errorMessage = "Error reading ";
    errorMessage.append(path);
    errorMessage.append("!");
    throw std::runtime_error(errorMessage);
  }

  return XXH3_64bits_digest(&hashState);
}

int main(int argc, char** argv) {
  std::ios_base::sync_with_stdio(false);

  try {
    RROptions const options = readOptions(argc, argv);

    if (!fs::is_directory(options.rootSearchDirectory)) {
      std::cerr << "<dir> must be a directory!" << std::endl;
      return 1;
    }

    // https://stackoverflow.com/a/6012671
    std::string dbFileName = std::to_string(std::time(nullptr)) + ".db";
    std::ofstream dbStream(dbFileName);

    std::unordered_map<XXH64_hash_t, fs::path> fileMap;

    std::stack<fs::path> searchContainer;
    searchContainer.push(options.rootSearchDirectory);
    while (!searchContainer.empty()) {
      fs::path node = searchContainer.top();
      searchContainer.pop();

      if (fs::is_directory(node)) {
        for (auto const& child: fs::directory_iterator(node)) {
          searchContainer.push(child.path());
        }

        continue;
      }

      XXH64_hash_t hash;

      try {

#ifdef __RRPROGRESS
        using Resolution = std::chrono::duration<double, std::milli>;

        auto const startTime = std::chrono::steady_clock::now();
        constexpr size_t NUM_BYTES_PER_MEBIBYTE = 1024 * 1024;
        auto const nodeSizeMB = (static_cast<double>(fs::file_size(node)) /
                                 static_cast<double>(NUM_BYTES_PER_MEBIBYTE));

        hash = hashFile(node);

        auto const secondsElapsed =
                (Resolution(std::chrono::steady_clock::now() - startTime).count() /
                 1000.0);

        std::cerr << hash << " for " << node << ", " << nodeSizeMB / secondsElapsed
                  << "MiB/s" << std::endl;
#else
        hash = hashFile(node);
#endif

        auto const& [it, valueInserted] = fileMap.emplace(hash, std::move(node));
        if (!valueInserted) {
          std::cerr << "[COLLISION] " << it->second << " hashed to the same value as "
                    << node << '\n';
          continue;
        }

        dbStream << hash << '|' << node << '\n';
      } catch (std::runtime_error& error) {
        std::cerr << error.what() << '\n';
      }
    }

  } catch (std::runtime_error& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}