// main.cpp
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cctype>
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
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
                s.erase(s.begin());
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
                s.pop_back();
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
        std::cerr << "Failed to retrieve token. Status: "
                  << tokenResponse.status_code << "\n"
                  << tokenResponse.text << "\n";
        return 1;
    }

    auto jsonResponse = nlohmann::json::parse(tokenResponse.text);
    std::string accessToken = jsonResponse.value("access_token", "");
    if (accessToken.empty())
    {
        std::cerr << "Error: 'access_token' not found.\n";
        return 1;
    }

    nlohmann::json filteredCursusUsers = nlohmann::json::array();
    int currentPage = 1;
    const int pageSize = 100;

    while (true)
    {
        cpr::Response cursusUsersResponse = cpr::Get(
            cpr::Url{"https://api.intra.42.fr/v2/cursus_users"},
            cpr::Parameters{
                {"filter[cursus_id]", "21"},
                {"filter[campus_id]", "64"},
                {"page[size]",        std::to_string(pageSize)},
                {"page[number]",      std::to_string(currentPage)}
            },
            cpr::Header{{"Authorization", "Bearer " + accessToken}}
        );

        if (cursusUsersResponse.status_code != 200)
        {
            std::cerr << "Failed to retrieve cursus_users. HTTP status code: "
                      << cursusUsersResponse.status_code << "\n"
                      << "Response body:\n" << cursusUsersResponse.text << "\n";
            return 1;
        }

        nlohmann::json currentPageJson;
        try
        {
            currentPageJson = nlohmann::json::parse(cursusUsersResponse.text);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            std::cerr << "JSON parse error: " << e.what() << "\n";
            return 1;
        }

        if (!currentPageJson.is_array())
        {
            std::cerr << "Expected an array of cursus_users, but got something else.\n";
            return 1;
        }

        for (auto &item : currentPageJson)
        {
            nlohmann::json filteredItem;

            if (item.contains("begin_at") && item["begin_at"].is_string())
                filteredItem["begin_at"] = item["begin_at"];
            else
                filteredItem["begin_at"] = "";

            if (item.contains("grade") && item["grade"].is_string())
                filteredItem["grade"] = item["grade"];
            else
                filteredItem["grade"] = "";

            if (item.contains("level") && item["level"].is_number_float())
                filteredItem["level"] = item["level"];
            else
                filteredItem["level"] = 0.0;

            {
                std::string cursusValue;
                if (item.contains("cursus") && item["cursus"].is_object())
                {
                    auto cursusObj = item["cursus"];
                    if (cursusObj.contains("slug") && cursusObj["slug"].is_string())
                        cursusValue = cursusObj["slug"];
                }
                filteredItem["cursus"] = cursusValue;
            }

            {
                std::string userUrl;
                if (item.contains("user") && item["user"].is_object())
                {
                    auto userObj = item["user"];
                    if (userObj.contains("url") && userObj["url"].is_string())
                        userUrl = userObj["url"];
                }
                filteredItem["url"] = userUrl;
            }

            {
                std::string imageUrl;
                if (item.contains("user") && item["user"].is_object())
                {
                    auto userObj = item["user"];
                    if (userObj.contains("image") && userObj["image"].is_object())
                    {
                        auto imageObj = userObj["image"];
                        // "link"
                        if (imageObj.contains("link") && imageObj["link"].is_string())
                        {
                            imageUrl = imageObj["link"];
                        }
                        // or fallback to "versions"
                        else if (imageObj.contains("versions") && imageObj["versions"].is_object())
                        {
                            auto versionsObj = imageObj["versions"];
                            if (versionsObj.contains("large") && versionsObj["large"].is_string())
                                imageUrl = versionsObj["large"];
                            else if (versionsObj.contains("small") && versionsObj["small"].is_string())
                                imageUrl = versionsObj["small"];
                        }
                    }
                }
                filteredItem["image_url"] = imageUrl;
            }

            {
                std::string userLogin;
                if (item.contains("user") && item["user"].is_object())
                {
                    auto userObj = item["user"];
                    if (userObj.contains("login") && userObj["login"].is_string())
                        userLogin = userObj["login"];
                }
                filteredItem["user_login"] = userLogin;
            }

            {
                std::string userEmail;
                if (item.contains("user") && item["user"].is_object())
                {
                    auto userObj = item["user"];
                    if (userObj.contains("email") && userObj["email"].is_string())
                        userEmail = userObj["email"];
                }
                filteredItem["user_email"] = userEmail;
            }

            filteredCursusUsers.push_back(filteredItem);
        }

        if (currentPageJson.size() < static_cast<size_t>(pageSize))
            break;

        currentPage++;
    }

    std::sort(
        filteredCursusUsers.begin(),
        filteredCursusUsers.end(),
        [](const nlohmann::json &a, const nlohmann::json &b)
        {
            double levelA = a.contains("level") && a["level"].is_number_float()
                            ? a["level"].get<double>()
                            : 0.0;
            double levelB = b.contains("level") && b["level"].is_number_float()
                            ? b["level"].get<double>()
                            : 0.0;
            return (levelA > levelB);
        }
    );

    std::ofstream outFile("cursus_users_filtered.json");
    if (!outFile.is_open())
    {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }

    outFile << filteredCursusUsers.dump(4) << std::endl;
    outFile.close();

    std::cout << "Filtered cursus_users data sorted by level (desc) written to cursus_users_filtered.json.\n";

    return 0;
}
