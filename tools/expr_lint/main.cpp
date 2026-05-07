// tools/expr_lint/main.cpp
//
// Usage:
//   expr_lint <file.yaml> [--base-signals <signals.json>] [--json]
//
// Validates a SignalForge expression yaml file against schema v1.
//
// Without --base-signals: syntax + cycle checks only (skips source-id
// existence and type validation; spec §4.6).
//
// With --base-signals <signals.json>: full validation. The JSON file
// holds a base-signal catalog as exported from the running app, e.g.:
//
//   {
//     "signals": [
//       {"id": "voltage", "name": "Voltage", "unit": "V", "type": "double"},
//       {"id": "current", "name": "Current", "unit": "A", "type": "double"}
//     ]
//   }
//
// Recognised type names: "double", "bool", "int64", "qstring".
//
// Exit codes:
//   0 — file is valid
//   1 — file is invalid (one or more validation errors)
//   2 — usage error (missing path / unknown flag / unreadable file /
//                    bad signals.json)
//
// Output (default, human-readable):
//   OK: <file> (N expressions, M source signals)
//   FAIL: <file>
//     <file>:<line> — <expressionId>: <message>
//     ...
//
// Output (--json):
//   {"path": "...", "valid": true/false,
//    "expressions": N, "sources": M,
//    "errors": [{"path":"...","line":N,"expression":"...","message":"..."}]}

#include "decode/decoder_interface.hpp"
#include "expression/expression_validator.hpp"

#include <QFile>
#include <QString>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::expression::ExpressionSet;
using signalforge::expression::ExpressionValidationError;
using signalforge::expression::ExpressionValidationResult;
using signalforge::expression::ExpressionValidator;

namespace {

void printUsage(std::ostream& out) {
    out << "Usage: expr_lint <file.yaml> [--base-signals <signals.json>] [--json]\n"
        << "\n"
        << "Validates a SignalForge expression yaml file against schema v1.\n"
        << "\n"
        << "Options:\n"
        << "  --base-signals <signals.json>\n"
        << "      Optional JSON catalog of base signals. Without this flag,\n"
        << "      only syntax + cycle checks run (source-id existence + type\n"
        << "      checks are skipped).\n"
        << "  --json\n"
        << "      Emit machine-readable JSON instead of human-readable output.\n"
        << "  --help, -h\n"
        << "      Print this help and exit 0.\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  file is valid\n"
        << "  1  file is invalid (one or more validation errors)\n"
        << "  2  usage error (missing path / unknown flag / unreadable file)\n";
}

std::optional<SignalType> parseSignalType(const std::string& s) {
    if (s == "double") {
        return SignalType::Double;
    }
    if (s == "bool") {
        return SignalType::Bool;
    }
    if (s == "int64") {
        return SignalType::Int64;
    }
    if (s == "qstring" || s == "QString" || s == "string") {
        return SignalType::String;
    }
    return std::nullopt;
}

struct CatalogLoadError {
    std::string message;
};

// On success, returns the parsed catalog. On failure, returns a string
// describing the load error suitable for direct user display.
std::expected<std::vector<SignalMetadata>, CatalogLoadError> loadSignalsCatalog(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected(CatalogLoadError{"cannot open base-signals file: " + path});
    }
    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(CatalogLoadError{std::string{"base-signals JSON parse error: "} + e.what()});
    }
    if (!j.is_object() || !j.contains("signals") || !j["signals"].is_array()) {
        return std::unexpected(
            CatalogLoadError{"base-signals JSON must be an object with key 'signals' (array of {id,type,...})"});
    }
    std::vector<SignalMetadata> out;
    out.reserve(j["signals"].size());
    for (const auto& entry : j["signals"]) {
        if (!entry.is_object() || !entry.contains("id") || !entry.contains("type")) {
            return std::unexpected(
                CatalogLoadError{"each base-signals entry must be an object with at least 'id' and 'type' fields"});
        }
        SignalMetadata m;
        m.id = QString::fromStdString(entry["id"].get<std::string>());
        m.name = entry.value("name", entry["id"].get<std::string>()).empty()
                     ? m.id
                     : QString::fromStdString(entry.value("name", std::string{}));
        if (m.name.isEmpty()) {
            m.name = m.id;
        }
        m.unit = QString::fromStdString(entry.value("unit", std::string{}));
        const auto t = parseSignalType(entry["type"].get<std::string>());
        if (!t) {
            return std::unexpected(CatalogLoadError{"unrecognised type '" + entry["type"].get<std::string>() +
                                                    "' for signal '" + entry["id"].get<std::string>() +
                                                    "' (allowed: double, bool, int64, qstring)"});
        }
        m.type = *t;
        out.push_back(std::move(m));
    }
    return out;
}

