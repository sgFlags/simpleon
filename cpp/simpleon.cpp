#include "simpleon.hpp"
#include <vector>
#include <iostream>
#include <deque>
#include <iomanip>

using namespace simpleon;
using namespace std;

string IData_DUMMY_STRING;
list<IData *> IData_DUMMY_LIST;
map<string, IData *> IData_DUMMY_MAP;

bool IData::GetBool() { return false; }
int IData::GetInt() { return 0; }
double IData::GetFloat() { return 0.0; }
const string & IData::GetString() { return IData_DUMMY_STRING; }
const list<IData *> & IData::GetList() { return IData_DUMMY_LIST; }
const map<string, IData *> & IData::GetDict() { return IData_DUMMY_MAP; }

class NullData : public IData {
public:
    Type GetType() override { return T_NULL; }
};

class IntData : public IData {
public:
    int value;
    Type GetType() override { return T_INT; }
    int GetInt() override { return value; } 
};

class BoolData : public IData {
public:
    bool value;
    Type GetType() override { return T_BOOL; }
    bool GetBool() override { return value; } 
};

class FloatData : public IData {
public:
    double value;
    Type GetType() override { return T_FLOAT; }
    double GetFloat() override { return value; } 
};

class StringData : public IData {
private:
    bool _quoted;
public:
    StringData(bool quoted) : _quoted(quoted) {}
    string value;
    Type GetType() override { return _quoted ? T_STRING : T_UQ_STRING; }
    const string & GetString() override { return value; } 
};

class ListData : public IData {
public:
    list<IData *> value;
    Type GetType() override { return T_LIST; }
    const list<IData *> & GetList() override { return value; }
    ~ListData() override {
        for (auto d : value)
            delete d;
    }
};

class DictData : public IData {
public:
    map<string, IData *> value;
    Type GetType() override { return T_DICT; }
    const map<string, IData *> & GetDict() override { return value; }
    ~DictData() override {
        for (auto && kv : value)
            delete kv.second;
    }
};

ParseException::ParseException(const char * w) : _what(w) { }
const char * ParseException::what() const noexcept { return _what.c_str(); }

template<typename I>
struct CharMap {
    I m[256];

    CharMap(const char * v, I defaultValue, I setValue) {
        for (int i = 0; i < 256; ++i) m[i] = defaultValue;
        while (*v) {
            m[*v] = setValue;
            ++v;
        }
    }

    CharMap(const char * v, I defaultValue, const initializer_list<I> & l) {
        for (int i = 0; i < 256; ++i) m[i] = defaultValue;
        auto it = begin(l);
        while (*v && it != end(l)) {
            m[*v] = *it;
            ++v; ++it;
        }
    }
};

static const CharMap<bool> SPECIAL_CHARS(" \t[]{}:\",#", false, true);
static const CharMap<bool> NUM_CHARS("+-.0123456789", false, true);
static const CharMap<int> HEX_TRANSLATE("0123456789abcdefABCDEF", -1,
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 10, 11, 12, 13, 14, 15}
);

#define IS_SPECIAL_CHAR(c) (SPECIAL_CHARS.m[c])
#define BUF_CLEAN_THRESHOLD 4096

class SimpleONParser : public IParser {
private:

    enum State {
        STATE_ELEMENT_START,
        STATE_ELEMENT_END,
        STATE_DICT_PRE_KEY,
        STATE_DICT_KEY,
        STATE_DICT_POST_KEY,
        STATE_DICT_VALUE,
        STATE_DICT_POST_VALUE,
        STATE_LIST,
        STATE_QUOTED_STRING,
        STATE_QUOTELESS_STRING,
        STATE_MULTILINE_STRING
    };
    
    bool            _convert;
    bool            _multi;
    bool            _sealed;
    string          _buf;
    size_t          _readPos;
    vector<string>  _keyStack;
    vector<IData *> _valueStack;
    vector<State>   _stateStack;
    deque<IData *>  _results;
    
public:

    SimpleONParser(bool convert, bool multi) {
        _convert = convert;
        _multi = multi;
        _sealed = false;
        _readPos = 0;
        _stateStack.push_back(STATE_ELEMENT_START);
    }
    
