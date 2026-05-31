// src/query/filter_expr.cpp

#include "query/filter_expr.hpp"

#include <QChar>
#include <vector>

namespace signalforge::query {

namespace {

enum class Op { Eq, Ne, Lt, Le, Gt, Ge, Contains };
enum class LitKind { Number, Bool, String };
enum class Kind { And, Or, Not, Compare };

enum class TokType { LParen, RParen, And, Or, Not, OpEq, OpNe, OpLt, OpLe, OpGt, OpGe, Word, Str, End };

struct Token {
    TokType type;
    QString text;
    int pos = 0;
};

/// Thrown internally on a parse error; caught in FilterExpr::parse.
struct ParseException {
    QString message;
    int pos;
};

bool isWordChar(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('/') || c == QLatin1Char(':') ||
           c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('+');
}

std::vector<Token> tokenize(const QString& src) {
    std::vector<Token> out;
    int i = 0;
    const int n = src.size();
    while (i < n) {
        const QChar c = src[i];
        if (c.isSpace()) {
            ++i;
            continue;
        }
        const int start = i;
        if (c == QLatin1Char('(')) {
            out.push_back({TokType::LParen, QStringLiteral("("), start});
            ++i;
        } else if (c == QLatin1Char(')')) {
            out.push_back({TokType::RParen, QStringLiteral(")"), start});
            ++i;
        } else if (c == QLatin1Char('&')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('&')) {
                out.push_back({TokType::And, QStringLiteral("&&"), start});
                i += 2;
            } else {
                throw ParseException{QStringLiteral("expected '&&'"), start};
            }
        } else if (c == QLatin1Char('|')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('|')) {
                out.push_back({TokType::Or, QStringLiteral("||"), start});
                i += 2;
            } else {
                throw ParseException{QStringLiteral("expected '||'"), start};
            }
        } else if (c == QLatin1Char('=')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('=')) {
                out.push_back({TokType::OpEq, QStringLiteral("=="), start});
                i += 2;
            } else {
                throw ParseException{QStringLiteral("expected '=='"), start};
            }
        } else if (c == QLatin1Char('!')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('=')) {
                out.push_back({TokType::OpNe, QStringLiteral("!="), start});
                i += 2;
            } else {
                out.push_back({TokType::Not, QStringLiteral("!"), start});
                ++i;
            }
        } else if (c == QLatin1Char('<')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('=')) {
                out.push_back({TokType::OpLe, QStringLiteral("<="), start});
                i += 2;
            } else {
                out.push_back({TokType::OpLt, QStringLiteral("<"), start});
                ++i;
            }
        } else if (c == QLatin1Char('>')) {
            if (i + 1 < n && src[i + 1] == QLatin1Char('=')) {
                out.push_back({TokType::OpGe, QStringLiteral(">="), start});
                i += 2;
            } else {
                out.push_back({TokType::OpGt, QStringLiteral(">"), start});
                ++i;
            }
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const QChar quote = c;
            ++i;  // opening quote
            QString text;
            bool closed = false;
            while (i < n) {
                if (src[i] == QLatin1Char('\\') && i + 1 < n) {
                    text.append(src[i + 1]);
                    i += 2;
                    continue;
                }
                if (src[i] == quote) {
                    closed = true;
                    ++i;
                    break;
                }
                text.append(src[i]);
                ++i;
            }
            if (!closed) {
                throw ParseException{QStringLiteral("unterminated string"), start};
            }
            out.push_back({TokType::Str, text, start});
        } else if (isWordChar(c)) {
            int j = i;
            while (j < n && isWordChar(src[j])) {
                ++j;
            }
            out.push_back({TokType::Word, src.mid(i, j - i), start});
            i = j;
        } else {
            throw ParseException{QStringLiteral("unexpected character '%1'").arg(c), start};
        }
    }
    out.push_back({TokType::End, QString(), n});
    return out;
}

}  // namespace