int runHumanReadable(const QString& path, const ExpressionValidationResult& result) {
    if (result.has_value()) {
        const auto& s = *result;
        std::cout << "OK: " << path.toStdString() << " (" << s.expressions.size() << " expression"
                  << (s.expressions.size() == 1 ? "" : "s") << ", " << s.baseSignalIds.size() << " source signal"
                  << (s.baseSignalIds.size() == 1 ? "" : "s") << ")\n";
        return 0;
    }
    std::cout << "FAIL: " << path.toStdString() << "\n";
    for (const auto& e : result.error()) {
        std::cout << "  " << e.filePath.toStdString() << ":" << e.lineNumber << " — "
                  << (e.expressionId.isEmpty() ? "<top>" : e.expressionId.toStdString()) << ": "
                  << e.message.toStdString() << "\n";
    }
    return 1;
}

int runJson(const QString& path, const ExpressionValidationResult& result) {
    nlohmann::json j;
    j["path"] = path.toStdString();
    if (result.has_value()) {
        const auto& s = *result;
        j["valid"] = true;
        j["expressions"] = s.expressions.size();
        j["sources"] = s.baseSignalIds.size();
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
        ej["expression"] = e.expressionId.toStdString();
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

    QString yamlPath;
    std::optional<std::string> signalsJsonPath;
    bool jsonOutput = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--json") {
            jsonOutput = true;
        } else if (a == "--help" || a == "-h") {
            printUsage(std::cout);
            return 0;
        } else if (a == "--base-signals") {
            if (i + 1 >= argc) {
                std::cerr << "expr_lint: --base-signals requires a path argument\n";
                printUsage(std::cerr);
                return 2;
            }
            signalsJsonPath = std::string{argv[++i]};
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "expr_lint: unknown option '" << a << "'\n";
            printUsage(std::cerr);
            return 2;
        } else if (yamlPath.isEmpty()) {
            yamlPath = QString::fromStdString(a);
        } else {
            std::cerr << "expr_lint: only one yaml file may be specified\n";
            printUsage(std::cerr);
            return 2;
        }
    }

    if (yamlPath.isEmpty()) {
        printUsage(std::cerr);
        return 2;
    }

    if (!QFile::exists(yamlPath)) {
        if (jsonOutput) {
            nlohmann::json j;
            j["path"] = yamlPath.toStdString();
            j["valid"] = false;
            j["errors"] = nlohmann::json::array({nlohmann::json{{"path", yamlPath.toStdString()},
                                                                {"line", -1},
                                                                {"expression", ""},
                                                                {"message", "file does not exist"}}});
            std::cout << j.dump() << "\n";
        } else {
            std::cerr << "expr_lint: file does not exist: " << yamlPath.toStdString() << "\n";
        }
        return 2;
    }

    ExpressionValidationResult result = std::unexpected(std::vector<ExpressionValidationError>{});
    if (signalsJsonPath.has_value()) {
        auto cat = loadSignalsCatalog(*signalsJsonPath);
        if (!cat.has_value()) {
            std::cerr << "expr_lint: " << cat.error().message << "\n";
            return 2;
        }
        result = ExpressionValidator::validateFile(yamlPath, *cat);
    } else {
        result = ExpressionValidator::validateFileSyntaxOnly(yamlPath);
    }

    if (jsonOutput) {
        return runJson(yamlPath, result);
    }
    return runHumanReadable(yamlPath, result);
}
