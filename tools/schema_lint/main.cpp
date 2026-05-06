// tools/schema_lint/main.cpp
//
// Usage:
//   schema_lint <file.yaml> [--json]
//
// Validates a SignalForge decoder schema yaml file against schema v1.
//
// Exit codes:
//   0 — file is valid
//   1 — file is invalid (one or more validation errors)
//   2 — usage error (missing file path / unknown flag / unreadable file)
//
// Output (default, human-readable):
//   OK: <file> (N layouts, M fields)
//   FAIL: <file>
//     <file>:<line> — <fieldPath>: <message>
//     ...
//
// Output (--json):
//   {"path": "...", "valid": true/false,
//    "errors": [{"path": "...", "line": N, "field": "...", "message": "..."}]}
#include "decode/schema_validator.hpp"

#include <QFile>
#include <QString>
#include <QTextStream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using signalforge::decoder::Schema;
using signalforge::decoder::SchemaValidator;
using signalforge::decoder::ValidationError;

namespace {

void printUsage(std::ostream& out) {
    out << "Usage: schema_lint <file.yaml> [--json]\n"
        << "\n"
        << "Validates a SignalForge decoder schema yaml file against schema v1.\n"
        << "\n"
        << "Options:\n"
        << "  --json    Emit machine-readable JSON instead of human-readable output.\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  file is valid\n"
        << "  1  file is invalid (one or more validation errors)\n"
        << "  2  usage error (missing path / unknown flag / unreadable file)\n";
}

int totalFields(const Schema& s) {
    int n = 0;
    for (const auto& l : s.layouts) {
        n += static_cast<int>(l.fields.size());
    }
    return n;
}

int runHumanReadable(const QString& path, const std::expected<Schema, std::vector<ValidationError>>& result) {
    if (result.has_value()) {
        const auto& s = *result;
        std::cout << "OK: " << path.toStdString() << " (" << s.layouts.size() << " layout"
                  << (s.layouts.size() == 1 ? "" : "s") << ", " << totalFields(s) << " field"
                  << (totalFields(s) == 1 ? "" : "s") << ")\n";
        return 0;
    }
    std::cout << "FAIL: " << path.toStdString() << "\n";
    for (const auto& e : result.error()) {
        std::cout << "  " << e.filePath.toStdString() << ":" << e.lineNumber << " — " << e.fieldPath.toStdString()
                  << ": " << e.message.toStdString() << "\n";
    }
    return 1;
}

int runJson(const QString& path, const std::expected<Schema, std::vector<ValidationError>>& result) {
    nlohmann::json j;
    j["path"] = path.toStdString();
    if (result.has_value()) {
        const auto& s = *result;
        j["valid"] = true;
        j["layouts"] = s.layouts.size();
        j["fields"] = totalFields(s);
        j["errors"] = nlohmann::json::array();
        std::cout << j.dump() << "\n";
        return 0;
    }
    j["valid"] = false;
    auto errs = nlohmann::json::array();
    for (const auto& e : result.error()) {
        nlohmann::json ej;
        ej["path"] = e.filePath.toStdString();
        ej["line"] = e.lineNumber;
        ej["field"] = e.fieldPath.toStdString();
        ej["message"] = e.message.toStdString();
        errs.push_back(std::move(ej));
    }
    j["errors"] = std::move(errs);
    std::cout << j.dump() << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(std::cerr);
        return 2;
    }

    QString path;
    bool jsonOutput = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--json") {
            jsonOutput = true;
        } else if (a == "--help" || a == "-h") {
            printUsage(std::cout);
            return 0;
        } else if (a.size() > 0 && a[0] == '-') {
            std::cerr << "schema_lint: unknown option '" << a << "'\n";
            printUsage(std::cerr);
            return 2;
        } else if (path.isEmpty()) {
            path = QString::fromStdString(a);
        } else {
            std::cerr << "schema_lint: only one yaml file may be specified\n";
            printUsage(std::cerr);
            return 2;
        }
    }

    if (path.isEmpty()) {
        printUsage(std::cerr);
        return 2;
    }

    if (!QFile::exists(path)) {
        if (jsonOutput) {
            nlohmann::json j;
            j["path"] = path.toStdString();
            j["valid"] = false;
            j["errors"] = nlohmann::json::array({nlohmann::json{
                {"path", path.toStdString()}, {"line", -1}, {"field", "<file>"}, {"message", "file does not exist"}}});
            std::cout << j.dump() << "\n";
        } else {
            std::cerr << "schema_lint: file does not exist: " << path.toStdString() << "\n";
        }
        return 2;
    }

    const auto result = SchemaValidator::validateFile(path);
    if (jsonOutput) {
        return runJson(path, result);
    }
    return runHumanReadable(path, result);
}