/// AST node. Public-by-name (FilterExpr::Node) but opaque to callers.
struct FilterExpr::Node {
    Kind kind;
    // Compare:
    QString field;
    Op op = Op::Eq;
    LitKind litKind = LitKind::String;
    FieldValue literal;
    // And/Or/Not:
    std::shared_ptr<const Node> left;
    std::shared_ptr<const Node> right;
};

namespace {

using NodePtr = std::shared_ptr<const FilterExpr::Node>;

/// Recursive-descent parser over a token vector.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : toks_(std::move(tokens)) {}

    NodePtr parse() {
        NodePtr e = parseOr();
        if (peek().type != TokType::End) {
            throw ParseException{QStringLiteral("unexpected '%1'").arg(peek().text), peek().pos};
        }
        return e;
    }

private:
    const Token& peek() const {
        return toks_[idx_];
    }
    const Token& advance() {
        return toks_[idx_++];
    }
    bool accept(TokType t) {
        if (peek().type == t) {
            ++idx_;
            return true;
        }
        return false;
    }

    NodePtr parseOr() {
        NodePtr left = parseAnd();
        while (accept(TokType::Or)) {
            NodePtr right = parseAnd();
            auto node = std::make_shared<FilterExpr::Node>();
            node->kind = Kind::Or;
            node->left = left;
            node->right = right;
            left = node;
        }
        return left;
    }

    NodePtr parseAnd() {
        NodePtr left = parseUnary();
        while (accept(TokType::And)) {
            NodePtr right = parseUnary();
            auto node = std::make_shared<FilterExpr::Node>();
            node->kind = Kind::And;
            node->left = left;
            node->right = right;
            left = node;
        }
        return left;
    }

    NodePtr parseUnary() {
        if (accept(TokType::Not)) {
            auto node = std::make_shared<FilterExpr::Node>();
            node->kind = Kind::Not;
            node->left = parseUnary();
            return node;
        }
        return parsePrimary();
    }

    NodePtr parsePrimary() {
        if (accept(TokType::LParen)) {
            NodePtr e = parseOr();
            if (!accept(TokType::RParen)) {
                throw ParseException{QStringLiteral("expected ')'"), peek().pos};
            }
            return e;
        }
        return parseComparison();
    }

    NodePtr parseComparison() {
        const Token& fieldTok = peek();
        if (fieldTok.type != TokType::Word) {
            throw ParseException{QStringLiteral("expected a field name"), fieldTok.pos};
        }
        advance();

        Op op{};
        const Token& opTok = peek();
        switch (opTok.type) {
        case TokType::OpEq:
            op = Op::Eq;
            advance();
            break;
        case TokType::OpNe:
            op = Op::Ne;
            advance();
            break;
        case TokType::OpLt:
            op = Op::Lt;
            advance();
            break;
        case TokType::OpLe:
            op = Op::Le;
            advance();
            break;
        case TokType::OpGt:
            op = Op::Gt;
            advance();
            break;
        case TokType::OpGe:
            op = Op::Ge;
            advance();
            break;
        case TokType::Word:
            if (opTok.text.compare(QLatin1String("contains"), Qt::CaseInsensitive) == 0) {
                op = Op::Contains;
                advance();
                break;
            }
            throw ParseException{QStringLiteral("expected a comparison operator"), opTok.pos};
        default:
            throw ParseException{QStringLiteral("expected a comparison operator"), opTok.pos};
        }

        const Token& litTok = peek();
        auto node = std::make_shared<FilterExpr::Node>();
        node->kind = Kind::Compare;
        node->field = fieldTok.text;
        node->op = op;
        if (litTok.type == TokType::Str) {
            node->litKind = LitKind::String;
            node->literal = litTok.text;
            advance();
        } else if (litTok.type == TokType::Word) {
            const QString& w = litTok.text;
            if (w.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
                node->litKind = LitKind::Bool;
                node->literal = true;
            } else if (w.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
                node->litKind = LitKind::Bool;
                node->literal = false;
            } else {
                bool numeric = false;
                const double d = w.toDouble(&numeric);
                if (numeric) {
                    node->litKind = LitKind::Number;
                    node->literal = d;
                } else {
                    node->litKind = LitKind::String;
                    node->literal = w;
                }
            }
            advance();
        } else {
            throw ParseException{QStringLiteral("expected a value"), litTok.pos};
        }
        return node;
    }

