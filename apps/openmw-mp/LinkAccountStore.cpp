#include "LinkAccountStore.hpp"
#include <components/openmw-mp/arenalinkauth.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#ifdef _WIN32
#include <codecvt>
#include <locale>
#endif

namespace
{
    using Tree = boost::property_tree::ptree;
    namespace fs = boost::filesystem;
    fs::path utf8Path(const std::string& text)
    {
#ifdef _WIN32
        return fs::path(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(text));
#else
        return fs::path(text);
#endif
    }
    std::string utf8Name(const fs::path& path)
    {
#ifdef _WIN32
        return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(path.filename().wstring());
#else
        return path.filename().string();
#endif
    }
    std::string lower(std::string text)
    {
        for (char& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return text;
    }
    std::string accountFilename(std::string name)
    {
        // Same substitutions as CoreScripts fileHelper.fixFilename.
        for (char& c : name)
        {
            if (c == ':') c = ';';
            else if (c == '.') c = ',';
            else if (std::string("<>\"/\\|*?\r\n").find(c) != std::string::npos) c = '_';
        }
        static const std::set<std::string> reserved = {"CON","PRN","AUX","NUL",
            "COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
            "LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"};
        if (reserved.count(name)) name = "_" + name;
        return name + ".json";
    }
    bool readJson(const fs::path& path, Tree& tree)
    {
        try
        {
            if (!fs::is_regular_file(path) || fs::is_symlink(path) || fs::file_size(path) > 16 * 1024 * 1024)
                return false;
            fs::ifstream input(path, std::ios::binary);
            if (!input) return false;
            boost::property_tree::read_json(input, tree);
            return true;
        }
        catch (const std::exception&) { return false; }
    }
    struct Store
    {
        fs::path root;
        std::map<std::string, std::uint32_t> ids;
        std::uint32_t nextId = 1;
        bool banned(const char* list, const std::string& value) const
        {
            Tree bans;
            // Missing or temporarily unreadable banlist fails closed.
            if (!readJson(root / "banlist.json", bans)) return true;
            auto entries = bans.get_child_optional(list);
            if (!entries) return false;
            for (const auto& item : *entries)
                if (lower(item.second.get_value<std::string>()) == lower(value)) return true;
            return false;
        }
        bool find(const std::string& name, mwmp::LinkAccount& account)
        {
            try
            {
                if (name.empty() || name.size() > ArenaLink::sMaxNick || name.find('\0') != std::string::npos)
                    return false;
                const std::string wanted = lower(accountFilename(name));
                fs::path selected;
                const fs::path folder = root / "player";
                if (!fs::is_directory(folder)) return false;
                for (const fs::directory_entry& item : fs::directory_iterator(folder))
                {
                    if (lower(utf8Name(item.path())) != wanted) continue;
                    // Never guess between case-colliding profiles on Linux.
                    if (!selected.empty()) return false;
                    selected = item.path();
                }
                if (selected.empty()) return false;
                Tree data;
                if (!readJson(selected, data)) return false;
                account.name = data.get<std::string>("login.name", name);
                account.passwordSalt = data.get<std::string>("login.passwordSalt", "");
                account.passwordSha256 = data.get<std::string>("login.passwordHash", "");
                if (account.passwordSalt.empty() || account.passwordSalt.size() > 128 || account.passwordSha256.size() != 64)
                    return false;
                auto& id = ids[wanted];
                if (id == 0) id = nextId++;
                account.userId = id;
                account.level = static_cast<std::uint16_t>(std::clamp(data.get<int>("stats.level", 1), 0, 65535));
                account.className = data.get<std::string>("character.class", "");
                account.banned = banned("playerNames", name) || banned("playerNames", account.name);
                const std::string color = data.get<std::string>("customVariables.chatColor", "");
                if (color.size() == 7 && color[0] == '#'
                    && std::all_of(color.begin()+1, color.end(), [](unsigned char c) { return std::isxdigit(c); }))
                    account.color = static_cast<std::uint32_t>(std::stoul(color.substr(1), nullptr, 16));
                return true;
            }
            catch (const std::exception&) { return false; }
        }
    };
}
namespace mwmp
{
    LinkCallbacks jsonLinkCallbacks(const std::string& dataDirectory)
    {
        auto store = std::make_shared<Store>();
        store->root = utf8Path(dataDirectory);
        LinkCallbacks callbacks;
        callbacks.findAccount = [store](const std::string& name, LinkAccount& account) { return store->find(name, account); };
        callbacks.isAddressBanned = [store](const std::string& address) { return store->banned("ipAddresses", address); };
        callbacks.verifyProof = [](const LinkAccount& account, const std::string& nonce, const std::string& proof) {
            return ArenaLink::verifyStoredProof(account.passwordSha256, nonce, proof);
        };
        return callbacks;
    }
}
