#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <tuple>
#include <chrono>
#include <thread>
#include <windows.h>
#include "WinReg.hpp"
//#define DEBUG

namespace fs = std::filesystem;

std::tuple<std::wstring, std::vector<std::wstring>, fs::path> ReadConfig(const fs::path& configFile) {
    std::wifstream file(configFile);
    std::wstring line, steamid3;
    std::vector<std::wstring> gameIDs;
    fs::path userdataPath;

    std::wcout << L"Reading configuration from: " << configFile << std::endl;
    while (std::getline(file, line)) {
        std::wistringstream is_line(line);
        std::wstring key, value;
        if (std::getline(is_line, key, L'=') && std::getline(is_line, value)) {
            if (key == L"gameids") {
                std::wistringstream value_stream(value);
                std::wstring gameid;
                while (std::getline(value_stream, gameid, L',')) {
                    gameIDs.push_back(gameid);
                    std::wcout << L"Configured GameID: " << gameid << std::endl;
                }
            }
            else if (key == L"steamid3") {
                steamid3 = value;
                std::wcout << L"Configured SteamID3: " << steamid3 << std::endl;
            }
            else if (key == L"userdataPath") {
                userdataPath = value;
                std::wcout << L"Userdata Path: " << userdataPath << std::endl;
            }
        }
    }

    if (steamid3.empty() || gameIDs.empty() || userdataPath.empty()) {
        std::wcerr << L"Configuration incomplete or missing." << std::endl;
    }

    return { steamid3, gameIDs, userdataPath };
}

std::wstring getSteamID3FromRegistry() {
    try {
        winreg::RegKey key{ HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess" };
        DWORD activeUser = key.GetDwordValue(L"ActiveUser");
        std::wstring steamID3 = std::to_wstring(activeUser);
        std::wcout << L"Current SteamID3 from registry: " << steamID3 << std::endl;
        return steamID3;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Failed to get SteamID3 from registry: " << e.what() << std::endl;
        return L"";
    }
}

bool CopyGameFolders(const fs::path& sourcePath, const fs::path& targetPath) {
    try {
        // Check if the source directory exists
        if (!fs::exists(sourcePath)) {
            std::wcout << L"Source path does not exist: " << sourcePath << std::endl;
            return false; 
        }

        if (!fs::exists(targetPath)) {
            std::wcout << L"Target path does not exist, creating: " << targetPath << std::endl;
            fs::create_directories(targetPath.parent_path()); // Ensure the parent directory exists
        }
        else {
            std::wcout << L"Target path exists, removing existing files: " << targetPath << std::endl;
            fs::remove_all(targetPath);
        }

        std::wcout << L"Copying files from: " << sourcePath << L" to: " << targetPath << std::endl;
        fs::copy(sourcePath, targetPath, fs::copy_options::recursive);
        std::wcout << L"Successfully copied files." << std::endl;
        return true;
    }
    catch (const fs::filesystem_error& e) {
        std::wcerr << L"Error copying folders: " << e.what() << std::endl;
        return false;
    }
}




void CreateConsole() {
    AllocConsole();
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
//#ifdef DEBUG
//    CreateConsole();
//#endif
    std::wcout << L"Program started." << std::endl;
    auto [configuredSteamID3, gameIDs, baseUserdataPath] = ReadConfig(L"config.ini");
    std::set<std::wstring> processedSteamIDs;

    while (true) {
        std::wstring currentSteamID3 = getSteamID3FromRegistry();
        if (currentSteamID3 == L"0") {
            std::wcout << L"Current SteamID3 is 0, indicating no user is logged in. Skipping..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue; 
        }
        if (!currentSteamID3.empty() && currentSteamID3 != configuredSteamID3 && processedSteamIDs.find(currentSteamID3) == processedSteamIDs.end()) {
            std::wcout << L"Detected new SteamID3, proceeding with copy operation." << std::endl;
            for (const auto& gameID : gameIDs) {
                fs::path sourcePath = baseUserdataPath / configuredSteamID3 / gameID;
                fs::path targetPath = baseUserdataPath / currentSteamID3 / gameID;
                if (CopyGameFolders(sourcePath, targetPath)) {
                    std::wcout << L"Successfully copied game folder for gameID: " << gameID << std::endl;
                }
                else {
                    std::wcerr << L"Failed to copy game folder for gameID: " << gameID << std::endl;
                }
            }
            processedSteamIDs.insert(currentSteamID3);
        }
        else if (!currentSteamID3.empty()) {
            std::wcout << L"No new SteamID3 detected or already processed." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