    void ParseLine(const string & line) override {
        if (_sealed) return;

        _buf.append(line);
        ParseBuf();
    }

    void Seal() override {
        if (_sealed) return;

        _sealed = true;
        ParseBuf();

        _readPos = 0;
        _buf.clear();
        _stateStack.clear();
        for (auto d : _valueStack) {
            delete d;
        }
        _valueStack.clear();
    }

    ~SimpleONParser() {
        Seal();
        for (auto d : _results) {
            delete d;
        }
    }

    void HandleEscape() {
        if (_readPos >= _buf.size()) return;
        auto && value = (*(StringData *)_valueStack.back()).value;
        switch (_buf[_readPos]) {
        case 'n':
            value += '\n';
            ++_readPos;
            break;
        case 't':
            value += '\t';
            ++_readPos;
            break;
        case 'r':
            value += '\r';
            ++_readPos;
            break;
        case 'f':
            value += '\f';
            ++_readPos;
            break;
        case 'x':
        {
            unsigned char c;
            int digit;

            if (_readPos + 2 >= _buf.size()) goto InputError;
            ++_readPos;
            digit = HEX_TRANSLATE.m[_buf[_readPos]]; if (digit < 0) goto InputError;
            c = digit; ++_readPos;
            digit = HEX_TRANSLATE.m[_buf[_readPos]]; if (digit < 0) goto InputError;
            c = (c << 4) + digit; ++_readPos;
            value += c;

            break;

        InputError:
            throw ParseException("Expect 2 hex chars for utf-8 escape");
        }
        case '/':
        case '\\':
        case '"':
            value += _buf[_readPos];
            ++_readPos;
            break;
        default:
            value += '\\';
            break;
        }
    }

    void HandleUnquotedString(size_t s, size_t e, bool convert) {
        if (convert) {
            string word(_buf, s, e - s);
            if (NUM_CHARS.m[_buf[s]]) {
                int vint;
                bool succ = true;
                size_t idx;

                try {
                    vint = stoi(word, &idx);
                    if (idx != e - s) succ = false;
                }
                catch (const exception &) {
                    succ = false;
                }

                if (succ) {
                    auto v = new IntData();
                    v->value = vint;
                    _valueStack.push_back(v);
                    return;
                }

                double dint;
                succ = true;

                try {
                    dint = stod(word, &idx);
                    if (idx != e - s) succ = false;
                }
                catch (const exception &) {
                    succ = false;
                }

                if (succ) {
                    auto v = new FloatData();
                    v->value = dint;
                    _valueStack.push_back(v);
                    return;
                }
            }
            else if (word[0] == 'n' && word == "null") {
                auto v = new NullData();
                _valueStack.push_back(v);
                return;
            }
            else if (word[0] == 't' && word == "true") {
                auto v = new BoolData();
                v->value = true;
                _valueStack.push_back(v);
                return;
            }
            else if (word[0] == 'f' && word == "false") {
                auto v = new BoolData();
                v->value = false;
                _valueStack.push_back(v);
                return;
            }
        }

        auto v = new StringData(false);
        v->value.assign(_buf, s, e - s);
        _valueStack.push_back(v);
    }

    void CleanBuf() {
        if (_readPos > _buf.size()) _readPos = _buf.size(); 
        if (_readPos > BUF_CLEAN_THRESHOLD) {
            _buf = _buf.substr(_readPos, _buf.size() - _readPos);
            _readPos = 0;
        }
    }

