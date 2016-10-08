#include "simpleon.hpp"

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

class DoubleData : public IData {
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
};

class DictData : public IData {
public:
    map<string, IData *> value;
    Type GetType() override { return T_DICT; }
    const map<string, IData *> & GetDict() override { return value; }
};

#define IS_SPECIAL_CHAR(c) ((c) == '[' || (c) == ']' || (c) == '{' || (c) == '}' || (c) == ':' || (c) == '"' || (c) == '\\' || (c) == ',')

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
    
    string _buf;
    int _readPos;
    vector<string>  _keyStack;
    vector<IData *> _valueStack;
    vector<State>   _stateStack;
    
public:

    SimpleONParser() {
        _readPos = 0;
        _stateStack.push_back(STATE_ELEMENT_START);
    }
    
    void ParseLine(const string & line) {
        if (_stackStack.size() == 0) return;
        
        _buf.append(line);
        ParseBuf();
    }

    void ParseBuf() {
        if (_stateStack.size() == 0) return;
        int limit = _buf.size();
        while (_readPos < limit) {

            auto state = _stateStack.back();
            switch (state) {
            case STATE_ELEMENT_END: {
                auto value = _valueStack.back();
                
                _stateStack.pop_back();
                if (_stateStack.size() == 0) break;
                _valueStack.pop_back();
                
                switch (_stateStack.back()) {
                case STATE_DICT_KEY:
                    _stateStack.back() = STATE_DICT_POST_KEY;
                    _keyStack.push_back(_valueStack.GetString());
                    break;
                    
                case STATE_DICT_VALUE:
                    (*(DictData *)_valueStack.back())[_keyStack.back()] = value;
                    _keyStack.pop_back();
                    _stateStack.back() = STATE_DICT_POST_VALUE;
                    break;
                    
                case STATE_LIST:
                    (*(ListData *)_valueStack.back()).push_back(value);
                    break;

                default:
                    throw logic_error("invalid state to insert element");
                }
                break;
            }
            case STATE_QUOTED_STRING: {
                int s = _readPos;
                while (s < limit && _buf[s] != '"' && _buf[s] != '\\') ++s;
                (*(StringData *)_valueStack.back()).value.append(_buf, _readPos, s - _readPos);
                
                if (s == limit) {
                    (*(StringData *)_valueStack.back()).value.append(_buf, _readPos, limit - _readPos);
                    _readPos = limit;
                    _stateStack.back() = STATE_ELEMENT_END;
                }
                else {
                    (*(StringData *)_valueStack.back()).value.append(_buf, _readPos, s - _readPos);
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
                int s = _readPos;
                while (s < limit && _buf[s] != '"' && _buf[s] != '\\') ++s;
                (*(StringData *)_valueStack.back()).value.append(_buf, _readPos, s - _readPos);
                
                if (s == limit) {
                    (*(StringData *)_valueStack.back()).value.append(1, '\n');
                    _readPos = s;
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
                        (*(StringData *)_valueStack.back()).value.append(1, '"');
                        _readPos = s + 1;
                    }
                }
                break;
            }
            case STATE_DICT_PRE_KEY: {
                int s = _readPos;
                while (s < limit && _buf[s] != ' ' && _buf[s] != '\t') ++s;

                if (s == limit) {
                    _readPos = s;
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
                    throw logic_error("format error -expecting dict key or end")
                }
                
                break;
            }
            case STATE_DICT_KEY: {
                break;
            }
            case STATE_DICT_POST_KEY: {
                int s = _readPos;
                while (s < limit && _buf[s] != ' ' && _buf[s] != '\t') ++s;

                if (s == limit) {
                    _readPos = s;
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
                    throw logic_error("format error - expecting key-value-separator");
                }
                
                break;
            }
            case STATE_DICT_VALUE: {
                break;
            }
            case STATE_DICT_POST_VALUE: {
                int s = _readPos;
                while (s < limit && _buf[s] != ' ' && _buf[s] != '\t') ++s;

                if (s == limit) {
                    _readPos = s;
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
                    throw logic_error("format error in dict");
                }
                
                break;
            }
            case STATE_LIST: {
                int s = _readPos;
                while (s < limit && _buf[s] != ' ' && _buf[s] != '\t') ++s;
                
                if (s == limit) {
                    _readPos = s;
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
                    _stateStack.push_back(STATE_ELEMENT_STARRT);
                }
                
                break;
            }
            case STATE_ELEMENT_START: {
                int s = _readPos;
                while (s < limit && _buf[s] != ' ' && _buf[s] != '\t') ++s;

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
                    _stackStack.back() = STATE_LIST;
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
                    
                }
                
                break;
            }
            }
        }
    }
};
