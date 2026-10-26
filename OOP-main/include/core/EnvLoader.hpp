#ifndef ENV_LOADER_HPP
#define ENV_LOADER_HPP

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace Banking {

// Đọc cấu hình bí mật (TURSO_URL, TURSO_TOKEN...) từ file .env nằm NGOÀI git. Không bao giờ in giá trị ra log.
// Biến môi trường thật của hệ điều hành luôn được ưu tiên hơn file.
class EnvLoader {
public:
    // Trả false nếu không mở được file. Dòng hợp lệ: KEY=VALUE (bỏ qua dòng trống, #comment, dấu nháy bao ngoài).
    static bool load(const std::string& path = ".env") {
        // Paths produced from QString are UTF-8; u8path preserves them on Windows too.
        std::ifstream f(std::filesystem::u8path(path));
        if (!f) return false;
        std::string line;
        bool first = true;
        while (std::getline(f, line)) {
            if (first && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);  // BOM
            first = false;
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (line.compare(0, 7, "export ") == 0) line = trim(line.substr(7));
            const size_t eq = line.find('=');
            if (eq == std::string::npos || eq == 0) continue;
            const std::string key = trim(line.substr(0, eq));
            std::string value = trim(line.substr(eq + 1));
            if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
                value = value.substr(1, value.size() - 2);
            store()[key] = value;
        }
        return true;
    }

    static std::string get(const std::string& key, const std::string& fallback = "") {
        if (const char* env = std::getenv(key.c_str())) return env;
        auto it = store().find(key);
        return it != store().end() ? it->second : fallback;
    }

    static bool has(const std::string& key) { return !get(key).empty(); }

private:
    static std::unordered_map<std::string, std::string>& store() {
        static std::unordered_map<std::string, std::string> s;
        return s;
    }
    static std::string trim(const std::string& s) {
        size_t b = 0, e = s.size();
        while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
        while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
        return s.substr(b, e - b);
    }
};

}  // namespace Banking

#endif // ENV_LOADER_HPP
