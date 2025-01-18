// main.cpp
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

bool loadEnvFile(const std::string& filename, std::string& uid, std::string& secret)
{
    std::ifstream envFile(filename);
    if (!envFile.is_open())
    {
        std::cerr << "Error: Could not open .env file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(envFile, line))
    {
        if (line.empty() || (line.find('=') == std::string::npos))
            continue;

        std::string key = line.substr(0, line.find('='));
        std::string value = line.substr(line.find('=') + 1);

        auto trim = [](std::string &s)
        {
            while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
            while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
        };

        trim(key);
        trim(value);
        if (key == "UID")
            uid = value;
        else if (key == "SECRET")
            secret = value;
    }

    envFile.close();
    if (uid.empty() || secret.empty())
    {
        std::cerr << "Error: Could not find UID or SECRET in .env file.\n";
        return false;
    }

    return true;
}

int main()
{
    std::string uid, secret;
    if (!loadEnvFile(".env", uid, secret))
        return 1;

    cpr::Response tokenResponse = cpr::Post(
        cpr::Url{"https://api.intra.42.fr/oauth/token"},
        cpr::Payload{
            {"grant_type",    "client_credentials"},
            {"client_id",     uid},
            {"client_secret", secret}
        }
    );

    if (tokenResponse.status_code != 200)
    {
        std::cerr << "Failed to retrieve token. HTTP status code: "
                  << tokenResponse.status_code << "\n"
                  << "Response body:\n" << tokenResponse.text << "\n";
        return 1;
    }

    auto jsonResponse = nlohmann::json::parse(tokenResponse.text);
    std::string accessToken = jsonResponse.value("access_token", "");
    if (accessToken.empty())
    {
        std::cerr << "Error: 'access_token' not found in the token response.\n";
        return 1;
    }

    cpr::Response campusResponse = cpr::Get(
        cpr::Url{"https://api.intra.42.fr/v2/campus"},
        cpr::Parameters{{"filter[name]", "Singapore"}},
        cpr::Header{{"Authorization", "Bearer " + accessToken}}
    );

    if (campusResponse.status_code != 200)
    {
        std::cerr << "Failed to retrieve campus data. HTTP status code: "
                  << campusResponse.status_code << "\n"
                  << "Response body:\n" << campusResponse.text << "\n";
        return 1;
    }

    nlohmann::json campusJson;
    try
    {
        campusJson = nlohmann::json::parse(campusResponse.text);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "JSON parse error (campus): " << e.what() << "\n";
        return 1;
    }

    std::ofstream campusFile("campus_response.json");
    if (!campusFile.is_open())
    {
        std::cerr << "Failed to open campus_response.json for writing.\n";
        return 1;
    }
    campusFile << campusJson.dump(4) << std::endl;
    campusFile.close();
    std::cout << "Campus data has been written to campus_response.json\n";
    cpr::Response cursusResponse = cpr::Get(
        cpr::Url{"https://api.intra.42.fr/v2/cursus"},
        cpr::Parameters{{"filter[name]", "42cursus"}},
        cpr::Header{{"Authorization", "Bearer " + accessToken}}
    );

    if (cursusResponse.status_code != 200)
    {
        std::cerr << "Failed to retrieve cursus data. HTTP status code: "
                  << cursusResponse.status_code << "\n"
                  << "Response body:\n" << cursusResponse.text << "\n";
        return 1;
    }

    nlohmann::json cursusJson;
    try
    {
        cursusJson = nlohmann::json::parse(cursusResponse.text);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "JSON parse error (cursus): " << e.what() << "\n";
        return 1;
    }

    std::ofstream cursusFile("cursus_response.json");
    if (!cursusFile.is_open())
    {
        std::cerr << "Failed to open cursus_response.json for writing.\n";
        return 1;
    }
    cursusFile << cursusJson.dump(4) << std::endl;
    cursusFile.close();
    std::cout << "Cursus data has been written to cursus_response.json\n";
    return 0;
}