    void ParseBuf() {
        size_t limit = _buf.size();
        while (_readPos < limit) {
            if (_stateStack.size() == 0) {
                Seal();
                return;
            }

            auto state = _stateStack.back();
            
            switch (state) {
            case STATE_ELEMENT_END: {
                auto value = _valueStack.back();
                
                _stateStack.pop_back();
                _valueStack.pop_back();
				
				if (_stateStack.size() == 0) {
                    _results.push_back(value);

                    if (_multi) {
                        _stateStack.push_back(STATE_ELEMENT_START);
                    }

                    break;
                }
                
                switch (_stateStack.back()) {
                case STATE_DICT_KEY:
                    _stateStack.back() = STATE_DICT_POST_KEY;
                    _keyStack.push_back(value->GetString());
                    delete value;
                    break;
                    
                case STATE_DICT_VALUE:
                    ((DictData *)_valueStack.back())->value[_keyStack.back()] = value;
                    _keyStack.pop_back();
                    _stateStack.back() = STATE_DICT_POST_VALUE;
                    break;
                    
                case STATE_LIST:
                    ((ListData *)_valueStack.back())->value.push_back(value);
                    break;

                default:
                    throw ParseException("invalid state to insert element");
                }
                break;
            }
            case STATE_QUOTED_STRING: {
                size_t s = _readPos;
                while (s < limit && _buf[s] != '"' && _buf[s] != '\\') ++s;

                ((StringData *)_valueStack.back())->value.append(_buf, _readPos, s - _readPos);
                
                if (s >= limit) {
                    _readPos = limit;
                    _stateStack.back() = STATE_ELEMENT_END;
                }
                else {
                    if (_buf[s] == '\\') {
                        _readPos = s + 1;
                        HandleEscape();
                    }
                    else {
                        _readPos = s + 1;
                        _stateStack.back() = STATE_ELEMENT_END;
                    }
                }
                break;
            }
            case STATE_MULTILINE_STRING: {
                size_t s = _readPos;
                while (s < limit && _buf[s] != '"' && _buf[s] != '\\') ++s;
                ((StringData *)_valueStack.back())->value.append(_buf, _readPos, s - _readPos);
                
                if (s >= limit) {
                    ((StringData *)_valueStack.back())->value += '\n';
                    _readPos = limit;
                }
                else {
                    if (_buf[s] == '\\') {
                        _readPos = s + 1;
                        HandleEscape();
                    }
                    else if (s + 2 < limit && _buf[s + 1] == '"' && _buf[s + 2] == '"') {
                        _readPos = s + 3;
                        _stateStack.back() = STATE_ELEMENT_END;
                    }
                    else {
                        ((StringData *)_valueStack.back())->value += '"';
                        _readPos = s + 1;
                    }
                }
                break;
            }
            case STATE_DICT_PRE_KEY: {
                size_t s = _readPos;
                while (s < limit && (_buf[s] == ' ' || _buf[s] == '\t')) ++s;

                if (s >= limit) {
                    _readPos = limit;
                }
                else if (_buf[s] == '"' || !IS_SPECIAL_CHAR(_buf[s])) {
                    _readPos = s;
                    _stateStack.back() = STATE_DICT_KEY;
                    _stateStack.push_back(STATE_ELEMENT_START);
                }
                else if (_buf[s] == '}') {
                    _stateStack.back() = STATE_ELEMENT_END;
                    _readPos = s + 1;
                }
                else if (_buf[s] == '#') {
                    _readPos = limit;
                }
                else {
                    throw ParseException("format error - expecting dict key or end");
                }
                
                break;
            }
            case STATE_DICT_KEY: {
                break;
            }
            case STATE_DICT_POST_KEY: {
                size_t s = _readPos;
                while (s < limit && (_buf[s] == ' ' || _buf[s] == '\t')) ++s;

                if (s >= limit) {
                    _readPos = limit;
                }
                else if (_buf[s] == ':') {
                    _readPos = s + 1;
                    _stateStack.back() = STATE_DICT_VALUE;
                    _stateStack.push_back(STATE_ELEMENT_START);
                }
                else if (_buf[s] == '#') {
                    _readPos = limit;
                }
                else {
                    throw ParseException("format error - expecting key-value-separator");
                }
                
                break;
            }
            case STATE_DICT_VALUE: {
                break;
            }
            case STATE_DICT_POST_VALUE: {
                size_t s = _readPos;
                while (s < limit && (_buf[s] == ' ' || _buf[s] == '\t')) ++s;

                if (s >= limit) {
                    _readPos = limit;
                }
                else if (_buf[s] == '"' || !IS_SPECIAL_CHAR(_buf[s])) {
                    _stateStack.back() = STATE_DICT_PRE_KEY;
                    _readPos = s;
                }
                else if (_buf[s] == ',') {
                    _stateStack.back() = STATE_DICT_PRE_KEY;
                    _readPos = s + 1;
                }
                else if (_buf[s] == '}') {
                    _stateStack.back() = STATE_ELEMENT_END;
                    _readPos = s + 1;
                }
                else if (_buf[s] == '#') {
                    _readPos = limit;
                }
                else {
                    throw ParseException("format error in dict");
                }
                
                break;
            }
            case STATE_LIST: {
                size_t s = _readPos;
                while (s < limit && (_buf[s] == ' ' || _buf[s] == '\t')) ++s;

                if (s >= limit) {
                    _readPos = limit;
                }
                else if (_buf[s] == ']') {
                    _stateStack.back() = STATE_ELEMENT_END;
                    _readPos = s + 1;
                }
                else if (_buf[s] == ',') {
                    _readPos = s + 1;
                    _stateStack.push_back(STATE_ELEMENT_START);
                }
                else if (_buf[s] == '#') {
                    _readPos = limit;
                }
                else {
                    _readPos = s;
                    _stateStack.push_back(STATE_ELEMENT_START);
                }
                
                break;
            }
            case STATE_ELEMENT_START: {
                size_t s = _readPos;
                while (s < limit && (_buf[s] == ' ' || _buf[s] == '\t')) ++s;

                if (s >= limit) {
                    _readPos = limit;
                }
                else if (_buf[s] == '{') {
                    _valueStack.push_back(new DictData());
                    _stateStack.back() = STATE_DICT_PRE_KEY;
                    _readPos = s + 1;
                }
                else if (_buf[s] == '[') {
                    _valueStack.push_back(new ListData());
                    _stateStack.back() = STATE_LIST;
                    _readPos = s + 1;
                }
                else if (_buf[s] == '"') {
                    _valueStack.push_back(new StringData(true));
                    if (s + 2 < limit &&
                        _buf[s + 1] == '"' && _buf[s + 2] == '"') {
                        _stateStack.back() = STATE_MULTILINE_STRING;
                        _readPos = s + 3;
                    }
                    else {
                        _stateStack.back() = STATE_QUOTED_STRING;
                        _readPos = s + 1;
                    }
                }
                else if (_buf[s] == '#') {
                    _readPos = limit;
                }
                else {
                    size_t e = s;
                    while (e < limit && !IS_SPECIAL_CHAR(_buf[e])) ++e;

                    if (_stateStack.size() > 1 && _stateStack[_stateStack.size() - 2] == STATE_DICT_KEY) {
                        HandleUnquotedString(s, e, false);
                    }
                    else {
                        HandleUnquotedString(s, e, _convert);
                    }

                    _stateStack.back() = STATE_ELEMENT_END;
                    _readPos = e;
                }
                
                break;
            }
            }
        }

        CleanBuf();
    }
    