    std::vector<Token> toks_;
    std::size_t idx_ = 0;
};

double toDouble(const FieldValue& v, bool& ok) {
    if (const auto* d = std::get_if<double>(&v)) {
        ok = true;
        return *d;
    }
    if (const auto* b = std::get_if<bool>(&v)) {
        ok = true;
        return *b ? 1.0 : 0.0;
    }
    return std::get<QString>(v).toDouble(&ok);
}

bool toBool(const FieldValue& v) {
    if (const auto* d = std::get_if<double>(&v)) {
        return *d != 0.0;
    }
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b;
    }
    const QString& s = std::get<QString>(v);
    return s.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || s == QLatin1String("1");
}

QString toStr(const FieldValue& v) {
    if (const auto* d = std::get_if<double>(&v)) {
        return QString::number(*d, 'g', 12);
    }
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b ? QStringLiteral("true") : QStringLiteral("false");
    }
    return std::get<QString>(v);
}

bool numCompare(Op op, double a, double b) {
    switch (op) {
    case Op::Eq:
        return a == b;
    case Op::Ne:
        return a != b;
    case Op::Lt:
        return a < b;
    case Op::Le:
        return a <= b;
    case Op::Gt:
        return a > b;
    case Op::Ge:
        return a >= b;
    case Op::Contains:
        return false;  // handled before this call
    }
    return false;
}

bool ordCompare(Op op, int cmp) {
    switch (op) {
    case Op::Eq:
        return cmp == 0;
    case Op::Ne:
        return cmp != 0;
    case Op::Lt:
        return cmp < 0;
    case Op::Le:
        return cmp <= 0;
    case Op::Gt:
        return cmp > 0;
    case Op::Ge:
        return cmp >= 0;
    case Op::Contains:
        return false;  // handled before this call
    }
    return false;
}

bool evalNode(const FilterExpr::Node& node, const FieldLookup& lookup) {
    switch (node.kind) {
    case Kind::And:
        return evalNode(*node.left, lookup) && evalNode(*node.right, lookup);
    case Kind::Or:
        return evalNode(*node.left, lookup) || evalNode(*node.right, lookup);
    case Kind::Not:
        return !evalNode(*node.left, lookup);
    case Kind::Compare:
        break;
    }

    const std::optional<FieldValue> fv = lookup(node.field);
    if (!fv) {
        return false;  // missing field never matches
    }
    if (node.op == Op::Contains) {
        return toStr(*fv).contains(toStr(node.literal), Qt::CaseInsensitive);
    }
    switch (node.litKind) {
    case LitKind::Number: {
        bool ok = false;
        const double d = toDouble(*fv, ok);
        return ok && numCompare(node.op, d, std::get<double>(node.literal));
    }
    case LitKind::Bool: {
        const bool b = toBool(*fv);
        const bool lit = std::get<bool>(node.literal);
        if (node.op == Op::Eq) {
            return b == lit;
        }
        if (node.op == Op::Ne) {
            return b != lit;
        }
        return numCompare(node.op, b ? 1.0 : 0.0, lit ? 1.0 : 0.0);
    }
    case LitKind::String:
        return ordCompare(node.op, QString::compare(toStr(*fv), std::get<QString>(node.literal), Qt::CaseInsensitive));
    }
    return false;
}

}  // namespace

FilterExpr::ParseResult FilterExpr::parse(const QString& source) {
    ParseResult result;
    if (source.trimmed().isEmpty()) {
        result.expr = FilterExpr{};  // match-all
        return result;
    }
    try {
        Parser parser(tokenize(source));
        NodePtr root = parser.parse();
        result.expr = FilterExpr{root};
    } catch (const ParseException& e) {
        result.error = e.message;
        result.errorPos = e.pos;
    }
    return result;
}

bool FilterExpr::matches(const FieldLookup& lookup) const {
    if (root_ == nullptr) {
        return true;
    }
    return evalNode(*root_, lookup);
}

}  // namespace signalforge::query
