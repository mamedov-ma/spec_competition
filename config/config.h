#pragma once
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <sstream>

class MdConfig {
public:
    MdConfig(const std::string &filename)
    {
        load(filename);
    }

    std::vector<std::string> tickers_list;
    struct in_addr inc_ip;       // currently unused
    struct in_addr snapshot_ip;  // currently unused
    ~MdConfig() = default;

private:
    void load(const std::string &filename)
    {
        std::ifstream file(filename);
        std::string inc_ip_str;
        std::string snapshot_ip_str;

        if (!file.is_open()) {
            std::cerr << "Could not open file: " + filename;
        }

        std::string line;
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            if (!(ss >> inc_ip_str >> snapshot_ip_str)) {
                std::cerr << "Invalid IP address format in file: " + filename;
            }
        } else {
            std::cerr << "File is empty or missing IP addresses: " + filename;
        }

        inet_pton(AF_INET, inc_ip_str.c_str(), &inc_ip);
        inet_pton(AF_INET, snapshot_ip_str.c_str(), &snapshot_ip);

        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (!line.empty()) {
                tickers_list.push_back(line);
            }
        }

        file.close();
    }
};