    IData * Extract() override {
        if (_results.size() > 0) {
            auto v = _results.front();
            _results.pop_front();
            return v;
        }
        else {
            return nullptr;
        }
    }
};

IParser * simpleon::CreateSimpleONParser(bool convert, bool multi) {
    return new SimpleONParser(convert, multi);
}

void simpleon::Dump(ostream & o, IData * d) {
    if (d == nullptr) {
        o << "(Null)";
        return;
    }

    switch (d->GetType()) {
    case IData::T_NULL:
        o << "null";
        break;
    case IData::T_BOOL:
        o << d->GetBool() ? "true" : "false";
        break;
    case IData::T_INT:
        o << d->GetInt();
        break;
    case IData::T_FLOAT:
        o << d->GetFloat();
        break;
    case IData::T_STRING:
    case IData::T_UQ_STRING:
        o << quoted(d->GetString());
        break;
    case IData::T_LIST: {
        o << '[';
        bool first = true;
        for (IData * ele : d->GetList()) {
            if (first) first = false;
            else o << ',';
            Dump(o, ele);
        }
        o << ']';
        break;
    }
    case IData::T_DICT: {
        o << '{';
        bool first = true;
        for (auto && kv : d->GetDict()) {
            if (first) first = false;
            else o << ',';
            o << quoted(kv.first) << ':';
            Dump(o, kv.second);
        }
        o << '}';
        break;
    }
    }
}
